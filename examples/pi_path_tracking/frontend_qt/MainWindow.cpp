// MainWindow.cpp — Qt6 GUI for the path-tracking demo.

#include "MainWindow.hpp"
#include "Widgets.hpp"
#include "track_core.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

namespace track_qt {

namespace {

QDoubleSpinBox* makeDbl(double lo, double hi, double val, double step,
                        int decimals)
{
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi); b->setSingleStep(step); b->setDecimals(decimals);
    b->setValue(val); b->setMinimumWidth(110);
    return b;
}

QVector<double> toQ(const std::vector<double>& v) {
    QVector<double> out; out.reserve(static_cast<int>(v.size()));
    for (double x : v) out.push_back(x);
    return out;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Path tracking PI (Qt6) — %1")
                       .arg(track_core_version()));
    resize(1320, 820);

    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(30);
    connect(debounce_, &QTimer::timeout, this, &MainWindow::runSimulation);

    buildUi();
    resetParametersToDefaults();
    runSimulation();
}

void MainWindow::buildUi() {
    auto* central = new QWidget; setCentralWidget(central);

    // ---- parameter panel ----
    auto* params = new QWidget;
    auto* pl = new QVBoxLayout(params);
    pl->setContentsMargins(6, 6, 6, 6); pl->setSpacing(6);

    // Plant
    auto* plant = new QGroupBox(QStringLiteral("Plant"));
    {
        auto* f = new QFormLayout(plant);
        m_   = makeDbl(0.001, 100.0, 0.1, 0.01, 4);
        izz_ = makeDbl(0.001, 100.0, 1.0, 0.05, 4);
        cp_  = makeDbl(0.0, 1000.0, 20.0, 1.0, 3);
        f->addRow(QStringLiteral("m"),               m_);
        f->addRow(QStringLiteral("izz"),             izz_);
        f->addRow(QStringLiteral("cornering_power"), cp_);
    }
    pl->addWidget(plant);

    // Sampling
    auto* samp = new QGroupBox(QStringLiteral("Sampling"));
    {
        auto* f = new QFormLayout(samp);
        h_  = makeDbl(1e-6, 0.1, 1e-4, 1e-5, 6);
        tc_ = makeDbl(1e-6, 1.0, 1e-3, 1e-4, 6);
        totalTime_   = makeDbl(0.01, 100.0, 0.90, 0.05, 4);
        targetSpeed_ = makeDbl(0.0, 50.0, 1.0, 0.05, 4);
        f->addRow(QStringLiteral("h [s]"),          h_);
        f->addRow(QStringLiteral("tc [s]"),         tc_);
        f->addRow(QStringLiteral("total_time [s]"), totalTime_);
        f->addRow(QStringLiteral("target_speed"),   targetSpeed_);
    }
    pl->addWidget(samp);

    // Gains
    auto* gains = new QGroupBox(QStringLiteral("Gains"));
    {
        auto* f = new QFormLayout(gains);
        kyp_       = makeDbl(0.0, 5000.0, 400.0, 5.0, 2);
        kyi_       = makeDbl(0.0, 5000.0, 0.0, 0.5, 2);
        kpsip_     = makeDbl(0.0, 5000.0, 200.0, 5.0, 2);
        kpsii_     = makeDbl(0.0, 5000.0, 0.0, 0.5, 2);
        krDamping_ = makeDbl(0.0, 1000.0, 20.0, 1.0, 2);
        f->addRow(QStringLiteral("ky_p"),       kyp_);
        f->addRow(QStringLiteral("ky_i"),       kyi_);
        f->addRow(QStringLiteral("kpsi_p"),     kpsip_);
        f->addRow(QStringLiteral("kpsi_i"),     kpsii_);
        f->addRow(QStringLiteral("kr_damping"), krDamping_);
    }
    pl->addWidget(gains);

    // Limits
    auto* lim = new QGroupBox(QStringLiteral("Limits"));
    {
        auto* f = new QFormLayout(lim);
        nLim_  = makeDbl(1.0, 100000.0, 500.0, 10.0, 2);
        fxLim_ = makeDbl(0.01, 1000.0, 5.0, 0.5, 2);
        iLim_  = makeDbl(0.0, 100.0, 0.2, 0.05, 4);
        lookahead_ = new QSpinBox;
        lookahead_->setRange(0, 1000); lookahead_->setValue(60);
        lookahead_->setMinimumWidth(110);
        f->addRow(QStringLiteral("n_moment_limit"),       nLim_);
        f->addRow(QStringLiteral("fx_limit"),             fxLim_);
        f->addRow(QStringLiteral("error_integral_limit"), iLim_);
        f->addRow(QStringLiteral("lookahead_index"),      lookahead_);
    }
    pl->addWidget(lim);

    // Scenario
    auto* sc = new QGroupBox(QStringLiteral("Scenario"));
    {
        auto* f = new QFormLayout(sc);
        y0Off_  = makeDbl(-1.0, 1.0, -0.03, 0.005, 4);
        hd0_    = makeDbl(-90.0, 90.0, 3.0, 0.1, 3);
        str1_   = makeDbl(0.0, 10.0, 0.30, 0.01, 4);
        radius_ = makeDbl(0.01, 10.0, 0.20, 0.01, 4);
        str2_   = makeDbl(0.0, 10.0, 0.30, 0.01, 4);
        ds_     = makeDbl(1e-4, 1.0, 0.002, 1e-4, 5);
        f->addRow(QStringLiteral("initial_y_offset"),    y0Off_);
        f->addRow(QStringLiteral("initial_heading_deg"), hd0_);
        f->addRow(QStringLiteral("straight1_len"),       str1_);
        f->addRow(QStringLiteral("radius"),              radius_);
        f->addRow(QStringLiteral("straight2_len"),       str2_);
        f->addRow(QStringLiteral("ds"),                  ds_);
    }
    pl->addWidget(sc);

    pl->addStretch(1);

    auto* scroll = new QScrollArea;
    scroll->setWidget(params); scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(380);

    // ---- centre: 4 tabs ----
    tabs_ = new QTabWidget;

    // Tab 1: XY map
    xyPlot_ = new XyPlot;
    tabs_->addTab(xyPlot_, QStringLiteral("XY"));

    // Tab 2: states (3x2 grid)
    auto* statesPage = new QWidget;
    auto* sg = new QGridLayout(statesPage);
    sg->setContentsMargins(4, 4, 4, 4); sg->setSpacing(4);
    uPlot_     = new LinePlot; uPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("u [m/s]"));
    vPlot_     = new LinePlot; vPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("v [m/s]"));
    rPlot_     = new LinePlot; rPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("r [rad/s]"));
    psiPlot_   = new LinePlot; psiPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("psi [rad]"));
    betaPlot_  = new LinePlot; betaPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("beta [rad]"));
    speedPlot_ = new LinePlot; speedPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("speed [m/s]"));
    sg->addWidget(uPlot_,     0, 0);
    sg->addWidget(vPlot_,     0, 1);
    sg->addWidget(rPlot_,     1, 0);
    sg->addWidget(psiPlot_,   1, 1);
    sg->addWidget(betaPlot_,  2, 0);
    sg->addWidget(speedPlot_, 2, 1);
    tabs_->addTab(statesPage, QStringLiteral("States"));

    // Tab 3: errors (3x1)
    auto* errorsPage = new QWidget;
    auto* eg = new QVBoxLayout(errorsPage);
    eg->setContentsMargins(4, 4, 4, 4); eg->setSpacing(4);
    pathErrPlot_ = new LinePlot; pathErrPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("Path error [m]"));
    eyPlot_      = new LinePlot; eyPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("e_y [m]"));
    epsiPlot_    = new LinePlot; epsiPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("e_psi [rad]"));
    eg->addWidget(pathErrPlot_); eg->addWidget(eyPlot_); eg->addWidget(epsiPlot_);
    tabs_->addTab(errorsPage, QStringLiteral("Errors"));

    // Tab 4: inputs (2x1)
    auto* inputsPage = new QWidget;
    auto* ig = new QVBoxLayout(inputsPage);
    ig->setContentsMargins(4, 4, 4, 4); ig->setSpacing(4);
    nPlot_  = new LinePlot; nPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("n_moment"));
    fxPlot_ = new LinePlot; fxPlot_->setAxisLabels(QStringLiteral("Time [s]"), QStringLiteral("Fx [N]"));
    ig->addWidget(nPlot_); ig->addWidget(fxPlot_);
    tabs_->addTab(inputsPage, QStringLiteral("Inputs"));

    // ---- bottom: metrics + buttons ----
    auto* bot = new QWidget; auto* bl = new QVBoxLayout(bot);
    bl->setContentsMargins(6, 6, 6, 6); bl->setSpacing(4);
    metrics_ = new QPlainTextEdit; metrics_->setReadOnly(true);
    metrics_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    metrics_->setMaximumHeight(160);
    bl->addWidget(metrics_);
    auto* btns = new QHBoxLayout;
    runBtn_   = new QPushButton(QStringLiteral("Run"));
    resetBtn_ = new QPushButton(QStringLiteral("Reset defaults"));
    saveBtn_  = new QPushButton(QStringLiteral("Save CSV…"));
    btns->addWidget(runBtn_); btns->addWidget(resetBtn_); btns->addWidget(saveBtn_);
    btns->addStretch(1);
    bl->addLayout(btns);

    // ---- assemble ----
    auto* split = new QSplitter(Qt::Horizontal);
    split->addWidget(scroll); split->addWidget(tabs_);
    split->setStretchFactor(0, 0); split->setStretchFactor(1, 1);
    split->setSizes({400, 920});

    auto* outer = new QVBoxLayout(central); outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(split, 1); outer->addWidget(bot);
    statusBar()->setSizeGripEnabled(false);

    // Wire up parameter changes -> debounce -> re-run.
    const QVector<QDoubleSpinBox*> dbls = {
        m_, izz_, cp_, h_, tc_, totalTime_, targetSpeed_,
        kyp_, kyi_, kpsip_, kpsii_, krDamping_,
        nLim_, fxLim_, iLim_, y0Off_, hd0_, str1_, radius_, str2_, ds_};
    for (auto* w : dbls) {
        connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::onAnyParamChanged);
    }
    connect(lookahead_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onAnyParamChanged);

    connect(runBtn_,   &QPushButton::clicked, this, &MainWindow::runSimulation);
    connect(resetBtn_, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(saveBtn_,  &QPushButton::clicked, this, &MainWindow::onSaveClicked);
}

void MainWindow::onAnyParamChanged() { debounce_->start(); }
void MainWindow::onResetClicked()    { resetParametersToDefaults(); runSimulation(); }

void MainWindow::resetParametersToDefaults() {
    TrackConfig cfg;
    track_core_default_config(&cfg);
    m_->setValue(cfg.m); izz_->setValue(cfg.izz); cp_->setValue(cfg.cornering_power);
    h_->setValue(cfg.h); tc_->setValue(cfg.tc);
    totalTime_->setValue(cfg.total_time);
    targetSpeed_->setValue(cfg.target_speed);
    kyp_->setValue(cfg.ky_p); kyi_->setValue(cfg.ky_i);
    kpsip_->setValue(cfg.kpsi_p); kpsii_->setValue(cfg.kpsi_i);
    krDamping_->setValue(cfg.kr_damping);
    nLim_->setValue(cfg.n_moment_limit); fxLim_->setValue(cfg.fx_limit);
    iLim_->setValue(cfg.error_integral_limit);
    lookahead_->setValue(cfg.lookahead_index);
    y0Off_->setValue(cfg.initial_y_offset);
    hd0_->setValue(cfg.initial_heading_deg);
    str1_->setValue(cfg.straight1_len); radius_->setValue(cfg.radius);
    str2_->setValue(cfg.straight2_len); ds_->setValue(cfg.ds);
}

TrackConfig MainWindow::collectConfig() const {
    TrackConfig cfg;
    cfg.m               = m_->value();
    cfg.izz             = izz_->value();
    cfg.cornering_power = cp_->value();
    cfg.h               = h_->value();
    cfg.tc              = tc_->value();
    cfg.total_time      = totalTime_->value();
    cfg.target_speed    = targetSpeed_->value();
    cfg.ky_p            = kyp_->value();
    cfg.ky_i            = kyi_->value();
    cfg.kpsi_p          = kpsip_->value();
    cfg.kpsi_i          = kpsii_->value();
    cfg.kr_damping      = krDamping_->value();
    cfg.n_moment_limit  = nLim_->value();
    cfg.fx_limit        = fxLim_->value();
    cfg.error_integral_limit = iLim_->value();
    cfg.lookahead_index = lookahead_->value();
    cfg.initial_y_offset = y0Off_->value();
    cfg.initial_heading_deg = hd0_->value();
    cfg.straight1_len   = str1_->value();
    cfg.radius          = radius_->value();
    cfg.straight2_len   = str2_->value();
    cfg.ds              = ds_->value();
    return cfg;
}

void MainWindow::runSimulation() {
    const TrackConfig cfg = collectConfig();

    TrackReferencePath* ref = track_core_make_reference(&cfg);
    TrackSimulation*    sim = track_core_simulate(&cfg);
    if (!ref || !sim) {
        if (ref) track_core_free_reference(ref);
        if (sim) track_core_free_simulation(sim);
        metrics_->setPlainText(QStringLiteral("Simulation failed — check parameters."));
        return;
    }

    const int nref = track_core_ref_length(ref);
    std::vector<double> rx(nref), ry(nref);
    track_core_ref_copy_x(ref, rx.data(), nref);
    track_core_ref_copy_y(ref, ry.data(), nref);

    const int n = track_core_sim_length(sim);
    auto fetch = [&](auto fn) {
        std::vector<double> v(n);
        fn(sim, v.data(), n);
        return v;
    };
    auto T   = fetch(track_core_sim_copy_time);
    auto X   = fetch(track_core_sim_copy_x);
    auto Y   = fetch(track_core_sim_copy_y);
    auto PSI = fetch(track_core_sim_copy_psi);
    auto U   = fetch(track_core_sim_copy_u);
    auto V   = fetch(track_core_sim_copy_v);
    auto R   = fetch(track_core_sim_copy_r);
    auto B   = fetch(track_core_sim_copy_beta);
    auto EY  = fetch(track_core_sim_copy_ey);
    auto EP  = fetch(track_core_sim_copy_epsi);
    auto NM  = fetch(track_core_sim_copy_nmoment);
    auto FX  = fetch(track_core_sim_copy_fx);
    auto XR  = fetch(track_core_sim_copy_x_ref);
    auto YR  = fetch(track_core_sim_copy_y_ref);
    auto PR  = fetch(track_core_sim_copy_psi_ref);

    std::vector<double> SPEED(n), PE(n);
    for (int i = 0; i < n; ++i) {
        SPEED[i] = std::sqrt(U[i] * U[i] + V[i] * V[i]);
        const double dx = X[i] - XR[i];
        const double dy = Y[i] - YR[i];
        PE[i] = std::sqrt(dx * dx + dy * dy);
    }

    const double m_pe_rms  = track_core_sim_path_error_rms(sim);
    const double m_pe_max  = track_core_sim_path_error_max(sim);
    const double m_ey_rms  = track_core_sim_ey_rms(sim);
    const double m_ey_max  = track_core_sim_ey_max_abs(sim);
    const double m_ep_rms  = track_core_sim_epsi_rms(sim);
    const double m_ep_max  = track_core_sim_epsi_max_abs(sim);
    const double m_nm_max  = track_core_sim_nmoment_max_abs(sim);
    track_core_free_reference(ref);
    track_core_free_simulation(sim);

    // ---- XY tab ----
    xyPlot_->setData(toQ(rx), toQ(ry), toQ(X), toQ(Y));

    // ---- States tab ----
    auto setSingle = [&](LinePlot* p, const std::vector<double>& xs,
                         const std::vector<double>& ys, const QColor& c)
    {
        QVector<LinePlot::Series> ser;
        ser.push_back(LinePlot::Series{QString(), c, toQ(xs), toQ(ys), false});
        p->setSeries(ser);
    };
    setSingle(uPlot_,     T, U,     QColor(31, 119, 180));
    setSingle(vPlot_,     T, V,     QColor(31, 119, 180));
    setSingle(rPlot_,     T, R,     QColor(31, 119, 180));
    {
        QVector<LinePlot::Series> ser;
        ser.push_back(LinePlot::Series{QStringLiteral("psi"),
                      QColor(31, 119, 180), toQ(T), toQ(PSI), false});
        ser.push_back(LinePlot::Series{QStringLiteral("psi_ref"),
                      QColor(214, 40, 40),  toQ(T), toQ(PR), true});
        psiPlot_->setSeries(ser);
    }
    setSingle(betaPlot_,  T, B,     QColor(31, 119, 180));
    setSingle(speedPlot_, T, SPEED, QColor(31, 119, 180));

    // ---- Errors tab ----
    setSingle(pathErrPlot_, T, PE, QColor(31, 119, 180));
    setSingle(eyPlot_,      T, EY, QColor(31, 119, 180));
    setSingle(epsiPlot_,    T, EP, QColor(31, 119, 180));

    // ---- Inputs tab ----
    setSingle(nPlot_,  T, NM, QColor(31, 119, 180));
    nPlot_->setHorizontalLines({ cfg.n_moment_limit, -cfg.n_moment_limit });
    setSingle(fxPlot_, T, FX, QColor(31, 119, 180));
    fxPlot_->setHorizontalLines({ cfg.fx_limit, -cfg.fx_limit });

    // ---- Metrics ----
    QString txt;
    QTextStream ts(&txt);
    ts.setRealNumberPrecision(6); ts.setRealNumberNotation(QTextStream::FixedNotation);
    ts << "samples              : " << n << "\n";
    ts << "reference points     : " << nref << "\n";
    ts << "path_error_rms [m]   : " << m_pe_rms << "\n";
    ts << "path_error_max [m]   : " << m_pe_max << "\n";
    ts << "e_y_rms        [m]   : " << m_ey_rms << "\n";
    ts << "e_y_max        [m]   : " << m_ey_max << "\n";
    ts << "e_psi_rms      [rad] : " << m_ep_rms << "\n";
    ts << "e_psi_max      [rad] : " << m_ep_max << "\n";
    ts << "max|n_moment|        : " << m_nm_max;
    metrics_->setPlainText(txt);
}

void MainWindow::onSaveClicked() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save log CSV"),
        QStringLiteral("tracking_log.csv"),
        QStringLiteral("CSV (*.csv);;All files (*)"));
    if (path.isEmpty()) return;

    const TrackConfig cfg = collectConfig();
    TrackSimulation* sim = track_core_simulate(&cfg);
    if (!sim) {
        QApplication::beep(); return;
    }
    const int n = track_core_sim_length(sim);
    std::vector<double> T(n), X(n), Y(n), PSI(n), U(n), V(n), R(n), B(n);
    std::vector<double> EY(n), EP(n), NM(n), FX(n), XR(n), YR(n), PR(n);
    track_core_sim_copy_time   (sim, T.data(), n);
    track_core_sim_copy_x      (sim, X.data(), n);
    track_core_sim_copy_y      (sim, Y.data(), n);
    track_core_sim_copy_psi    (sim, PSI.data(), n);
    track_core_sim_copy_u      (sim, U.data(), n);
    track_core_sim_copy_v      (sim, V.data(), n);
    track_core_sim_copy_r      (sim, R.data(), n);
    track_core_sim_copy_beta   (sim, B.data(), n);
    track_core_sim_copy_ey     (sim, EY.data(), n);
    track_core_sim_copy_epsi   (sim, EP.data(), n);
    track_core_sim_copy_nmoment(sim, NM.data(), n);
    track_core_sim_copy_fx     (sim, FX.data(), n);
    track_core_sim_copy_x_ref  (sim, XR.data(), n);
    track_core_sim_copy_y_ref  (sim, YR.data(), n);
    track_core_sim_copy_psi_ref(sim, PR.data(), n);
    track_core_free_simulation(sim);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "time_s,x_m,y_m,psi_rad,u_mps,v_mps,r_radps,beta_rad,"
        << "e_y_m,e_psi_rad,n_moment,fx,"
        << "x_ref_m,y_ref_m,psi_ref_rad,path_error_m\n";
    for (int i = 0; i < n; ++i) {
        const double dx = X[i] - XR[i];
        const double dy = Y[i] - YR[i];
        const double pe = std::sqrt(dx * dx + dy * dy);
        out << QString::number(T[i], 'g', 12) << ","
            << QString::number(X[i], 'g', 12) << ","
            << QString::number(Y[i], 'g', 12) << ","
            << QString::number(PSI[i], 'g', 12) << ","
            << QString::number(U[i], 'g', 12) << ","
            << QString::number(V[i], 'g', 12) << ","
            << QString::number(R[i], 'g', 12) << ","
            << QString::number(B[i], 'g', 12) << ","
            << QString::number(EY[i], 'g', 12) << ","
            << QString::number(EP[i], 'g', 12) << ","
            << QString::number(NM[i], 'g', 12) << ","
            << QString::number(FX[i], 'g', 12) << ","
            << QString::number(XR[i], 'g', 12) << ","
            << QString::number(YR[i], 'g', 12) << ","
            << QString::number(PR[i], 'g', 12) << ","
            << QString::number(pe, 'g', 12) << "\n";
    }
    statusBar()->showMessage(QStringLiteral("Saved: %1").arg(path), 5000);
}

}  // namespace track_qt
