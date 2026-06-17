#include "modules/LearningStatistics.h"

#include <QSqlQuery>
#include <QVariant>

LearningStatistics::LearningStatistics(QSqlDatabase database)
    : m_database(database)
{
}

void LearningStatistics::setDatabase(QSqlDatabase database)
{
    m_database = database;
}

QString LearningStatistics::summaryForUser(int userId) const
{
    const LearningSummary summary = loadSummary(userId);
    if (!m_database.isOpen() || userId <= 0) {
        return "暂无登录用户或数据库未连接。";
    }

    return QString("累计练习 %1 次，平均分 %2，错题 %3 个，最近练习：%4。")
        .arg(summary.practiceCount)
        .arg(summary.averageScore, 0, 'f', 1)
        .arg(summary.wrongAnswerCount)
        .arg(summary.lastPracticeTime.isEmpty() ? "暂无" : summary.lastPracticeTime);
}

LearningSummary LearningStatistics::loadSummary(int userId) const
{
    LearningSummary summary;
    if (!m_database.isOpen() || userId <= 0) {
        return summary;
    }

    QSqlQuery query(m_database);
    query.prepare(
        "SELECT COUNT(*) AS cnt, COALESCE(AVG(total_score), 0) AS avg_score, "
        "MAX(practiced_at) AS last_time FROM practice_records WHERE user_id = ?");
    query.addBindValue(userId);
    if (query.exec() && query.next()) {
        summary.practiceCount = query.value("cnt").toInt();
        summary.averageScore = query.value("avg_score").toDouble();
        summary.lastPracticeTime = query.value("last_time").toString();
    }

    QSqlQuery wrongQuery(m_database);
    wrongQuery.prepare("SELECT COUNT(*) AS cnt FROM wrong_answer_book WHERE user_id = ?");
    wrongQuery.addBindValue(userId);
    if (wrongQuery.exec() && wrongQuery.next()) {
        summary.wrongAnswerCount = wrongQuery.value("cnt").toInt();
    }

    return summary;
}

QStringList LearningStatistics::wrongAnswersForUser(int userId) const
{
    QStringList items;
    if (!m_database.isOpen() || userId <= 0) {
        items << "暂无登录用户或数据库未连接。";
        return items;
    }

    QSqlQuery query(m_database);
    query.prepare(
        "SELECT g.name, w.last_score, w.error_count, w.last_wrong_time "
        "FROM wrong_answer_book w "
        "JOIN gesture_library g ON g.id = w.gesture_id "
        "WHERE w.user_id = ? "
        "ORDER BY w.error_count DESC, w.last_wrong_time DESC");
    query.addBindValue(userId);
    if (query.exec()) {
        while (query.next()) {
            items << QString("%1：最近 %2 分，错误 %3 次")
                .arg(query.value("name").toString())
                .arg(query.value("last_score").toDouble(), 0, 'f', 1)
                .arg(query.value("error_count").toInt());
        }
    }

    if (items.isEmpty()) {
        items << "暂无错题记录。";
    }
    return items;
}

QStringList LearningStatistics::recentPracticeItems(int userId, int limit) const
{
    QStringList items;
    if (!m_database.isOpen() || userId <= 0) {
        items << "暂无登录用户或数据库未连接。";
        return items;
    }

    QSqlQuery query(m_database);
    query.prepare(
        "SELECT g.name, p.total_score, p.practiced_at "
        "FROM practice_records p "
        "JOIN gesture_library g ON g.id = p.gesture_id "
        "WHERE p.user_id = ? "
        "ORDER BY p.practiced_at DESC, p.id DESC "
        "LIMIT ?");
    query.addBindValue(userId);
    query.addBindValue(limit);
    if (query.exec()) {
        while (query.next()) {
            items << QString("%1：%2 分  %3")
                .arg(query.value("name").toString())
                .arg(query.value("total_score").toDouble(), 0, 'f', 1)
                .arg(query.value("practiced_at").toString());
        }
    }

    if (items.isEmpty()) {
        items << "暂无练习记录。";
    }
    return items;
}

QString LearningStatistics::scoreTrendText(int userId, int limit) const
{
    if (!m_database.isOpen() || userId <= 0) {
        return "连接数据库并登录后显示最近得分趋势。";
    }

    QStringList scores;
    QSqlQuery query(m_database);
    query.prepare(
        "SELECT total_score FROM practice_records "
        "WHERE user_id = ? "
        "ORDER BY practiced_at DESC, id DESC "
        "LIMIT ?");
    query.addBindValue(userId);
    query.addBindValue(limit);
    if (query.exec()) {
        while (query.next()) {
            scores.prepend(QString::number(query.value("total_score").toDouble(), 'f', 1));
        }
    }

    if (scores.isEmpty()) {
        return "暂无练习记录，完成练习后会生成最近得分趋势。";
    }
    return "最近得分趋势：" + scores.join("  →  ");
}
