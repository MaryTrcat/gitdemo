#pragma once

#include "ai/GestureScorer.h"
#include "database/DatabaseManager.h"
#include "models/Gesture.h"
#include "modules/AssessmentManager.h"
#include "modules/CameraCapture.h"
#include "modules/FeedbackGenerator.h"
#include "modules/GestureLibrary.h"
#include "modules/HandKeypointDetector.h"
#include "modules/LearningStatistics.h"
#include "modules/UserManager.h"

#include <QComboBox>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QtGlobal>
#if QT_VERSION_MAJOR >= 6
#include <QAudioOutput>
#endif
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVector>
#include <QVideoWidget>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void connectDatabase();
    void registerUser();
    void loginUser();
    void logoutUser();
    void showLoginDialog();
    void reloadGestures();
    void startCamera();
    void stopCamera();
    void importOrUpdateGestureVideo();
    void showAccountManagementDialog();
    void playTeachingVideo();
    void playExamReferenceVideo();
    void stopTeachingVideo();
    void updateTeachingVideoStatus();
    void refreshDashboard();
    void practiceSelectedGesture();
    void runAssessment();

private:
    void buildUi();
    void showModule(int index, const QString& moduleName);
    void refreshGestureList();
    void refreshLibrarySummary();
    Gesture selectedGesture() const;
    void showMessage(const QString& message);
    void refreshLearningPanels();
    void updateResponsiveLayout();
    void updateMainScrollBarPolicy();
    void updateAdminControls();
    void finishPracticeScoring(const HandDetectionResult& detection);
    void ensureTeachingVideoWidget();
    void reloadAccountUsers();
    int selectedAccountUserId() const;
    void applySharedTeachingVideos();
    bool storeSharedTeachingVideo(const Gesture& gesture, const QString& sourcePath, QString* storedPath, QString* errorMessage);
    QString sharedTeachingVideoPath(int gestureId) const;

    DatabaseManager m_databaseManager;
    UserManager m_userManager;
    GestureLibrary m_gestureLibrary;
    CameraCapture m_cameraCapture;
    HandKeypointDetector m_detector;
    GestureScorer m_scorer;
    FeedbackGenerator m_feedbackGenerator;
    LearningStatistics m_statistics;
    AssessmentManager m_assessment;

    QVector<Gesture> m_gestures;
    QVector<QPushButton*> m_navButtons;

    QLineEdit* m_hostEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QLineEdit* m_databaseEdit = nullptr;
    QLineEdit* m_dbUserEdit = nullptr;
    QLineEdit* m_dbPasswordEdit = nullptr;
    QPushButton* m_connectDbButton = nullptr;
    QProgressBar* m_dbConnectingProgress = nullptr;
    QLabel* m_dbConnectingLabel = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLineEdit* m_passwordEdit = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_currentUserLabel = nullptr;
    QLabel* m_moduleTitleLabel = nullptr;
    QLabel* m_moduleDescriptionLabel = nullptr;
    QScrollArea* m_mainScrollArea = nullptr;
    QStackedWidget* m_moduleStack = nullptr;
    QComboBox* m_gestureCombo = nullptr;
    QPushButton* m_importVideoButton = nullptr;
    QPushButton* m_manageAccountsButton = nullptr;
    QTableWidget* m_accountTable = nullptr;
    int m_accountManagementIndex = -1;
    QFrame* m_cameraFrame = nullptr;
    QWidget* m_practiceActionPanel = nullptr;
    QHBoxLayout* m_workLayout = nullptr;
    QFrame* m_practiceBox = nullptr;
    QStackedWidget* m_cameraStack = nullptr;
    QVideoWidget* m_videoWidget = nullptr;
    QLabel* m_cameraPlaceholder = nullptr;
    QLabel* m_cameraStatusLabel = nullptr;
    QFrame* m_teachingVideoFrame = nullptr;
    QFrame* m_libraryBox = nullptr;
    QWidget* m_teachingActionPanel = nullptr;
    QLabel* m_videoLibraryStatusLabel = nullptr;
    QStackedWidget* m_teachingVideoStack = nullptr;
    QLabel* m_teachingVideoPlaceholder = nullptr;
    QVideoWidget* m_teachingVideoWidget = nullptr;
    QLabel* m_teachingVideoStatusLabel = nullptr;
    QMediaPlayer* m_teachingPlayer = nullptr;
    QString m_currentTeachingVideoPath;
#if QT_VERSION_MAJOR >= 6
    QAudioOutput* m_teachingAudio = nullptr;
#endif
    QLabel* m_scoreLabel = nullptr;
    QFrame* m_examCameraFrame = nullptr;
    QStackedWidget* m_examCameraStack = nullptr;
    QLabel* m_examCameraPlaceholder = nullptr;
    QVideoWidget* m_examVideoWidget = nullptr;
    QLabel* m_examCameraStatusLabel = nullptr;
    QFrame* m_examReferenceVideoFrame = nullptr;
    QVideoWidget* m_examReferenceVideoWidget = nullptr;
    QLabel* m_examReferenceVideoStatusLabel = nullptr;
    QLabel* m_examScoreLabel = nullptr;
    QPlainTextEdit* m_examFeedbackText = nullptr;
    QLabel* m_homePracticeValue = nullptr;
    QLabel* m_homeAverageValue = nullptr;
    QLabel* m_homeWrongValue = nullptr;
    QLabel* m_homeTaskLabel = nullptr;
    QListWidget* m_homeRecentList = nullptr;
    QLabel* m_libraryDetailLabel = nullptr;
    QListWidget* m_libraryOverviewList = nullptr;
    QListWidget* m_libraryCategoryList = nullptr;
    QLabel* m_scoreTrendLabel = nullptr;
    QListWidget* m_recentPracticeList = nullptr;
    QLabel* m_examRuleLabel = nullptr;
    QLabel* m_systemStatusDetailLabel = nullptr;
    QLabel* m_statsSummaryLabel = nullptr;
    QListWidget* m_wrongAnswerList = nullptr;
    QPlainTextEdit* m_feedbackText = nullptr;
    QFutureWatcher<HandDetectionResult>* m_detectionWatcher = nullptr;
    bool m_detectionRunning = false;
    int m_pendingScoringGestureId = 0;
    QString m_pendingAssessmentText;
};
