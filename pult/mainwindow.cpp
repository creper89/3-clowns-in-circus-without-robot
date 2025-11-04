#include "mainwindow.h"
#include <QMessageBox>
#include <QPainter>
#include <QDir>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      gen(rd()),
      distDist(5.0, 200.0),
      tempDist(10.0, 40.0),
      battDist(60, 100),
      humDist(30, 80),
      signalDist(40, 100),
      gpsDist(0.0, 0.001),
      highQuality(true),
      distance(100.0),
      temperature(25.0),
      humidity(55),
      isConnected(true),
      cameraAvailable(false),
      maxFramesInBuffer(900)  // ~30 секунд при 30 FPS
{
    setupUI();
    initCamera();

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
    centralWidget->installEventFilter(this);
    
    // Таймер обновления датчиков (каждые 500 мс)
    sensorUpdateTimer = new QTimer(this);
    connect(sensorUpdateTimer, &QTimer::timeout, this, &MainWindow::updateSensorData);
    sensorUpdateTimer->start(500);
    
    // Таймер видео (каждые 33 мс ≈ 30 FPS)
    videoTimer = new QTimer(this);
    connect(videoTimer, &QTimer::timeout, this, &MainWindow::updateVideoFrame);
    videoTimer->start(33);
    
    // Таймер проверки соединения
    /*connectionTimer = new QTimer(this);
    connect(connectionTimer, &QTimer::timeout, this, [this]() {
        if (signalStrength < 50) {
            isConnected = (std::uniform_int_distribution<>(0, 1)(gen) == 1);
        } else {
            isConnected = true;
        }
        
        if (isConnected) {
            connectionStatusLabel->setText("Связь: <font color='green'>УСТАНОВЛЕНА</font>");
        } else {
            connectionStatusLabel->setText("Связь: <font color='red'>ПОТЕРЯНА</font>");
        }
    });
    connectionTimer->start(1000);*/
}


MainWindow::~MainWindow()
{
    if (camera.isOpened()) {
        camera.release();
    }
}

void MainWindow::initCamera()
{
    // Попытка открыть камеру (0 - встроенная камера)
    camera.open(0);
    
    if (camera.isOpened()) {
        cameraAvailable = true;
        telemetryLog->append("[CAMERA] Камера успешно подключена");
    } else {
        cameraAvailable = false;
        telemetryLog->append("[CAMERA] Камера не найдена - используется симуляция");
    }
}

void MainWindow::updateVideoFrame()
{
    if (cameraAvailable && camera.isOpened()) {
        cv::Mat frame;
        camera >> frame;

        if (!frame.empty()) {
            cv::Mat outFrame = frame;
            // Обрезаем/снижаем качество только если надо
            if (!highQuality) {
                // Снизить разрешение, вызывается только для низкого качества
                cv::resize(frame, outFrame, cv::Size(128, 72));
            }

            // Конвертация BGR -> RGB для Qt
            cv::cvtColor(outFrame, outFrame, cv::COLOR_BGR2RGB);

            QImage qimg(outFrame.data, outFrame.cols, outFrame.rows, outFrame.step, QImage::Format_RGB888);
            QPixmap pixmap = QPixmap::fromImage(qimg).scaled(
                videoLabel->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
            videoLabel->setPixmap(pixmap);

            // Записываем (если надо) всегда тот же outFrame
            videoFrameBuffer.enqueue(outFrame.clone());
            if (videoFrameBuffer.size() > maxFramesInBuffer) {
                videoFrameBuffer.dequeue();
            }
        }
    } else {
        // Имитация/симуляция
        QPixmap videoFrame(640, 480);
        videoFrame.fill(QColor(26, 26, 26));
        QPainter painter(&videoFrame);
        painter.setPen(QColor(100, 255, 100));
        painter.setFont(QFont("Arial", 20));
        painter.drawText(videoFrame.rect(), Qt::AlignCenter,
                        "VIDEO STREAM\n[Simulated]\nКамера не подключена");
        videoLabel->setPixmap(videoFrame);
    }
}


void MainWindow::saveVideoStream()
{
    if (videoFrameBuffer.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Нет кадров для сохранения");
        return;
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("video_%1.mp4").arg(timestamp);
    
    // Создаём папку videos если её нет
    QDir dir;
    if (!dir.exists("videos")) {
        dir.mkdir("videos");
    }
    
    QString filepath = QString("videos/%1").arg(filename);
    
    // Параметры видеозаписи
    int codec = cv::VideoWriter::fourcc('m', 'p', '4', 'v');  // MP4V codec
    double fps = 30.0;
    cv::Size frameSize(1280, 720);
    
    // Открываем VideoWriter
    cv::VideoWriter videoWriter(filepath.toStdString(), codec, fps, frameSize, true);
    
    if (!videoWriter.isOpened()) {
        QMessageBox::critical(this, "Ошибка", "Не удалось открыть файл для записи");
        return;
    }
    
    // Записываем все кадры из буфера
    int frameCount = 0;
    while (!videoFrameBuffer.isEmpty()) {
        cv::Mat frame = videoFrameBuffer.dequeue();
        
        // Изменяем размер кадра если нужно
        if (frame.cols != frameSize.width || frame.rows != frameSize.height) {
            cv::resize(frame, frame, frameSize);
        }
        
        videoWriter.write(frame);
        frameCount++;
    }
    
    videoWriter.release();
    
    telemetryLog->append(QString("[VIDEO] Видеопоток сохранен: %1 (%2 кадров)")
                         .arg(filepath).arg(frameCount));
    QMessageBox::information(this, "Видеопоток сохранен", 
                            QString("Видеопоток успешно сохранен:\n%1\n\nКадров: %2")
                            .arg(filepath).arg(frameCount));
}


void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    centralWidget->setFocusPolicy(Qt::StrongFocus);
    centralWidget->setFocus();
    
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    
    // Левая панель - видео и управление
    QVBoxLayout *leftPanel = new QVBoxLayout();
    leftPanel->addLayout(createVideoPanel());  // ОДНО окно видео
    leftPanel->addLayout(createControlPanel());
    
    // Правая панель - телеметрия и статус
    QVBoxLayout *rightPanel = new QVBoxLayout();
    rightPanel->addLayout(createTelemetryPanel());
    rightPanel->addLayout(createStatusPanel());
    
    mainLayout->addLayout(leftPanel, 2);
    mainLayout->addLayout(rightPanel, 1);
}

QVBoxLayout* MainWindow::createVideoPanel()
{
    QGroupBox *videoGroup = new QGroupBox("Видеопоток");
    QVBoxLayout *videoLayout = new QVBoxLayout();
    
    videoLabel = new QLabel();
    videoLabel->setFixedSize(640, 360);
    videoLabel->setStyleSheet("background-color: #1a1a1a; border: 2px solid #333;");
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setScaledContents(false);
    
    videoLayout->addWidget(videoLabel);
    
    // Кнопки управления видео
    QHBoxLayout *videoControls = new QHBoxLayout();
    btnSaveFrame = new QPushButton("Сохранить кадр");
    btnSaveVideoStream = new QPushButton("Сохранить видеопоток");
    btnToggleQuality = new QPushButton("Переключить качество");
    videoQualityLabel = new QLabel("Качество: <b>Высокое</b>");
    
    videoControls->addWidget(btnSaveFrame);
    videoControls->addWidget(btnSaveVideoStream);
    videoControls->addWidget(btnToggleQuality);
    videoControls->addWidget(videoQualityLabel);
    videoLayout->addLayout(videoControls);
    
    videoGroup->setLayout(videoLayout);
    
    connect(btnSaveFrame, &QPushButton::clicked, this, &MainWindow::saveSnapshot);
    connect(btnSaveVideoStream, &QPushButton::clicked, this, &MainWindow::saveVideoStream);
    connect(btnToggleQuality, &QPushButton::clicked, this, &MainWindow::toggleVideoQuality);
    
    QVBoxLayout *result = new QVBoxLayout();
    result->addWidget(videoGroup);
    return result;
}


QVBoxLayout* MainWindow::createControlPanel()
{
    QGroupBox *controlGroup = new QGroupBox("Управление роботом");
    QVBoxLayout *controlLayout = new QVBoxLayout();
    
    // Кнопки управления движением
    QGridLayout *moveButtons = new QGridLayout();
    
    btnForward = new QPushButton("▲ Вперед");
    btnBackward = new QPushButton("▼ Назад");
    btnLeft = new QPushButton("◄ Влево");
    btnRight = new QPushButton("► Вправо");
    btnStop = new QPushButton("■ СТОП");
    btnSound = new QPushButton("♪ Звук ♪");
    
    btnStop->setStyleSheet("background-color: #ff4444; color: white; font-weight: bold;");
    btnForward->setStyleSheet("background-color: #2196F3; color: white;");
    btnBackward->setStyleSheet("background-color: #2196F3; color: white;");
    btnLeft->setStyleSheet("background-color: #2196F3; color: white;");
    btnRight->setStyleSheet("background-color: #2196F3; color: white;");
    
    moveButtons->addWidget(btnForward, 0, 1);
    moveButtons->addWidget(btnLeft, 1, 0);
    moveButtons->addWidget(btnStop, 1, 1);
    moveButtons->addWidget(btnRight, 1, 2);
    moveButtons->addWidget(btnBackward, 2, 1);
    
    controlLayout->addLayout(moveButtons);
    controlLayout->addWidget(btnSound);
    
    
    // Пакетные команды
    QLabel *cmdLabel = new QLabel("Пакетная команда:");
    cmdLabel->setMaximumHeight(20);
    controlLayout->addWidget(cmdLabel);
    
    packetCommandEdit = new QTextEdit();
    packetCommandEdit->setMaximumHeight(60);
    packetCommandEdit->setPlaceholderText("forward 2000\nright 90\nstop");
    controlLayout->addWidget(packetCommandEdit);
    
    btnSendPacket = new QPushButton("Отправить пакет");
    btnMoveToObstacle = new QPushButton("Движение до препятствия");
    controlLayout->addWidget(btnSendPacket);
    controlLayout->addWidget(btnMoveToObstacle);
    
    controlGroup->setLayout(controlLayout);
    
    // Подключение сигналов
    connect(btnForward, &QPushButton::clicked, this, &MainWindow::moveForward);
    connect(btnBackward, &QPushButton::clicked, this, &MainWindow::moveBackward);
    connect(btnLeft, &QPushButton::clicked, this, &MainWindow::turnLeft);
    connect(btnRight, &QPushButton::clicked, this, &MainWindow::turnRight);
    connect(btnStop, &QPushButton::clicked, this, &MainWindow::stopRobot);
    connect(btnSound, &QPushButton::clicked, this, &MainWindow::soundSignal);
    connect(btnSendPacket, &QPushButton::clicked, this, &MainWindow::sendPacketCommand);
    connect(btnMoveToObstacle, &QPushButton::clicked, this, &MainWindow::moveToObstacle);
    
    QVBoxLayout *result = new QVBoxLayout();
    result->addWidget(controlGroup);
    return result;
}

QVBoxLayout* MainWindow::createTelemetryPanel()
{
    QGroupBox *telemetryGroup = new QGroupBox("Телеметрия датчиков");
    QGridLayout *telemetryLayout = new QGridLayout();
    
    telemetryLayout->addWidget(new QLabel("Расстояние (см):"), 0, 0);
    distanceSensorDisplay = new QLCDNumber();
    distanceSensorDisplay->setDigitCount(6);
    distanceSensorDisplay->setSegmentStyle(QLCDNumber::Flat);
    telemetryLayout->addWidget(distanceSensorDisplay, 0, 1);
    
    telemetryLayout->addWidget(new QLabel("Температура (°C):"), 1, 0);
    temperatureDisplay = new QLCDNumber();
    temperatureDisplay->setDigitCount(5);
    temperatureDisplay->setSegmentStyle(QLCDNumber::Flat);
    telemetryLayout->addWidget(temperatureDisplay, 1, 1);
    
    
    telemetryLayout->addWidget(new QLabel("Влажность (%):"), 3, 0);
    humidityDisplay = new QLCDNumber();
    humidityDisplay->setDigitCount(3);
    humidityDisplay->setSegmentStyle(QLCDNumber::Flat);
    telemetryLayout->addWidget(humidityDisplay, 3, 1);
    
    
    telemetryLayout->addWidget(new QLabel("Время:"), 6, 0);
    timestampLabel = new QLabel();
    telemetryLayout->addWidget(timestampLabel, 6, 1);
    
    telemetryGroup->setLayout(telemetryLayout);
    
    QVBoxLayout *result = new QVBoxLayout();
    result->addWidget(telemetryGroup);
    return result;
}

QVBoxLayout* MainWindow::createStatusPanel()
{
    QGroupBox *statusGroup = new QGroupBox("Статус и журнал");
    QVBoxLayout *statusLayout = new QVBoxLayout();
    
    connectionStatusLabel = new QLabel("Связь: <font color='green'>УСТАНОВЛЕНА</font>");
    connectionStatusLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    statusLayout->addWidget(connectionStatusLabel);
    
    statusLayout->addWidget(new QLabel("Журнал:"));
    telemetryLog = new QTextEdit();
    telemetryLog->setReadOnly(true);
    telemetryLog->setMaximumHeight(150);
    statusLayout->addWidget(telemetryLog);
    
    btnSaveTelemetry = new QPushButton("Сохранить лог");
    statusLayout->addWidget(btnSaveTelemetry);
    
    connect(btnSaveTelemetry, &QPushButton::clicked, this, &MainWindow::saveTelemetryToFile);
    
    statusGroup->setLayout(statusLayout);
    
    QVBoxLayout *result = new QVBoxLayout();
    result->addWidget(statusGroup);
    return result;
}

void MainWindow::updateSensorData()
{
    distance = distDist(gen);
    temperature = tempDist(gen);
    humidity = humDist(gen);
    
    distanceSensorDisplay->display(QString::number(distance, 'f', 1));
    temperatureDisplay->display(QString::number(temperature, 'f', 1));
    humidityDisplay->display(humidity);
    
    QString timestamp = QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss");
    timestampLabel->setText(timestamp);
    
    QString logEntry = QString("[%1] Dist: %2cm, Temp: %3°C, Hum: %4%")
                       .arg(timestamp)
                       .arg(distance, 0, 'f', 1)
                       .arg(temperature, 0, 'f', 1)
                       .arg(humidity);
    
    telemetryLog->append(logEntry);
    
    if (telemetryLog->document()->lineCount() > 100) {
        QTextCursor cursor = telemetryLog->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar();
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == centralWidget && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->isAutoRepeat()) return false;

        switch (keyEvent->key()) {
        case Qt::Key_W:
            moveForward();
            return true;
        case Qt::Key_S:
            moveBackward();
            return true;
        case Qt::Key_A:
            turnLeft();
            return true;
        case Qt::Key_D:
            turnRight();
            return true;
        case Qt::Key_Space:
            stopRobot();
            return true;
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}


void MainWindow::moveForward()
{
    //TODO
    telemetryLog->append("[CMD] ВПЕРЕД");
}


void MainWindow::moveBackward()
{
    //TODO
    telemetryLog->append("[CMD] НАЗАД");
}


void MainWindow::turnLeft()
{
    //TODO
    telemetryLog->append("[CMD] Поворот ВЛЕВО");
}

void MainWindow::turnRight()
{
    //TODO
    telemetryLog->append("[CMD] Поворот ВПРАВО");
}

void MainWindow::stopRobot()
{
    //TODO
    telemetryLog->append("[CMD] ОСТАНОВКА");
    QMessageBox::information(this, "Команда", "Робот остановлен");
}

void MainWindow::sendPacketCommand()
{
    QString commands = packetCommandEdit->toPlainText();
    if (commands.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите пакет команд");
        return;
    }
    
    telemetryLog->append("[CMD] Отправка пакета команд:");
    telemetryLog->append(commands);
    QMessageBox::information(this, "Команда", "Пакет команд отправлен");
}

void MainWindow::moveToObstacle()
{
    telemetryLog->append("[CMD] Движение до препятствия");
    QMessageBox::information(this, "Команда", "Робот движется до препятствия");
}

void MainWindow::saveSnapshot()
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("snapshot_%1.png").arg(timestamp);
    
    // Создаём папку snapshots если её нет
    QDir dir;
    if (!dir.exists("snapshots")) {
        dir.mkdir("snapshots");
    }
    
    QString filepath = QString("snapshots/%1").arg(filename);
    
    // Сохраняем текущий кадр
    QPixmap pixmap = videoLabel->pixmap();
    if (!pixmap.isNull()) {                    
        pixmap.save(filepath);                  
        telemetryLog->append(QString("[SAVE] Кадр сохранен: %1").arg(filepath));
        telemetryLog->append(QString("[DATA] Dist: %1cm, Temp: %2°C, GPS: %3, %4")
                             .arg(distance, 0, 'f', 1)
                             .arg(temperature, 0, 'f', 1));
        QMessageBox::information(this, "Сохранено", QString("Кадр сохранен:\n%1").arg(filepath));
    } else {
        QMessageBox::warning(this, "Ошибка", "Нет кадра для сохранения");
    }
}



void MainWindow::toggleVideoQuality()
{
    highQuality = !highQuality;
    if (highQuality) {
        videoQualityLabel->setText("Качество: <b>Высокое</b>");
        telemetryLog->append("[VIDEO] Переключено на высокое качество");
    } else {
        videoQualityLabel->setText("Качество: <b>Низкое</b>");
        telemetryLog->append("[VIDEO] Переключено на низкое качество");
    }
}



void MainWindow::soundSignal()
{
    telemetryLog->append("[CMD] Звуковой сигнал");
    QMessageBox::information(this, "Команда", "Звуковой сигнал отправлен");
}

void MainWindow::saveTelemetryToFile()
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString filename = QString("telemetry_%1.txt").arg(timestamp);
    
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << telemetryLog->toPlainText();
        file.close();
        
        QMessageBox::information(this, "Сохранено", 
                                QString("Телеметрия сохранена:\n%1").arg(filename));
        telemetryLog->append(QString("[SAVE] Лог сохранен: %1").arg(filename));
    } else {
        QMessageBox::critical(this, "Ошибка", "Не удалось сохранить файл");
    }
}
