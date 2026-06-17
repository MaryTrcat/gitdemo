#pragma once

#include <QCamera>
#include <QImage>
#include <QtGlobal>
#if QT_VERSION_MAJOR >= 6
#include <QMediaCaptureSession>
#endif
#include <QScopedPointer>
#include <QString>

class QVideoWidget;

class CameraCapture {
public:
    bool start(QVideoWidget* outputWidget, QString* statusMessage);
    void stop();
    bool isRunning() const;
    QImage lastFrame() const;

private:
    bool m_running = false;
    QImage m_lastFrame;
#if QT_VERSION_MAJOR >= 6
    QMediaCaptureSession m_captureSession;
#endif
    QScopedPointer<QCamera> m_camera;
};
