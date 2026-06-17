#pragma once

#include <QSqlDatabase>
#include <QHash>
#include <QString>
#include <QVector>

struct ManagedUser {
    int id = 0;
    QString username;
    QString role;
};

class UserManager {
public:
    explicit UserManager(QSqlDatabase database = QSqlDatabase());

    void setDatabase(QSqlDatabase database);
    void setLocalStorePath(const QString& path);
    bool registerUser(const QString& username, const QString& password, QString* errorMessage);
    bool login(const QString& username, const QString& password, QString* errorMessage);
    bool syncCurrentUserToDatabase(QString* errorMessage);
    bool syncLocalUsersToDatabase(QString* errorMessage);
    void logout();
    int currentUserId() const;
    QString currentUsername() const;
    bool currentUserIsAdmin() const;
    QVector<ManagedUser> loadUsers(QString* errorMessage) const;
    bool deleteUser(int userId, QString* errorMessage);
    bool resetUserPassword(int userId, const QString& newPassword, QString* errorMessage);
    bool updateUserRole(int userId, const QString& role, QString* errorMessage);

private:
    QString hashPassword(const QString& password) const;
    bool isValidUsernameFormat(const QString& username) const;
    bool isReservedUsername(const QString& username) const;
    bool isBuiltInAdmin(const QString& username, const QString& password) const;
    void ensureLocalUsersLoaded();
    bool saveLocalUsers(QString* errorMessage) const;
    QString defaultLocalStorePath() const;

    QSqlDatabase m_database;
    QHash<QString, QString> m_localUsers;
    bool m_localUsersLoaded = false;
    QString m_localStorePath;
    int m_currentUserId = 0;
    QString m_currentUsername;
    bool m_currentUserIsAdmin = false;
};
