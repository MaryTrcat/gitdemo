cmake_minimum_required(VERSION 3.5)

project(SignLanguageLearningSystem VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Multimedia MultimediaWidgets Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Multimedia MultimediaWidgets Concurrent)

set(APP_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/database/DatabaseManager.cpp
    src/models/Gesture.cpp
    src/models/PracticeRecord.cpp
    src/modules/UserManager.cpp
    src/modules/GestureLibrary.cpp
    src/modules/CameraCapture.cpp
    src/modules/HandKeypointDetector.cpp
    src/modules/FeedbackGenerator.cpp
    src/modules/LearningStatistics.cpp
    src/modules/AssessmentManager.cpp
    src/ai/GestureScorer.cpp
)

add_executable(SignLanguageLearningSystem WIN32 ${APP_SOURCES})

target_include_directories(SignLanguageLearningSystem PRIVATE src)
target_link_libraries(SignLanguageLearningSystem PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
    Qt${QT_VERSION_MAJOR}::Concurrent
)

add_custom_command(TARGET SignLanguageLearningSystem POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SignLanguageLearningSystem>/scripts
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SignLanguageLearningSystem>/models
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/scripts/hand_keypoint_detector.py
        $<TARGET_FILE_DIR:SignLanguageLearningSystem>/scripts/hand_keypoint_detector.py
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/models/hand_landmarker.task
        $<TARGET_FILE_DIR:SignLanguageLearningSystem>/models/hand_landmarker.task
)

add_executable(GestureScorerTests
    tests/test_scorer.cpp
    src/ai/GestureScorer.cpp
)
target_include_directories(GestureScorerTests PRIVATE src)

add_executable(UserManagerTests
    tests/test_user_manager.cpp
    src/modules/UserManager.cpp
)
target_include_directories(UserManagerTests PRIVATE src)
target_link_libraries(UserManagerTests PRIVATE Qt${QT_VERSION_MAJOR}::Sql)

add_executable(GestureDisplayTests
    tests/test_gesture_display.cpp
)
target_include_directories(GestureDisplayTests PRIVATE src)
target_link_libraries(GestureDisplayTests PRIVATE Qt${QT_VERSION_MAJOR}::Core)

add_executable(GestureLibraryCacheTests
    tests/test_gesture_library_cache.cpp
    src/modules/GestureLibrary.cpp
)
target_include_directories(GestureLibraryCacheTests PRIVATE src)
target_link_libraries(GestureLibraryCacheTests PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)

add_executable(LoginSubmitGuardTests
    tests/test_login_submit_guard.cpp
)
target_include_directories(LoginSubmitGuardTests PRIVATE src)

add_executable(AppLogoTests
    tests/test_app_logo.cpp
)
target_include_directories(AppLogoTests PRIVATE src)
target_link_libraries(AppLogoTests PRIVATE Qt${QT_VERSION_MAJOR}::Gui)
