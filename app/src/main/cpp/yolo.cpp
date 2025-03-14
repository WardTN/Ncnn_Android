//
// Created by deqi_chen on 2024/7/24.
//

#include "yolo.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "cpu.h"

/**
 * 快速计算指数函数实现
 * @param x
 * @return
 */
static float fast_exp(float x) {
    union {
        uint32_t i;
        float f;
    } v{};
    v.i = (1 << 23) * (1.4426950409 * x + 126.93490512f);
    return v.f;
}

/**
 * Sigmod 激活函数
 * @param x
 * @return
 */
static float sigmoid(float x) {
    return 1.0f / (1.0f + fast_exp(-x));
}

/**
 * 计算两个矩形区域交集面积的函数
 * @param a
 * @param b
 * @return
 */
static float intersection_area(const Object& a, const Object& b){
    cv::Rect_<float> inter = a.rect & b.rect;
    return inter.area();
}

static void qsort_descent_inplace(std::vector<Object>& faceobjects, int left, int right){
    int i = left;
    int j = right;
    float p = faceobjects[(left + right) / 2].prob;

    while (i <= j)
    {
        while (faceobjects[i].prob > p)
            i++;

        while (faceobjects[j].prob < p)
            j--;

        if (i <= j)
        {
            // swap
            std::swap(faceobjects[i], faceobjects[j]);

            i++;
            j--;
        }
    }

    //     #pragma omp parallel sections
    {
        //         #pragma omp section
        {
            if (left < j) qsort_descent_inplace(faceobjects, left, j);
        }
        //         #pragma omp section
        {
            if (i < right) qsort_descent_inplace(faceobjects, i, right);
        }
    }
}


static void qsort_descent_inplace(std::vector<Object>& faceobjects)
{
    if (faceobjects.empty())
        return;

    qsort_descent_inplace(faceobjects, 0, faceobjects.size() - 1);
}

/**
 * 实现非极大值 抑制 NMS 的目的 是去除多余的重叠框，只保留最有代表性的框
 * @param faceobjects
 * @param picked
 * @param nms_threshold
 */
static void nms_sorted_bboxes(const std::vector<Object>& faceobjects, std::vector<int>& picked, float nms_threshold){
    picked.clear();

    const int n = faceobjects.size();

    std::vector<float> areas(n);
    for (int i = 0; i < n; i++)
    {
        areas[i] = faceobjects[i].rect.width * faceobjects[i].rect.height;
    }

    for (int i = 0; i < n; i++)
    {
        const Object& a = faceobjects[i];

        int keep = 1;
        for (int j = 0; j < (int)picked.size(); j++)
        {
            const Object& b = faceobjects[picked[j]];

            // intersection over union
            float inter_area = intersection_area(a, b);
            float union_area = areas[i] + areas[picked[j]] - inter_area;
            // float IoU = inter_area / union_area
            if (inter_area / union_area > nms_threshold)
                keep = 0;
        }

        if (keep)
            picked.push_back(i);
    }
}

static void generate_grids_and_stride(const int target_w, const int target_h, std::vector<int>& strides, std::vector<GridAndStride>& grid_strides)
{
    for (int i = 0; i < (int)strides.size(); i++)
    {
        int stride = strides[i];
        int num_grid_w = target_w / stride;
        int num_grid_h = target_h / stride;
        for (int g1 = 0; g1 < num_grid_h; g1++)
        {
            for (int g0 = 0; g0 < num_grid_w; g0++)
            {
                GridAndStride gs;
                gs.grid0 = g0;
                gs.grid1 = g1;
                gs.stride = stride;
                grid_strides.push_back(gs);
            }
        }
    }
}

/**
 *  基于预测结果生成检测框的功能
 *  它遍历每个预测点，找到置信度最高的类别
 *  并计算出对应的检测框坐标，然后根据置信度阈值筛选有效的检测框，并将其存储到 objects 向量中
 * @param grid_strides
 * @param pred
 * @param prob_threshold
 * @param objects
 */
static void generate_proposals(std::vector<GridAndStride> grid_strides, const ncnn::Mat& pred, float prob_threshold, std::vector<Object>& objects)
{
    const int num_points = grid_strides.size(); // 表示预测点的数量
    const int num_class = 80; // 表示类别的数量
    const int reg_max_1 = 16; // 表示回归最大值

    // 遍历每个预测点
    for (int i = 0; i < num_points; i++)
    {
        // 获取每个预测点的得分
        const float* scores = pred.row(i) + 4 * reg_max_1;

        // find label with max score 找到置信度最高的类比
        int label = -1;
        float score = -FLT_MAX;
        for (int k = 0; k < num_class; k++)
        {
            float confidence = scores[k];
            if (confidence > score)
            {
                label = k;
                score = confidence;
            }
        }


        // 计算置信度并筛选有效检测框
        // 通过 Sigmoid 函数计算置信度，并与阈值进行比较
        float box_prob = sigmoid(score);
        if (box_prob >= prob_threshold)
        {
            // 进行Softmax 处理
            ncnn::Mat bbox_pred(reg_max_1, 4, (void*)pred.row(i));
            {
                ncnn::Layer* softmax = ncnn::create_layer("Softmax");

                ncnn::ParamDict pd;
                pd.set(0, 1); // axis
                pd.set(1, 1);
                softmax->load_param(pd);

                ncnn::Option opt;
                opt.num_threads = 1;
                opt.use_packing_layout = false;

                softmax->create_pipeline(opt);

                softmax->forward_inplace(bbox_pred, opt);

                softmax->destroy_pipeline(opt);

                delete softmax;
            }

            // 计算预测框的坐标
            float pred_ltrb[4];
            for (int k = 0; k < 4; k++)
            {
                float dis = 0.f;
                const float* dis_after_sm = bbox_pred.row(k);
                for (int l = 0; l < reg_max_1; l++)
                {
                    dis += l * dis_after_sm[l];
                }

                pred_ltrb[k] = dis * grid_strides[i].stride;
            }

            // 计算中心点和框的边界坐标
            float pb_cx = (grid_strides[i].grid0 + 0.5f) * grid_strides[i].stride;
            float pb_cy = (grid_strides[i].grid1 + 0.5f) * grid_strides[i].stride;

            float x0 = pb_cx - pred_ltrb[0];
            float y0 = pb_cy - pred_ltrb[1];
            float x1 = pb_cx + pred_ltrb[2];
            float y1 = pb_cy + pred_ltrb[3];


            // 创建 Object 实例并存储
            Object obj;
            obj.rect.x = x0;
            obj.rect.y = y0;
            obj.rect.width = x1 - x0;
            obj.rect.height = y1 - y0;
            obj.label = label;
            obj.prob = box_prob;

            objects.push_back(obj);
        }
    }
}

Yolo::Yolo()
{
    blob_pool_allocator.set_size_compare_ratio(0.f);
    workspace_pool_allocator.set_size_compare_ratio(0.f);
}


/**
 *
 * @param mgr  AAssetManager 对象，用于从 Android 资产目录中加载文件。
 * @param modeltype
 * @param _target_size
 * @param _mean_vals
 * @param _norm_vals
 * @param use_gpu
 * @return
 */
int Yolo::load(AAssetManager* mgr, const char* modeltype, int _target_size, const float* _mean_vals, const float* _norm_vals, bool use_gpu)
{

    // 清理之前的模型和分配器：确保重新加载模型时没有遗留的内存或资源。
    yolo.clear();
    blob_pool_allocator.clear();
    workspace_pool_allocator.clear();

    // 设置 CPU 电源保存模式和 OpenMP 线程数：优化计算性能，使用大核（big cores）进行计算
    ncnn::set_cpu_powersave(2);
    ncnn::set_omp_num_threads(ncnn::get_big_cpu_count());

    // 配置 ncnn 选项
    // use_vulkan_compute：如果使用 GPU，设置为 true（需要在编译时启用 Vulkan 支持）。
    // num_threads：设置线程数为大核数。
    // blob_allocator 和 workspace_allocator：设置内存分配器。
    yolo.opt = ncnn::Option();

#if NCNN_VULKAN
    yolo.opt.use_vulkan_compute = use_gpu;
#endif

    yolo.opt.num_threads = ncnn::get_big_cpu_count();
    yolo.opt.blob_allocator = &blob_pool_allocator;
    yolo.opt.workspace_allocator = &workspace_pool_allocator;

    // parampath: 参数文件路径
    // modelpath: 模型文件路径
    char parampath[256];
    char modelpath[256];
    sprintf(parampath, "yolov8%s.param", modeltype);
    sprintf(modelpath, "yolov8%s.bin", modeltype);

    yolo.load_param(mgr, parampath);
    yolo.load_model(mgr, modelpath);

    // 设置目标尺寸和预处理参数
    // target_size：输入图像的目标尺寸
    // mean_vals 和 norm_vals：图像预处理所需的均值和归一化值
    target_size = _target_size;
    mean_vals[0] = _mean_vals[0];
    mean_vals[1] = _mean_vals[1];
    mean_vals[2] = _mean_vals[2];
    norm_vals[0] = _norm_vals[0];
    norm_vals[1] = _norm_vals[1];
    norm_vals[2] = _norm_vals[2];

    return 0;
}

/**
 * 用于进行目标检测
 * 对输入图像的预处理、模型推理、后处理和结果筛选
 * @param rgb
 * @param objects
 * @param prob_threshold
 * @param nms_threshold
 * @return
 */
int Yolo::detect(const cv::Mat& rgb, std::vector<Object>& objects, float prob_threshold, float nms_threshold)
{
    // 图像尺寸调整
    // 将图像调整为目标大小 target_size 的正方形。保持原始图像的宽高比。
    // 缩放因子 scale：用于调整图像的宽度和高度
    int width = rgb.cols;
    int height = rgb.rows;

    // pad to multiple of 32
    int w = width;
    int h = height;
    float scale = 1.f;
    if (w > h)
    {
        scale = (float)target_size / w;
        w = target_size;
        h = h * scale;
    }
    else
    {
        scale = (float)target_size / h;
        h = target_size;
        w = w * scale;
    }

    // 图像填充 将调整后的图像填充到target_size 的矩形。填充大小为32的倍数，方便后续的卷积计算。
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(rgb.data, ncnn::Mat::PIXEL_RGB2BGR, width, height, w, h);

    // pad to target_size rectangle
    int wpad = (w + 31) / 32 * 32 - w;
    int hpad = (h + 31) / 32 * 32 - h;
    ncnn::Mat in_pad;
    ncnn::copy_make_border(in, in_pad, hpad / 2, hpad - hpad / 2, wpad / 2, wpad - wpad / 2, ncnn::BORDER_CONSTANT, 0.f);
    // 归一化
    in_pad.substract_mean_normalize(0, norm_vals);

    // 模型推理
    // 创建提取器：从 YOLO 对象中创建提取器
    ncnn::Extractor ex = yolo.create_extractor();
    // 输入数据：将预处理后的图像输入到模型中
    ex.input("images", in_pad);

    std::vector<Object> proposals;
    // 获取输出：提取模型的输出
    ncnn::Mat out;
    ex.extract("output", out);

    // 生成网格和候选框
    std::vector<int> strides = {8, 16, 32}; // might have stride=64
    std::vector<GridAndStride> grid_strides;

    // 生成网格和步幅：为不同的步幅生成网格
    // 生成候选框：从模型的输出中生成初步的候选框
    generate_grids_and_stride(in_pad.w, in_pad.h, strides, grid_strides);
    generate_proposals(grid_strides, out, prob_threshold, proposals);


    // 后处理
    // sort all proposals by score from highest to lowest
    // 排序：按分数对候选框进行降序排序。 确保在执行 NMS 时，优先考虑高置信度的候选框。
    qsort_descent_inplace(proposals);

    // apply nms with nms_threshold
    std::vector<int> picked;
    // 非极大值抑制 (NMS)：去除重叠度高于 nms_threshold 的重复候选框。
    nms_sorted_bboxes(proposals, picked, nms_threshold);

    // 调整偏移量到原始图像尺寸
    // 将候选框的坐标调整回原始图像的坐标系。这是因为在处理过程中图像会被缩放和填充。

    // 去除填充：从坐标中减去填充的偏移量。
    // 缩放：调整坐标到原始图像尺寸。
    // 裁剪：确保坐标值在图像边界内，以避免无效区域
    int count = picked.size();

    objects.resize(count);
    for (int i = 0; i < count; i++)
    {
        objects[i] = proposals[picked[i]];

        // adjust offset to original unpadded
        float x0 = (objects[i].rect.x - (wpad / 2)) / scale;
        float y0 = (objects[i].rect.y - (hpad / 2)) / scale;
        float x1 = (objects[i].rect.x + objects[i].rect.width - (wpad / 2)) / scale;
        float y1 = (objects[i].rect.y + objects[i].rect.height - (hpad / 2)) / scale;

        // clip
        x0 = std::max(std::min(x0, (float)(width - 1)), 0.f);
        y0 = std::max(std::min(y0, (float)(height - 1)), 0.f);
        x1 = std::max(std::min(x1, (float)(width - 1)), 0.f);
        y1 = std::max(std::min(y1, (float)(height - 1)), 0.f);

        objects[i].rect.x = x0;
        objects[i].rect.y = y0;
        objects[i].rect.width = x1 - x0;
        objects[i].rect.height = y1 - y0;
    }

    // 按检测框的面积对检测结果进行排序，较大的框排在前面。
    // sort objects by area
    struct
    {
        bool operator()(const Object& a, const Object& b) const
        {
            return a.rect.area() > b.rect.area();
        }
    } objects_area_greater;
    std::sort(objects.begin(), objects.end(), objects_area_greater);

    return 0;
}

int Yolo::draw(cv::Mat& rgb, const std::vector<Object>& objects)
{
    static const char* class_names[] = {
            "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
            "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
            "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
            "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
            "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
            "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
            "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
            "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
            "hair drier", "toothbrush"
    };

    static const unsigned char colors[19][3] = {
            { 54,  67, 244},
            { 99,  30, 233},
            {176,  39, 156},
            {183,  58, 103},
            {181,  81,  63},
            {243, 150,  33},
            {244, 169,   3},
            {212, 188,   0},
            {136, 150,   0},
            { 80, 175,  76},
            { 74, 195, 139},
            { 57, 220, 205},
            { 59, 235, 255},
            {  7, 193, 255},
            {  0, 152, 255},
            { 34,  87, 255},
            { 72,  85, 121},
            {158, 158, 158},
            {139, 125,  96}
    };

    int color_index = 0;

    for (size_t i = 0; i < objects.size(); i++)
    {
        const Object& obj = objects[i];

//         fprintf(stderr, "%d = %.5f at %.2f %.2f %.2f x %.2f\n", obj.label, obj.prob,
//                 obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height);

        const unsigned char* color = colors[color_index % 19];
        color_index++;

        cv::Scalar cc(color[0], color[1], color[2]);

        cv::rectangle(rgb, obj.rect, cc, 2);

        char text[256];
        sprintf(text, "%s %.1f%%", class_names[obj.label], obj.prob * 100);

        int baseLine = 0;
        cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

        int x = obj.rect.x;
        int y = obj.rect.y - label_size.height - baseLine;
        if (y < 0)
            y = 0;
        if (x + label_size.width > rgb.cols)
            x = rgb.cols - label_size.width;

        cv::rectangle(rgb, cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)), cc, -1);

        cv::Scalar textcc = (color[0] + color[1] + color[2] >= 381) ? cv::Scalar(0, 0, 0) : cv::Scalar(255, 255, 255);

        cv::putText(rgb, text, cv::Point(x, y + label_size.height), cv::FONT_HERSHEY_SIMPLEX, 0.5, textcc, 1);
    }

    return 0;
}











