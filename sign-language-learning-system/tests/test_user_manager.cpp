#include "modules/UserManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <iostream>

static int expect(bool condition, const char* message)
{
    if (!condition) {
        std::cout << "FAIL: " << message << '\n';
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir temporaryDir;
    const QString userStorePath = temporaryDir.path() + QDir::separator() + "users.dat";

    UserManager manager;
    manager.setLocalStorePath(userStorePath);
    QString error;
    int failures = 0;

    failures += expect(!manager.login("alice", "123456", &error),
                       "unregistered local user must not log in");
    failures += expect(error == "用户名不存在。", "unregistered local login should explain missing user");

    failures += expect(manager.registerUser("alice", "123456", &error),
                       "local registration should succeed");
    failures += expect(manager.currentUserId() == 0,
                       "registration should not automatically log in");
    failures += expect(!manager.registerUser("alice", "another-password", &error),
                       "duplicate local username must not register");
    failures += expect(error == "用户名已存在。", "duplicate local username should explain duplicate");
    failures += expect(manager.registerUser("Alice", "another-password", &error),
                       "local username with different case should be allowed");
    failures += expect(!manager.registerUser("张三", "000000", &error),
                       "chinese local username must not register");
    failures += expect(error == "用户名只能使用英文字母、数字和常用英文符号，不能包含中文或空格。",
                       "chinese username should explain allowed characters");
    failures += expect(manager.registerUser("user_01@test", "000000", &error),
                       "username with safe symbols should register");
    failures += expect(!manager.registerUser("admin", "000000", &error),
                       "admin username must be reserved for administrator");

    failures += expect(!manager.login("alice", "wrong-password", &error),
                       "wrong local password must not log in");
    failures += expect(error == "密码错误。", "wrong local password should explain password error");

    failures += expect(manager.login("alice", "123456", &error),
                       "registered local user should log in with correct password");
    failures += expect(manager.currentUserId() == 1, "successful local login should set current user id");
    failures += expect(manager.currentUsername() == "alice", "successful local login should set username");
    failures += expect(QFile::exists(userStorePath), "local registration should create a user store file");

    UserManager restartedManager;
    restartedManager.setLocalStorePath(userStorePath);
    failures += expect(restartedManager.login("alice", "123456", &error),
                       "registered local user should still log in after restart");
    failures += expect(restartedManager.currentUsername() == "alice",
                       "persistent local login should restore username");
    failures += expect(!restartedManager.currentUserIsAdmin(),
                       "registered local user should not be admin");

    UserManager adminManager;
    adminManager.setLocalStorePath(userStorePath);
    failures += expect(!adminManager.login("admin", "wrong-password", &error),
                       "built-in admin with wrong password must not log in");
    failures += expect(error == "密码错误。", "wrong built-in admin password should explain password error");
    failures += expect(adminManager.login("admin", "123456", &error),
                       "built-in admin should log in");
    failures += expect(adminManager.currentUserIsAdmin(),
                       "built-in admin login should set admin role");

    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "user_manager_sync_test");
        database.setDatabaseName(":memory:");
        failures += expect(database.open(), "test database should open");
        {
            QSqlQuery createQuery(database);
            failures += expect(createQuery.exec("CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, username TEXT UNIQUE, password_hash TEXT, role TEXT)"),
                               "test users table should be created");
        }

        UserManager syncManager(database);
        syncManager.setLocalStorePath(userStorePath);
        failures += expect(syncManager.syncLocalUsersToDatabase(&error),
                           "local users should sync to database");

        QStringList syncedUsers;
        QStringList syncedRoles;
        {
            QSqlQuery userQuery(database);
            failures += expect(userQuery.exec("SELECT username, role FROM users ORDER BY username"),
                               "synced users should be queryable");
            while (userQuery.next()) {
                syncedUsers << userQuery.value(0).toString();
                syncedRoles << userQuery.value(1).toString();
            }
        }
        failures += expect(syncedUsers.contains("alice"), "synced database should contain alice");
        failures += expect(syncedUsers.contains("Alice"), "synced database should contain case-distinct Alice");
        failures += expect(syncedUsers.contains("user_01@test"), "synced database should contain symbol username");
        failures += expect(!syncedUsers.contains("admin"), "local sync should not create built-in admin");
        failures += expect(!syncedRoles.contains("admin"), "local synced users should remain normal users");

        database.close();
    }
    QSqlDatabase::removeDatabase("user_manager_sync_test");

    return failures == 0 ? 0 : 1;
}
