#pragma once

#include <QString>
#include <vector>

#include "../ai/GestureScorer.h"

struct Gesture {
    int id = 0;
    QString name;
    QString category;
    int difficulty = 1;
    QString description;
    QString videoPath;
    std::vector<HandKeypoint> referenceKeypoints;
};

inline QString gestureDisplayText(const Gesture& gesture)
{
    return QString("%1  难度:%2")
        .arg(gesture.name)
        .arg(gesture.difficulty);
}
