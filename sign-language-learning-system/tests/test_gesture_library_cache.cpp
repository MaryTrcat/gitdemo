#include "modules/GestureLibrary.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <cassert>

int main()
{
    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString cachePath = QDir(tempDir.path()).filePath("gesture-cache.json");

    Gesture hello;
    hello.id = 10;
    hello.name = "你好";
    hello.difficulty = 1;
    hello.description = "缓存里的问候动作。";
    hello.videoPath = "videos/hello.mp4";

    Gesture custom;
    custom.id = 11;
    custom.name = "新词";
    custom.difficulty = 2;
    custom.description = "数据库同步后的新增动作。";

    GestureLibrary writer;
    writer.setCacheFilePath(cachePath);
    assert(writer.cacheGesturesForOfflineUse({hello, custom}));
    assert(QFile::exists(cachePath));

    GestureLibrary offlineReader;
    offlineReader.setCacheFilePath(cachePath);
    const QVector<Gesture> gestures = offlineReader.loadGestures();

    assert(gestures.size() == 2);
    assert(gestures.at(0).name == "你好");
    assert(gestures.at(1).name == "新词");
    assert(gestures.at(1).difficulty == 2);

    return 0;
}
