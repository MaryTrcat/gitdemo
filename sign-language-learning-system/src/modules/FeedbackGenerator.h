#pragma once

#include "ai/GestureScorer.h"

#include <QString>

class FeedbackGenerator {
public:
    QString buildFeedback(const ScoreResult& result) const;
};

