#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    MainWindow window;
    window.setWindowTitle("Пульт оператора ТУПР v1.0");
    window.resize(1200, 720); 
    window.show();

    window.activateWindow();
    window.raise();
    window.setFocus();
    
    return app.exec();
}
