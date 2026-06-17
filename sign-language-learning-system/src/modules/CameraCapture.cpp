#include "modules/CameraCapture.h"

#include <QtGlobal>
#if QT_VERSION_MAJOR >= 6
#include <QCameraDevice>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QVideoSink>
#else
#include <QCameraInfo>
#endif
#include <QVideoWidget>

bool CameraCapture::start(QVideoWidget* outputWidget, QString* statusMessage)
{
    if (!outputWidget) {
        if (statusMessage) {
            *statusMessage = "摄像头预览控件不存在。";
        }
        return false;
    }

#if QT_VERSION_MAJOR >= 6
    const QCameraDevice cameraDevice = QMediaDevices::defaultVideoInput();
    if (cameraDevice.isNull()) {
        if (statusMessage) {
            *statusMessage = "未检测到可用摄像头。";
        }
        return false;
    }

    stop();
    m_camera.reset(new QCamera(cameraDevice));
    m_captureSession.setCamera(m_camera.data());
    m_captureSession.setVideoOutput(outputWidget);
    if (QVideoSink* sink = outputWidget->videoSink()) {
        QObject::connect(sink, &QVideoSink::videoFrameChanged, sink, [this](const QVideoFrame& frame) {
            const QImage image = frame.toImage();
            if (!image.isNull()) {
                m_lastFrame = image;
            }
        });
    }
    m_camera->start();

    m_running = true;
    if (statusMessage) {
        *statusMessage = "摄像头模块已启动：" + cameraDevice.description();
    }
#else
    const QCameraInfo cameraInfo = QCameraInfo::defaultCamera();
    if (cameraInfo.isNull()) {
        if (statusMessage) {
            *statusMessage = "未检测到可用摄像头。";
        }
        return false;
    }

    stop();
    m_camera.reset(new QCamera(cameraInfo));
    m_camera->setViewfinder(outputWidget);
    m_camera->start();

    m_running = true;
    if (statusMessage) {
        *statusMessage = "摄像头模块已启动：" + cameraInfo.description();
    }
#endif
    return true;
}

void CameraCapture::stop()
{
    if (m_camera) {
        m_camera->stop();
    }
#if QT_VERSION_MAJOR >= 6
    m_captureSession.setCamera(nullptr);
    m_captureSession.setVideoOutput(nullptr);
#endif
    m_camera.reset();
    m_lastFrame = QImage();
    m_running = false;
}

bool CameraCapture::isRunning() const
{
    return m_running;
}

QImage CameraCapture::lastFrame() const
{
    return m_lastFrame;
}
