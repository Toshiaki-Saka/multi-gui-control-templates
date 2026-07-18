// MainWindow.hpp — Qt6 GUI for the interactive PID demo.

#ifndef PID_QT_MAINWINDOW_HPP
#define PID_QT_MAINWINDOW_HPP

#include "pid_core.h"

#include <QMainWindow>
#include <QString>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;

namespace pid_qt {

class ResponsePlot;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onAnyParamChanged();
    void onResetClicked();
    void onSaveClicked();
    void runSimulation();

private:
    void buildUi();
    void resetParametersToDefaults();
    PidConfig collectConfig() const;

    QDoubleSpinBox* thetaStart_ = nullptr;
    QSlider*        sThetaStart_ = nullptr;

    QDoubleSpinBox* thetaGoal_  = nullptr;
    QSlider*        sThetaGoal_ = nullptr;

    QDoubleSpinBox* offset_  = nullptr;
    QSlider*        sOffset_ = nullptr;

    QSpinBox*       timeLength_  = nullptr;
    QSlider*        sTimeLength_ = nullptr;

    QDoubleSpinBox* kp_  = nullptr;  QSlider* sKp_ = nullptr;
    QDoubleSpinBox* ki_  = nullptr;  QSlider* sKi_ = nullptr;
    QDoubleSpinBox* kd_  = nullptr;  QSlider* sKd_ = nullptr;

    QDoubleSpinBox* dt_            = nullptr;  QSlider* sDt_            = nullptr;
    QDoubleSpinBox* integralClamp_ = nullptr;  QSlider* sIntegralClamp_ = nullptr;
    QDoubleSpinBox* outputClamp_   = nullptr;  QSlider* sOutputClamp_   = nullptr;

    QPushButton* resetBtn_ = nullptr;
    QPushButton* saveBtn_  = nullptr;

    ResponsePlot* plot_   = nullptr;
    QLabel*       status_ = nullptr;
    QTimer*       debounce_ = nullptr;

    // To avoid recursive feedback while two-way-syncing slider <-> spin.
    bool syncing_ = false;

    // Helpers for the slider<->spin pair.
    void linkDouble(QDoubleSpinBox* spin, QSlider* slider,
                    double lo, double hi);
    void linkInt   (QSpinBox*       spin, QSlider* slider,
                    int lo, int hi);
};

}  // namespace pid_qt

#endif
