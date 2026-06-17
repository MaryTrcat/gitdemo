#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

struct LearningSummary {
    int practiceCount = 0;
    double averageScore = 0.0;
    int wrongAnswerCount = 0;
    QString lastPracticeTime;
};

class LearningStatistics {
public:
    explicit LearningStatistics(QSqlDatabase database = QSqlDatabase());

    void setDatabase(QSqlDatabase database);
    QString summaryForUser(int userId) const;
    LearningSummary loadSummary(int userId) const;
    QStringList wrongAnswersForUser(int userId) const;
    QStringList recentPracticeItems(int userId, int limit = 5) const;
    QString scoreTrendText(int userId, int limit = 5) const;

private:
    QSqlDatabase m_database;
};
