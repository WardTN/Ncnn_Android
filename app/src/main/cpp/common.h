//
// Created by deqi_chen on 2024/8/1.
//

#ifndef NCNN_ANDROID_COMMON_H
#define NCNN_ANDROID_COMMON_H

#include <opencv2/opencv.hpp>
#include <vector>
#include "clipper.hpp"
#include <jni.h>

struct TextBox {
    std::vector<cv::Point> boxPoint;
    float score;
    std::string text;
};
struct TextLine {
    std::string text;
    std::vector<float> charScores;
};

struct Angle {
    int index;
    float score;
};

std::vector<cv::Point> getMinBoxes(const std::vector<cv::Point>& inVec, float& minSideLen, float& allEdgeSize);
float boxScoreFast(const cv::Mat& inMat, const std::vector<cv::Point>& inBox);
std::vector<cv::Point> unClip(const std::vector<cv::Point>& inBox, float perimeter, float unClipRatio);
cv::Mat getRotateCropImage(const cv::Mat& src, std::vector<cv::Point> box);
std::vector<cv::Mat> getPartImages(const cv::Mat& src, std::vector<TextBox>& textBoxes);
cv::Mat matRotateClockWise180(cv::Mat src);
cv::Mat makePadding(cv::Mat& src, const int padding);



#endif //NCNN_ANDROID_COMMON_H
