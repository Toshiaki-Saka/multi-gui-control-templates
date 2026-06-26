// MainWindow.cpp — Qt6 GUI for the 2-DOF comparison demo.

#include "MainWindow.hpp"
#include "Widgets.hpp"
#include "tdof_core.h"

#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace tdof_qt {

namespace {

QDoubleSpinBox* makeDbl(double lo, double hi, double val, double step,
                        int decimals = 4)
{
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi); b->setSingleStep(step); b->setDecimals(decimals);
    b->setValue(val); b->setMinimumWidth(110);
    return b;
}

QString fmtTf(const char* name, const TdofConfig& cfg, int which) {
    int32_t nN = 32, nD = 32;
    std::vector<double> num(nN), den(nD);
    if (!tdof_core_get_tf(&cfg, which, num.data(), &nN, den.data(), &nD))
        return QStringLiteral("%1: <error>\n").arg(name);
    QString s = QStringLiteral("%1:\n  num [").arg(name);
    for (int i = 0; i < nN; ++i)
        s += QStringLiteral("%1%2").arg(i ? "  " : "").arg(num[i], 0, 'g', 4);
    s += QStringLiteral("]\n  den [");
    for (int i = 0; i < nD; ++i)
        s += QStringLiteral("%1%2").arg(i ? "  " : "").arg(den[i], 0, 'g', 4);
    s += QStringLiteral("]\n");
    return s;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("2-DOF control comparison (Qt6) — %1")
                       .arg(tdof_core_version()));
    resize(1320, 720);
    buildUi();
    resetParametersToDefaults();
    runSimulation();
}

void MainWindow::buildUi() {
    auto* central = new QWidget; setCentralWidget(central);

    // ---- left: parameters ----
    auto* left = new QWidget; auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(8, 8, 8, 8); ll->setSpacing(6);

    auto* pg = new QGroupBox(QStringLiteral("Plant  P(s)=1/(m s²+c s+k)"));
    auto* pf = new QFormLayout(pg);
    m_ = makeDbl(1e-4, 100.0, 0.01,  0.005);
    c_ = makeDbl(0.0,  100.0, 0.015, 0.005);
    k_ = makeDbl(0.0,  100.0, 1.0,   0.1);
    pf->addRow("m", m_); pf->addRow("c", c_); pf->addRow("k", k_);
    ll->addWidget(pg);

    auto* gg = new QGroupBox(QStringLiteral("PID gains"));
    auto* gf = new QFormLayout(gg);
    kp_ = makeDbl(0.0, 1000.0, 2.0,  0.1);
    ki_ = makeDbl(0.0, 1000.0, 10.0, 0.5);
    kd_ = makeDbl(0.0, 1000.0, 0.1,  0.01);
    gf->addRow("kp", kp_); gf->addRow("ki", ki_); gf->addRow("kd", kd_);
    ll->addWidget(gg);

    auto* sg = new QGroupBox(QStringLiteral("Scenario"));
    auto* sf = new QFormLayout(sg);
    ref_  = makeDbl(-1000.0, 1000.0, 10.0, 0.5);
    tEnd_ = makeDbl(0.1, 100.0, 2.0, 0.5);
    dt_   = makeDbl(1e-4, 1.0, 0.01, 0.005);
    sf->addRow("ref",   ref_);
    sf->addRow("t_end", tEnd_);
    sf->addRow("dt",    dt_);
    ll->addWidget(sg);

    auto* btnRow = new QHBoxLayout;
    runBtn_   = new QPushButton(QStringLiteral("Run"));
    resetBtn_ = new QPushButton(QStringLiteral("Reset defaults"));
    btnRow->addWidget(runBtn_); btnRow->addWidget(resetBtn_);
    ll->addLayout(btnRow);
    ll->addStretch(1);

    // ---- centre: plots ----
    auto* centre = new QWidget; auto* cl = new QVBoxLayout(centre);
    cl->setContentsMargins(8, 8, 8, 8); cl->setSpacing(6);
    refPlot_ = new LinePlot;
    refPlot_->setTitle(QStringLiteral("Reference Signal Comparison"));
    refPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("input"));
    cl->addWidget(refPlot_, 1);
    outPlot_ = new LinePlot;
    outPlot_->setTitle(QStringLiteral("Output Response Comparison"));
    outPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("output"));
    cl->addWidget(outPlot_, 1);

    // ---- right: TF + log ----
    auto* right = new QWidget; auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(8, 8, 8, 8); rl->setSpacing(6);
    auto mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    rl->addWidget(new QLabel(QStringLiteral("Transfer functions")));
    tfEdit_ = new QPlainTextEdit; tfEdit_->setReadOnly(true);
    tfEdit_->setFont(mono); tfEdit_->setMaximumHeight(240);
    rl->addWidget(tfEdit_);
    rl->addWidget(new QLabel(QStringLiteral("Metrics")));
    logEdit_ = new QPlainTextEdit; logEdit_->setReadOnly(true);
    logEdit_->setFont(mono);
    rl->addWidget(logEdit_, 1);

    // ---- assemble ----
    auto* split = new QSplitter(Qt::Horizontal);
    split->addWidget(left); split->addWidget(centre); split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 1);
    split->setSizes({320, 640, 360});

    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(split, 1);

    statusLbl_ = new QLabel(QStringLiteral("Ready"));
    statusBar()->addWidget(statusLbl_);

    connect(runBtn_,   &QPushButton::clicked, this, &MainWindow::onRunClicked);
    connect(resetBtn_, &QPushButton::clicked, this, &MainWindow::onResetClicked);
}

void MainWindow::resetParametersToDefaults() {
    TdofConfig cfg;
    tdof_core_default_config(&cfg);
    m_->setValue(cfg.m); c_->setValue(cfg.c); k_->setValue(cfg.k);
    kp_->setValue(cfg.kp); ki_->setValue(cfg.ki); kd_->setValue(cfg.kd);
    ref_->setValue(cfg.ref); tEnd_->setValue(cfg.t_end); dt_->setValue(cfg.dt);
}

TdofConfig MainWindow::collectConfig() const {
    TdofConfig cfg;
    cfg.m = m_->value(); cfg.c = c_->value(); cfg.k = k_->value();
    cfg.kp = kp_->value(); cfg.ki = ki_->value(); cfg.kd = kd_->value();
    cfg.ref = ref_->value(); cfg.t_end = tEnd_->value(); cfg.dt = dt_->value();
    return cfg;
}

void MainWindow::onRunClicked()   { runSimulation(); }
void MainWindow::onResetClicked() { resetParametersToDefaults(); runSimulation(); }

bool MainWindow::runSimulation() {
    const TdofConfig cfg = collectConfig();

    TdofSimulation* sim = tdof_core_simulate(&cfg);
    if (!sim) {
        statusLbl_->setText(QStringLiteral("Simulation failed (check parameters)."));
        return false;
    }

    const int n = tdof_core_sim_length(sim);
    std::vector<double> t(n), r(n), z(n), ypid(n), y2(n);
    tdof_core_sim_copy_time  (sim, t.data(),    n);
    tdof_core_sim_copy_r     (sim, r.data(),    n);
    tdof_core_sim_copy_z     (sim, z.data(),    n);
    tdof_core_sim_copy_y_pid (sim, ypid.data(), n);
    tdof_core_sim_copy_y_2dof(sim, y2.data(),   n);
    tdof_core_free_simulation(sim);

    QVector<double> tv(t.begin(), t.end());

    // Reference plot.
    {
        QVector<LinePlot::Series> series;
        series.push_back({QStringLiteral("Original Reference"),
                          QColor(31, 119, 180), tv,
                          QVector<double>(r.begin(), r.end())});
        series.push_back({QStringLiteral("Filtered Reference"),
                          QColor(255, 127, 14), tv,
                          QVector<double>(z.begin(), z.end())});
        refPlot_->setSeries(series);
    }
    // Output plot.
    {
        QVector<LinePlot::Series> series;
        series.push_back({QStringLiteral("PID"),
                          QColor(31, 119, 180), tv,
                          QVector<double>(ypid.begin(), ypid.end())});
        series.push_back({QStringLiteral("2DOF-like"),
                          QColor(255, 127, 14), tv,
                          QVector<double>(y2.begin(), y2.end())});
        outPlot_->setSeries(series);
    }

    // TF readout.
    QString tf;
    tf += fmtTf("Plant P",    cfg, 0);
    tf += fmtTf("PID K1",     cfg, 1);
    tf += fmtTf("Filter K2",  cfg, 2);
    tf += fmtTf("Closed Gyz", cfg, 3);
    tfEdit_->setPlainText(tf);

    // Metrics.
    double pidMax = *std::max_element(ypid.begin(), ypid.end());
    double twoMax = *std::max_element(y2.begin(), y2.end());
    const double ref = cfg.ref;
    QString log;
    log += QStringLiteral("samples         : %1\n").arg(n);
    log += QStringLiteral("y_pid  peak     : %1\n").arg(pidMax, 0, 'f', 4);
    log += QStringLiteral("y_pid  final    : %1\n").arg(ypid[n-1], 0, 'f', 4);
    log += QStringLiteral("y_pid  overshoot: %1%\n")
               .arg(100.0 * (pidMax / ref - 1.0), 0, 'f', 1);
    log += QStringLiteral("y_2dof peak     : %1\n").arg(twoMax, 0, 'f', 4);
    log += QStringLiteral("y_2dof final    : %1\n").arg(y2[n-1], 0, 'f', 4);
    log += QStringLiteral("y_2dof overshoot: %1%\n")
               .arg(100.0 * (twoMax / ref - 1.0), 0, 'f', 1);
    log += QStringLiteral("z final         : %1").arg(z[n-1], 0, 'f', 4);
    logEdit_->setPlainText(log);

    statusLbl_->setText(QStringLiteral("OK (%1 samples)").arg(n));
    return true;
}

}  // namespace tdof_qt
