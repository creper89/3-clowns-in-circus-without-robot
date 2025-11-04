#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QProgressBar>
#include <QLCDNumber>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QPixmap>
#include <QImage>
#include <QQueue>
#include <random>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateSensorData();
    void updateVideoFrame();
    void moveForward();
    void moveBackward();
    void turnLeft();
    void turnRight();
    void stopRobot();
    void sendPacketCommand();
    void moveToObstacle();
    void saveSnapshot();
    void toggleVideoQuality();
    void soundSignal();
    void saveVideoStream();

private:
    void setupUI();
    QVBoxLayout* createControlPanel();
    QVBoxLayout* createTelemetryPanel();
    QVBoxLayout* createVideoPanel();
    QVBoxLayout* createStatusPanel();
    void saveTelemetryToFile();
    void initCamera();
    
    // UI элементы
    QWidget *centralWidget;
    
    // Панель управления
    QPushButton *btnForward;
    QPushButton *btnBackward;
    QPushButton *btnLeft;
    QPushButton *btnRight;
    QPushButton *btnStop;
    QPushButton *btnSound;
    
    // Пакетные команды
    QTextEdit *packetCommandEdit;
    QPushButton *btnSendPacket;
    QPushButton *btnMoveToObstacle;
    
    // Видеопоток
    QLabel *videoLabel;
    QPushButton *btnSaveFrame;
    QPushButton *btnSaveVideoStream;
    QPushButton *btnToggleQuality;
    QLabel *videoQualityLabel;
    bool highQuality;
    
    // Телеметрия
    QLCDNumber *distanceSensorDisplay;
    QLCDNumber *temperatureDisplay;
    QLCDNumber *humidityDisplay;
    QLabel *connectionStatusLabel;
    QLabel *timestampLabel;
    
    // Лог телеметрии
    QTextEdit *telemetryLog;
    QPushButton *btnSaveTelemetry;
    
    // Таймеры
    QTimer *sensorUpdateTimer;
    QTimer *connectionTimer;
    QTimer *videoTimer;
    
    // Камера и буфер видеокадров
    cv::VideoCapture camera;
    bool cameraAvailable;
    QQueue<cv::Mat> videoFrameBuffer;
    int maxFramesInBuffer;
    
    // Данные датчиков
    double distance;
    double temperature;
    int humidity;
    bool isConnected;
    
    // Генератор случайных чисел
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<> distDist;
    std::uniform_real_distribution<> tempDist;
    std::uniform_int_distribution<> battDist;
    std::uniform_int_distribution<> humDist;
    std::uniform_int_distribution<> signalDist;
    std::uniform_real_distribution<> gpsDist;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    
};

#endif // MAINWINDOW_H
