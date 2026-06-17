#pragma once

#include "../models/PracticeRecord.h"

#include <QSqlDatabase>
#include <QString>

class DatabaseManager {
public:
    bool connectToMySql(const QString& host,
                        int port,
                        const QString& database,
                        const QString& username,
                        const QString& password,
                        QString* errorMessage);
    QSqlDatabase database() const;
    bool isOpen() const;
    bool initializeSchema(QString* errorMessage);
    bool savePracticeRecord(const PracticeRecord& record, QString* errorMessage);
    bool saveWrongAnswer(int userId, int gestureId, double score, QString* errorMessage);
    bool updateGestureVideoPath(int gestureId, const QString& videoPath, QString* errorMessage);
    bool updateGestureVideoAndReference(int gestureId, const QString& videoPath, const QString& referenceKeypointsJson, QString* errorMessage);

private:
    QSqlDatabase m_database;
};
