#pragma once

#include "models/Gesture.h"

#include <QSqlDatabase>
#include <QVector>

class GestureLibrary {
public:
    explicit GestureLibrary(QSqlDatabase database = QSqlDatabase());

    void setDatabase(QSqlDatabase database);
    void setCacheFilePath(const QString& path);
    QVector<Gesture> loadGestures() const;
    Gesture fallbackGesture() const;
    bool addGesture(const Gesture& gesture, int* newGestureId, QString* errorMessage) const;
    bool cacheGesturesForOfflineUse(const QVector<Gesture>& gestures) const;

private:
    QString cacheFilePath() const;
    QVector<Gesture> loadCachedGestures() const;
    QVector<Gesture> fallbackGestures() const;

    QSqlDatabase m_database;
    QString m_cacheFilePath;
};
