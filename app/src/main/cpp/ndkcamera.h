//
// Created by deqi_chen on 2024/7/24.
//

#ifndef NCNN_ANDROID_NDKCAMERA_H
#define NCNN_ANDROID_NDKCAMERA_H

#include <android/looper.h>
#include <android/native_window.h>
#include <android/sensor.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <media/NdkImageReader.h>

#include <opencv2/core/core.hpp>


class NdkCamera
{
public:
    NdkCamera();
    virtual ~NdkCamera();

    // facing 0=front 1=back
    int open(int camera_facing = 0);
    void close();

    // 自定义方法 对帧进行操作
    virtual void on_image(const cv::Mat& rgb) const;

    virtual void on_image(const unsigned char* nv21, int nv21_width, int nv21_height) const;

public:
    int camera_facing;
    int camera_orientation;

    /**
     * 1.Camera2 初始化 打开摄像头 配置参数
     * 2.ImageReader 创建 配置参数 设置target
     * 3.OnImageAvailable 获取YUV420_888 转换为 Bimtap
     * 4.Canvas 画框画字 更新Bitmap View 负责旋转
     */
private:
    ACameraManager* camera_manager;
    ACameraDevice* camera_device;
    AImageReader* image_reader;
    ANativeWindow* image_reader_surface;
    ACameraOutputTarget* image_reader_target;
    ACaptureRequest* capture_request;
    ACaptureSessionOutputContainer* capture_session_output_container;
    ACaptureSessionOutput* capture_session_output;
    ACameraCaptureSession* capture_session;
};

class NdkCameraWindow : public NdkCamera
{
public:
    NdkCameraWindow();
    virtual ~NdkCameraWindow();

    void set_window(ANativeWindow* win);

    virtual void on_image_render(cv::Mat& rgb) const;

    virtual void on_image(const unsigned char* nv21, int nv21_width, int nv21_height) const;

public:
    mutable int accelerometer_orientation;

private:
    ASensorManager* sensor_manager;
    mutable ASensorEventQueue* sensor_event_queue;
    const ASensor* accelerometer_sensor;
    ANativeWindow* win;
};

#endif //NCNN_ANDROID_NDKCAMERA_H
