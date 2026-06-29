// MainWindow.hpp — Qt6 GUI for the 2-DOF comparison demo.
#ifndef TDOF_QT_MAINWINDOW_HPP
#define TDOF_QT_MAINWINDOW_HPP

#include "tdof_core.h"

#include <QMainWindow>

class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace tdof_qt {

class LinePlot;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onRunClicked();
    void onResetClicked();

private:
    void buildUi();
    void resetParametersToDefaults();
    bool runSimulation();
    TdofConfig collectConfig() const;

    QDoubleSpinBox* m_  = nullptr;
    QDoubleSpinBox* c_  = nullptr;
    QDoubleSpinBox* k_  = nullptr;
    QDoubleSpinBox* kp_ = nullptr;
    QDoubleSpinBox* ki_ = nullptr;
    QDoubleSpinBox* kd_ = nullptr;
    QDoubleSpinBox* ref_   = nullptr;
    QDoubleSpinBox* tEnd_  = nullptr;
    QDoubleSpinBox* dt_    = nullptr;

    QPushButton* runBtn_   = nullptr;
    QPushButton* resetBtn_ = nullptr;

    LinePlot* refPlot_ = nullptr;
    LinePlot* outPlot_ = nullptr;
    QPlainTextEdit* tfEdit_  = nullptr;
    QPlainTextEdit* logEdit_ = nullptr;
    QLabel*         statusLbl_ = nullptr;
};

}  // namespace tdof_qt

#endif
