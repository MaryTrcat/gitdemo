#pragma once

#include <QString>

struct PracticeRecord {
    int userId = 0;
    int gestureId = 0;
    double totalScore = 0.0;
    double positionScore = 0.0;
    double angleScore = 0.0;
    double stabilityScore = 0.0;
    QString feedback;
};

