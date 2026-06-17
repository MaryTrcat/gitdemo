#include "modules/UserManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

UserManager::UserManager(QSqlDatabase database)
    : m_database(database)
{
}

void UserManager::setDatabase(QSqlDatabase database)
{
    m_database = database;
}

void UserManager::setLocalStorePath(const QString& path)
{
    m_localStorePath = path;
    m_localUsers.clear();
    m_localUsersLoaded = false;
}

bool UserManager::registerUser(const QString& username, const QString& password, QString* errorMessage)
{
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "用户名和密码不能为空。";
        }
        return false;
    }

    if (!isValidUsernameFormat(username)) {
        if (errorMessage) {
            *errorMessage = "用户名只能使用英文字母、数字和常用英文符号，不能包含中文或空格。";
        }
        return false;
    }

    if (isReservedUsername(username)) {
        if (errorMessage) {
            *errorMessage = "该用户名为管理员保留账号，不能注册。";
        }
        return false;
    }

    if (!m_database.isOpen()) {
        ensureLocalUsersLoaded();
        const QString normalizedUsername = username.trimmed();
        if (m_localUsers.contains(normalizedUsername)) {
            if (errorMessage) {
                *errorMessage = "用户名已存在。";
            }
            return false;
        }
        m_localUsers.insert(normalizedUsername, hashPassword(password));
        return saveLocalUsers(errorMessage);
    }

    QSqlQuery duplicateQuery(m_database);
    duplicateQuery.prepare("SELECT 1 FROM users WHERE username = ? LIMIT 1");
    duplicateQuery.addBindValue(username.trimmed());
    if (!duplicateQuery.exec()) {
        if (errorMessage) {
            *errorMessage = duplicateQuery.lastError().text();
        }
        return false;
    }
    if (duplicateQuery.next()) {
        if (errorMessage) {
            *errorMessage = "用户名已存在。";
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT INTO users (username, password_hash, role) VALUES (?, ?, 'user')");
    query.addBindValue(username.trimmed());
    query.addBindValue(hashPassword(password));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}

bool UserManager::login(const QString& username, const QString& password, QString* errorMessage)
{
    if (username.trimmed().isEmpty() || password.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "用户名和密码不能为空。";
        }
        return false;
    }

    if (!m_database.isOpen()) {
        if (isBuiltInAdmin(username, password)) {
            m_currentUserId = 9999;
            m_currentUsername = username.trimmed();
            m_currentUserIsAdmin = true;
            return true;
        }
        if (username.trimmed().compare("admin", Qt::CaseInsensitive) == 0) {
            if (errorMessage) {
                *errorMessage = "密码错误。";
            }
            return false;
        }

        ensureLocalUsersLoaded();
        const QString normalizedUsername = username.trimmed();
        if (!m_localUsers.contains(normalizedUsername)) {
            if (errorMessage) {
                *errorMessage = "用户名不存在。";
            }
            return false;
        }

        if (m_localUsers.value(normalizedUsername) != hashPassword(password)) {
            if (errorMessage) {
                *errorMessage = "密码错误。";
            }
            return false;
        }

        m_currentUserId = 1;
        m_currentUsername = normalizedUsername;
        m_currentUserIsAdmin = false;
        return true;
    }

    QSqlQuery query(m_database);
    query.prepare("SELECT id, password_hash, role FROM users WHERE username = ?");
    query.addBindValue(username.trimmed());

    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = "用户名不存在。";
        }
        return false;
    }

    if (query.value("password_hash").toString() != hashPassword(password)) {
        if (errorMessage) {
            *errorMessage = "密码错误。";
        }
        return false;
    }

    m_currentUserId = query.value("id").toInt();
    m_currentUsername = username.trimmed();
    m_currentUserIsAdmin = query.value("role").toString() == "admin";
    return true;
}

bool UserManager::syncCurrentUserToDatabase(QString* errorMessage)
{
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }
    if (m_currentUsername.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = "当前没有登录用户。";
        }
        return false;
    }

    QSqlQuery insertQuery(m_database);
    insertQuery.prepare("INSERT IGNORE INTO users (username, password_hash, role) VALUES (?, ?, ?)");
    insertQuery.addBindValue(m_currentUsername);
    insertQuery.addBindValue(hashPassword("local-session"));
    insertQuery.addBindValue(m_currentUserIsAdmin ? "admin" : "user");
    if (!insertQuery.exec()) {
        if (errorMessage) {
            *errorMessage = insertQuery.lastError().text();
        }
        return false;
    }

    QSqlQuery selectQuery(m_database);
    selectQuery.prepare("SELECT id FROM users WHERE username = ?");
    selectQuery.addBindValue(m_currentUsername);
    if (!selectQuery.exec() || !selectQuery.next()) {
        if (errorMessage) {
            *errorMessage = "同步用户后未找到用户记录。";
        }
        return false;
    }

    m_currentUserId = selectQuery.value("id").toInt();
    return true;
}

bool UserManager::syncLocalUsersToDatabase(QString* errorMessage)
{
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }

    ensureLocalUsersLoaded();
    QHash<QString, QString>::const_iterator it = m_localUsers.constBegin();
    for (; it != m_localUsers.constEnd(); ++it) {
        QSqlQuery lookupQuery(m_database);
        lookupQuery.prepare("SELECT id FROM users WHERE username = ? LIMIT 1");
        lookupQuery.addBindValue(it.key());
        if (!lookupQuery.exec()) {
            if (errorMessage) {
                *errorMessage = lookupQuery.lastError().text();
            }
            return false;
        }

        QSqlQuery saveQuery(m_database);
        if (lookupQuery.next()) {
            saveQuery.prepare("UPDATE users SET password_hash = ? WHERE username = ?");
            saveQuery.addBindValue(it.value());
            saveQuery.addBindValue(it.key());
        } else {
            saveQuery.prepare("INSERT INTO users (username, password_hash, role) VALUES (?, ?, 'user')");
            saveQuery.addBindValue(it.key());
            saveQuery.addBindValue(it.value());
        }

        if (!saveQuery.exec()) {
            if (errorMessage) {
                *errorMessage = saveQuery.lastError().text();
            }
            return false;
        }
    }

    return true;
}

void UserManager::logout()
{
    m_currentUserId = 0;
    m_currentUsername.clear();
    m_currentUserIsAdmin = false;
}

int UserManager::currentUserId() const
{
    return m_currentUserId;
}

QString UserManager::currentUsername() const
{
    return m_currentUsername;
}

bool UserManager::currentUserIsAdmin() const
{
    return m_currentUserIsAdmin;
}

QVector<ManagedUser> UserManager::loadUsers(QString* errorMessage) const
{
    QVector<ManagedUser> users;
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return users;
    }

    QSqlQuery query(m_database);
    if (!query.exec("SELECT id, username, role FROM users ORDER BY id")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return users;
    }

    while (query.next()) {
        users.push_back({
            query.value("id").toInt(),
            query.value("username").toString(),
            query.value("role").toString()
        });
    }
    return users;
}

bool UserManager::deleteUser(int userId, QString* errorMessage)
{
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }
    if (userId == m_currentUserId) {
        if (errorMessage) {
            *errorMessage = "不能删除当前登录的管理员账号。";
        }
        return false;
    }

    QSqlQuery targetQuery(m_database);
    targetQuery.prepare("SELECT role FROM users WHERE id = ?");
    targetQuery.addBindValue(userId);
    if (!targetQuery.exec() || !targetQuery.next()) {
        if (errorMessage) {
            *errorMessage = "用户不存在。";
        }
        return false;
    }

    if (targetQuery.value("role").toString() == "admin") {
        QSqlQuery countQuery(m_database);
        if (!countQuery.exec("SELECT COUNT(*) FROM users WHERE role = 'admin'") || !countQuery.next()
            || countQuery.value(0).toInt() <= 1) {
            if (errorMessage) {
                *errorMessage = "不能删除最后一个管理员账号。";
            }
            return false;
        }
    }

    QSqlQuery query(m_database);
    query.prepare("DELETE FROM users WHERE id = ?");
    query.addBindValue(userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

bool UserManager::resetUserPassword(int userId, const QString& newPassword, QString* errorMessage)
{
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }
    if (newPassword.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "新密码不能为空。";
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET password_hash = ? WHERE id = ?");
    query.addBindValue(hashPassword(newPassword));
    query.addBindValue(userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool UserManager::updateUserRole(int userId, const QString& role, QString* errorMessage)
{
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接。";
        }
        return false;
    }
    if (role != "admin" && role != "user") {
        if (errorMessage) {
            *errorMessage = "角色只能是 admin 或 user。";
        }
        return false;
    }
    if (userId == m_currentUserId && role != "admin") {
        if (errorMessage) {
            *errorMessage = "不能取消当前登录管理员自己的管理员权限。";
        }
        return false;
    }

    QSqlQuery targetQuery(m_database);
    targetQuery.prepare("SELECT role FROM users WHERE id = ?");
    targetQuery.addBindValue(userId);
    if (!targetQuery.exec() || !targetQuery.next()) {
        if (errorMessage) {
            *errorMessage = "用户不存在。";
        }
        return false;
    }

    if (targetQuery.value("role").toString() == "admin" && role != "admin") {
        QSqlQuery countQuery(m_database);
        if (!countQuery.exec("SELECT COUNT(*) FROM users WHERE role = 'admin'") || !countQuery.next()
            || countQuery.value(0).toInt() <= 1) {
            if (errorMessage) {
                *errorMessage = "不能取消最后一个管理员账号。";
            }
            return false;
        }
    }

    QSqlQuery query(m_database);
    query.prepare("UPDATE users SET role = ? WHERE id = ?");
    query.addBindValue(role);
    query.addBindValue(userId);
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

QString UserManager::hashPassword(const QString& password) const
{
    return QString::fromUtf8(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool UserManager::isValidUsernameFormat(const QString& username) const
{
    static const QRegularExpression pattern("^[A-Za-z0-9_@.\\-#$!%+~]+$");
    return pattern.match(username.trimmed()).hasMatch();
}

bool UserManager::isBuiltInAdmin(const QString& username, const QString& password) const
{
    return username.trimmed().compare("admin", Qt::CaseInsensitive) == 0 && password == "123456";
}

bool UserManager::isReservedUsername(const QString& username) const
{
    return username.trimmed() == "admin";
}

void UserManager::ensureLocalUsersLoaded()
{
    if (m_localUsersLoaded) {
        return;
    }

    m_localUsers.clear();
    QFile file(m_localStorePath.isEmpty() ? defaultLocalStorePath() : m_localStorePath);
    if (!file.exists()) {
        m_localUsersLoaded = true;
        return;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_localUsersLoaded = true;
        return;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int separatorIndex = line.indexOf(':');
        if (separatorIndex <= 0) {
            continue;
        }

        const QString username = line.left(separatorIndex).trimmed();
        const QString passwordHash = line.mid(separatorIndex + 1).trimmed();
        if (!username.isEmpty() && !passwordHash.isEmpty()) {
            m_localUsers.insert(username, passwordHash);
        }
    }

    m_localUsersLoaded = true;
}

bool UserManager::saveLocalUsers(QString* errorMessage) const
{
    const QString storePath = m_localStorePath.isEmpty() ? defaultLocalStorePath() : m_localStorePath;
    QDir directory(QFileInfo(storePath).absolutePath());
    if (!directory.exists() && !directory.mkpath(".")) {
        if (errorMessage) {
            *errorMessage = "无法创建本地账户目录。";
        }
        return false;
    }

    QFile file(storePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = "无法保存本地账户数据。";
        }
        return false;
    }

    QTextStream stream(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
    QHash<QString, QString>::const_iterator it = m_localUsers.constBegin();
    for (; it != m_localUsers.constEnd(); ++it) {
        stream << it.key() << ':' << it.value() << '\n';
    }

    return true;
}

QString UserManager::defaultLocalStorePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + QDir::separator() + ".sign-language-learning-system";
    }
    return dataDir + QDir::separator() + "local-users.dat";
}
