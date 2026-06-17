#include "MainWindow.h"

#include "models/PracticeRecord.h"
#include "ui/AppLogo.h"
#include "ui/LoginSubmitGuard.h"

#include <QApplication>
#include <QAbstractScrollArea>
#if QT_VERSION_MAJOR < 6
#include <QDesktopWidget>
#endif
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QDialog>
#include <QInputDialog>
#include <QInputMethod>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cmath>

static void centerWidgetOnScreen(QWidget* widget, QWidget* reference = nullptr)
{
    if (!widget) {
        return;
    }

#if QT_VERSION_MAJOR >= 6
    QScreen* screen = reference && reference->screen() ? reference->screen() : QGuiApplication::primaryScreen();
    const QRect availableGeometry = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
#else
    QDesktopWidget* desktop = QApplication::desktop();
    const QRect availableGeometry = desktop
        ? desktop->availableGeometry(reference ? reference : widget)
        : QRect(0, 0, 1280, 720);
#endif

    const QSize windowSize = widget->size().isValid() ? widget->size() : widget->sizeHint();
    const int x = availableGeometry.x() + (availableGeometry.width() - windowSize.width()) / 2;
    const int y = availableGeometry.y() + (availableGeometry.height() - windowSize.height()) / 2;
    widget->move(qMax(availableGeometry.x(), x), qMax(availableGeometry.y(), y));
}

static void preferEnglishInput(QLineEdit* edit)
{
    if (!edit) {
        return;
    }
    edit->setInputMethodHints(Qt::ImhPreferLatin | Qt::ImhLatinOnly | Qt::ImhNoPredictiveText);
}

static void preferEnglishPasswordInput(QLineEdit* edit)
{
    if (!edit) {
        return;
    }
    edit->setInputMethodHints(Qt::ImhPreferLatin | Qt::ImhLatinOnly | Qt::ImhHiddenText |
                              Qt::ImhSensitiveData | Qt::ImhNoPredictiveText);
}

static void focusEnglishInput(QLineEdit* edit)
{
    if (!edit) {
        return;
    }
#ifdef Q_OS_WIN
    LoadKeyboardLayoutW(L"00000409", KLF_ACTIVATE);
#endif
    edit->setFocus(Qt::OtherFocusReason);
    if (QInputMethod* inputMethod = QGuiApplication::inputMethod()) {
        inputMethod->reset();
        inputMethod->update(Qt::ImQueryAll);
    }
}

static QString serializeKeypoints(const std::vector<HandKeypoint>& keypoints)
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

static int inferGestureDifficulty(const std::vector<HandKeypoint>& keypoints)
{
    if (keypoints.size() != 21) {
        return 1;
    }
    double minX = keypoints[0].x;
    double maxX = keypoints[0].x;
    double minY = keypoints[0].y;
    double maxY = keypoints[0].y;
    for (const HandKeypoint& point : keypoints) {
        minX = qMin(minX, point.x);
        maxX = qMax(maxX, point.x);
        minY = qMin(minY, point.y);
        maxY = qMax(maxY, point.y);
    }
    const double spread = std::sqrt((maxX - minX) * (maxX - minX) + (maxY - minY) * (maxY - minY));
    if (spread < 0.28) {
        return 1;
    }
    if (spread < 0.42) {
        return 2;
    }
    return 3;
}

static void showLoginNotice(QWidget* parent,
                            const QString& title,
                            const QString& message,
                            const QString& iconText = QStringLiteral("!"),
                            const QString& buttonText = QStringLiteral("确定"))
{
    QDialog notice(parent);
    notice.setWindowTitle(title);
    notice.setModal(true);
    notice.setFixedSize(340, 210);
    notice.setObjectName("noticeDialog");

    auto* root = new QVBoxLayout(&notice);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(18);

    auto* body = new QHBoxLayout();
    body->setSpacing(14);
    auto* icon = new QLabel(iconText, &notice);
    icon->setObjectName("noticeIcon");
    icon->setFixedSize(48, 48);
    icon->setAlignment(Qt::AlignCenter);
    auto* text = new QLabel(message, &notice);
    text->setObjectName("noticeText");
    text->setWordWrap(true);
    body->addWidget(icon);
    body->addWidget(text, 1);
    root->addLayout(body);
    root->addStretch();

    auto* actionRow = new QHBoxLayout();
    actionRow->addStretch();
    auto* okButton = new QPushButton(buttonText, &notice);
    okButton->setObjectName("noticePrimaryButton");
    okButton->setMinimumSize(92, 48);
    QObject::connect(okButton, &QPushButton::clicked, &notice, &QDialog::accept);
    actionRow->addWidget(okButton);
    root->addLayout(actionRow);

    notice.setStyleSheet(R"(
        QDialog#noticeDialog {
            background: #f8f9fa;
        }
        QLabel#noticeIcon {
            color: #ffffff;
            background: #1a56db;
            border-radius: 14px;
            font-size: 24px;
            font-weight: 900;
        }
        QLabel#noticeText {
            color: #191c1d;
            font-size: 16px;
            font-weight: 700;
            line-height: 1.5;
        }
        QPushButton#noticePrimaryButton {
            color: #ffffff;
            background: #1a56db;
            border: 0;
            border-radius: 13px;
            font-size: 16px;
            font-weight: 900;
        }
        QPushButton#noticePrimaryButton:hover {
            background: #003fb1;
        }
    )");

    notice.exec();
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    connectDatabase();
    reloadGestures();
    QTimer::singleShot(0, this, &MainWindow::showLoginDialog);
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    central->setObjectName("page");
    auto tuneActionButton = [](QPushButton* button, int width = 132) {
        button->setMinimumWidth(width);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    };

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* navbar = new QFrame(central);
    navbar->setObjectName("navbar");
    auto* navbarLayout = new QHBoxLayout(navbar);
    navbarLayout->setContentsMargins(32, 14, 32, 14);
    navbarLayout->setSpacing(14);
    auto* navbarLogo = new QLabel(navbar);
    navbarLogo->setObjectName("navbarLogo");
    navbarLogo->setFixedSize(40, 40);
    navbarLogo->setPixmap(QPixmap::fromImage(createAppLogoHandGlyphImage(28)));
    navbarLogo->setAlignment(Qt::AlignCenter);
    auto* navbarTitle = new QLabel("手语动作学习系统", navbar);
    navbarTitle->setObjectName("navbarTitle");
    navbarLayout->addWidget(navbarLogo);
    navbarLayout->addWidget(navbarTitle);
    navbarLayout->addStretch();
    m_currentUserLabel = new QLabel("当前用户：未登录", navbar);
    m_currentUserLabel->setObjectName("userBadge");
    auto* logoutButton = new QPushButton("退出登录", navbar);
    logoutButton->setObjectName("logoutButton");
    navbarLayout->addWidget(m_currentUserLabel);
    navbarLayout->addWidget(logoutButton);
    root->addWidget(navbar);

    auto* body = new QWidget(central);
    body->setObjectName("body");
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto* sidebar = new QFrame(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(280);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(24, 32, 24, 24);
    sidebarLayout->setSpacing(8);

    const QStringList navItems = {"📊  首页总览", "🎯  动作练习", "📚  手势资源库", "📈  学习统计", "📝  考核评测", "⚙  系统设置"};
    for (int i = 0; i < navItems.size(); ++i) {
        auto* item = new QPushButton(navItems[i], sidebar);
        item->setObjectName(i == 1 ? "navActive" : "navItem");
        item->setFlat(true);
        item->setCursor(Qt::PointingHandCursor);
        m_navButtons.push_back(item);
        connect(item, &QPushButton::clicked, this, [this, i, navItems]() {
            showModule(i, navItems[i]);
        });
        sidebarLayout->addWidget(item);
    }
    m_manageAccountsButton = new QPushButton("👥  账号管理", sidebar);
    m_manageAccountsButton->setObjectName("navItem");
    m_manageAccountsButton->setFlat(true);
    m_manageAccountsButton->setCursor(Qt::PointingHandCursor);
    m_manageAccountsButton->hide();
    sidebarLayout->addWidget(m_manageAccountsButton);
    sidebarLayout->addStretch();
    bodyLayout->addWidget(sidebar);

    auto* mainArea = new QWidget(central);
    mainArea->setObjectName("mainArea");
    auto* mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(32, 28, 32, 24);
    mainLayout->setSpacing(24);

    auto* titleRow = new QHBoxLayout();
    auto* titleBox = new QWidget(mainArea);
    auto* titleLayout = new QVBoxLayout(titleBox);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    m_moduleTitleLabel = new QLabel("动作练习", titleBox);
    m_moduleTitleLabel->setObjectName("title");
    m_moduleDescriptionLabel = new QLabel("选择手势、观看标准教学视频，并通过摄像头完成动作评分与纠错反馈。", titleBox);
    m_moduleDescriptionLabel->setObjectName("subtitle");
    titleLayout->addWidget(m_moduleTitleLabel);
    titleLayout->addWidget(m_moduleDescriptionLabel);
    titleRow->addWidget(titleBox, 1);
    mainLayout->addLayout(titleRow);

    m_statusLabel = new QLabel("系统状态：等待连接数据库。", mainArea);
    m_statusLabel->setObjectName("statusBar");
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    auto* topGrid = new QGridLayout();
    topGrid->setHorizontalSpacing(12);
    topGrid->setVerticalSpacing(12);

    auto* dbBox = new QFrame(mainArea);
    dbBox->setObjectName("card");
    auto* dbLayout = new QGridLayout(dbBox);
    dbLayout->setContentsMargins(14, 12, 14, 12);
    dbLayout->setHorizontalSpacing(8);
    dbLayout->setVerticalSpacing(8);
    auto* dbTitle = new QLabel("MySQL 数据库连接", dbBox);
    dbTitle->setObjectName("cardTitle");
#ifdef Q_OS_WIN
    m_hostEdit = new QLineEdit("127.0.0.1", dbBox);
#else
    m_hostEdit = new QLineEdit("192.168.215.1", dbBox);
#endif
    m_portSpin = new QSpinBox(dbBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(3306);
    m_databaseEdit = new QLineEdit("sign_language_learning", dbBox);
    m_dbUserEdit = new QLineEdit("root", dbBox);
    m_dbPasswordEdit = new QLineEdit("000506", dbBox);
    m_dbPasswordEdit->setEchoMode(QLineEdit::Password);
    preferEnglishInput(m_hostEdit);
    preferEnglishInput(m_databaseEdit);
    preferEnglishInput(m_dbUserEdit);
    preferEnglishPasswordInput(m_dbPasswordEdit);
    m_connectDbButton = new QPushButton("连接 MySQL", dbBox);
    m_connectDbButton->setObjectName("primaryButton");
    m_dbConnectingProgress = new QProgressBar(dbBox);
    m_dbConnectingProgress->setObjectName("connectingProgress");
    m_dbConnectingProgress->setRange(0, 0);
    m_dbConnectingProgress->setTextVisible(false);
    m_dbConnectingProgress->hide();
    m_dbConnectingLabel = new QLabel("正在连接 MySQL，请稍等…", dbBox);
    m_dbConnectingLabel->setObjectName("connectingLabel");
    m_dbConnectingLabel->hide();
    dbLayout->addWidget(dbTitle, 0, 0, 1, 6);
    dbLayout->addWidget(new QLabel("主机"), 1, 0);
    dbLayout->addWidget(m_hostEdit, 1, 1);
    dbLayout->addWidget(new QLabel("端口"), 1, 2);
    dbLayout->addWidget(m_portSpin, 1, 3);
    dbLayout->addWidget(new QLabel("库名"), 1, 4);
    dbLayout->addWidget(m_databaseEdit, 1, 5);
    dbLayout->addWidget(new QLabel("用户"), 2, 0);
    dbLayout->addWidget(m_dbUserEdit, 2, 1);
    dbLayout->addWidget(new QLabel("密码"), 2, 2);
    dbLayout->addWidget(m_dbPasswordEdit, 2, 3);
    dbLayout->addWidget(m_connectDbButton, 2, 4, 1, 2);
    dbLayout->addWidget(m_dbConnectingProgress, 3, 0, 1, 2);
    dbLayout->addWidget(m_dbConnectingLabel, 3, 2, 1, 4);

    auto* userBox = new QFrame(mainArea);
    userBox->setObjectName("card");
    auto* userLayout = new QGridLayout(userBox);
    userLayout->setContentsMargins(14, 12, 14, 12);
    userLayout->setHorizontalSpacing(8);
    userLayout->setVerticalSpacing(8);
    auto* userTitle = new QLabel("用户管理", userBox);
    userTitle->setObjectName("cardTitle");
    m_usernameEdit = new QLineEdit(userBox);
    m_passwordEdit = new QLineEdit(userBox);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    preferEnglishInput(m_usernameEdit);
    preferEnglishPasswordInput(m_passwordEdit);
    auto* registerButton = new QPushButton("注册", userBox);
    auto* loginButton = new QPushButton("登录", userBox);
    loginButton->setObjectName("primaryButton");
    userLayout->addWidget(userTitle, 0, 0, 1, 4);
    userLayout->addWidget(new QLabel("用户名"), 1, 0);
    userLayout->addWidget(m_usernameEdit, 1, 1, 1, 3);
    userLayout->addWidget(new QLabel("密码"), 2, 0);
    userLayout->addWidget(m_passwordEdit, 2, 1, 1, 3);
    userLayout->addWidget(registerButton, 3, 0, 1, 2);
    userLayout->addWidget(loginButton, 3, 2, 1, 2);

    auto* metricOne = new QFrame(mainArea);
    metricOne->setObjectName("metricCard");
    auto* metricOneLayout = new QVBoxLayout(metricOne);
    metricOneLayout->setContentsMargins(14, 12, 14, 12);
    metricOneLayout->addWidget(new QLabel("累计练习"));
    auto* metricOneValue = new QLabel("0 次", metricOne);
    metricOneValue->setObjectName("metricValue");
    metricOneLayout->addWidget(metricOneValue);

    auto* metricTwo = new QFrame(mainArea);
    metricTwo->setObjectName("metricCard");
    auto* metricTwoLayout = new QVBoxLayout(metricTwo);
    metricTwoLayout->setContentsMargins(14, 12, 14, 12);
    metricTwoLayout->addWidget(new QLabel("当前手势"));
    auto* metricTwoValue = new QLabel("你好", metricTwo);
    metricTwoValue->setObjectName("metricValue");
    metricTwoLayout->addWidget(metricTwoValue);

    userBox->hide();
    topGrid->setColumnStretch(0, 2);
    topGrid->setColumnStretch(1, 2);
    topGrid->setColumnStretch(2, 2);
    topGrid->setColumnStretch(3, 1);

    m_moduleStack = new QStackedWidget(mainArea);
    m_moduleStack->setObjectName("moduleStack");

    auto* homePage = new QWidget(m_moduleStack);
    auto* homeLayout = new QVBoxLayout(homePage);
    homeLayout->setContentsMargins(0, 0, 0, 0);
    homeLayout->setSpacing(14);
    auto* homeTop = new QHBoxLayout();
    QStringList homeNames = {"今日推荐", "学习概况", "待复习手势"};
    QStringList homeValues = {"你好", "0 次 / 平均分 0", "0 个"};
    for (int i = 0; i < homeNames.size(); ++i) {
        auto* card = new QFrame(homePage);
        card->setObjectName("card");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 14, 16, 14);
        auto* name = new QLabel(homeNames[i], card);
        name->setObjectName("sectionTitle");
        auto* value = new QLabel(homeValues[i], card);
        value->setObjectName("metricValue");
        if (i == 0) {
            m_homePracticeValue = value;
        } else if (i == 1) {
            m_homeAverageValue = value;
        } else {
            m_homeWrongValue = value;
        }
        cardLayout->addWidget(name);
        cardLayout->addWidget(value);
        homeTop->addWidget(card);
    }
    auto* homeGuide = new QFrame(homePage);
    homeGuide->setObjectName("card");
    auto* homeGuideLayout = new QVBoxLayout(homeGuide);
    homeGuideLayout->setContentsMargins(18, 16, 18, 16);
    auto* homeGuideTitle = new QLabel("学习流程", homeGuide);
    homeGuideTitle->setObjectName("cardTitle");
    auto* homeGuideText = new QLabel("1. 选择手势  2. 观看标准教学视频  3. 启动摄像头练习  4. 查看评分与纠错反馈  5. 保存学习记录", homeGuide);
    homeGuideText->setWordWrap(true);
    homeGuideText->setObjectName("hintText");
    homeGuideLayout->addWidget(homeGuideTitle);
    homeGuideLayout->addWidget(homeGuideText);
    auto* homeTaskCard = new QFrame(homePage);
    homeTaskCard->setObjectName("card");
    auto* homeTaskLayout = new QVBoxLayout(homeTaskCard);
    homeTaskLayout->setContentsMargins(18, 16, 18, 16);
    auto* homeTaskTitle = new QLabel("今日学习任务", homeTaskCard);
    homeTaskTitle->setObjectName("cardTitle");
    m_homeTaskLabel = new QLabel("建议完成 3 次动作练习、1 次考核，并复习低分手势。", homeTaskCard);
    m_homeTaskLabel->setWordWrap(true);
    m_homeTaskLabel->setObjectName("hintText");
    homeTaskLayout->addWidget(homeTaskTitle);
    homeTaskLayout->addWidget(m_homeTaskLabel);
    auto* homeRecentCard = new QFrame(homePage);
    homeRecentCard->setObjectName("card");
    auto* homeRecentLayout = new QVBoxLayout(homeRecentCard);
    homeRecentLayout->setContentsMargins(18, 16, 18, 16);
    auto* homeRecentTitle = new QLabel("最近练习记录", homeRecentCard);
    homeRecentTitle->setObjectName("cardTitle");
    m_homeRecentList = new QListWidget(homeRecentCard);
    m_homeRecentList->addItem("暂无练习记录。");
    homeRecentLayout->addWidget(homeRecentTitle);
    homeRecentLayout->addWidget(m_homeRecentList);
    homeLayout->addLayout(homeTop);
    homeLayout->addWidget(homeGuide);
    homeLayout->addWidget(homeTaskCard);
    homeLayout->addWidget(homeRecentCard);
    homeLayout->addStretch();
    m_moduleStack->addWidget(homePage);

    auto* practicePage = new QWidget(m_moduleStack);
    auto* practicePageLayout = new QVBoxLayout(practicePage);
    practicePageLayout->setContentsMargins(0, 0, 0, 0);
    practicePageLayout->setSpacing(14);
    auto* practiceMetricsLayout = new QHBoxLayout();
    practiceMetricsLayout->setSpacing(14);
    practiceMetricsLayout->addWidget(metricOne);
    practiceMetricsLayout->addWidget(metricTwo);
    practicePageLayout->addLayout(practiceMetricsLayout);

    m_workLayout = new QHBoxLayout();
    m_workLayout->setSpacing(14);

    m_practiceBox = new QFrame(mainArea);
    m_practiceBox->setObjectName("card");
    auto* practiceLayout = new QVBoxLayout(m_practiceBox);
    practiceLayout->setContentsMargins(16, 14, 16, 16);
    practiceLayout->setSpacing(10);
    auto* practiceTitle = new QLabel("📹  动作练习", m_practiceBox);
    practiceTitle->setObjectName("cardTitle");
    m_cameraFrame = new QFrame(m_practiceBox);
    m_cameraFrame->setObjectName("cameraFrame");
    m_cameraFrame->setMinimumHeight(320);
    m_cameraFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_cameraFrame->setMinimumWidth(0);
    auto* cameraFrameLayout = new QVBoxLayout(m_cameraFrame);
    cameraFrameLayout->setContentsMargins(0, 0, 0, 0);
    cameraFrameLayout->setSpacing(0);
    m_cameraStack = new QStackedWidget(m_cameraFrame);
    m_cameraStack->setObjectName("cameraStack");
    m_cameraStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_cameraPlaceholder = new QLabel("📷\n摄像头未启动", m_cameraStack);
    m_cameraPlaceholder->setObjectName("cameraPlaceholder");
    m_cameraPlaceholder->setAlignment(Qt::AlignCenter);
    m_videoWidget = new QVideoWidget(m_cameraStack);
    m_videoWidget->setObjectName("videoPanel");
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    m_cameraStack->addWidget(m_cameraPlaceholder);
    m_cameraStack->addWidget(m_videoWidget);
    m_cameraStack->setCurrentWidget(m_cameraPlaceholder);
    cameraFrameLayout->addWidget(m_cameraStack);
    m_cameraStatusLabel = new QLabel("摄像头视频采集模块：未启动", m_practiceBox);
    m_cameraStatusLabel->setObjectName("hintText");
    m_practiceActionPanel = new QWidget(m_practiceBox);
    m_practiceActionPanel->setObjectName("practiceActionPanel");
    m_practiceActionPanel->setMinimumHeight(82);
    m_practiceActionPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* actionRow = new QHBoxLayout(m_practiceActionPanel);
    actionRow->setContentsMargins(4, 8, 4, 4);
    actionRow->setSpacing(8);
    auto* cameraButton = new QPushButton("📹 启动摄像头", m_practiceBox);
    cameraButton->setObjectName("successButton");
    auto* stopCameraButton = new QPushButton("🛑 关闭摄像头", m_practiceBox);
    auto* practiceButton = new QPushButton("🎯 练习选中手势", m_practiceBox);
    practiceButton->setObjectName("infoButton");
    tuneActionButton(cameraButton);
    tuneActionButton(stopCameraButton);
    tuneActionButton(practiceButton);
    actionRow->addWidget(cameraButton);
    actionRow->addWidget(stopCameraButton);
    actionRow->addWidget(practiceButton);
    m_scoreLabel = new QLabel("动作评分模块：等待练习", m_practiceBox);
    m_scoreLabel->setObjectName("scoreText");
    practiceLayout->addWidget(practiceTitle);
    practiceLayout->addWidget(m_cameraFrame);
    practiceLayout->addWidget(m_cameraStatusLabel);
    practiceLayout->addWidget(m_practiceActionPanel, 0, Qt::AlignHCenter);
    practiceLayout->addWidget(m_scoreLabel);

    m_libraryBox = new QFrame(mainArea);
    m_libraryBox->setObjectName("card");
    auto* libraryLayout = new QVBoxLayout(m_libraryBox);
    libraryLayout->setContentsMargins(16, 14, 16, 16);
    libraryLayout->setSpacing(10);
    auto* libraryTitle = new QLabel("📚  手势资源库", m_libraryBox);
    libraryTitle->setObjectName("cardTitle");
    m_gestureCombo = new QComboBox(m_libraryBox);
    m_gestureCombo->setObjectName("gestureCombo");
    m_gestureCombo->setMinimumHeight(52);
    auto* reloadButton = new QPushButton("🔄 刷新手势库", m_libraryBox);
    reloadButton->setMaximumWidth(190);
    reloadButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* teachingTitle = new QLabel("🎬  标准动作教学视频", m_libraryBox);
    teachingTitle->setObjectName("sectionTitle");
    m_teachingVideoFrame = new QFrame(m_libraryBox);
    m_teachingVideoFrame->setObjectName("teachingVideoFrame");
    m_teachingVideoFrame->setMinimumHeight(320);
    m_teachingVideoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_teachingVideoFrame->setMinimumWidth(0);
    auto* teachingVideoLayout = new QVBoxLayout(m_teachingVideoFrame);
    teachingVideoLayout->setContentsMargins(0, 0, 0, 0);
    teachingVideoLayout->setSpacing(0);
    m_teachingVideoStack = new QStackedWidget(m_teachingVideoFrame);
    m_teachingVideoStack->setObjectName("teachingVideoStack");
    m_teachingVideoStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_teachingVideoPlaceholder = new QLabel("🎬\n视频待播放", m_teachingVideoStack);
    m_teachingVideoPlaceholder->setObjectName("teachingVideoPlaceholder");
    m_teachingVideoPlaceholder->setAlignment(Qt::AlignCenter);
    m_teachingVideoStack->addWidget(m_teachingVideoPlaceholder);
    m_teachingVideoStack->setCurrentWidget(m_teachingVideoPlaceholder);
    teachingVideoLayout->addWidget(m_teachingVideoStack);
    m_teachingVideoStatusLabel = new QLabel("当前手势未绑定教学视频", m_libraryBox);
    m_teachingVideoStatusLabel->setObjectName("hintText");
    m_teachingActionPanel = new QWidget(m_libraryBox);
    m_teachingActionPanel->setObjectName("teachingActionPanel");
    m_teachingActionPanel->setMinimumHeight(58);
    m_teachingActionPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto* videoActionRow = new QHBoxLayout(m_teachingActionPanel);
    videoActionRow->setContentsMargins(0, 6, 0, 2);
    videoActionRow->setSpacing(8);
    auto* playVideoButton = new QPushButton("▶ 播放", m_libraryBox);
    playVideoButton->setObjectName("successButton");
    auto* stopVideoButton = new QPushButton("⏸ 停止", m_libraryBox);
    stopVideoButton->setObjectName("warningButton");
    tuneActionButton(playVideoButton, 62);
    tuneActionButton(stopVideoButton, 62);
    for (auto* videoButton : {playVideoButton, stopVideoButton}) {
        videoButton->setMinimumHeight(46);
        videoButton->setMaximumHeight(52);
        videoButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    videoActionRow->addWidget(playVideoButton);
    videoActionRow->addWidget(stopVideoButton);
    libraryLayout->addWidget(libraryTitle);
    libraryLayout->addWidget(m_gestureCombo);
    libraryLayout->addWidget(reloadButton);
    libraryLayout->addWidget(teachingTitle);
    libraryLayout->addWidget(m_teachingVideoFrame);
    libraryLayout->addWidget(m_teachingVideoStatusLabel);
    libraryLayout->addWidget(m_teachingActionPanel, 0, Qt::AlignHCenter);

    m_teachingPlayer = new QMediaPlayer(this);
#if QT_VERSION_MAJOR >= 6
    m_teachingAudio = new QAudioOutput(this);
    m_teachingPlayer->setAudioOutput(m_teachingAudio);
    m_teachingAudio->setVolume(0.7);
#else
    m_teachingPlayer->setVolume(70);
#endif
    connect(m_teachingPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (!m_teachingVideoStack || !m_teachingVideoWidget) {
            return;
        }
        if (status == QMediaPlayer::LoadedMedia
            || status == QMediaPlayer::BufferedMedia
            || status == QMediaPlayer::EndOfMedia) {
            m_teachingVideoStack->setCurrentWidget(m_teachingVideoWidget);
        }
    });

    m_workLayout->addWidget(m_practiceBox, 1);
    m_workLayout->addWidget(m_libraryBox, 1);
    practicePageLayout->addLayout(m_workLayout, 1);

    auto* feedbackBox = new QFrame(mainArea);
    feedbackBox->setObjectName("card");
    auto* feedbackLayout = new QVBoxLayout(feedbackBox);
    feedbackLayout->setContentsMargins(16, 14, 16, 16);
    auto* feedbackTitle = new QLabel("智能纠错反馈与学习统计", feedbackBox);
    feedbackTitle->setObjectName("cardTitle");
    m_feedbackText = new QPlainTextEdit(feedbackBox);
    m_feedbackText->setReadOnly(true);
    m_feedbackText->setFocusPolicy(Qt::NoFocus);
    m_feedbackText->setTextInteractionFlags(Qt::NoTextInteraction);
    m_feedbackText->setCursor(Qt::ArrowCursor);
    m_feedbackText->viewport()->setCursor(Qt::ArrowCursor);
    m_feedbackText->setPlaceholderText("智能纠错反馈、学习统计和错题提示会显示在这里。");
    m_feedbackText->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_feedbackText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_feedbackText->setMinimumHeight(130);
    feedbackLayout->addWidget(feedbackTitle);
    feedbackLayout->addWidget(m_feedbackText);
    practicePageLayout->addWidget(feedbackBox);
    m_moduleStack->addWidget(practicePage);

    auto* libraryPage = new QWidget(m_moduleStack);
    auto* libraryPageLayout = new QVBoxLayout(libraryPage);
    libraryPageLayout->setContentsMargins(0, 0, 0, 0);
    auto* libraryOverview = new QFrame(libraryPage);
    libraryOverview->setObjectName("card");
    auto* libraryOverviewLayout = new QVBoxLayout(libraryOverview);
    libraryOverviewLayout->setContentsMargins(18, 16, 18, 16);
    auto* libraryOverviewTitle = new QLabel("手势资源库管理", libraryOverview);
    libraryOverviewTitle->setObjectName("cardTitle");
    auto* libraryOverviewText = new QLabel("这里显示当前资源库的完整手势清单、视频绑定状态和评分模板状态。管理员上传一次后，动作练习和考核评测都会共用同一份视频。", libraryOverview);
    libraryOverviewText->setWordWrap(true);
    libraryOverviewText->setObjectName("hintText");
    m_libraryOverviewList = new QListWidget(libraryOverview);
    m_libraryOverviewList->setMinimumHeight(120);
    auto* libraryInfoRow = new QHBoxLayout();
    auto* libraryCategoryCard = new QFrame(libraryOverview);
    libraryCategoryCard->setObjectName("card");
    auto* libraryCategoryLayout = new QVBoxLayout(libraryCategoryCard);
    libraryCategoryLayout->setContentsMargins(16, 14, 16, 14);
    auto* libraryCategoryTitle = new QLabel("难度说明", libraryCategoryCard);
    libraryCategoryTitle->setObjectName("sectionTitle");
    m_libraryCategoryList = new QListWidget(libraryCategoryCard);
    libraryCategoryLayout->addWidget(libraryCategoryTitle);
    libraryCategoryLayout->addWidget(m_libraryCategoryList);
    auto* libraryDetailCard = new QFrame(libraryOverview);
    libraryDetailCard->setObjectName("card");
    auto* libraryDetailLayout = new QVBoxLayout(libraryDetailCard);
    libraryDetailLayout->setContentsMargins(16, 14, 16, 14);
    auto* libraryDetailTitle = new QLabel("当前手势详情", libraryDetailCard);
    libraryDetailTitle->setObjectName("sectionTitle");
    m_libraryDetailLabel = new QLabel("选择手势后显示难度、说明、教学视频绑定状态和练习建议。", libraryDetailCard);
    m_libraryDetailLabel->setWordWrap(true);
    m_libraryDetailLabel->setObjectName("hintText");
    libraryDetailLayout->addWidget(libraryDetailTitle);
    libraryDetailLayout->addWidget(m_libraryDetailLabel);
    libraryInfoRow->addWidget(libraryCategoryCard);
    libraryInfoRow->addWidget(libraryDetailCard);
    auto* videoLibraryCard = new QFrame(libraryOverview);
    videoLibraryCard->setObjectName("card");
    auto* videoLibraryLayout = new QVBoxLayout(videoLibraryCard);
    videoLibraryLayout->setContentsMargins(16, 14, 16, 14);
    videoLibraryLayout->setSpacing(10);
    auto* videoLibraryTitle = new QLabel("视频素材库", videoLibraryCard);
    videoLibraryTitle->setObjectName("sectionTitle");
    auto* videoLibraryHint = new QLabel("这里是标准教学视频的统一管理入口。管理员上传后，所有用户在动作练习和考核评测中都会直接读取这份视频。", videoLibraryCard);
    videoLibraryHint->setWordWrap(true);
    videoLibraryHint->setObjectName("hintText");
    m_videoLibraryStatusLabel = new QLabel("请选择一个手势查看视频绑定状态。", videoLibraryCard);
    m_videoLibraryStatusLabel->setWordWrap(true);
    m_videoLibraryStatusLabel->setObjectName("scoreText");
    auto* videoLibraryActions = new QHBoxLayout();
    videoLibraryActions->setSpacing(10);
    m_importVideoButton = new QPushButton("✨ 批量导入/更新手势视频", videoLibraryCard);
    m_importVideoButton->setObjectName("primaryButton");
    tuneActionButton(m_importVideoButton, 176);
    videoLibraryActions->addWidget(m_importVideoButton);
    videoLibraryActions->addStretch();
    videoLibraryLayout->addWidget(videoLibraryTitle);
    videoLibraryLayout->addWidget(videoLibraryHint);
    videoLibraryLayout->addWidget(m_videoLibraryStatusLabel);
    videoLibraryLayout->addLayout(videoLibraryActions);
    libraryOverviewLayout->addWidget(libraryOverviewTitle);
    libraryOverviewLayout->addWidget(libraryOverviewText);
    libraryOverviewLayout->addWidget(m_libraryOverviewList);
    libraryOverviewLayout->addLayout(libraryInfoRow);
    libraryOverviewLayout->addWidget(videoLibraryCard);
    libraryPageLayout->addWidget(libraryOverview);
    m_moduleStack->addWidget(libraryPage);

    auto* statsPage = new QWidget(m_moduleStack);
    auto* statsLayout = new QVBoxLayout(statsPage);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(14);
    auto* statsCards = new QHBoxLayout();
    for (const char* text : {"累计练习：连接数据库后统计", "平均得分：按练习记录计算", "错题数量：低分自动进入错题本"}) {
        auto* card = new QFrame(statsPage);
        card->setObjectName("card");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 14, 16, 14);
        auto* value = new QLabel(text, card);
        value->setObjectName("scoreText");
        value->setWordWrap(true);
        layout->addWidget(value);
        statsCards->addWidget(card);
    }
    auto* trendCard = new QFrame(statsPage);
    trendCard->setObjectName("card");
    auto* trendLayout = new QVBoxLayout(trendCard);
    trendLayout->setContentsMargins(18, 16, 18, 16);
    auto* trendTitle = new QLabel("学习趋势与错误分析", trendCard);
    trendTitle->setObjectName("cardTitle");
    m_statsSummaryLabel = new QLabel("连接 MySQL 并登录后显示真实学习统计。", trendCard);
    m_statsSummaryLabel->setWordWrap(true);
    m_statsSummaryLabel->setObjectName("hintText");
    trendLayout->addWidget(trendTitle);
    trendLayout->addWidget(m_statsSummaryLabel);
    m_scoreTrendLabel = new QLabel("最近得分趋势会在完成练习后显示。", trendCard);
    m_scoreTrendLabel->setWordWrap(true);
    m_scoreTrendLabel->setObjectName("hintText");
    m_recentPracticeList = new QListWidget(trendCard);
    m_recentPracticeList->addItem("暂无练习记录。");
    trendLayout->addWidget(m_scoreTrendLabel);
    trendLayout->addWidget(m_recentPracticeList);
    statsLayout->addLayout(statsCards);
    statsLayout->addWidget(trendCard);
    statsLayout->addStretch();
    m_moduleStack->addWidget(statsPage);

    auto* examPage = new QWidget(m_moduleStack);
    auto* examLayout = new QHBoxLayout(examPage);
    examLayout->setContentsMargins(0, 0, 0, 0);
    examLayout->setSpacing(14);
    auto* examCard = new QFrame(examPage);
    examCard->setObjectName("card");
    auto* examCardLayout = new QVBoxLayout(examCard);
    examCardLayout->setContentsMargins(18, 16, 18, 16);
    auto* examTitle = new QLabel("考核评测", examCard);
    examTitle->setObjectName("cardTitle");
    auto* examText = new QLabel("随机抽取手势进行限时练习，系统根据动作评分判断是否达标。低于 70 分的手势会进入错题复习。", examCard);
    examText->setWordWrap(true);
    examText->setObjectName("hintText");
    m_examRuleLabel = new QLabel("考核规则：随机抽取 1 个手势，启动摄像头后进行 AI 关键点评分；低于 70 分自动加入错题复习。", examCard);
    m_examRuleLabel->setWordWrap(true);
    m_examRuleLabel->setObjectName("scoreText");
    m_examCameraFrame = new QFrame(examCard);
    m_examCameraFrame->setObjectName("cameraFrame");
    m_examCameraFrame->setMinimumHeight(260);
    m_examCameraFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_examCameraFrame->setMinimumWidth(0);
    auto* examCameraLayout = new QVBoxLayout(m_examCameraFrame);
    examCameraLayout->setContentsMargins(0, 0, 0, 0);
    m_examCameraStack = new QStackedWidget(m_examCameraFrame);
    m_examCameraPlaceholder = new QLabel("📷\n考核摄像头未启动", m_examCameraStack);
    m_examCameraPlaceholder->setObjectName("cameraPlaceholder");
    m_examCameraPlaceholder->setAlignment(Qt::AlignCenter);
    m_examVideoWidget = new QVideoWidget(m_examCameraStack);
    m_examVideoWidget->setObjectName("videoPanel");
    m_examVideoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    m_examCameraStack->addWidget(m_examCameraPlaceholder);
    m_examCameraStack->addWidget(m_examVideoWidget);
    m_examCameraStack->setCurrentWidget(m_examCameraPlaceholder);
    examCameraLayout->addWidget(m_examCameraStack);
    m_examCameraStatusLabel = new QLabel("考核摄像头模块：未启动", examCard);
    m_examCameraStatusLabel->setObjectName("hintText");
    auto* examReferenceTitle = new QLabel("标准教学视频", examCard);
    examReferenceTitle->setObjectName("sectionTitle");
    m_examReferenceVideoFrame = new QFrame(examCard);
    m_examReferenceVideoFrame->setObjectName("teachingVideoFrame");
    m_examReferenceVideoFrame->setMinimumHeight(150);
    m_examReferenceVideoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_examReferenceVideoFrame->setMinimumWidth(0);
    auto* examReferenceVideoLayout = new QVBoxLayout(m_examReferenceVideoFrame);
    examReferenceVideoLayout->setContentsMargins(0, 0, 0, 0);
    examReferenceVideoLayout->setSpacing(0);
    m_examReferenceVideoWidget = new QVideoWidget(m_examReferenceVideoFrame);
    m_examReferenceVideoWidget->setObjectName("teachingVideo");
    m_examReferenceVideoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    m_examReferenceVideoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    examReferenceVideoLayout->addWidget(m_examReferenceVideoWidget);
    m_examReferenceVideoStatusLabel = new QLabel("点击“查看标准视频”后，会在这里播放统一素材库里的标准动作视频。", examCard);
    m_examReferenceVideoStatusLabel->setWordWrap(true);
    m_examReferenceVideoStatusLabel->setObjectName("hintText");
    auto* examActionPanel = new QWidget(examCard);
    auto* examActionRow = new QHBoxLayout(examActionPanel);
    examActionRow->setContentsMargins(0, 8, 0, 4);
    examActionRow->setSpacing(10);
    auto* examStartCameraButton = new QPushButton("📹 启动考核摄像头", examCard);
    examStartCameraButton->setObjectName("successButton");
    auto* examStopCameraButton = new QPushButton("🛑 关闭摄像头", examCard);
    auto* examReferenceButton = new QPushButton("▶ 查看标准视频", examCard);
    examReferenceButton->setObjectName("infoButton");
    auto* examStartButton = new QPushButton("开始一次随机考核", examCard);
    examStartButton->setObjectName("primaryButton");
    tuneActionButton(examStartCameraButton);
    tuneActionButton(examStopCameraButton);
    tuneActionButton(examReferenceButton);
    tuneActionButton(examStartButton);
    examActionRow->addWidget(examStartCameraButton);
    examActionRow->addWidget(examStopCameraButton);
    examActionRow->addWidget(examReferenceButton);
    examActionRow->addWidget(examStartButton);
    m_examScoreLabel = new QLabel("考核评分模块：等待开始考核", examCard);
    m_examScoreLabel->setObjectName("scoreText");
    m_examFeedbackText = new QPlainTextEdit(examCard);
    m_examFeedbackText->setReadOnly(true);
    m_examFeedbackText->setFocusPolicy(Qt::NoFocus);
    m_examFeedbackText->setTextInteractionFlags(Qt::NoTextInteraction);
    m_examFeedbackText->setCursor(Qt::ArrowCursor);
    m_examFeedbackText->viewport()->setCursor(Qt::ArrowCursor);
    m_examFeedbackText->setMinimumHeight(130);
    m_examFeedbackText->setPlaceholderText("考核抽题、AI评分和纠错反馈会显示在这里。");
    connect(examStartCameraButton, &QPushButton::clicked, this, &MainWindow::startCamera);
    connect(examStopCameraButton, &QPushButton::clicked, this, &MainWindow::stopCamera);
    connect(examReferenceButton, &QPushButton::clicked, this, &MainWindow::playExamReferenceVideo);
    connect(examStartButton, &QPushButton::clicked, this, &MainWindow::runAssessment);
    examCardLayout->addWidget(examTitle);
    examCardLayout->addWidget(examText);
    examCardLayout->addWidget(m_examRuleLabel);
    examCardLayout->addWidget(m_examCameraFrame);
    examCardLayout->addWidget(m_examCameraStatusLabel);
    examCardLayout->addWidget(examReferenceTitle);
    examCardLayout->addWidget(m_examReferenceVideoFrame, 0, Qt::AlignHCenter);
    examCardLayout->addWidget(m_examReferenceVideoStatusLabel);
    examCardLayout->addWidget(examActionPanel);
    examCardLayout->addWidget(m_examScoreLabel);
    examCardLayout->addWidget(m_examFeedbackText);
    auto* wrongCard = new QFrame(examPage);
    wrongCard->setObjectName("card");
    examCard->setMinimumWidth(0);
    wrongCard->setMinimumWidth(0);
    auto* wrongLayout = new QVBoxLayout(wrongCard);
    wrongLayout->setContentsMargins(18, 16, 18, 16);
    auto* wrongTitle = new QLabel("错题复习", wrongCard);
    wrongTitle->setObjectName("cardTitle");
    m_wrongAnswerList = new QListWidget(wrongCard);
    m_wrongAnswerList->addItems({"低分手势会自动记录", "可按错误次数排序复习", "后续可扩展错题详情页"});
    wrongLayout->addWidget(wrongTitle);
    wrongLayout->addWidget(m_wrongAnswerList);
    examLayout->addWidget(examCard, 2);
    examLayout->addWidget(wrongCard, 1);
    m_moduleStack->addWidget(examPage);

    auto* settingsPage = new QWidget(m_moduleStack);
    auto* settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    auto* settingsCard = new QFrame(settingsPage);
    settingsCard->setObjectName("card");
    auto* settingsCardLayout = new QVBoxLayout(settingsCard);
    settingsCardLayout->setContentsMargins(18, 16, 18, 16);
    auto* settingsTitle = new QLabel("系统设置", settingsCard);
    settingsTitle->setObjectName("cardTitle");
    auto* settingsText = new QLabel("这里用于集中配置 MySQL、摄像头、教学视频目录和后续 AI 模型路径。登录不依赖数据库；连接 MySQL 后，练习记录和错题本会保存到数据库。", settingsCard);
    settingsText->setWordWrap(true);
    settingsText->setObjectName("hintText");
    settingsCardLayout->addWidget(settingsTitle);
    settingsCardLayout->addWidget(settingsText);
    settingsCardLayout->addWidget(dbBox);
    m_systemStatusDetailLabel = new QLabel("数据库：未连接；摄像头：未启动；AI 模型：MediaPipe HandLandmarker。", settingsCard);
    m_systemStatusDetailLabel->setWordWrap(true);
    m_systemStatusDetailLabel->setObjectName("scoreText");
    settingsCardLayout->addWidget(m_systemStatusDetailLabel);
    settingsLayout->addWidget(settingsCard);
    settingsLayout->addStretch();
    m_moduleStack->addWidget(settingsPage);

    auto* accountPage = new QWidget(m_moduleStack);
    auto* accountLayout = new QVBoxLayout(accountPage);
    accountLayout->setContentsMargins(0, 0, 0, 0);
    accountLayout->setSpacing(14);
    auto* accountCard = new QFrame(accountPage);
    accountCard->setObjectName("card");
    auto* accountCardLayout = new QVBoxLayout(accountCard);
    accountCardLayout->setContentsMargins(18, 16, 18, 16);
    accountCardLayout->setSpacing(12);
    auto* accountTitle = new QLabel("管理员端 - 账号管理", accountCard);
    accountTitle->setObjectName("cardTitle");
    auto* accountHint = new QLabel("可直接在主功能区查看数据库用户、删除普通账号、重置密码、调整用户角色。", accountCard);
    accountHint->setObjectName("hintText");
    accountHint->setWordWrap(true);
    m_accountTable = new QTableWidget(accountCard);
    m_accountTable->setColumnCount(3);
    m_accountTable->setHorizontalHeaderLabels({"ID", "用户名", "角色"});
    m_accountTable->horizontalHeader()->setStretchLastSection(true);
    m_accountTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_accountTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_accountTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_accountTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_accountTable->setMinimumHeight(260);
    auto* accountActionRow = new QHBoxLayout();
    accountActionRow->setSpacing(10);
    auto* accountRefreshButton = new QPushButton("刷新列表", accountCard);
    auto* accountResetPasswordButton = new QPushButton("重置密码", accountCard);
    auto* accountSetUserButton = new QPushButton("设为普通用户", accountCard);
    auto* accountSetAdminButton = new QPushButton("设为管理员", accountCard);
    auto* accountDeleteButton = new QPushButton("删除账号", accountCard);
    accountDeleteButton->setObjectName("warningButton");
    accountActionRow->addWidget(accountRefreshButton);
    accountActionRow->addWidget(accountResetPasswordButton);
    accountActionRow->addWidget(accountSetUserButton);
    accountActionRow->addWidget(accountSetAdminButton);
    accountActionRow->addWidget(accountDeleteButton);
    accountCardLayout->addWidget(accountTitle);
    accountCardLayout->addWidget(accountHint);
    accountCardLayout->addWidget(m_accountTable, 1);
    accountCardLayout->addLayout(accountActionRow);
    accountLayout->addWidget(accountCard);
    m_accountManagementIndex = m_moduleStack->addWidget(accountPage);

    connect(accountRefreshButton, &QPushButton::clicked, this, &MainWindow::reloadAccountUsers);
    connect(accountResetPasswordButton, &QPushButton::clicked, this, [this]() {
        const int userId = selectedAccountUserId();
        if (userId <= 0) {
            showMessage("请先选择一个账号。");
            return;
        }
        bool ok = false;
        const QString password = QInputDialog::getText(this, "重置密码", "请输入新密码：",
                                                       QLineEdit::Password, QString(), &ok);
        if (!ok) {
            return;
        }
        QString error;
        if (!m_userManager.resetUserPassword(userId, password, &error)) {
            QMessageBox::warning(this, "账号管理", "重置失败：" + error);
            return;
        }
        reloadAccountUsers();
        QMessageBox::information(this, "账号管理", "密码已重置。");
    });
    connect(accountSetUserButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (!m_userManager.updateUserRole(selectedAccountUserId(), "user", &error)) {
            QMessageBox::warning(this, "账号管理", "修改角色失败：" + error);
            return;
        }
        reloadAccountUsers();
    });
    connect(accountSetAdminButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (!m_userManager.updateUserRole(selectedAccountUserId(), "admin", &error)) {
            QMessageBox::warning(this, "账号管理", "修改角色失败：" + error);
            return;
        }
        reloadAccountUsers();
    });
    connect(accountDeleteButton, &QPushButton::clicked, this, [this]() {
        if (!m_accountTable) {
            return;
        }
        const int row = m_accountTable->currentRow();
        const QString username = row >= 0 && m_accountTable->item(row, 1)
            ? m_accountTable->item(row, 1)->text()
            : "该用户";
        if (QMessageBox::question(this, "删除账号", "确定删除账号「" + username + "」吗？")
            != QMessageBox::Yes) {
            return;
        }
        QString error;
        if (!m_userManager.deleteUser(selectedAccountUserId(), &error)) {
            QMessageBox::warning(this, "账号管理", "删除失败：" + error);
            return;
        }
        reloadAccountUsers();
    });

    m_moduleStack->setCurrentIndex(1);
    mainLayout->addWidget(m_moduleStack, 1);

    m_mainScrollArea = new QScrollArea(central);
    m_mainScrollArea->setObjectName("mainScrollArea");
    m_mainScrollArea->setWidgetResizable(true);
    m_mainScrollArea->setFrameShape(QFrame::NoFrame);
    m_mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainScrollArea->setWidget(mainArea);
    bodyLayout->addWidget(m_mainScrollArea, 1);
    root->addWidget(body, 1);

    const auto keepInputCursor = [](QWidget* widget) {
        return qobject_cast<QLineEdit*>(widget) || qobject_cast<QSpinBox*>(widget);
    };
    const auto mainWidgets = central->findChildren<QWidget*>();
    central->setCursor(Qt::ArrowCursor);
    for (QWidget* widget : mainWidgets) {
        if (!widget || keepInputCursor(widget)) {
            continue;
        }
        widget->setCursor(Qt::ArrowCursor);
        if (auto* button = qobject_cast<QPushButton*>(widget)) {
            button->setCursor(Qt::PointingHandCursor);
        }
        if (auto* scrollWidget = qobject_cast<QAbstractScrollArea*>(widget)) {
            scrollWidget->viewport()->setCursor(Qt::ArrowCursor);
        }
    }

    setCentralWidget(central);
    setWindowTitle("基于 Qt 与计算机视觉的手语动作学习系统");
    setMinimumSize(1360, 820);
    resize(1480, 900);
    centerWidgetOnScreen(this);

    m_detectionWatcher = new QFutureWatcher<HandDetectionResult>(this);
    connect(m_detectionWatcher, &QFutureWatcher<HandDetectionResult>::finished, this, [this]() {
        m_detectionRunning = false;
        finishPracticeScoring(m_detectionWatcher->result());
    });

    setStyleSheet(R"(
        QWidget#page {
            background: #f8f9fa;
            color: #191c1d;
            font-family: "Microsoft YaHei";
            font-size: 14px;
        }
        QWidget#body {
            background: transparent;
        }
        QFrame#navbar {
            background: #ffffff;
            border-bottom: 1px solid #e2e8f0;
        }
        QLabel#navbarLogo {
            color: #ffffff;
            background: #1a56db;
            border-radius: 8px;
            font-size: 20px;
        }
        QLabel#navbarTitle {
            color: #191c1d;
            font-size: 18px;
            font-weight: 900;
        }
        QFrame#sidebar {
            background: #ffffff;
            border-right: 1px solid #e2e8f0;
        }
        QPushButton#navItem, QPushButton#navActive {
            min-height: 44px;
            border-radius: 10px;
            padding-left: 16px;
            text-align: left;
            border: 0;
            font-size: 14px;
            font-weight: 700;
        }
        QPushButton#navItem {
            color: #434654;
            background: transparent;
        }
        QPushButton#navItem:hover {
            color: #1a56db;
            background: #f3f6ff;
        }
        QPushButton#navActive {
            color: #ffffff;
            background: #1a56db;
            font-weight: 900;
        }
        QWidget#mainArea {
            background: transparent;
        }
        QScrollArea#mainScrollArea {
            background: transparent;
            border: 0;
        }
        QScrollArea#mainScrollArea > QWidget > QWidget {
            background: transparent;
        }
        QScrollArea#mainScrollArea QScrollBar:vertical {
            background: #edf2f7;
            width: 10px;
            margin: 8px 0 8px 0;
            border-radius: 5px;
        }
        QScrollArea#mainScrollArea QScrollBar::handle:vertical {
            background: #a8b7d4;
            min-height: 44px;
            border-radius: 5px;
        }
        QScrollArea#mainScrollArea QScrollBar::handle:vertical:hover {
            background: #7f93bd;
        }
        QScrollArea#mainScrollArea QScrollBar::add-line:vertical,
        QScrollArea#mainScrollArea QScrollBar::sub-line:vertical {
            height: 0;
            border: 0;
            background: transparent;
        }
        QLabel#title {
            font-size: 24px;
            font-weight: 900;
            color: #191c1d;
        }
        QLabel#subtitle, QLabel#hintText {
            color: #434654;
        }
        QLabel#userBadge {
            color: #434654;
            background: #f3f4f5;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 700;
        }
        QPushButton#logoutButton {
            color: #e53e3e;
            background: #ffffff;
            border: 1px solid #feb2b2;
            border-radius: 8px;
            min-height: 34px;
            padding: 6px 18px;
            font-weight: 700;
        }
        QPushButton#logoutButton:hover {
            background: #fff5f5;
            border-color: #fc8181;
        }
        QFrame#card, QFrame#metricCard {
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
        }
        QStackedWidget#moduleStack {
            background: transparent;
            border: 0;
        }
        QLabel#cardTitle {
            color: #191c1d;
            font-size: 16px;
            font-weight: 800;
        }
        QLabel#sectionTitle {
            color: #191c1d;
            font-size: 14px;
            font-weight: 800;
            margin-top: 8px;
        }
        QLabel#metricValue {
            color: #1a56db;
            font-size: 28px;
            font-weight: 900;
        }
        QLineEdit, QSpinBox, QComboBox, QListWidget, QPlainTextEdit {
            background: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 8px;
            padding: 10px 12px;
            selection-background-color: #1a56db;
        }
        QComboBox#gestureCombo {
            color: #24324a;
            background: #fbfdff;
            border: 1px solid #c3c5d7;
            border-radius: 8px;
            padding: 10px 16px;
            font-size: 15px;
            font-weight: 700;
        }
        QComboBox#gestureCombo:hover {
            border-color: #1a56db;
            background: #f8fbff;
        }
        QComboBox#gestureCombo::drop-down {
            width: 34px;
            border: 0;
        }
        QComboBox#gestureCombo::down-arrow {
            image: none;
            width: 0;
            height: 0;
        }
        QComboBox#gestureCombo QAbstractItemView {
            background: #ffffff;
            border: 1px solid #c3c5d7;
            border-radius: 8px;
            padding: 6px;
            selection-background-color: #dbe7ff;
            selection-color: #003fb1;
            outline: 0;
        }
        QListWidget {
            alternate-background-color: #f7fafc;
        }
        QPlainTextEdit {
            background: #ffffff;
        }
        QPushButton {
            min-height: 38px;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            background: #ffffff;
            color: #4a5568;
            padding: 8px 16px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #f7fafc;
            border-color: #cbd5e0;
        }
        QPushButton#primaryButton {
            background: #1a56db;
            color: white;
            border: 0;
            border-radius: 8px;
        }
        QPushButton#primaryButton:hover {
            background: #003fb1;
            border-radius: 8px;
        }
        QPushButton#successButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #48bb78, stop:1 #38a169);
            color: white;
            border: 0;
            border-radius: 8px;
        }
        QPushButton#infoButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #4299e1, stop:1 #3182ce);
            color: white;
            border: 0;
            border-radius: 8px;
        }
        QPushButton#warningButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #ed8936, stop:1 #dd6b20);
            color: white;
            border: 0;
            border-radius: 8px;
        }
        QStackedWidget#cameraStack {
            background: transparent;
            border: 0;
        }
        QFrame#cameraFrame {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1a202c, stop:1 #2d3748);
            border: 2px solid #4a5568;
            border-radius: 12px;
        }
        QLabel#cameraPlaceholder {
            background: transparent;
            color: #a0aec0;
            border-radius: 12px;
            font-size: 16px;
            font-weight: 700;
        }
        QVideoWidget#videoPanel {
            background: #111111;
        }
        QFrame#teachingVideoFrame {
            background: #05070b;
            border: 2px solid #2d3748;
            border-radius: 12px;
        }
        QVideoWidget#teachingVideo {
            background: #05070b;
            border: 0;
        }
        QStackedWidget#teachingVideoStack {
            background: #05070b;
            border: 0;
        }
        QLabel#teachingVideoPlaceholder {
            color: #cbd5e0;
            background: #05070b;
            font-size: 18px;
            font-weight: 900;
        }
        QLabel#scoreText {
            color: #276749;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #f0fff4, stop:1 #c6f6d5);
            border-left: 4px solid #48bb78;
            border-radius: 10px;
            padding: 12px;
            font-weight: 700;
        }
        QLabel#statusBar {
            color: #48bb78;
            background: #ffffff;
            border: 1px solid #e2e8f0;
            border-radius: 12px;
            padding: 10px 12px;
            font-weight: 700;
        }
        QProgressBar#connectingProgress {
            min-height: 12px;
            max-height: 12px;
            border: 0;
            border-radius: 6px;
            background: #edf2f7;
        }
        QProgressBar#connectingProgress::chunk {
            border-radius: 6px;
            background: #1a56db;
        }
        QLabel#connectingLabel {
            color: #1a56db;
            font-weight: 800;
            padding-left: 4px;
        }
    )");

    connect(m_connectDbButton, &QPushButton::clicked, this, &MainWindow::connectDatabase);
    connect(m_hostEdit, &QLineEdit::returnPressed, m_connectDbButton, &QPushButton::click);
    connect(m_databaseEdit, &QLineEdit::returnPressed, m_connectDbButton, &QPushButton::click);
    connect(m_dbUserEdit, &QLineEdit::returnPressed, m_connectDbButton, &QPushButton::click);
    connect(m_dbPasswordEdit, &QLineEdit::returnPressed, m_connectDbButton, &QPushButton::click);
    if (auto* portEdit = m_portSpin->findChild<QLineEdit*>()) {
        connect(portEdit, &QLineEdit::returnPressed, m_connectDbButton, &QPushButton::click);
    }
    connect(registerButton, &QPushButton::clicked, this, &MainWindow::registerUser);
    connect(loginButton, &QPushButton::clicked, this, &MainWindow::loginUser);
    connect(m_usernameEdit, &QLineEdit::returnPressed, loginButton, &QPushButton::click);
    connect(m_passwordEdit, &QLineEdit::returnPressed, loginButton, &QPushButton::click);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logoutUser);
    connect(reloadButton, &QPushButton::clicked, this, &MainWindow::reloadGestures);
    connect(m_manageAccountsButton, &QPushButton::clicked, this, &MainWindow::showAccountManagementDialog);
    connect(m_gestureCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::updateTeachingVideoStatus);
    connect(cameraButton, &QPushButton::clicked, this, &MainWindow::startCamera);
    connect(stopCameraButton, &QPushButton::clicked, this, &MainWindow::stopCamera);
    connect(m_importVideoButton, &QPushButton::clicked, this, &MainWindow::importOrUpdateGestureVideo);
    connect(playVideoButton, &QPushButton::clicked, this, &MainWindow::playTeachingVideo);
    connect(stopVideoButton, &QPushButton::clicked, this, &MainWindow::stopTeachingVideo);
    connect(practiceButton, &QPushButton::clicked, this, &MainWindow::practiceSelectedGesture);

    showModule(1, "动作练习");
    updateAdminControls();
    updateResponsiveLayout();
    QTimer::singleShot(0, this, &MainWindow::updateResponsiveLayout);
    QTimer::singleShot(0, this, &MainWindow::updateMainScrollBarPolicy);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateResponsiveLayout();
    updateMainScrollBarPolicy();
}

void MainWindow::updateResponsiveLayout()
{
    if (!m_cameraFrame || !m_teachingVideoFrame) {
        return;
    }

    const int moduleHeight = m_moduleStack ? qMax(1, m_moduleStack->height()) : qMax(1, height());
    const int moduleWidth = m_moduleStack ? qMax(1, m_moduleStack->width()) : qMax(1, width());
    const bool compactSize = moduleWidth < 1280;
    const int workSpacing = compactSize ? 12 : 14;
    const int equalColumnWidth = qMax(260, (moduleWidth - workSpacing) / 2);

    if (m_workLayout) {
        m_workLayout->setDirection(QBoxLayout::LeftToRight);
        m_workLayout->setSpacing(workSpacing);
        m_workLayout->setStretch(0, 1);
        m_workLayout->setStretch(1, 1);
    }
    if (m_practiceBox) {
        m_practiceBox->setMinimumWidth(0);
        m_practiceBox->setMaximumWidth(QWIDGETSIZE_MAX);
        m_practiceBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    if (m_libraryBox) {
        m_libraryBox->setMinimumWidth(0);
        m_libraryBox->setMaximumWidth(QWIDGETSIZE_MAX);
        m_libraryBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }

    int sharedFrameHeight = compactSize ? 300 : 360;
    const int practiceLiveWidth = m_cameraFrame->parentWidget() && m_cameraFrame->parentWidget()->isVisible()
        ? m_cameraFrame->parentWidget()->width() - 32
        : 0;
    const int libraryLiveWidth = m_teachingVideoFrame->parentWidget() && m_teachingVideoFrame->parentWidget()->isVisible()
        ? m_teachingVideoFrame->parentWidget()->width() - 32
        : 0;
    const int sharedAvailableWidth = qMax(260, qMin(equalColumnWidth - 32,
                                                   qMax(260, qMin(qMax(practiceLiveWidth, 260),
                                                                  qMax(libraryLiveWidth, 260)))));
    const int widthBasedSharedHeight = sharedAvailableWidth * 9 / 16;
    const int heightBasedSharedLimit = moduleHeight * (compactSize ? 50 : 56) / 100;
    sharedFrameHeight = qBound(compactSize ? 300 : 360,
                               qMin(widthBasedSharedHeight, heightBasedSharedLimit),
                               compactSize ? 440 : 560);

    if (m_cameraFrame->parentWidget()) {
        m_cameraFrame->setMinimumWidth(0);
        m_cameraFrame->setMaximumWidth(QWIDGETSIZE_MAX);
        m_cameraFrame->setMinimumHeight(sharedFrameHeight);
        m_cameraFrame->setMaximumHeight(sharedFrameHeight);
        if (m_cameraStack) {
            m_cameraStack->setMinimumWidth(0);
            m_cameraStack->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        if (m_videoWidget) {
            m_videoWidget->setMinimumSize(0, 0);
            m_videoWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
        if (m_practiceActionPanel) {
            m_practiceActionPanel->setMinimumWidth(0);
            m_practiceActionPanel->setMaximumWidth(QWIDGETSIZE_MAX);
        }
    }

    if (m_teachingVideoFrame->parentWidget()) {
        m_teachingVideoFrame->setMinimumWidth(0);
        m_teachingVideoFrame->setMaximumWidth(QWIDGETSIZE_MAX);
        m_teachingVideoFrame->setMinimumHeight(sharedFrameHeight);
        m_teachingVideoFrame->setMaximumHeight(sharedFrameHeight);
        if (m_teachingVideoWidget) {
            m_teachingVideoWidget->setMinimumSize(0, 0);
            m_teachingVideoWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
        if (m_teachingActionPanel) {
            m_teachingActionPanel->setMinimumWidth(0);
            m_teachingActionPanel->setMaximumWidth(QWIDGETSIZE_MAX);
        }
    }
    if (m_examCameraFrame) {
        const int examAvailableWidth = qMax(280, (moduleWidth - 14) * 2 / 3 - 36);
        const int liveWidth = m_examCameraFrame->parentWidget() && m_examCameraFrame->parentWidget()->isVisible()
            ? m_examCameraFrame->parentWidget()->width() - 36
            : 0;
        const int examWidth = qMax(280, qMin(examAvailableWidth, qMax(liveWidth, 280)));
        const int examHeight = qBound(220, examWidth * 9 / 28, 360);
        const int examReferenceWidth = qBound(360, examWidth * 68 / 100, compactSize ? qMax(360, examWidth - 28) : qMax(420, examWidth - 48));
        const int examReferenceHeight = qBound(145, examReferenceWidth * 9 / 16, compactSize ? 220 : 245);
        m_examCameraFrame->setMinimumWidth(0);
        m_examCameraFrame->setMaximumWidth(QWIDGETSIZE_MAX);
        m_examCameraFrame->setMinimumHeight(examHeight);
        m_examCameraFrame->setMaximumHeight(examHeight);
        if (m_examCameraStack) {
            m_examCameraStack->setMinimumWidth(0);
            m_examCameraStack->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        if (m_examVideoWidget) {
            m_examVideoWidget->setMinimumSize(0, 0);
            m_examVideoWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
        if (m_examReferenceVideoFrame) {
            m_examReferenceVideoFrame->setMinimumSize(examReferenceWidth, examReferenceHeight);
            m_examReferenceVideoFrame->setMaximumSize(examReferenceWidth, examReferenceHeight);
        }
        if (m_examReferenceVideoWidget) {
            m_examReferenceVideoWidget->setMinimumSize(0, 0);
            m_examReferenceVideoWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
    }
    updateMainScrollBarPolicy();
}

void MainWindow::updateMainScrollBarPolicy()
{
    if (!m_mainScrollArea || !m_mainScrollArea->widget()) {
        return;
    }
    m_mainScrollArea->widget()->adjustSize();
    const int contentHeight = m_mainScrollArea->widget()->sizeHint().height();
    const int viewportHeight = m_mainScrollArea->viewport() ? m_mainScrollArea->viewport()->height() : m_mainScrollArea->height();
    const bool needsScroll = contentHeight > viewportHeight + 8;
    m_mainScrollArea->setVerticalScrollBarPolicy(needsScroll ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

void MainWindow::showModule(int index, const QString& moduleName)
{
    if (!m_moduleStack || index < 0 || index >= m_moduleStack->count()) {
        return;
    }

    m_moduleStack->setCurrentIndex(index);
    QTimer::singleShot(0, this, &MainWindow::updateMainScrollBarPolicy);
    static const QStringList descriptions = {
        "查看今日推荐、最近练习和待复习手势，快速了解当前学习状态。",
        "选择手势、观看标准教学视频，并通过摄像头完成动作评分与纠错反馈。",
        "浏览手势难度说明和教学资源，为练习模块提供标准动作素材。",
        "统计练习次数、平均得分、最近练习时间和错题情况，辅助分析学习效果。",
        "随机抽取手势进行阶段测试，低分动作会自动进入错题复习列表。",
        "配置 MySQL 数据库连接、摄像头、教学视频路径和后续 AI 模型参数。",
        "管理员可在这里查看、删除、重置和调整账号权限。"
    };
    if (m_moduleTitleLabel) {
        QString cleanModuleName = moduleName;
        cleanModuleName.remove(QRegularExpression("^[^\\p{Han}]+"));
        m_moduleTitleLabel->setText(cleanModuleName.trimmed());
    }
    if (m_moduleDescriptionLabel && index < descriptions.size()) {
        m_moduleDescriptionLabel->setText(descriptions[index]);
    }
    refreshDashboard();
    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons[i]->setObjectName(i == index ? "navActive" : "navItem");
        m_navButtons[i]->style()->unpolish(m_navButtons[i]);
        m_navButtons[i]->style()->polish(m_navButtons[i]);
    }
    if (m_manageAccountsButton) {
        const bool active = index == m_accountManagementIndex;
        m_manageAccountsButton->setObjectName(active ? "navActive" : "navItem");
        m_manageAccountsButton->style()->unpolish(m_manageAccountsButton);
        m_manageAccountsButton->style()->polish(m_manageAccountsButton);
    }

    showMessage("已切换到「" + moduleName + "」模块。");
}

void MainWindow::connectDatabase()
{
    if (m_connectDbButton) {
        m_connectDbButton->setEnabled(false);
        m_connectDbButton->setText("请稍等…");
    }
    if (m_dbConnectingProgress) {
        m_dbConnectingProgress->show();
    }
    if (m_dbConnectingLabel) {
        m_dbConnectingLabel->setText("正在连接 MySQL，请稍等…");
        m_dbConnectingLabel->show();
    }
    if (m_statusLabel) {
        m_statusLabel->setText("系统状态：正在连接 MySQL，请稍等…");
    }
    QApplication::processEvents();

    auto finishConnecting = [this](const QString& buttonText = QStringLiteral("连接 MySQL")) {
        if (m_connectDbButton) {
            m_connectDbButton->setEnabled(true);
            m_connectDbButton->setText(buttonText);
        }
        if (m_dbConnectingProgress) {
            m_dbConnectingProgress->hide();
        }
        if (m_dbConnectingLabel) {
            m_dbConnectingLabel->hide();
        }
    };

    QString error;
    const bool ok = m_databaseManager.connectToMySql(
        m_hostEdit->text(),
        m_portSpin->value(),
        m_databaseEdit->text(),
        m_dbUserEdit->text(),
        m_dbPasswordEdit->text(),
        &error);

    if (!ok) {
        finishConnecting();
        showMessage("MySQL 连接失败：" + error);
        return;
    }

    m_userManager.setDatabase(m_databaseManager.database());
    m_gestureLibrary.setDatabase(m_databaseManager.database());
    m_statistics.setDatabase(m_databaseManager.database());

    QString localSyncError;
    if (!m_userManager.syncLocalUsersToDatabase(&localSyncError)) {
        finishConnecting();
        showMessage("MySQL 已连接，但本地账号同步失败：" + localSyncError);
        return;
    }

    if (m_userManager.currentUserId() > 0) {
        QString syncError;
        if (!m_userManager.syncCurrentUserToDatabase(&syncError)) {
            finishConnecting();
            showMessage("MySQL 已连接，但当前用户同步失败：" + syncError);
            return;
        }
        updateAdminControls();
    }

    finishConnecting("已连接");
    showMessage("MySQL 连接成功。");
    reloadGestures();
    refreshLearningPanels();
    refreshDashboard();
}

void MainWindow::registerUser()
{
    QString error;
    if (m_userManager.registerUser(m_usernameEdit->text(), m_passwordEdit->text(), &error)) {
        showMessage("注册成功，可以登录。");
    } else {
        showMessage("注册失败：" + error);
    }
}

void MainWindow::loginUser()
{
    QString error;
    if (m_userManager.login(m_usernameEdit->text(), m_passwordEdit->text(), &error)) {
        m_currentUserLabel->setText("当前用户：" + m_userManager.currentUsername());
        updateAdminControls();
        showMessage("登录成功。");
        m_feedbackText->setPlainText(m_statistics.summaryForUser(m_userManager.currentUserId()));
        refreshLearningPanels();
    } else {
        showMessage("登录失败：" + error);
    }
}

void MainWindow::logoutUser()
{
    stopCamera();
    stopTeachingVideo();
    m_userManager.logout();
    m_currentUserLabel->setText("当前用户：未登录");
    updateAdminControls();
    refreshDashboard();
    showMessage("已退出登录。");
    showLoginDialog();
}

void MainWindow::showLoginDialog()
{
    if (m_userManager.currentUserId() > 0) {
        return;
    }

    const bool previousQuitOnLastWindowClosed = QApplication::quitOnLastWindowClosed();
    QApplication::setQuitOnLastWindowClosed(false);
    hide();

    QDialog dialog(this);
    dialog.setWindowTitle("登录 - 手语动作学习系统");
    dialog.setModal(true);
    dialog.setFixedSize(500, 690);
    dialog.setObjectName("loginDialog");
    centerWidgetOnScreen(&dialog, this);

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(44, 30, 44, 24);
    root->setSpacing(10);

    auto* logoBadge = new QFrame(&dialog);
    logoBadge->setObjectName("loginLogoBadge");
    logoBadge->setFixedSize(94, 94);
    auto* logoLayout = new QVBoxLayout(logoBadge);
    logoLayout->setContentsMargins(0, 0, 0, 0);
    auto* logoIcon = new QLabel(logoBadge);
    logoIcon->setObjectName("loginLogoIcon");
    logoIcon->setPixmap(QPixmap::fromImage(createAppLogoHandGlyphImage(56)));
    logoIcon->setAlignment(Qt::AlignCenter);
    logoLayout->addWidget(logoIcon);
    root->addWidget(logoBadge, 0, Qt::AlignHCenter);

    auto* title = new QLabel("手语动作学习", &dialog);
    title->setObjectName("loginTitle");
    title->setAlignment(Qt::AlignCenter);
    auto* subtitle = new QLabel("SIGN LANGUAGE LEARNING", &dialog);
    subtitle->setObjectName("loginSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    root->addWidget(title);
    root->addWidget(subtitle, 0, Qt::AlignHCenter);
    root->addSpacing(10);

    auto* authStack = new QStackedWidget(&dialog);
    authStack->setObjectName("authStack");
    root->addWidget(authStack, 1);

    auto addAuthField = [](QVBoxLayout* layout, QWidget* parent, const QString& iconText,
                           const QString& placeholder, bool password) {
        auto* shell = new QFrame(parent);
        shell->setObjectName("loginInputShell");
        shell->setMinimumHeight(54);
        auto* shellLayout = new QHBoxLayout(shell);
        shellLayout->setContentsMargins(18, 0, 18, 0);
        shellLayout->setSpacing(12);

        auto* icon = new QLabel(iconText, shell);
        icon->setObjectName("loginFieldIcon");
        icon->setAlignment(Qt::AlignCenter);
        icon->setFixedWidth(24);
        shellLayout->addWidget(icon);

        auto* edit = new QLineEdit(shell);
        edit->setObjectName("loginInput");
        edit->setPlaceholderText(placeholder);
        edit->setMinimumHeight(40);
        if (password) {
            edit->setEchoMode(QLineEdit::Password);
            preferEnglishPasswordInput(edit);
        } else {
            preferEnglishInput(edit);
        }
        shellLayout->addWidget(edit, 1);
        layout->addWidget(shell);
        return edit;
    };

    auto* loginPage = new QWidget(authStack);
    auto* loginLayout = new QVBoxLayout(loginPage);
    loginLayout->setContentsMargins(0, 0, 0, 0);
    loginLayout->setSpacing(12);
    auto* loginHeading = new QLabel("欢迎回来", loginPage);
    loginHeading->setObjectName("authHeading");
    loginHeading->setAlignment(Qt::AlignCenter);
    auto* loginHint = new QLabel("登录以继续你的手语学习之旅", loginPage);
    loginHint->setObjectName("authHint");
    loginHint->setAlignment(Qt::AlignCenter);
    loginLayout->addWidget(loginHeading);
    loginLayout->addWidget(loginHint);
    loginLayout->addSpacing(14);
    auto* loginUsernameEdit = addAuthField(loginLayout, loginPage, "@", "用户名", false);
    loginLayout->addSpacing(6);
    auto* loginPasswordEdit = addAuthField(loginLayout, loginPage, "*", "密码", true);
    loginLayout->addSpacing(14);
    auto* loginButton = new QPushButton("登录", loginPage);
    loginButton->setObjectName("primaryButton");
    loginButton->setMinimumHeight(54);
    loginButton->setAutoDefault(false);
    loginButton->setDefault(false);
    loginLayout->addWidget(loginButton);
    auto* toRegisterRow = new QHBoxLayout();
    toRegisterRow->setSpacing(8);
    toRegisterRow->addStretch();
    auto* registerPrompt = new QLabel("还没有账号？", loginPage);
    registerPrompt->setObjectName("authFootnote");
    auto* showRegisterButton = new QPushButton("立即注册 >", loginPage);
    showRegisterButton->setObjectName("linkButton");
    showRegisterButton->setFlat(true);
    showRegisterButton->setAutoDefault(false);
    showRegisterButton->setDefault(false);
    showRegisterButton->setCursor(Qt::PointingHandCursor);
    toRegisterRow->addWidget(registerPrompt);
    toRegisterRow->addWidget(showRegisterButton);
    toRegisterRow->addStretch();
    loginLayout->addLayout(toRegisterRow);
    loginLayout->addStretch();

    auto* registerPage = new QWidget(authStack);
    auto* registerLayout = new QVBoxLayout(registerPage);
    registerLayout->setContentsMargins(0, 0, 0, 0);
    registerLayout->setSpacing(12);
    auto* registerHeading = new QLabel("创建账号", registerPage);
    registerHeading->setObjectName("authHeading");
    registerHeading->setAlignment(Qt::AlignCenter);
    auto* registerHint = new QLabel("注册后即可开始保存你的学习进度", registerPage);
    registerHint->setObjectName("authHint");
    registerHint->setAlignment(Qt::AlignCenter);
    registerLayout->addWidget(registerHeading);
    registerLayout->addWidget(registerHint);
    registerLayout->addSpacing(14);
    auto* registerUsernameEdit = addAuthField(registerLayout, registerPage, "@", "用户名", false);
    registerLayout->addSpacing(6);
    auto* registerPasswordEdit = addAuthField(registerLayout, registerPage, "*", "密码", true);
    registerLayout->addSpacing(6);
    auto* registerConfirmEdit = addAuthField(registerLayout, registerPage, "*", "确认密码", true);
    registerLayout->addSpacing(14);
    auto* registerSubmitButton = new QPushButton("注册", registerPage);
    registerSubmitButton->setObjectName("primaryButton");
    registerSubmitButton->setMinimumHeight(54);
    registerSubmitButton->setAutoDefault(false);
    registerSubmitButton->setDefault(false);
    registerLayout->addWidget(registerSubmitButton);
    auto* backToLoginButton = new QPushButton("< 返回登录", registerPage);
    backToLoginButton->setObjectName("loginSecondaryButton");
    backToLoginButton->setMinimumHeight(44);
    backToLoginButton->setAutoDefault(false);
    backToLoginButton->setDefault(false);
    registerLayout->addWidget(backToLoginButton);
    registerLayout->addStretch();

    authStack->addWidget(loginPage);
    authStack->addWidget(registerPage);
    authStack->setCurrentWidget(loginPage);

    dialog.setStyleSheet(dialog.styleSheet() + R"(
        QDialog#loginDialog {
            background: #f8f9fa;
        }
        QFrame#loginLogoBadge {
            background: #1a56db;
            border: 0;
            border-radius: 28px;
        }
        QLabel#loginLogoIcon {
            color: white;
            font-size: 36px;
        }
        QLabel#loginTitle {
            color: #191c1d;
            font-size: 31px;
            font-weight: 900;
        }
        QLabel#loginSubtitle {
            color: #003fb1;
            background: #dbe7ff;
            border-radius: 13px;
            padding: 3px 26px;
            font-size: 15px;
            font-weight: 800;
            letter-spacing: 0px;
        }
        QLabel#authHeading {
            color: #191c1d;
            font-size: 23px;
            font-weight: 900;
        }
        QLabel#authHint {
            color: #434654;
            font-size: 14px;
            font-weight: 600;
        }
        QLabel#authFootnote {
            color: #434654;
            font-size: 14px;
            font-weight: 600;
        }
        QFrame#loginInputShell {
            background: #fbfdff;
            border: 2px solid #dfe6f1;
            border-radius: 20px;
            min-height: 54px;
        }
        QLabel#loginFieldIcon {
            color: #1a56db;
            font-size: 20px;
            font-weight: 900;
        }
        QLineEdit#loginInput {
            background: transparent;
            border: 0;
            color: #273244;
            font-size: 15px;
            padding: 0;
            font-weight: 800;
            selection-background-color: #1a56db;
        }
        QLineEdit#loginInput::placeholder {
            color: #96a4b6;
        }
        QPushButton#loginSecondaryButton {
            color: #1a56db;
            background: #ffffff;
            border: 2px solid #e1e7f0;
            border-radius: 18px;
            font-size: 15px;
            font-weight: 900;
        }
        QPushButton#loginSecondaryButton:hover {
            background: #f3f6ff;
            border-color: #b5c4ff;
        }
        QPushButton#primaryButton {
            color: #ffffff;
            background: #1a56db;
            border: 0;
            border-radius: 22px;
            font-size: 18px;
            font-weight: 900;
        }
        QPushButton#primaryButton:hover {
            background: #003fb1;
        }
        QPushButton#linkButton {
            color: #003fb1;
            background: #dbe7ff;
            border: 0;
            border-radius: 12px;
            padding: 4px 12px;
            font-size: 14px;
            font-weight: 900;
        }
        QPushButton#linkButton:hover {
            background: #c9d8ff;
        }
    )");

    connect(showRegisterButton, &QPushButton::clicked, &dialog, [authStack, registerPage, registerUsernameEdit]() {
        authStack->setCurrentWidget(registerPage);
        focusEnglishInput(registerUsernameEdit);
    });
    connect(backToLoginButton, &QPushButton::clicked, &dialog, [authStack, loginPage, loginUsernameEdit]() {
        authStack->setCurrentWidget(loginPage);
        focusEnglishInput(loginUsernameEdit);
    });
    connect(registerSubmitButton, &QPushButton::clicked, &dialog, [this, registerUsernameEdit, registerPasswordEdit, registerConfirmEdit,
                                                                   loginUsernameEdit, loginPasswordEdit, authStack, loginPage, &dialog]() {
        if (registerPasswordEdit->text() != registerConfirmEdit->text()) {
            showLoginNotice(&dialog, "注册失败", "两次输入的密码不一致。", "!");
            focusEnglishInput(registerConfirmEdit);
            registerConfirmEdit->selectAll();
            return;
        }
        QString error;
        if (m_userManager.registerUser(registerUsernameEdit->text(), registerPasswordEdit->text(), &error)) {
            showLoginNotice(&dialog, "注册成功", "注册成功，请点击返回登录。", "✓");
            loginUsernameEdit->setText(registerUsernameEdit->text());
            loginPasswordEdit->clear();
            registerPasswordEdit->clear();
            registerConfirmEdit->clear();
        } else {
            showLoginNotice(&dialog, "注册失败", error, "!");
        }
    });
    auto* loginSubmitGuard = new LoginSubmitGuard();
    auto unlockLoginSubmit = [loginSubmitGuard]() {
        loginSubmitGuard->unlock();
    };
    auto attemptLogin = [this, authStack, loginPage, loginUsernameEdit, loginPasswordEdit, loginSubmitGuard, unlockLoginSubmit, &dialog]() {
        if (!loginSubmitGuard->tryBegin()) {
            return;
        }
        QString error;
        if (m_userManager.login(loginUsernameEdit->text(), loginPasswordEdit->text(), &error)) {
            m_currentUserLabel->setText("当前用户：" + m_userManager.currentUsername());
            updateAdminControls();
            m_feedbackText->setPlainText(m_statistics.summaryForUser(m_userManager.currentUserId()));
            refreshLearningPanels();
            showMessage("登录成功。");
        } else {
            showLoginNotice(&dialog, "登录失败", error, "!");
            authStack->setCurrentWidget(loginPage);
            QTimer::singleShot(0, &dialog, [authStack, loginPage, loginPasswordEdit, unlockLoginSubmit]() {
                authStack->setCurrentWidget(loginPage);
                focusEnglishInput(loginPasswordEdit);
                loginPasswordEdit->selectAll();
                unlockLoginSubmit();
            });
            return;
        }

        if (m_userManager.currentUserId() > 0) {
            dialog.accept();
        } else {
            QTimer::singleShot(0, &dialog, unlockLoginSubmit);
        }
    };
    connect(&dialog, &QDialog::finished, &dialog, [loginSubmitGuard]() {
        delete loginSubmitGuard;
    });
    connect(loginButton, &QPushButton::clicked, &dialog, attemptLogin);
    connect(loginUsernameEdit, &QLineEdit::returnPressed, &dialog, attemptLogin);
    connect(loginPasswordEdit, &QLineEdit::returnPressed, &dialog, attemptLogin);
    connect(registerUsernameEdit, &QLineEdit::returnPressed, &dialog, [registerPasswordEdit]() {
        focusEnglishInput(registerPasswordEdit);
    });
    connect(registerPasswordEdit, &QLineEdit::returnPressed, &dialog, [registerConfirmEdit]() {
        focusEnglishInput(registerConfirmEdit);
    });
    connect(registerConfirmEdit, &QLineEdit::returnPressed, &dialog, [authStack, registerPage, registerSubmitButton]() {
        if (authStack->currentWidget() == registerPage) {
            registerSubmitButton->click();
        }
    });
    QTimer::singleShot(0, &dialog, [authStack, loginPage, loginUsernameEdit]() {
        authStack->setCurrentWidget(loginPage);
        focusEnglishInput(loginUsernameEdit);
    });

    if (dialog.exec() == QDialog::Accepted && m_userManager.currentUserId() > 0) {
        showNormal();
        centerWidgetOnScreen(this);
        raise();
        activateWindow();
        QTimer::singleShot(0, this, &MainWindow::updateResponsiveLayout);
        QApplication::setQuitOnLastWindowClosed(previousQuitOnLastWindowClosed);
    } else {
        QApplication::setQuitOnLastWindowClosed(previousQuitOnLastWindowClosed);
        close();
    }
}

void MainWindow::reloadGestures()
{
    m_gestures = m_gestureLibrary.loadGestures();
    applySharedTeachingVideos();
    refreshGestureList();
    updateTeachingVideoStatus();
}

void MainWindow::startCamera()
{
    QString status;
    const bool examMode = m_moduleStack && m_moduleStack->currentIndex() == 4 && m_examVideoWidget;
    QVideoWidget* targetVideo = examMode ? m_examVideoWidget : m_videoWidget;
    const bool started = m_cameraCapture.start(targetVideo, &status);
    if (examMode && m_examCameraStack) {
        m_examCameraStack->setCurrentWidget(started ? static_cast<QWidget*>(m_examVideoWidget)
                                                    : static_cast<QWidget*>(m_examCameraPlaceholder));
    } else {
        m_cameraStack->setCurrentWidget(started ? static_cast<QWidget*>(m_videoWidget)
                                                : static_cast<QWidget*>(m_cameraPlaceholder));
    }
    m_cameraStatusLabel->setText(status);
    if (m_examCameraStatusLabel) {
        m_examCameraStatusLabel->setText(status);
    }
    refreshDashboard();
    showMessage(status);
}

void MainWindow::stopCamera()
{
    m_cameraCapture.stop();
    m_cameraStack->setCurrentWidget(m_cameraPlaceholder);
    if (m_examCameraStack) {
        m_examCameraStack->setCurrentWidget(m_examCameraPlaceholder);
    }
    m_cameraStatusLabel->setText("摄像头视频采集模块：已关闭");
    if (m_examCameraStatusLabel) {
        m_examCameraStatusLabel->setText("考核摄像头模块：已关闭");
    }
    updateResponsiveLayout();
    QTimer::singleShot(0, this, &MainWindow::updateResponsiveLayout);
    refreshDashboard();
    showMessage("摄像头已关闭。");
}

void MainWindow::importOrUpdateGestureVideo()
{
    if (!m_userManager.currentUserIsAdmin()) {
        showMessage("只有管理员可以导入或更新手势视频。");
        return;
    }
    if (!m_databaseManager.isOpen()) {
        QMessageBox::information(this, "导入/更新手势视频", "请先连接 MySQL 数据库，再导入或更新手势视频。");
        return;
    }

    const QStringList choices = {"批量新增手势", "更新当前手势"};
    bool ok = false;
    const QString mode = QInputDialog::getItem(
        this,
        "导入/更新手势视频",
        "请选择操作方式：",
        choices,
        0,
        false,
        &ok);
    if (!ok || mode.isEmpty()) {
        return;
    }
    const bool addNewGesture = mode == "批量新增手势";
    Gesture currentGesture = selectedGesture();
    if (!addNewGesture && currentGesture.id <= 0) {
        showMessage("请先在下拉栏选择要更新的手势。");
        return;
    }

    const QStringList filePaths = addNewGesture
        ? QFileDialog::getOpenFileNames(this, "选择一个或多个手势视频", QString(), "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv);;所有文件 (*.*)")
        : QStringList{QFileDialog::getOpenFileName(this, "选择用于更新当前手势的视频", QString(), "视频文件 (*.mp4 *.avi *.mov *.mkv *.wmv);;所有文件 (*.*)")};
    if (filePaths.isEmpty() || filePaths.first().isEmpty()) {
        return;
    }

    int successCount = 0;
    QStringList failures;
    int lastGestureId = 0;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (const QString& filePath : filePaths) {
        if (filePath.isEmpty()) {
            continue;
        }
        const QFileInfo fileInfo(filePath);
        Gesture gesture = addNewGesture ? Gesture{} : currentGesture;
        if (addNewGesture) {
            gesture.name = fileInfo.completeBaseName().trimmed();
            gesture.category = "";
            gesture.description = "由标准视频批量导入的手语动作。";
        }
        if (gesture.name.isEmpty()) {
            failures << fileInfo.fileName() + "：文件名无法作为手势名称";
            continue;
        }

        showMessage(QString("正在处理视频：%1").arg(fileInfo.fileName()));
        QApplication::processEvents();
        const HandDetectionResult detection = m_detector.detectReferenceFromVideo(filePath);
        if (!detection.success || detection.keypoints.size() != 21) {
            failures << fileInfo.fileName() + "：" + detection.message;
            continue;
        }
        gesture.referenceKeypoints = detection.keypoints;
        gesture.difficulty = addNewGesture ? inferGestureDifficulty(detection.keypoints) : currentGesture.difficulty;

        QString error;
        if (addNewGesture) {
            int newGestureId = 0;
            if (!m_gestureLibrary.addGesture(gesture, &newGestureId, &error)) {
                failures << fileInfo.fileName() + "：" + error;
                continue;
            }
            gesture.id = newGestureId;
        }

        QString storedPath;
        QString storeError;
        if (!storeSharedTeachingVideo(gesture, filePath, &storedPath, &storeError)) {
            failures << fileInfo.fileName() + "：" + storeError;
            continue;
        }
        if (!m_databaseManager.updateGestureVideoAndReference(gesture.id, storedPath, serializeKeypoints(gesture.referenceKeypoints), &error)) {
            failures << fileInfo.fileName() + "：" + error;
            continue;
        }
        ++successCount;
        lastGestureId = gesture.id;
        if (!addNewGesture) {
            break;
        }
    }
    QApplication::restoreOverrideCursor();

    reloadGestures();
    for (int index = 0; index < m_gestures.size(); ++index) {
        if (m_gestures[index].id == lastGestureId) {
            m_gestureCombo->setCurrentIndex(index);
            break;
        }
    }
    if (!failures.isEmpty()) {
        QMessageBox::warning(this, "导入/更新手势视频",
            QString("成功处理 %1 个视频。\n以下视频处理失败：\n%2")
                .arg(successCount)
                .arg(failures.join("\n")));
    }
    showMessage(addNewGesture
        ? QString("批量导入完成：成功加入 %1 个手势。").arg(successCount)
        : (successCount > 0 ? "已更新当前手势视频和评分模板。" : "更新失败，请检查视频内容。"));
}

void MainWindow::playTeachingVideo()
{
    const Gesture gesture = selectedGesture();
    if (gesture.id <= 0) {
        showMessage("请先选择一个手势。");
        return;
    }

    if (gesture.videoPath.isEmpty()) {
        showMessage(m_userManager.currentUserIsAdmin()
            ? "当前手势还没有绑定教学视频，请到“手势资源库”的视频素材库上传。"
            : "当前手势还没有公共教学视频，请联系管理员上传。");
        return;
    }

    ensureTeachingVideoWidget();
    m_teachingPlayer->setVideoOutput(m_teachingVideoWidget);
    if (m_teachingVideoStack && m_teachingVideoPlaceholder) {
        m_teachingVideoPlaceholder->setText("🎬\n视频加载中");
        m_teachingVideoStack->setCurrentWidget(m_teachingVideoPlaceholder);
    }
    if (m_currentTeachingVideoPath != gesture.videoPath) {
#if QT_VERSION_MAJOR >= 6
        m_teachingPlayer->setSource(QUrl::fromLocalFile(gesture.videoPath));
#else
        m_teachingPlayer->setMedia(QUrl::fromLocalFile(gesture.videoPath));
#endif
        m_currentTeachingVideoPath = gesture.videoPath;
    }
    m_teachingPlayer->play();
    m_teachingVideoStatusLabel->setText("正在播放：" + gesture.name);
    showMessage("正在播放「" + gesture.name + "」教学视频。");
}

void MainWindow::playExamReferenceVideo()
{
    const Gesture gesture = selectedGesture();
    if (gesture.id <= 0) {
        showMessage("请先选择或抽取一个考核手势。");
        return;
    }

    if (gesture.videoPath.isEmpty()) {
        showMessage(m_userManager.currentUserIsAdmin()
            ? "当前手势还没有绑定教学视频，请到“手势资源库”的视频素材库上传。"
            : "当前手势还没有公共教学视频，请联系管理员上传。");
        if (m_examReferenceVideoStatusLabel) {
            m_examReferenceVideoStatusLabel->setText("当前考核手势暂无标准教学视频。");
        }
        return;
    }

    if (m_examReferenceVideoWidget) {
        m_teachingPlayer->setVideoOutput(m_examReferenceVideoWidget);
    }
    if (m_currentTeachingVideoPath != gesture.videoPath) {
#if QT_VERSION_MAJOR >= 6
        m_teachingPlayer->setSource(QUrl::fromLocalFile(gesture.videoPath));
#else
        m_teachingPlayer->setMedia(QUrl::fromLocalFile(gesture.videoPath));
#endif
        m_currentTeachingVideoPath = gesture.videoPath;
    }
    m_teachingPlayer->play();
    if (m_examReferenceVideoStatusLabel) {
        m_examReferenceVideoStatusLabel->setText("正在播放考核参考视频：" + gesture.name);
    }
    if (m_examFeedbackText) {
        m_examFeedbackText->setPlainText("标准视频参考：正在播放「" + gesture.name
            + "」的教学视频。请对照标准动作后，再使用上方考核摄像头完成动作。");
    }
    showMessage("正在考核评测中播放「" + gesture.name + "」标准视频。");
}

void MainWindow::stopTeachingVideo()
{
    m_teachingPlayer->pause();
    m_teachingVideoStatusLabel->setText("教学视频已暂停，点击播放可继续观看");
    if (m_examReferenceVideoStatusLabel) {
        m_examReferenceVideoStatusLabel->setText("标准教学视频已暂停，点击查看标准视频可继续观看。");
    }
    showMessage("教学视频已暂停，画面已保留。");
}

void MainWindow::updateTeachingVideoStatus()
{
    const Gesture gesture = selectedGesture();
    if (m_teachingVideoStack && m_teachingVideoPlaceholder) {
        m_teachingVideoPlaceholder->setText(gesture.videoPath.isEmpty() ? "🎬\n暂无教学视频" : "🎬\n视频待播放");
        m_teachingVideoStack->setCurrentWidget(m_teachingVideoPlaceholder);
    }
    if (gesture.id <= 0) {
        m_teachingVideoStatusLabel->setText("请先选择一个手势");
        if (m_libraryDetailLabel) {
            m_libraryDetailLabel->setText("请先选择一个手势。");
        }
        if (m_videoLibraryStatusLabel) {
            m_videoLibraryStatusLabel->setText("请选择一个手势查看视频绑定状态。");
        }
        return;
    }

    if (gesture.videoPath.isEmpty()) {
        m_teachingVideoStatusLabel->setText("「" + gesture.name + "」未绑定教学视频");
        if (m_videoLibraryStatusLabel) {
            m_videoLibraryStatusLabel->setText("素材状态：「" + gesture.name + "」暂无标准视频。管理员可在这里上传一次，其他模块自动共用。");
        }
    } else {
        m_teachingVideoStatusLabel->setText("「" + gesture.name + "」已绑定教学视频");
        if (m_videoLibraryStatusLabel) {
            m_videoLibraryStatusLabel->setText("素材状态：「" + gesture.name + "」已绑定标准视频，动作练习和考核评测可直接播放。");
        }
    }
    if (m_libraryDetailLabel) {
        m_libraryDetailLabel->setText(QString("手势：%1\n难度：%2\n说明：%3\n教学视频：%4\n练习建议：先观看统一素材库里的标准视频，再启动摄像头完成 AI 评分。")
            .arg(gesture.name)
            .arg(gesture.difficulty)
            .arg(gesture.description)
            .arg(gesture.videoPath.isEmpty() ? "未绑定" : "已绑定"));
    }
}

void MainWindow::refreshDashboard()
{
    const int userId = m_userManager.currentUserId();
    const LearningSummary summary = m_statistics.loadSummary(userId);

    if (m_homePracticeValue) {
        QString recommendedGesture = "你好";
        const QStringList wrongItems = m_statistics.wrongAnswersForUser(userId);
        if (!wrongItems.isEmpty() && !wrongItems.first().startsWith("暂无")) {
            recommendedGesture = wrongItems.first().section("：", 0, 0);
        } else if (m_gestureCombo && m_gestureCombo->currentIndex() >= 0) {
            recommendedGesture = selectedGesture().name;
        }
        m_homePracticeValue->setText(recommendedGesture);
    }
    if (m_homeAverageValue) {
        m_homeAverageValue->setText(QString("%1 次 / 平均分 %2")
            .arg(summary.practiceCount)
            .arg(summary.averageScore, 0, 'f', 1));
    }
    if (m_homeWrongValue) {
        m_homeWrongValue->setText(QString("%1 个").arg(summary.wrongAnswerCount));
    }
    if (m_statsSummaryLabel) {
        m_statsSummaryLabel->setText(m_statistics.summaryForUser(userId));
    }
    if (m_wrongAnswerList) {
        m_wrongAnswerList->clear();
        m_wrongAnswerList->addItems(m_statistics.wrongAnswersForUser(userId));
    }
    if (m_homeRecentList) {
        m_homeRecentList->clear();
        m_homeRecentList->addItems(m_statistics.recentPracticeItems(userId, 4));
    }
    if (m_recentPracticeList) {
        m_recentPracticeList->clear();
        m_recentPracticeList->addItems(m_statistics.recentPracticeItems(userId, 6));
    }
    if (m_scoreTrendLabel) {
        m_scoreTrendLabel->setText(m_statistics.scoreTrendText(userId, 6));
    }
    if (m_homeTaskLabel) {
        const QString task = summary.wrongAnswerCount > 0
            ? QString("今日建议：先复习 %1 个低分手势，再完成 3 次动作练习和 1 次考核。").arg(summary.wrongAnswerCount)
            : "今日建议：完成 3 次动作练习、观看 1 个标准教学视频，并进行 1 次随机考核。";
        m_homeTaskLabel->setText(task);
    }
    if (m_systemStatusDetailLabel) {
        m_systemStatusDetailLabel->setText(QString("数据库：%1；摄像头：%2；AI 模型：MediaPipe HandLandmarker；调试帧：项目运行时生成。")
            .arg(m_databaseManager.isOpen() ? "已连接" : "未连接")
            .arg(m_cameraCapture.isRunning() ? "已启动" : "未启动"));
    }
}

void MainWindow::refreshLearningPanels()
{
    refreshDashboard();
    const QString summary = m_statistics.summaryForUser(m_userManager.currentUserId());
    if (m_feedbackText && m_feedbackText->toPlainText().trimmed().isEmpty()) {
        m_feedbackText->setPlainText(summary);
    }
    if (m_statsSummaryLabel) {
        m_statsSummaryLabel->setText(summary);
    }
}

void MainWindow::practiceSelectedGesture()
{
    const Gesture gesture = selectedGesture();
    if (gesture.id <= 0) {
        showMessage("请先选择一个手势。");
        return;
    }
    if (!m_cameraCapture.isRunning()) {
        const QString message = "请先启动摄像头，再进行动作评分。";
        m_scoreLabel->setText("动作评分模块：等待摄像头启动，未生成评分");
        m_feedbackText->setPlainText(
            "手部关键点识别模块：未采集到摄像头画面。\n"
            "智能纠错反馈模块：请先点击“启动摄像头”，看到实时画面后再练习选中手势。\n\n"
            "说明：当前已禁止使用模拟关键点直接出成绩，避免产生不真实的评分记录。");
        showMessage(message);
        return;
    }
    if (m_detectionRunning) {
        showMessage("AI 正在识别上一帧，请稍等完成后再点击。");
        return;
    }

    const QImage frame = m_cameraCapture.lastFrame();
    if (frame.isNull()) {
        m_scoreLabel->setText("动作评分模块：等待真实手部关键点，未生成评分");
        m_feedbackText->setPlainText(
            "手部关键点识别模块：摄像头已启动，但暂时没有采集到有效画面。\n"
            "智能纠错反馈模块：请等待摄像头画面稳定后再点击练习或考核。");
        showMessage("摄像头画面尚未准备好，请稍等。");
        return;
    }

    m_detectionRunning = true;
    m_pendingScoringGestureId = gesture.id;
    m_scoreLabel->setText("动作评分模块：正在调用 MediaPipe AI 识别手部关键点…");
    m_feedbackText->setPlainText(
        "手部关键点识别模块：正在读取摄像头当前画面。\n"
        "AI 评估模块：正在调用 MediaPipe Hands，请保持手部在画面中央并等待几秒。");
    showMessage("AI 手部关键点检测已在后台运行，请稍等…");

    const HandKeypointDetector detector = m_detector;
    const int gestureId = gesture.id;
    m_detectionWatcher->setFuture(QtConcurrent::run([detector, frame, gestureId]() {
        return detector.detectRealtimeHand(frame, gestureId);
    }));
}

void MainWindow::finishPracticeScoring(const HandDetectionResult& detection)
{
    const int gestureId = m_pendingScoringGestureId;
    if (gestureId <= 0) {
        return;
    }

    Gesture gesture;
    for (const Gesture& item : m_gestures) {
        if (item.id == gestureId) {
            gesture = item;
            break;
        }
    }
    if (gesture.id <= 0) {
        gesture = selectedGesture();
    }

    const auto reference = gesture.referenceKeypoints.empty()
        ? m_detector.referenceHand(gestureId)
        : gesture.referenceKeypoints;
    if (!detection.success || detection.keypoints.size() != reference.size()) {
        m_scoreLabel->setText("动作评分模块：" + detection.message.split('\n').value(0) + "，未生成评分");
        m_feedbackText->setPlainText(
            "手部关键点识别模块：" + detection.message + "\n"
            "智能纠错反馈模块：当前没有可用于评分的真实手部关键点。\n\n"
            "建议方案：接入 MediaPipe Hands / OpenCV，从摄像头画面中提取 21 个手部关键点，再与标准关键点进行对比评分。");
        if (!m_pendingAssessmentText.isEmpty()) {
            m_feedbackText->appendPlainText(m_pendingAssessmentText + "\n本次考核未生成成绩，请重新摆好手势后再试。");
            if (m_examScoreLabel) {
                m_examScoreLabel->setText("考核评分模块：未生成成绩");
            }
            if (m_examFeedbackText) {
                m_examFeedbackText->setPlainText("考核反馈：" + detection.message
                    + "\n本次考核未生成成绩，请重新摆好手势后再试。");
            }
            m_pendingAssessmentText.clear();
        }
        showMessage("未检测到真实手部关键点，暂不生成评分。");
        return;
    }

    const ScoreResult score = m_scorer.score(detection.keypoints, reference, detection.stabilityDeviation);
    const QString feedback = m_feedbackGenerator.buildFeedback(score);
    const bool invalidGesture = score.totalScore < 60.0;
    const QString recognitionText = invalidGesture
        ? "手部关键点识别模块：已检测到手部，但动作不符合当前目标手势。"
        : "手部关键点识别模块：已识别 21 个真实关键点。";

    m_scoreLabel->setText(QString("动作评分模块：总分 %1，位置 %2，角度 %3，稳定 %4")
        .arg(score.totalScore, 0, 'f', 1)
        .arg(score.positionScore, 0, 'f', 1)
        .arg(score.angleScore, 0, 'f', 1)
        .arg(score.stabilityScore, 0, 'f', 1));

    m_feedbackText->setPlainText(
        recognitionText + "\n"
        "智能纠错反馈模块：" + feedback + "\n\n"
        "学习数据统计模块：" + m_statistics.summaryForUser(m_userManager.currentUserId()));
    if (!m_pendingAssessmentText.isEmpty()) {
        m_feedbackText->appendPlainText(m_pendingAssessmentText);
        if (m_examScoreLabel) {
            m_examScoreLabel->setText(QString("考核评分模块：总分 %1，位置 %2，角度 %3，稳定 %4")
                .arg(score.totalScore, 0, 'f', 1)
                .arg(score.positionScore, 0, 'f', 1)
                .arg(score.angleScore, 0, 'f', 1)
                .arg(score.stabilityScore, 0, 'f', 1));
        }
        if (m_examFeedbackText) {
            m_examFeedbackText->setPlainText("考核反馈：" + feedback + m_pendingAssessmentText
                + (invalidGesture ? "\n判定结果：动作无效或不符合考核题目，请重新完成目标手势。" : ""));
        }
        m_pendingAssessmentText.clear();
    }

    if (m_userManager.currentUserId() > 0 && m_databaseManager.isOpen()) {
        PracticeRecord record;
        record.userId = m_userManager.currentUserId();
        record.gestureId = gestureId;
        record.totalScore = score.totalScore;
        record.positionScore = score.positionScore;
        record.angleScore = score.angleScore;
        record.stabilityScore = score.stabilityScore;
        record.feedback = feedback;

        QString error;
        if (!m_databaseManager.savePracticeRecord(record, &error)) {
            showMessage("练习记录保存失败：" + error);
        } else if (m_assessment.shouldAddToWrongBook(score.totalScore)
            && !m_databaseManager.saveWrongAnswer(record.userId, record.gestureId, record.totalScore, &error)) {
            showMessage("练习记录已保存，但错题本写入失败：" + error);
        } else {
            showMessage("练习完成，记录已保存。");
        }
        refreshLearningPanels();
    } else {
        showMessage("练习完成。当前未登录或未连接数据库，因此未保存记录。");
    }
}

void MainWindow::runAssessment()
{
    if (m_detectionRunning) {
        showMessage("AI 正在识别上一帧，请稍等完成后再开始考核。");
        return;
    }
    if (!m_cameraCapture.isRunning()) {
        m_scoreLabel->setText("动作评分模块：等待摄像头启动，未生成考核成绩");
        m_feedbackText->setPlainText(
            "考核评测模块：未启动摄像头，无法采集动作画面。\n"
            "请先点击“启动摄像头”，确认画面正常后再开始考核。");
        showMessage("请先启动摄像头，再开始考核。");
        return;
    }

    const Gesture question = m_assessment.pickQuestion(m_gestures);
    if (question.id <= 0) {
        showMessage("手势库为空，无法考核。");
        return;
    }

    for (int i = 0; i < m_gestures.size(); ++i) {
        if (m_gestures[i].id == question.id) {
            m_gestureCombo->setCurrentIndex(i);
            break;
        }
    }
    m_pendingAssessmentText = "\n考核评测与错题复习模块：本次考核题目为「" + question.name
        + "」。若得分低于 70 分，应加入错题本进行复习。";
    if (m_examScoreLabel) {
        m_examScoreLabel->setText("考核评分模块：正在识别「" + question.name + "」动作…");
    }
    if (m_examFeedbackText) {
        m_examFeedbackText->setPlainText("本次考核题目：「" + question.name
            + "」。请面对摄像头完成动作，AI 正在识别关键点。");
    }
    practiceSelectedGesture();
}

void MainWindow::refreshGestureList()
{
    m_gestureCombo->clear();
    for (const Gesture& gesture : m_gestures) {
        m_gestureCombo->addItem(gestureDisplayText(gesture));
        m_gestureCombo->setItemData(m_gestureCombo->count() - 1, gesture.description, Qt::ToolTipRole);
    }
    if (!m_gestures.isEmpty()) {
        m_gestureCombo->setCurrentIndex(0);
    }
    refreshLibrarySummary();
}

void MainWindow::refreshLibrarySummary()
{
    QMap<int, QStringList> gesturesByDifficulty;
    int videoCount = 0;
    int templateCount = 0;
    for (const Gesture& gesture : m_gestures) {
        gesturesByDifficulty[gesture.difficulty].append(gesture.name);
        if (!gesture.videoPath.trimmed().isEmpty()) {
            ++videoCount;
        }
        if (!gesture.referenceKeypoints.empty()) {
            ++templateCount;
        }
    }

    if (m_libraryOverviewList) {
        m_libraryOverviewList->clear();
        if (m_gestures.isEmpty()) {
            m_libraryOverviewList->addItem("当前手势资源库为空。");
        } else {
            for (const Gesture& gesture : m_gestures) {
                const QString videoStatus = gesture.videoPath.trimmed().isEmpty() ? "未绑定视频" : "已绑定视频";
                const QString templateStatus = gesture.referenceKeypoints.empty() ? "无评分模板" : "有评分模板";
                m_libraryOverviewList->addItem(QString("%1  |  难度:%2  |  %3  |  %4")
                                                   .arg(gesture.name)
                                                   .arg(gesture.difficulty)
                                                   .arg(videoStatus)
                                                   .arg(templateStatus));
            }
            m_libraryOverviewList->addItem(QString("资源统计：共 %1 个手势，%2 个已绑定视频，%3 个有评分模板")
                                               .arg(m_gestures.size())
                                               .arg(videoCount)
                                               .arg(templateCount));
        }
    }

    if (m_libraryCategoryList) {
        m_libraryCategoryList->clear();
        if (m_gestures.isEmpty()) {
            m_libraryCategoryList->addItem("连接数据库或导入视频后显示难度分组。");
        } else {
            QMap<int, QStringList>::const_iterator it = gesturesByDifficulty.constBegin();
            for (; it != gesturesByDifficulty.constEnd(); ++it) {
                m_libraryCategoryList->addItem(QString("难度 %1：共 %2 个手势")
                                                   .arg(it.key())
                                                   .arg(it.value().size()));
            }
            m_libraryCategoryList->addItem("难度 1：基础动作，适合入门练习。");
            m_libraryCategoryList->addItem("难度 2：进阶动作，手型或动作变化更多。");
            m_libraryCategoryList->addItem("新增手势：导入视频后自动识别难度，并同步到资源库清单。");
        }
    }
}

void MainWindow::updateAdminControls()
{
    const bool isAdmin = m_userManager.currentUserIsAdmin();
    if (m_importVideoButton) {
        m_importVideoButton->setVisible(isAdmin);
        m_importVideoButton->setEnabled(isAdmin);
    }
    if (m_manageAccountsButton) {
        m_manageAccountsButton->setVisible(isAdmin);
        m_manageAccountsButton->setEnabled(isAdmin);
    }
    if (m_currentUserLabel && m_userManager.currentUserId() > 0) {
        m_currentUserLabel->setText(QString("当前用户：%1%2")
            .arg(m_userManager.currentUsername())
            .arg(isAdmin ? "（管理员）" : ""));
    }
}

void MainWindow::ensureTeachingVideoWidget()
{
    if (m_teachingVideoWidget || !m_teachingVideoStack) {
        return;
    }

    m_teachingVideoWidget = new QVideoWidget(m_teachingVideoStack);
    m_teachingVideoWidget->setObjectName("teachingVideo");
    m_teachingVideoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    m_teachingVideoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_teachingVideoWidget->setMinimumSize(0, 0);
    m_teachingVideoWidget->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    m_teachingVideoStack->addWidget(m_teachingVideoWidget);
}

void MainWindow::showAccountManagementDialog()
{
    if (!m_userManager.currentUserIsAdmin()) {
        showMessage("只有管理员可以管理账号。");
        return;
    }
    if (!m_databaseManager.isOpen()) {
        showMessage("账号管理需要先连接 MySQL 数据库。");
        QMessageBox::information(this, "账号管理", "请先连接 MySQL 数据库，再管理账号。");
        return;
    }

    showModule(m_accountManagementIndex, "👥  账号管理");
    reloadAccountUsers();
}

void MainWindow::reloadAccountUsers()
{
    if (!m_accountTable) {
        return;
    }
    if (!m_databaseManager.isOpen()) {
        m_accountTable->setRowCount(0);
        showMessage("账号管理需要先连接 MySQL 数据库。");
        return;
    }

    QString error;
    const QVector<ManagedUser> users = m_userManager.loadUsers(&error);
    m_accountTable->setRowCount(0);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, "账号管理", "加载用户失败：" + error);
        return;
    }

    m_accountTable->setRowCount(users.size());
    for (int row = 0; row < users.size(); ++row) {
        const ManagedUser& user = users[row];
        auto* idItem = new QTableWidgetItem(QString::number(user.id));
        idItem->setData(Qt::UserRole, user.id);
        m_accountTable->setItem(row, 0, idItem);
        m_accountTable->setItem(row, 1, new QTableWidgetItem(user.username));
        m_accountTable->setItem(row, 2, new QTableWidgetItem(user.role == "admin" ? "管理员" : "普通用户"));
    }
    if (m_accountTable->rowCount() > 0) {
        m_accountTable->selectRow(0);
    }
}

int MainWindow::selectedAccountUserId() const
{
    if (!m_accountTable) {
        return 0;
    }
    const int row = m_accountTable->currentRow();
    if (row < 0 || !m_accountTable->item(row, 0)) {
        return 0;
    }
    return m_accountTable->item(row, 0)->data(Qt::UserRole).toInt();
}

void MainWindow::applySharedTeachingVideos()
{
    for (Gesture& gesture : m_gestures) {
        const QString sharedPath = sharedTeachingVideoPath(gesture.id);
        if (!sharedPath.isEmpty()) {
            gesture.videoPath = sharedPath;
        }
    }
}

bool MainWindow::storeSharedTeachingVideo(const Gesture& gesture,
                                          const QString& sourcePath,
                                          QString* storedPath,
                                          QString* errorMessage)
{
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = "源视频文件不存在。";
        }
        return false;
    }

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + QDir::separator() + ".sign-language-learning-system";
    }

    const QString videoDirPath = dataDir + QDir::separator() + "teaching-videos";
    QDir videoDir(videoDirPath);
    if (!videoDir.exists() && !videoDir.mkpath(".")) {
        if (errorMessage) {
            *errorMessage = "无法创建公共教学视频目录。";
        }
        return false;
    }

    const QString suffix = sourceInfo.suffix().isEmpty() ? "mp4" : sourceInfo.suffix();
    const QString targetPath = videoDir.filePath(QString("gesture_%1.%2").arg(gesture.id).arg(suffix));
    if (QFile::exists(targetPath) && !QFile::remove(targetPath)) {
        if (errorMessage) {
            *errorMessage = "无法覆盖旧教学视频。";
        }
        return false;
    }
    if (!QFile::copy(sourcePath, targetPath)) {
        if (errorMessage) {
            *errorMessage = "复制视频到公共目录失败。";
        }
        return false;
    }

    QSettings settings(dataDir + QDir::separator() + "shared-teaching-videos.ini", QSettings::IniFormat);
    settings.setValue(QString("gesture_%1").arg(gesture.id), targetPath);
    settings.sync();

    if (storedPath) {
        *storedPath = targetPath;
    }
    return true;
}

QString MainWindow::sharedTeachingVideoPath(int gestureId) const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QDir::homePath() + QDir::separator() + ".sign-language-learning-system";
    }

    QSettings settings(dataDir + QDir::separator() + "shared-teaching-videos.ini", QSettings::IniFormat);
    const QString path = settings.value(QString("gesture_%1").arg(gestureId)).toString();
    return QFile::exists(path) ? path : QString();
}

Gesture MainWindow::selectedGesture() const
{
    const int row = m_gestureCombo->currentIndex();
    if (row < 0 || row >= m_gestures.size()) {
        return {};
    }
    return m_gestures[row];
}

void MainWindow::showMessage(const QString& message)
{
    m_statusLabel->setText("系统状态：" + message);
    QTimer::singleShot(0, this, &MainWindow::updateMainScrollBarPolicy);
}
