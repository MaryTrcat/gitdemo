#include "modules/GestureLibrary.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

static QString keypointsToJson(const std::vector<HandKeypoint>& keypoints)
{
    QJsonArray array;
    for (const HandKeypoint& point : keypoints) {
        QJsonObject object;
        object.insert("x", point.x);
        object.insert("y", point.y);
        object.insert("z", point.z);
        array.append(object);
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

static std::vector<HandKeypoint> keypointsFromJson(const QString& json)
{
    std::vector<HandKeypoint> keypoints;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        return keypoints;
    }
    const QJsonArray array = document.array();
    keypoints.reserve(array.size());
    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        keypoints.push_back({
            object.value("x").toDouble(),
            object.value("y").toDouble(),
            object.value("z").toDouble()
        });
    }
    return keypoints;
}

static QJsonObject gestureToJsonObject(const Gesture& gesture)
{
    QJsonObject object;
    object.insert("id", gesture.id);
    object.insert("name", gesture.name);
    object.insert("category", gesture.category);
    object.insert("difficulty", gesture.difficulty);
    object.insert("description", gesture.description);
    object.insert("videoPath", gesture.videoPath);
    object.insert("referenceKeypoints", QJsonDocument::fromJson(keypointsToJson(gesture.referenceKeypoints).toUtf8()).array());
    return object;
}

static Gesture gestureFromJsonObject(const QJsonObject& object)
{
    Gesture gesture;
    gesture.id = object.value("id").toInt();
    gesture.name = object.value("name").toString();
    gesture.category = object.value("category").toString();
    gesture.difficulty = object.value("difficulty").toInt(1);
    gesture.description = object.value("description").toString();
    gesture.videoPath = object.value("videoPath").toString();
    gesture.referenceKeypoints = keypointsFromJson(QString::fromUtf8(QJsonDocument(object.value("referenceKeypoints").toArray()).toJson(QJsonDocument::Compact)));
    return gesture;
}

GestureLibrary::GestureLibrary(QSqlDatabase database)
    : m_database(database)
{
}

void GestureLibrary::setDatabase(QSqlDatabase database)
{
    m_database = database;
}

void GestureLibrary::setCacheFilePath(const QString& path)
{
    m_cacheFilePath = path;
}

QVector<Gesture> GestureLibrary::loadGestures() const
{
    QVector<Gesture> gestures;

    if (m_database.isOpen()) {
        QSqlQuery query(m_database);
        query.exec("SELECT id, name, category, difficulty, description, video_path, reference_keypoints FROM gesture_library ORDER BY difficulty, id");
        while (query.next()) {
            gestures.push_back({
                query.value("id").toInt(),
                query.value("name").toString(),
                query.value("category").toString(),
                query.value("difficulty").toInt(),
                query.value("description").toString(),
                query.value("video_path").toString(),
                keypointsFromJson(query.value("reference_keypoints").toString())
            });
        }
        if (!gestures.isEmpty()) {
            cacheGesturesForOfflineUse(gestures);
        }
    }

    if (gestures.isEmpty()) {
        gestures = loadCachedGestures();
    }

    if (gestures.isEmpty()) {
        gestures = fallbackGestures();
    }

    return gestures;
}

Gesture GestureLibrary::fallbackGesture() const
{
    return {1, "你好", "常用词", 1, "常用问候手语动作。", "", {}};
}

QVector<Gesture> GestureLibrary::fallbackGestures() const
{
    return {
        fallbackGesture(),
        {2, "谢谢", "常用词", 1, "表达感谢的手语动作。", "", {}},
        {3, "学习", "学习生活", 2, "表示学习行为的手语动作。", "", {}},
        {4, "我爱你", "常用词", 2, "常见表达手语动作。", "", {}}
    };
}

QString GestureLibrary::cacheFilePath() const
{
    if (!m_cacheFilePath.isEmpty()) {
        return m_cacheFilePath;
    }

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + QDir::separator() + ".sign-language-learning-system";
    }
    return dataDir + QDir::separator() + "gesture-library-cache.json";
}

QVector<Gesture> GestureLibrary::loadCachedGestures() const
{
    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        return {};
    }

    QVector<Gesture> gestures;
    for (const QJsonValue& value : document.array()) {
        const QJsonObject object = value.toObject();
        const Gesture gesture = gestureFromJsonObject(object);
        if (!gesture.name.trimmed().isEmpty()) {
            gestures.push_back(gesture);
        }
    }
    return gestures;
}

bool GestureLibrary::cacheGesturesForOfflineUse(const QVector<Gesture>& gestures) const
{
    const QString path = cacheFilePath();
    QDir dir(QFileInfo(path).absolutePath());
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    QJsonArray array;
    for (const Gesture& gesture : gestures) {
        if (!gesture.name.trimmed().isEmpty()) {
            array.append(gestureToJsonObject(gesture));
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
    return true;
}

bool GestureLibrary::addGesture(const Gesture& gesture, int* newGestureId, QString* errorMessage) const
{
    if (!m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "MySQL 尚未连接，无法加入手势资源库。";
        }
        return false;
    }

    QSqlQuery duplicateQuery(m_database);
    duplicateQuery.prepare("SELECT id FROM gesture_library WHERE name = ? LIMIT 1");
    duplicateQuery.addBindValue(gesture.name.trimmed());
    if (!duplicateQuery.exec()) {
        if (errorMessage) {
            *errorMessage = duplicateQuery.lastError().text();
        }
        return false;
    }
    if (duplicateQuery.next()) {
        if (errorMessage) {
            *errorMessage = "手势名称已存在，请换一个名称。";
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT INTO gesture_library (name, category, difficulty, description, video_path, reference_keypoints) "
                  "VALUES (?, ?, ?, ?, ?, CONVERT(? USING utf8mb4))");
    query.addBindValue(gesture.name.trimmed());
    query.addBindValue(gesture.category.trimmed());
    query.addBindValue(gesture.difficulty);
    query.addBindValue(gesture.description.trimmed());
    query.addBindValue(gesture.videoPath);
    query.addBindValue(keypointsToJson(gesture.referenceKeypoints));
    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    if (newGestureId) {
        *newGestureId = query.lastInsertId().toInt();
    }
    return true;
}
