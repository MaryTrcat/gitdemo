#pragma once

#include "ai/GestureScorer.h"

#include <QImage>
#include <QString>
#include <vector>

struct HandDetectionResult {
    bool success = false;
    std::vector<HandKeypoint> keypoints;
    double stabilityDeviation = 0.0;
    QString message;
};

class HandKeypointDetector {
public:
    HandDetectionResult detectRealtimeHand(const QImage& frame, int gestureId) const;
    HandDetectionResult detectReferenceFromVideo(const QString& videoPath) const;
    std::vector<HandKeypoint> detectMockHand(int gestureId, double noise) const;
    std::vector<HandKeypoint> referenceHand(int gestureId) const;
};
