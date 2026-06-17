#include "modules/FeedbackGenerator.h"

QString FeedbackGenerator::buildFeedback(const ScoreResult& result) const
{
    return QString("综合得分 %1。%2")
        .arg(result.totalScore, 0, 'f', 1)
        .arg(QString::fromStdString(result.feedback));
}

