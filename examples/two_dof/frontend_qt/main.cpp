// main.cpp — Qt6 entry point.
#include "MainWindow.hpp"
#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    tdof_qt::MainWindow w;
    w.show();
    return app.exec();
}
