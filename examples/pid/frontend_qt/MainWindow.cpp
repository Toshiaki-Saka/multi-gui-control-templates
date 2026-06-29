// MainWindow.cpp — Qt6 GUI for the interactive PID demo.

#include "MainWindow.hpp"
#include "Widgets.hpp"
#include "pid_core.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

namespace pid_qt {

namespace {

constexpr int kSliderResolution = 1000;

int dblToSlider(double v, double lo, double hi) {
    if (hi <= lo) return 0;
    const double f = (v - lo) / (hi - lo);
    const double c = std::max(0.0, std::min(1.0, f));
    return static_cast<int>(std::round(c * kSliderResolution));
}
double sliderToDbl(int s, double lo, double hi) {
    return lo + (s / static_cast<double>(kSliderResolution)) * (hi - lo);
}

QDoubleSpinBox* makeDbl(double lo, double hi, double val, double step,
                        int decimals)
{
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi); b->setSingleStep(step); b->setDecimals(decimals);
    b->setValue(val); b->setMinimumWidth(100);
    return b;
}
QSlider* makeSlider(int initSlider) {
    auto* s = new QSlider(Qt::Horizontal);
    s->setRange(0, kSliderResolution);
    s->setValue(initSlider);
    return s;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("PID interactive (Qt6) — %1")
                       .arg(pid_core_version()));
    resize(1100, 720);

    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(5);
    connect(debounce_, &QTimer::timeout, this, &MainWindow::runSimulation);

    buildUi();
    resetParametersToDefaults();
    runSimulation();
}

void MainWindow::buildUi() {
    auto* central = new QWidget; setCentralWidget(central);

    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(6, 6, 6, 6); outer->setSpacing(6);

    plot_ = new ResponsePlot;
    outer->addWidget(plot_, 1);

    // Slider grid: [label | slider | spin] x 7 rows.
    auto* grid = new QGridLayout;
    grid->setContentsMargins(8, 4, 8, 4);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(4);

    auto addRow = [&](int row, const QString& name,
                      QSlider* slider, QWidget* spin) {
        auto* lbl = new QLabel(name);
        lbl->setMinimumWidth(100);
        grid->addWidget(lbl,    row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(spin,   row, 2);
    };

    // theta_start ∈ [0, 359]
    thetaStart_  = makeDbl(0.0, 359.0, 0.0, 0.1, 1);
    sThetaStart_ = makeSlider(dblToSlider(0.0, 0.0, 359.0));
    addRow(0, "theta_start", sThetaStart_, thetaStart_);

    // theta_goal ∈ [0, 359]
    thetaGoal_   = makeDbl(0.0, 359.0, 90.0, 0.1, 1);
    sThetaGoal_  = makeSlider(dblToSlider(90.0, 0.0, 359.0));
    addRow(1, "theta_goal",  sThetaGoal_,  thetaGoal_);

    // offset ∈ [0, 100], step 0.01
    offset_  = makeDbl(0.0, 100.0, 0.0, 0.01, 2);
    sOffset_ = makeSlider(dblToSlider(0.0, 0.0, 100.0));
    addRow(2, "offset",      sOffset_,     offset_);

    // time_length ∈ [10, 2000], int
    timeLength_  = new QSpinBox;
    timeLength_->setRange(10, 2000); timeLength_->setValue(150);
    timeLength_->setMinimumWidth(100);
    sTimeLength_ = makeSlider(static_cast<int>(
        std::round((150.0 - 10.0) / (2000.0 - 10.0) * kSliderResolution)));
    addRow(3, "time_length", sTimeLength_, timeLength_);

    // kp, ki, kd ∈ [0, 1.5], step 0.001
    kp_  = makeDbl(0.0, 1.5, 0.10, 0.001, 3);
    sKp_ = makeSlider(dblToSlider(0.10, 0.0, 1.5));
    addRow(4, "kp", sKp_, kp_);
    ki_  = makeDbl(0.0, 1.5, 0.01, 0.001, 3);
    sKi_ = makeSlider(dblToSlider(0.01, 0.0, 1.5));
    addRow(5, "ki", sKi_, ki_);
    kd_  = makeDbl(0.0, 1.5, 0.20, 0.001, 3);
    sKd_ = makeSlider(dblToSlider(0.20, 0.0, 1.5));
    addRow(6, "kd", sKd_, kd_);

    // dt ∈ (0, 10], step 0.01
    dt_  = makeDbl(0.01, 10.0, 1.0, 0.01, 2);
    sDt_ = makeSlider(dblToSlider(1.0, 0.01, 10.0));
    addRow(7, "dt", sDt_, dt_);

    // integral_clamp ∈ [0, 5000], step 10 (0 = off)
    integralClamp_  = makeDbl(0.0, 5000.0, 0.0, 10.0, 0);
    sIntegralClamp_ = makeSlider(dblToSlider(0.0, 0.0, 5000.0));
    addRow(8, "i_clamp (0=off)", sIntegralClamp_, integralClamp_);

    // output_clamp ∈ [0, 100], step 0.5 (0 = off)
    outputClamp_  = makeDbl(0.0, 100.0, 0.0, 0.5, 1);
    sOutputClamp_ = makeSlider(dblToSlider(0.0, 0.0, 100.0));
    addRow(9, "o_clamp (0=off)", sOutputClamp_, outputClamp_);

    grid->setColumnStretch(1, 1);
    outer->addLayout(grid);

    // Button row.
    auto* btnRow = new QHBoxLayout;
    resetBtn_ = new QPushButton(QStringLiteral("Reset defaults"));
    saveBtn_  = new QPushButton(QStringLiteral("Save plot…"));
    btnRow->addWidget(resetBtn_); btnRow->addWidget(saveBtn_);
    btnRow->addStretch(1);
    status_ = new QLabel(QStringLiteral("Ready"));
    status_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    btnRow->addWidget(status_);
    outer->addLayout(btnRow);

    statusBar()->setSizeGripEnabled(false);

    // Two-way sync for each slider/spin pair.
    linkDouble(thetaStart_,  sThetaStart_, 0.0,   359.0);
    linkDouble(thetaGoal_,   sThetaGoal_,  0.0,   359.0);
    linkDouble(offset_,      sOffset_,     0.0,   100.0);
    linkInt   (timeLength_,  sTimeLength_, 10,    2000);
    linkDouble(kp_,          sKp_,         0.0,   1.5);
    linkDouble(ki_,          sKi_,         0.0,   1.5);
    linkDouble(kd_,          sKd_,         0.0,   1.5);
    linkDouble(dt_,          sDt_,         0.01,  10.0);
    linkDouble(integralClamp_, sIntegralClamp_, 0.0, 5000.0);
    linkDouble(outputClamp_,   sOutputClamp_,   0.0, 100.0);

    connect(resetBtn_, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(saveBtn_,  &QPushButton::clicked, this, &MainWindow::onSaveClicked);
}

void MainWindow::linkDouble(QDoubleSpinBox* spin, QSlider* slider,
                            double lo, double hi)
{
    connect(slider, &QSlider::valueChanged, this, [this, spin, lo, hi](int s) {
        if (syncing_) return;
        syncing_ = true;
        spin->setValue(sliderToDbl(s, lo, hi));
        syncing_ = false;
        onAnyParamChanged();
    });
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this, slider, lo, hi](double v) {
        if (syncing_) return;
        syncing_ = true;
        slider->setValue(dblToSlider(v, lo, hi));
        syncing_ = false;
        onAnyParamChanged();
    });
}

void MainWindow::linkInt(QSpinBox* spin, QSlider* slider, int lo, int hi) {
    connect(slider, &QSlider::valueChanged, this, [this, spin, lo, hi](int s) {
        if (syncing_) return;
        syncing_ = true;
        const double v = sliderToDbl(s, lo, hi);
        spin->setValue(static_cast<int>(std::round(v)));
        syncing_ = false;
        onAnyParamChanged();
    });
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this, slider, lo, hi](int v) {
        if (syncing_) return;
        syncing_ = true;
        slider->setValue(dblToSlider(static_cast<double>(v), lo, hi));
        syncing_ = false;
        onAnyParamChanged();
    });
}

void MainWindow::resetParametersToDefaults() {
    PidConfig cfg;
    pid_core_default_config(&cfg);
    syncing_ = true;
    thetaStart_->setValue(cfg.theta_start);
    thetaGoal_ ->setValue(cfg.theta_goal);
    offset_    ->setValue(cfg.offset);
    timeLength_->setValue(cfg.time_length);
    kp_->setValue(cfg.kp);
    ki_->setValue(cfg.ki);
    kd_->setValue(cfg.kd);
    dt_->setValue(cfg.dt);
    integralClamp_->setValue(cfg.integral_clamp);
    outputClamp_  ->setValue(cfg.output_clamp);
    // Sync sliders explicitly (their value-changed signals were
    // suppressed while syncing_ was true).
    sThetaStart_->setValue(dblToSlider(cfg.theta_start, 0.0, 359.0));
    sThetaGoal_ ->setValue(dblToSlider(cfg.theta_goal,  0.0, 359.0));
    sOffset_    ->setValue(dblToSlider(cfg.offset,      0.0, 100.0));
    sTimeLength_->setValue(dblToSlider(
        static_cast<double>(cfg.time_length), 10.0, 2000.0));
    sKp_->setValue(dblToSlider(cfg.kp, 0.0, 1.5));
    sKi_->setValue(dblToSlider(cfg.ki, 0.0, 1.5));
    sKd_->setValue(dblToSlider(cfg.kd, 0.0, 1.5));
    sDt_->setValue(dblToSlider(cfg.dt, 0.01, 10.0));
    sIntegralClamp_->setValue(dblToSlider(cfg.integral_clamp, 0.0, 5000.0));
    sOutputClamp_  ->setValue(dblToSlider(cfg.output_clamp,   0.0, 100.0));
    syncing_ = false;
}

void MainWindow::onAnyParamChanged() { debounce_->start(); }
void MainWindow::onResetClicked()    { resetParametersToDefaults(); runSimulation(); }

void MainWindow::onSaveClicked() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save plot"),
        QStringLiteral("pid_response.png"),
        QStringLiteral("PNG (*.png);;All files (*)"));
    if (path.isEmpty()) return;
    QPixmap pm = plot_->grab();
    if (pm.save(path)) {
        status_->setText(QStringLiteral("Saved: %1").arg(path));
    } else {
        status_->setText(QStringLiteral("Save failed."));
    }
}

PidConfig MainWindow::collectConfig() const {
    PidConfig cfg;
    cfg.theta_start = thetaStart_->value();
    cfg.theta_goal  = thetaGoal_ ->value();
    cfg.offset      = offset_    ->value();
    cfg.time_length = timeLength_->value();
    cfg.kp = kp_->value();
    cfg.ki = ki_->value();
    cfg.kd = kd_->value();
    cfg.dt             = dt_->value();
    cfg.integral_clamp = integralClamp_->value();
    cfg.output_clamp   = outputClamp_  ->value();
    return cfg;
}

void MainWindow::runSimulation() {
    const PidConfig cfg = collectConfig();
    PidSimulation* sim = pid_core_simulate(&cfg);
    if (!sim) {
        status_->setText(QStringLiteral("Simulation failed."));
        return;
    }
    const int n = pid_core_sim_length(sim);
    std::vector<double> t(n), th(n);
    pid_core_sim_copy_time (sim, t.data(),  n);
    pid_core_sim_copy_theta(sim, th.data(), n);
    const double fin = pid_core_sim_final_theta(sim);
    const double mx  = pid_core_sim_max_theta(sim);
    const double mn  = pid_core_sim_min_theta(sim);
    pid_core_free_simulation(sim);

    QVector<double> ts(t.begin(), t.end());
    QVector<double> ths(th.begin(), th.end());
    plot_->setData(ts, ths, cfg.theta_goal, fin,
                   static_cast<double>(cfg.time_length) * cfg.dt);

    status_->setText(QStringLiteral(
        "final=%1  max=%2  min=%3")
        .arg(fin, 7, 'f', 3).arg(mx, 7, 'f', 3).arg(mn, 7, 'f', 3));
}

}  // namespace pid_qt
