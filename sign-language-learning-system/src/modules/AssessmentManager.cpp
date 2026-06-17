#include "modules/AssessmentManager.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QRandomGenerator>
#else
#include <QtGlobal>
#endif

Gesture AssessmentManager::pickQuestion(const QVector<Gesture>& gestures) const
{
    if (gestures.isEmpty()) {
        return {};
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    const int index = QRandomGenerator::global()->bounded(gestures.size());
#else
    const int index = qrand() % gestures.size();
#endif
    return gestures[index];
}

bool AssessmentManager::shouldAddToWrongBook(double score) const
{
    return score < 70.0;
}
