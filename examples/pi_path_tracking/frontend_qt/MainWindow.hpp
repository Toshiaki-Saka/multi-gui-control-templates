// MainWindow.hpp — Qt6 GUI for the path-tracking demo.

#ifndef TRACK_QT_MAINWINDOW_HPP
#define TRACK_QT_MAINWINDOW_HPP

#include "track_core.h"

#include <QMainWindow>

class QDoubleSpinBox;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTimer;

namespace track_qt {

class XyPlot;
class LinePlot;

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
    TrackConfig collectConfig() const;

    // ---- parameter widgets ----
    QDoubleSpinBox* m_ = nullptr;
    QDoubleSpinBox* izz_ = nullptr;
    QDoubleSpinBox* cp_ = nullptr;
    QDoubleSpinBox* h_ = nullptr;
    QDoubleSpinBox* tc_ = nullptr;
    QDoubleSpinBox* totalTime_ = nullptr;
    QDoubleSpinBox* targetSpeed_ = nullptr;
    QDoubleSpinBox* kyp_ = nullptr;
    QDoubleSpinBox* kyi_ = nullptr;
    QDoubleSpinBox* kpsip_ = nullptr;
    QDoubleSpinBox* kpsii_ = nullptr;
    QDoubleSpinBox* krDamping_ = nullptr;
    QDoubleSpinBox* nLim_ = nullptr;
    QDoubleSpinBox* fxLim_ = nullptr;
    QDoubleSpinBox* iLim_ = nullptr;
    QSpinBox*       lookahead_ = nullptr;
    QDoubleSpinBox* y0Off_ = nullptr;
    QDoubleSpinBox* hd0_ = nullptr;
    QDoubleSpinBox* str1_ = nullptr;
    QDoubleSpinBox* radius_ = nullptr;
    QDoubleSpinBox* str2_ = nullptr;
    QDoubleSpinBox* ds_ = nullptr;

    QPushButton* runBtn_   = nullptr;
    QPushButton* resetBtn_ = nullptr;
    QPushButton* saveBtn_  = nullptr;

    // ---- plot widgets ----
    XyPlot*   xyPlot_   = nullptr;
    LinePlot* uPlot_    = nullptr;
    LinePlot* vPlot_    = nullptr;
    LinePlot* rPlot_    = nullptr;
    LinePlot* psiPlot_  = nullptr;
    LinePlot* betaPlot_ = nullptr;
    LinePlot* speedPlot_ = nullptr;
    LinePlot* pathErrPlot_ = nullptr;
    LinePlot* eyPlot_   = nullptr;
    LinePlot* epsiPlot_ = nullptr;
    LinePlot* nPlot_    = nullptr;
    LinePlot* fxPlot_   = nullptr;

    QTabWidget*     tabs_    = nullptr;
    QPlainTextEdit* metrics_ = nullptr;
    QTimer*         debounce_ = nullptr;
};

}  // namespace track_qt

#endif
