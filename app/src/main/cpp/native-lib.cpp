#include <jni.h>
#include <string>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/bitmap.h>
#include <android/log.h>
#include <vector>
#include <platform.h>
#include <benchmark.h>
#include "ndkcamera.h"


#include "scrfd.h"
#include "yolo.h"
#include "YoloObb.h"


#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

// ncnn
#include "layer.h"
#include "net.h"
#include "common.h"

static ncnn::UnlockedPoolAllocator g_blob_pool_allocator;
static ncnn::PoolAllocator g_workspace_pool_allocator;
const int dstHeight = 32;//when use PP-OCRv3 it should be 48
ncnn::Net dbNet;
ncnn::Net crnnNet;
std::vector<std::string> keys;

#if __ARM_NEON

#include <arm_neon.h>

#endif // __ARM_NEON

using namespace cv;

#define SCRFD_TYPE                             200
#define YOLO_TYPE                             SCRFD_TYPE + 1


int paramType = SCRFD_TYPE;

static int draw_unsupported(cv::Mat &rgb) {
    const char text[] = "unsupported";

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 1.0, 1, &baseLine);

    int y = (rgb.rows - label_size.height) / 2;
    int x = (rgb.cols - label_size.width) / 2;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y),
                                cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0));

    return 0;
}

static int draw_fps(cv::Mat &rgb) {

    // resolve moving average
    float avg_fps = 0.f;
    {
        static double t0 = 0.f;
        static float fps_history[10] = {0.f};

        double t1 = ncnn::get_current_time();
        if (t0 == 0.f) {
            t0 = t1;
            return 0;
        }

        float fps = 1000.f / (t1 - t0);
        t0 = t1;

        for (int i = 9; i >= 1; i--) {
            fps_history[i] = fps_history[i - 1];
        }
        fps_history[0] = fps;

        if (fps_history[9] == 0.f) {
            return 0;
        }

        for (int i = 0; i < 10; i++) {
            avg_fps += fps_history[i];
        }
        avg_fps /= 10.f;
    }

    char text[32];
    sprintf(text, "FPS=%.2f", avg_fps);

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

    int y = 0;
    int x = rgb.cols - label_size.width;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y),
                                cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));

    return 0;

}

//char *readKeysFromAssets(AAssetManager *mgr) {
//    if (mgr == NULL) {
//        return NULL;
//    }
//    char *buffer;
//
//    AAsset *asset = AAssetManager_open(mgr, "paddleocr_keys.txt", AASSET_MODE_UNKNOWN);
//    if (asset == NULL) {
//        return NULL;
//    }
//
//    off_t bufferSize = AAsset_getLength(asset);
//    buffer = (char *) malloc(bufferSize + 1);
//    buffer[bufferSize] = 0;
//    int numBytesRead = AAsset_read(asset, buffer, bufferSize);
//    AAsset_close(asset);
//
//    return buffer;
//}

//std::vector<TextBox> findRsBoxes(const cv::Mat &fMapMat, const cv::Mat &norfMapMat,
//                                 const float boxScoreThresh, const float unClipRatio) {
//
//    float minArea = 3;
//    std::vector<TextBox> rsBoxs;
//    rsBoxs.clear();
//
//    std::vector<std::vector<cv::Point>> contours;
//    cv::findContours(norfMapMat, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
//
//    for (int i = 0; i < contours.size(); ++i) {
//        float minSideLen, perimeter;
//        std::vector<cv::Point> minBox = getMinBoxes(contours[i], minSideLen, perimeter);
//        if (minSideLen < minArea)
//            continue;
//        float score = boxScoreFast(fMapMat, contours[i]);
//        if (score < boxScoreThresh) {
//
//        }
//    }
//}


//std::vector<TextBox>
//getTextBoxes(const cv::Mat &src, float boxScoreThresh, float boxThresh, float unClipRatio) {
//    int width = src.cols;
//    int height = src.rows;
//    int target_size = 640;
//    // pad to multiple of 32
//    int w = width;
//    int h = height;
//    float scale = 1.f;
//    if (w > h) {
//        scale = (float) target_size / w;
//        w = target_size;
//        h = h * scale;
//    } else {
//        scale = (float) target_size / h;
//        h = target_size;
//        w = w * scale;
//    }
//
//    ncnn::Mat input = ncnn::Mat::from_pixels_resize(src.data, ncnn::Mat::PIXEL_RGB, width, height,
//                                                    w, h);
//
//    // pad to target_size rectangle
//    int wpad = (w + 31) / 32 * 32 - w;
//    int hpad = (h + 31) / 32 * 32 - h;
//    ncnn::Mat in_pad;
//    ncnn::copy_make_border(input, in_pad, hpad / 2, hpad - hpad / 2, wpad / 2, wpad - wpad / 2,
//                           ncnn::BORDER_CONSTANT, 0.f);
//
//    const float meanValues[3] = {0.485 * 255, 0.456 * 255, 0.406 * 255};
//    const float normValues[3] = {1.0 / 0.229 / 255.0, 1.0 / 0.224 / 255.0, 1.0 / 0.225 / 255.0};
//
//    in_pad.substract_mean_normalize(meanValues, normValues);
//    ncnn::Extractor extractor = dbNet.create_extractor();
//
//    extractor.input("input0", in_pad);
//    ncnn::Mat out;
//    extractor.extract("out1", out);
//
//    cv::Mat fMapMat(in_pad.h, in_pad.w, CV_32FC1, (float *) out.data);
//    cv::Mat norfMapMat;
//    norfMapMat = fMapMat > boxThresh;
//
//    cv::dilate(norfMapMat, norfMapMat, cv::Mat(), cv::Point(-1, -1), 1);
//
//    std::vector<TextBox> result = findRsBoxes(fMapMat, norfMapMat, boxScoreThresh, 2.0f);
//    for (int i = 0; i < result.size(); i++) {
//        for (int j = 0; j < result[i].boxPoint.size(); j++) {
//            float x = (result[i].boxPoint[j].x - (wpad / 2)) / scale;
//            float y = (result[i].boxPoint[j].y - (hpad / 2)) / scale;
//            x = std::max(std::min(x, (float) (width - 1)), 0.f);
//            y = std::max(std::min(y, (float) (height - 1)), 0.f);
//            result[i].boxPoint[j].x = x;
//            result[i].boxPoint[j].y = y;
//        }
//    }
//
//    return result;
//}
//
//template<class ForwardIterator>
//inline static size_t argmax(ForwardIterator first, ForwardIterator last) {
//    return std::distance(first, std::max_element(first, last));
//}
//
//TextLine scoreToTextLine(const std::vector<float> &outputData, int h, int w) {
//    int keySize = keys.size();
//    std::string strRes;
//    std::vector<float> scores;
//    int lastIndex = 0;
//    int maxIndex;
//    float maxValue;
//
//    for (int i = 0; i < h; i++) {
//        maxIndex = 0;
//        maxValue = -1000.f;
//
//        maxIndex = int(argmax(outputData.begin() + i * w, outputData.begin() + i * w + w));
//        maxValue = float(*std::max_element(outputData.begin() + i * w,
//                                           outputData.begin() + i * w + w));// / partition;
//        if (maxIndex > 0 && maxIndex < keySize && (!(i > 0 && maxIndex == lastIndex))) {
//            scores.emplace_back(maxValue);
//            strRes.append(keys[maxIndex - 1]);
//        }
//        lastIndex = maxIndex;
//    }
//    return {strRes, scores};
//}
//
//
//TextLine getTextLine(const cv::Mat &src) {
//    float scale = (float) dstHeight / (float) src.rows;
//    int dstWidth = int((float) src.cols * scale);
//
//    cv::Mat srcResize;
//    cv::resize(src, srcResize, cv::Size(dstWidth, dstHeight));
//    //if you use PP-OCRv3 you should change PIXEL_RGB to PIXEL_RGB2BGR
//    ncnn::Mat input = ncnn::Mat::from_pixels(srcResize.data, ncnn::Mat::PIXEL_RGB, srcResize.cols,
//                                             srcResize.rows);
//    const float mean_vals[3] = {127.5, 127.5, 127.5};
//    const float norm_vals[3] = {1.0 / 127.5, 1.0 / 127.5, 1.0 / 127.5};
//    input.substract_mean_normalize(mean_vals, norm_vals);
//
//    ncnn::Extractor extractor = crnnNet.create_extractor();
//    //extractor.set_num_threads(2);
//    extractor.input("input", input);
//
//    ncnn::Mat out;
//    extractor.extract("out", out);
//    float *floatArray = (float *) out.data;
//    std::vector<float> outputData(floatArray, floatArray + out.h * out.w);
//    TextLine res = scoreToTextLine(outputData, out.h, out.w);
//    return res;
//}
//
//
//std::vector<TextLine> getTextLines(std::vector<cv::Mat> &partImg) {
//    int size = partImg.size();
//    std::vector<TextLine> textLines(size);
//    for (int i = 0; i < size; ++i) {
//        TextLine textLine = getTextLine(partImg[i]);
//        textLines[i] = textLine;
//    }
//    return textLines;
//}


// void MatToBitmap2(JNIEnv *env, cv::Mat &mat, jobject &bitmap, jboolean needPremultiplyAlpha) {
//    AndroidBitmapInfo info;
//    void *pixels = 0;
//    cv::Mat &src = mat;
//
//    try {
//        //LOGD("nMatToBitmap");
//        CV_Assert(AndroidBitmap_getInfo(env, bitmap, &info) >= 0);
//        CV_Assert(info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 ||
//                  info.format == ANDROID_BITMAP_FORMAT_RGB_565);
//        CV_Assert(src.dims == 2 && info.height == (uint32_t) src.rows &&
//                  info.width == (uint32_t) src.cols);
//        CV_Assert(src.type() == CV_8UC1 || src.type() == CV_8UC3 || src.type() == CV_8UC4);
//        CV_Assert(AndroidBitmap_lockPixels(env, bitmap, &pixels) >= 0);
//        CV_Assert(pixels);
//
//        if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
//            cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
//            if (src.type() == CV_8UC1) {
//                //LOGD("nMatToBitmap: CV_8UC1 -> RGBA_8888");
//                cv::cvtColor(src, tmp, cv::COLOR_GRAY2RGBA);
//            } else if (src.type() == CV_8UC3) {
//                //LOGD("nMatToBitmap: CV_8UC3 -> RGBA_8888");做修改了 BGR
//                cv::cvtColor(src, tmp, cv::COLOR_BGR2RGBA);
//            } else if (src.type() == CV_8UC4) {
//                //LOGD("nMatToBitmap: CV_8UC4 -> RGBA_8888");
//                if (needPremultiplyAlpha) {
//                    cv::cvtColor(src, tmp, cv::COLOR_RGBA2mRGBA);
//                } else {
//                    src.copyTo(tmp);
//                }
//            }
//        } else {
//            // info.format == ANDROID_BITMAP_FORMAT_RGB_565
//            cv::Mat tmp(info.height, info.width, CV_8UC2, pixels);
//            if (src.type() == CV_8UC1) {
//                //LOGD("nMatToBitmap: CV_8UC1 -> RGB_565");
//                cv::cvtColor(src, tmp, cv::COLOR_GRAY2BGR565);
//            } else if (src.type() == CV_8UC3) {
//                //LOGD("nMatToBitmap: CV_8UC3 -> RGB_565");
//                cv::cvtColor(src, tmp, cv::COLOR_RGB2BGR565);
//            } else if (src.type() == CV_8UC4) {
//                //LOGD("nMatToBitmap: CV_8UC4 -> RGB_565");
//                cv::cvtColor(src, tmp, cv::COLOR_RGBA2BGR565);
//            }
//        }
//        AndroidBitmap_unlockPixels(env, bitmap);
//        return;
//    }
//    catch (const cv::Exception &e) {
//        AndroidBitmap_unlockPixels(env, bitmap);
//        LOGE("nMatToBitmap catched cv::Exception: %s", e.what());
//        jclass je = env->FindClass("java/lang/Exception");
//        env->ThrowNew(je, e.what());
//        env->DeleteLocalRef(je);
//        return;
//    }
//    catch (...) {
//        AndroidBitmap_unlockPixels(env, bitmap);
//        LOGE("nMatToBitmap catched unknown exception (...)");
//        jclass je = env->FindClass("java/lang/Exception");
//        env->ThrowNew(je, "Unknown exception in JNI code {nMatToBitmap}");
//        env->DeleteLocalRef(je);
//        return;
//    }
//}



static SCRFD *g_scrfd = 0;
static Yolo *g_yolo = 0;

static ncnn::Mutex lock;


class MyNdkCamera : public NdkCameraWindow {
public:
    virtual void on_image_render(cv::Mat &rgb) const;
};

static MyNdkCamera *g_camera = 0;


void MyNdkCamera::on_image_render(cv::Mat &rgb) const {
    // scrfd
    {
        ncnn::MutexLockGuard g(lock);
        switch (paramType) {
            case SCRFD_TYPE:
                if (g_scrfd) {
                    std::vector<FaceObject> faceobjects;
                    g_scrfd->detect(rgb, faceobjects);
                    g_scrfd->draw(rgb, faceobjects);
                } else {
                    // 绘制当前不支持
                    draw_unsupported(rgb);
                }
                break;
            case YOLO_TYPE:
                if (g_yolo) {
                    std::vector<Object> objects;
                    g_yolo->detect(rgb, objects);
                    g_yolo->draw(rgb, objects);
                } else {
                    // 绘制当前不支持
                    draw_unsupported(rgb);
                }
                break;
        }
    }

    draw_fps(rgb);
}


extern "C"
JNIEXPORT jboolean JNICALL
Java_com_solex_ncnnmaster_BaseNcnn_openCamera(JNIEnv *env, jobject thiz, jint facing) {
    if (facing < 0 || facing > 1)
        return JNI_FALSE;

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "openCamera %d", facing);

    g_camera->open((int) facing);

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_solex_ncnnmaster_BaseNcnn_closeCamera(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "closeCamera");

    g_camera->close();

    return JNI_TRUE;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_solex_ncnnmaster_BaseNcnn_setOutputWindow(JNIEnv *env, jobject thiz, jobject surface) {
    ANativeWindow *win = ANativeWindow_fromSurface(env, surface);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "setOutputWindow %p", win);

    g_camera->set_window(win);

    return JNI_TRUE;
}


extern "C" {

//// FIXME DeleteGlobalRef is missing for objCls
//static jclass objCls = NULL;
//static jmethodID constructortorId;
//static jfieldID x0Id;
//static jfieldID y0Id;
//static jfieldID x1Id;
//static jfieldID y1Id;
//static jfieldID x2Id;
//static jfieldID y2Id;
//static jfieldID x3Id;
//static jfieldID y3Id;
//static jfieldID labelId;
//static jfieldID probId;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnLoad");

    g_camera = new MyNdkCamera;

//    ncnn::create_gpu_instance();


    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnUnload");

    {
        ncnn::MutexLockGuard g(lock);

        delete g_scrfd;
        g_scrfd = 0;
    }

    delete g_camera;
    g_camera = 0;
}


JNIEXPORT jboolean JNICALL
Java_com_solex_ncnnmaster_SCRFDNcnn_loadModel(JNIEnv *env, jobject thiz, jobject assetManager,
                                              jint modelid, jint cpugpu) {

    if (modelid < 0 || modelid > 7 || cpugpu < 0 || cpugpu > 1) {
        return JNI_FALSE;
    }

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "loadModel %p", mgr);

    const char *modeltypes[] =
            {
                    "500m",
                    "500m_kps",
                    "1g",
                    "2.5g",
                    "2.5g_kps",
                    "10g",
                    "10g_kps",
                    "34g"
            };

    const char *modeltype = modeltypes[(int) modelid];
    bool use_gpu = (int) cpugpu == 1;

    // reload
    {
        ncnn::MutexLockGuard g(lock);
        if (use_gpu && ncnn::get_gpu_count() == 0) {
            // no gpu
            delete g_scrfd;
            g_scrfd = 0;
        } else {
            if (!g_scrfd)
                g_scrfd = new SCRFD;
            g_scrfd->load(mgr, modeltype, use_gpu);
            paramType = SCRFD_TYPE;
        }
    }

    return JNI_TRUE;
}
}

// ----------------------YoloV8----------------------------------

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_solex_ncnnmaster_Yolov8Ncnn_loadModel(JNIEnv *env, jobject thiz, jobject assetManager,
                                               jint modelid, jint cpugpu) {
    if (modelid < 0 || modelid > 6 || cpugpu < 0 || cpugpu > 1) {
        return JNI_FALSE;
    }

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "loadModel %p", mgr);

    const char *modeltypes[] =
            {
                    "n",
                    "s",
            };
    const int target_sizes[] =
            {
                    320,
                    320,
            };

    const float mean_vals[][3] =
            {
                    {103.53f, 116.28f, 123.675f},
                    {103.53f, 116.28f, 123.675f},
            };

    const float norm_vals[][3] =
            {
                    {1 / 255.f, 1 / 255.f, 1 / 255.f},
                    {1 / 255.f, 1 / 255.f, 1 / 255.f},
            };

    const char *modeltype = modeltypes[(int) modelid];
    int target_size = target_sizes[(int) modelid];
    bool use_gpu = (int) cpugpu == 1;

    // reload
    {
        ncnn::MutexLockGuard g(lock);

        if (use_gpu && ncnn::get_gpu_count() == 0) {
            // no gpu
            delete g_yolo;
            g_yolo = 0;
        } else {
            if (!g_yolo)
                g_yolo = new Yolo;
            g_yolo->load(mgr, modeltype, target_size, mean_vals[(int) modelid],
                         norm_vals[(int) modelid], use_gpu);
            paramType = YOLO_TYPE;
        }
    }

    return JNI_TRUE;
}


static YoloObb *yoloObb = 0;


extern "C"
JNIEXPORT void JNICALL
Java_com_solex_ncnnmaster_Yolov8obb_loadObbModel(JNIEnv *env, jobject thiz, jobject assetManager,
                                                 jstring bin, jstring param) {
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);

    const char *paramPath = env->GetStringUTFChars(param, nullptr);
    const char *binPath = env->GetStringUTFChars(bin, nullptr);

    yoloObb = new YoloObb;
    yoloObb->loadModel(mgr, paramPath, binPath);

    env->ReleaseStringUTFChars(bin, binPath);
    env->ReleaseStringUTFChars(param, paramPath);
}

static bool MatToBitmap(JNIEnv *env, const cv::Mat &mat, jobject &bitmap) {
    AndroidBitmapInfo info;
    void* pixels = nullptr;

    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "ncnn","Failed to get Bitmap info.");
        return false;
    }

    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "ncnn","Failed to lock Bitmap pixels.");
        return false;
    }

    cv::Mat tmp(info.height, info.width, CV_8UC4, pixels);
    if (mat.type() == CV_8UC1) {
        cv::cvtColor(mat, tmp, cv::COLOR_GRAY2RGBA);
    } else if (mat.type() == CV_8UC3) {
        cv::cvtColor(mat, tmp, cv::COLOR_BGR2RGBA);
    } else if (mat.type() == CV_8UC4) {
        mat.copyTo(tmp);
    } else {
        AndroidBitmap_unlockPixels(env, bitmap);
        __android_log_print(ANDROID_LOG_DEBUG, "ncnn","Unsupported Mat type.");
        return false;
    }

    AndroidBitmap_unlockPixels(env, bitmap);
    return true;
}

extern "C"
JNIEXPORT jobject JNICALL
Java_com_solex_ncnnmaster_Yolov8obb_obbDetect(JNIEnv *env, jobject thiz, jobject bitmap) {


    AndroidBitmapInfo info;
    AndroidBitmap_getInfo(env, bitmap, &info);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "输入图片数据没有问题");
    ncnn::Mat in = ncnn::Mat::from_android_bitmap(env, bitmap, ncnn::Mat::PIXEL_RGB);
    std::vector<ObbObject> objects;
    cv::Mat mat = yoloObb->detectYolov8(in);

    // 检查 mat 是否为空
    if (mat.empty()) {
        __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "Detect result is empty.");
        return nullptr;
    }

    // 获取 Bitmap 类和创建 Bitmap 方法的 ID
    jclass bitmapClass = env->FindClass("android/graphics/Bitmap");
    jmethodID createBitmapMethod = env->GetStaticMethodID(bitmapClass, "createBitmap","(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    // 获取 Bitmap.Config 类和 ARGB_8888 静态成员
    jclass bitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
    jmethodID valueOfMethod = env->GetStaticMethodID(bitmapConfigClass, "valueOf","(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
    jstring argb8888Str = env->NewStringUTF("ARGB_8888");
    jobject argb8888Config = env->CallStaticObjectMethod(bitmapConfigClass, valueOfMethod, argb8888Str);

    // 创建一个与 Mat 大小匹配的 Bitmap 对象
    jobject outBitmap = env->CallStaticObjectMethod(bitmapClass, createBitmapMethod,
                                                    mat.cols, mat.rows, argb8888Config);
    if (outBitmap == nullptr) {
        __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "Failed to create output Bitmap!");
        return nullptr;
    }

    // 使用 MatToBitmap 函数将 Mat 转换为 Bitmap
    if (!MatToBitmap(env, mat, outBitmap)) {
        __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "Failed to convert Mat to Bitmap!");
        return nullptr;
    }

    return outBitmap;
}



extern "C"
JNIEXPORT jboolean JNICALL
Java_com_solex_ncnnmaster_PaddleOCRNcnn_Init(JNIEnv *env, jobject thiz, jobject mgr) {
    // TODO: implement Init()
}

extern "C"
JNIEXPORT void JNICALL
Java_com_solex_ncnnmaster_Yolov8obb_bitMapToMatTest(JNIEnv *env, jobject thiz, jobject bitmap) {

    ncnn::Mat in = ncnn::Mat::from_android_bitmap(env, bitmap, ncnn::Mat::PIXEL_RGB);
    cv::Mat rgb = cv::Mat::zeros(in.h, in.w, CV_8UC3);
    in.to_pixels(rgb.data, ncnn::Mat::PIXEL_RGB);

}
