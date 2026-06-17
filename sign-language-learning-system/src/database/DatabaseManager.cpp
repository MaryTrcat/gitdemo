#include "database/DatabaseManager.h"

#include <QCryptographicHash>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

static void configureMySqlConnection(QSqlDatabase& databaseConnection,
                                     const QString& host,
                                     int port,
                                     const QString& database,
                                     const QString& username,
                                     const QString& password)
{
    if (databaseConnection.driverName() == "QODBC") {
        QString connectionString = QString(
            "DRIVER={MySQL ODBC 8.4 Unicode Driver};"
            "SERVER=%1;"
            "PORT=%2;"
            "USER=%3;"
            "PASSWORD=%4;"
            "OPTION=3;")
            .arg(host)
            .arg(port)
            .arg(username)
            .arg(password);

        if (!database.isEmpty()) {
            connectionString += "DATABASE=" + database + ";";
        }

        databaseConnection.setDatabaseName(connectionString);
        return;
    }

    databaseConnection.setHostName(host);
    databaseConnection.setPort(port);
    databaseConnection.setDatabaseName(database);
    databaseConnection.setUserName(username);
    databaseConnection.setPassword(password);
}

static QString hashPasswordForSeed(const QString& password)
{
    return QString::fromUtf8(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool DatabaseManager::connectToMySql(const QString& host,
                                     int port,
                                     const QString& database,
                                     const QString& username,
                                     const QString& password,
                                     QString* errorMessage)
{
    const QString connectionName = "main_mysql";
    if (QSqlDatabase::contains(connectionName)) {
        m_database = QSqlDatabase::database(connectionName);
    } else if (QSqlDatabase::drivers().contains("QMYSQL")) {
        m_database = QSqlDatabase::addDatabase("QMYSQL", connectionName);
    } else {
        m_database = QSqlDatabase::addDatabase("QODBC", connectionName);
    }

    configureMySqlConnection(m_database, host, port, database, username, password);

    if (!m_database.open()) {
        const QString openError = m_database.lastError().text();
        if (openError.contains("Unknown database", Qt::CaseInsensitive)) {
            m_database.close();

            const QString bootstrapName = "mysql_bootstrap";
            if (QSqlDatabase::contains(bootstrapName)) {
                QSqlDatabase::removeDatabase(bootstrapName);
            }

            QSqlDatabase bootstrap = QSqlDatabase::addDatabase(m_database.driverName(), bootstrapName);
            configureMySqlConnection(bootstrap, host, port, QString(), username, password);
            if (!bootstrap.open()) {
                if (errorMessage) {
                    *errorMessage = "自动创建数据库失败：" + bootstrap.lastError().text();
                }
                bootstrap.close();
                QSqlDatabase::removeDatabase(bootstrapName);
                return false;
            }

            QSqlQuery createQuery(bootstrap);
            const QString createSql = QString("CREATE DATABASE IF NOT EXISTS `%1` DEFAULT CHARACTER SET utf8mb4").arg(database);
            if (!createQuery.exec(createSql)) {
                if (errorMessage) {
                    *errorMessage = "自动创建数据库失败：" + createQuery.lastError().text();
                }
                bootstrap.close();
                QSqlDatabase::removeDatabase(bootstrapName);
                return false;
            }

            bootstrap.close();
            QSqlDatabase::removeDatabase(bootstrapName);
            configureMySqlConnection(m_database, host, port, database, username, password);
            if (!m_database.open()) {
                if (errorMessage) {
                    *errorMessage = m_database.lastError().text();
                }
                return false;
            }
        } else {
            if (errorMessage) {
                *errorMessage = openError;
            }
            return false;
        }
    }

    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = m_database.lastError().text();
        }
        return false;
    }

    if (!initializeSchema(errorMessage)) {
        return false;
    }

    return true;
}

QSqlDatabase DatabaseManager::database() const
{
    return m_database;
}

bool DatabaseManager::isOpen() const
{
    return m_database.isValid() && m_database.isOpen();
}

bool DatabaseManager::initializeSchema(QString* errorMessage)
{
    const QStringList statements = {
        "CREATE TABLE IF NOT EXISTS users ("
        "id INT PRIMARY KEY AUTO_INCREMENT,"
        "username VARCHAR(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL UNIQUE,"
        "password_hash VARCHAR(128) NOT NULL,"
        "role VARCHAR(20) NOT NULL DEFAULT 'user',"
        "email VARCHAR(100),"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "last_login TIMESTAMP NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS gesture_library ("
        "id INT PRIMARY KEY AUTO_INCREMENT,"
        "name VARCHAR(100) NOT NULL,"
        "category VARCHAR(50) NOT NULL,"
        "difficulty INT DEFAULT 1,"
        "description TEXT,"
        "video_path VARCHAR(500),"
        "reference_keypoints JSON,"
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "INDEX idx_category (category)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS practice_records ("
        "id INT PRIMARY KEY AUTO_INCREMENT,"
        "user_id INT NOT NULL,"
        "gesture_id INT NOT NULL,"
        "total_score DECIMAL(5,2) NOT NULL,"
        "position_score DECIMAL(5,2) NOT NULL,"
        "angle_score DECIMAL(5,2) NOT NULL,"
        "stability_score DECIMAL(5,2) NOT NULL,"
        "feedback_text TEXT NOT NULL,"
        "practiced_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "FOREIGN KEY (user_id) REFERENCES users(id),"
        "FOREIGN KEY (gesture_id) REFERENCES gesture_library(id),"
        "INDEX idx_user_practice (user_id, practiced_at)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS wrong_answer_book ("
        "id INT PRIMARY KEY AUTO_INCREMENT,"
        "user_id INT NOT NULL,"
        "gesture_id INT NOT NULL,"
        "last_score DECIMAL(5,2) NOT NULL,"
        "error_count INT DEFAULT 1,"
        "last_wrong_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE KEY uk_user_gesture (user_id, gesture_id),"
        "FOREIGN KEY (user_id) REFERENCES users(id),"
        "FOREIGN KEY (gesture_id) REFERENCES gesture_library(id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    };

    for (const QString& statement : statements) {
        QSqlQuery query(m_database);
        if (!query.exec(statement)) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }

    QSqlQuery alterQuery(m_database);
    if (!alterQuery.exec("ALTER TABLE users MODIFY username VARCHAR(50) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL")) {
        const QString error = alterQuery.lastError().text();
        if (errorMessage) {
            *errorMessage = error;
        }
        return false;
    }

    if (!alterQuery.exec("ALTER TABLE users ADD COLUMN role VARCHAR(20) NOT NULL DEFAULT 'user'")) {
        const QString error = alterQuery.lastError().text();
        if (!error.contains("Duplicate column", Qt::CaseInsensitive)
            && !error.contains("1060")) {
            if (errorMessage) {
                *errorMessage = error;
            }
            return false;
        }
    }

    if (!alterQuery.exec("ALTER TABLE gesture_library ADD COLUMN video_path VARCHAR(500)")) {
        const QString error = alterQuery.lastError().text();
        if (!error.contains("Duplicate column", Qt::CaseInsensitive)
            && !error.contains("1060")) {
            if (errorMessage) {
                *errorMessage = error;
            }
            return false;
        }
    }

    if (!alterQuery.exec("ALTER TABLE gesture_library ADD COLUMN reference_keypoints JSON")) {
        const QString error = alterQuery.lastError().text();
        if (!error.contains("Duplicate column", Qt::CaseInsensitive)
            && !error.contains("1060")) {
            if (errorMessage) {
                *errorMessage = error;
            }
            return false;
        }
    }

    QSqlQuery adminQuery(m_database);
    adminQuery.prepare(
        "INSERT INTO users (username, password_hash, role) "
        "VALUES ('admin', ?, 'admin') "
        "ON DUPLICATE KEY UPDATE password_hash = VALUES(password_hash), role = 'admin'");
    adminQuery.addBindValue(hashPasswordForSeed("123456"));
    if (!adminQuery.exec()) {
        if (errorMessage) {
            *errorMessage = adminQuery.lastError().text();
        }
        return false;
    }

    const QStringList seedStatements = {
        "INSERT INTO gesture_library (name, category, difficulty, description) "
        "SELECT '你好', '常用词', 1, '常用问候手语动作。' "
        "WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '你好')",
        "INSERT INTO gesture_library (name, category, difficulty, description) "
        "SELECT '谢谢', '常用词', 1, '表达感谢的手语动作。' "
        "WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '谢谢')",
        "INSERT INTO gesture_library (name, category, difficulty, description) "
        "SELECT '学习', '学习生活', 2, '表示学习行为的手语动作。' "
        "WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '学习')",
        "INSERT INTO gesture_library (name, category, difficulty, description) "
        "SELECT '我爱你', '常用词', 2, '常见表达手语动作。' "
        "WHERE NOT EXISTS (SELECT 1 FROM gesture_library WHERE name = '我爱你')"
    };

    for (const QString& statement : seedStatements) {
        QSqlQuery query(m_database);
        if (!query.exec(statement)) {
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
            return false;
        }
    }

    return true;
}

bool DatabaseManager::savePracticeRecord(const PracticeRecord& record, QString* errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(
        "INSERT INTO practice_records "
        "(user_id, gesture_id, total_score, position_score, angle_score, stability_score, feedback_text) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(QVariant(record.userId));
    query.addBindValue(QVariant(record.gestureId));
    query.addBindValue(QVariant(record.totalScore));
    query.addBindValue(QVariant(record.positionScore));
    query.addBindValue(QVariant(record.angleScore));
    query.addBindValue(QVariant(record.stabilityScore));
    query.addBindValue(QVariant(record.feedback));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}

bool DatabaseManager::saveWrongAnswer(int userId, int gestureId, double score, QString* errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(
        "INSERT INTO wrong_answer_book (user_id, gesture_id, last_score, error_count) "
        "VALUES (?, ?, ?, 1) "
        "ON DUPLICATE KEY UPDATE last_score = VALUES(last_score), "
        "error_count = error_count + 1, last_wrong_time = CURRENT_TIMESTAMP");
    query.addBindValue(QVariant(userId));
    query.addBindValue(QVariant(gestureId));
    query.addBindValue(QVariant(score));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}

bool DatabaseManager::updateGestureVideoPath(int gestureId, const QString& videoPath, QString* errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE gesture_library SET video_path = ? WHERE id = ?");
    query.addBindValue(QVariant(videoPath));
    query.addBindValue(QVariant(gestureId));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}

bool DatabaseManager::updateGestureVideoAndReference(int gestureId,
                                                     const QString& videoPath,
                                                     const QString& referenceKeypointsJson,
                                                     QString* errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE gesture_library SET video_path = ?, reference_keypoints = CONVERT(? USING utf8mb4) WHERE id = ?");
    query.addBindValue(QVariant(videoPath));
    query.addBindValue(QVariant(referenceKeypointsJson));
    query.addBindValue(QVariant(gestureId));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}
