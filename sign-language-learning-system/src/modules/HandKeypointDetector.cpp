#include "modules/HandKeypointDetector.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>

#include <cmath>

static QString detectorScriptPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/scripts/hand_keypoint_detector.py",
        QDir::currentPath() + "/scripts/hand_keypoint_detector.py",
        QDir::currentPath() + "/sign-language-learning-system/scripts/hand_keypoint_detector.py"
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return QString();
}

static QString debugFramePath()
{
    const QString debugDir = QCoreApplication::applicationDirPath() + "/debug";
    QDir().mkpath(debugDir);
    return debugDir + "/last-ai-frame.png";
}

static HandDetectionResult runDetectorScript(const QStringList& arguments)
{
    HandDetectionResult result;
    const QString scriptPath = detectorScriptPath();
    if (scriptPath.isEmpty()) {
        result.message = "已获取画面，但未找到 MediaPipe 检测脚本。";
        return result;
    }

    QProcess process;
    QString pythonPath = QStandardPaths::findExecutable("python");
    if (pythonPath.isEmpty() && QFileInfo::exists("E:/python/python.exe")) {
        pythonPath = "E:/python/python.exe";
    }
    if (pythonPath.isEmpty()) {
        result.message = "无法找到 Python，不能启动 MediaPipe 检测。";
        return result;
    }

    QStringList processArguments;
    processArguments << scriptPath;
    processArguments << arguments;
    process.start(pythonPath, processArguments);
    if (!process.waitForStarted(3000)) {
        result.message = "无法启动 Python MediaPipe 检测进程，请确认 python 可用。";
        return result;
    }
    if (!process.waitForFinished(12000)) {
        process.kill();
        result.message = "MediaPipe 检测超时，请重试或检查画面。";
        return result;
    }

    const QByteArray output = process.readAllStandardOutput();
    const QByteArray errorOutput = process.readAllStandardError();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.message = "MediaPipe 检测结果解析失败。调试帧：" + debugFramePath()
            + "\n错误信息：" + QString::fromUtf8(errorOutput)
            + "\n输出内容：" + QString::fromUtf8(output.left(500));
        return result;
    }

    const QJsonObject object = document.object();
    result.success = object.value("success").toBool(false);
    result.message = object.value("message").toString();
    result.stabilityDeviation = object.value("stability_deviation").toDouble(0.04);

    const QJsonArray keypoints = object.value("keypoints").toArray();
    result.keypoints.reserve(keypoints.size());
    for (const QJsonValue& value : keypoints) {
        const QJsonObject point = value.toObject();
        result.keypoints.push_back({
            point.value("x").toDouble(),
            point.value("y").toDouble(),
            point.value("z").toDouble()
        });
    }

    if (result.success && result.keypoints.size() != 21) {
        result.success = false;
        result.message = "MediaPipe 返回的手部关键点数量不完整。";
    }
    return result;
}

HandDetectionResult HandKeypointDetector::detectRealtimeHand(const QImage& frame, int gestureId) const
{
    Q_UNUSED(gestureId);

    HandDetectionResult result;
    if (frame.isNull()) {
        result.message = "摄像头已启动，但暂时还没有采集到有效画面。请等待画面出现后再评分。";
        return result;
    }

    QTemporaryFile imageFile(QDir::tempPath() + "/sign-language-frame-XXXXXX.png");
    imageFile.setAutoRemove(true);
    if (!imageFile.open()) {
        result.message = "无法创建临时摄像头画面文件。";
        return result;
    }
    const QString imagePath = imageFile.fileName();
    imageFile.close();
    if (!frame.save(imagePath, "PNG")) {
        result.message = "无法保存当前摄像头画面用于 AI 检测。";
        return result;
    }
    frame.save(debugFramePath(), "PNG");

    result = runDetectorScript({imagePath});

    if (!result.success) {
        result.message += "\nAI 实际检测帧已保存到：" + debugFramePath();
    }
    return result;
}

HandDetectionResult HandKeypointDetector::detectReferenceFromVideo(const QString& videoPath) const
{
    if (!QFileInfo::exists(videoPath)) {
        HandDetectionResult result;
        result.message = "视频文件不存在。";
        return result;
    }
    return runDetectorScript({"--video", videoPath});
}

std::vector<HandKeypoint> HandKeypointDetector::detectMockHand(int gestureId, double noise) const
{
    auto points = referenceHand(gestureId);
    for (size_t i = 0; i < points.size(); ++i) {
        const double wave = std::sin(static_cast<double>(i + gestureId));
        points[i].x += noise * wave;
        points[i].y += noise * std::cos(static_cast<double>(i));
    }
    return points;
}

std::vector<HandKeypoint> HandKeypointDetector::referenceHand(int gestureId) const
{
    std::vector<HandKeypoint> points = {
        {0.00, 0.00, 0.00},
        {-0.18, -0.12, 0.01}, {-0.30, -0.28, 0.02}, {-0.38, -0.42, 0.02}, {-0.46, -0.55, 0.02},
        {-0.10, -0.42, -0.02}, {-0.12, -0.68, -0.03}, {-0.12, -0.88, -0.03}, {-0.12, -1.08, -0.02},
        {0.05, -0.46, -0.03}, {0.06, -0.76, -0.04}, {0.06, -1.02, -0.04}, {0.06, -1.25, -0.03},
        {0.20, -0.42, -0.03}, {0.24, -0.70, -0.04}, {0.26, -0.92, -0.04}, {0.28, -1.10, -0.03},
        {0.34, -0.34, -0.02}, {0.42, -0.56, -0.03}, {0.48, -0.74, -0.03}, {0.54, -0.90, -0.02}
    };

    const int variant = gestureId % 4;
    if (variant == 1) {
        for (int index : {8, 12, 16, 20}) {
            points[index].y += 0.08;
        }
    } else if (variant == 2) {
        for (int index : {4, 8, 12, 16, 20}) {
            points[index].x *= 0.88;
            points[index].y += 0.05;
        }
    } else if (variant == 3) {
        for (int index : {1, 2, 3, 4}) {
            points[index].x += 0.10;
            points[index].y += 0.10;
        }
    }

    return points;
}
