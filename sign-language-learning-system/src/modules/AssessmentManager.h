#pragma once

#include "models/Gesture.h"

#include <QVector>

class AssessmentManager {
public:
    Gesture pickQuestion(const QVector<Gesture>& gestures) const;
    bool shouldAddToWrongBook(double score) const;
};

