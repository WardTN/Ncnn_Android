//
// Created by deqi_chen on 2024/8/1.
//

#ifndef NCNN_ANDROID_YOLOOBB_H
#define NCNN_ANDROID_YOLOOBB_H

#include <net.h>
#include <opencv2/core/mat.hpp>

#define M_PI       3.14159265358979323846   // pi
#define M_PI_2     1.57079632679489661923   // pi/2

struct Point {
    float x, y;

    Point(const float &px = 0, const float &py = 0) : x(px), y(py) {}

    Point operator+(const Point &p) const { return Point(x + p.x, y + p.y); }

    Point &operator+=(const Point &p) {
        x += p.x;
        y += p.y;
        return *this;
    }

    Point operator-(const Point &p) const { return Point(x - p.x, y - p.y); }

    Point operator*(const float coeff) const { return Point(x * coeff, y * coeff); }
};

struct RotatedBox {
    float x_ctr, y_ctr, w, h, a;
};

struct ObbObject {
    cv::Rect_<float> rect;
    RotatedBox r_rect;
    int label;
    float prob;
};

struct ObbGridAndStride {
    int grid0;
    int grid1;
    int stride;
};


class YoloObb {
public:
    YoloObb();

    void loadModel(AAssetManager *mgr, const char *paramPath, const char *binPath);

    void detectYolov8(const char *imgPath);

private:
    ncnn::Net yolo;
    cv::Mat bgr;
    std::vector<ObbObject> objects;
};


#endif //NCNN_ANDROID_YOLOOBB_H
