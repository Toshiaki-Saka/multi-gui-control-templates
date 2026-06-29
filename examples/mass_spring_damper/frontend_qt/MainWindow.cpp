// MainWindow.cpp — Qt6 GUI for the MSD parameter-sweep demo.

#include "MainWindow.hpp"
#include "Widgets.hpp"
#include "msd_core.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

namespace msd_qt {

namespace {

QDoubleSpinBox* makeDbl(double lo, double hi, double val, double step,
                        int decimals)
{
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi); b->setSingleStep(step); b->setDecimals(decimals);
    b->setValue(val); b->setMinimumWidth(110);
    return b;
}

QString makeLabel(const QString& name, const MsdCase& c) {
    double wn, zeta;
    msd_core_derived(&c, &wn, &zeta);
    (void)name;
    return QStringLiteral("m=%1, c=%2, k=%3, F=%4sin(%5t), ωn=%6, ζ=%7")
        .arg(c.m, 0, 'g', 4).arg(c.c, 0, 'g', 4).arg(c.k, 0, 'g', 4)
        .arg(c.force_amplitude, 0, 'g', 4).arg(c.force_omega, 0, 'g', 4)
        .arg(wn, 0, 'f', 2).arg(zeta, 0, 'f', 2);
}

// matplotlib's default colour cycle, so the C++ frontend matches the
// Python reference at a glance.
const QColor kCycle[] = {
    QColor("#1f77b4"), QColor("#ff7f0e"), QColor("#2ca02c"),
    QColor("#d62728"), QColor("#9467bd"), QColor("#8c564b"),
    QColor("#e377c2"), QColor("#7f7f7f"), QColor("#bcbd22"),
    QColor("#17becf"),
};

CaseRow defaultRow(const char* name, double m, double c, double k,
                   double F, double w)
{
    CaseRow r;
    r.name = QString::fromUtf8(name);
    MsdCase cfg;
    msd_core_default_case(&cfg);
    cfg.m = m; cfg.c = c; cfg.k = k;
    cfg.force_amplitude = F; cfg.force_omega = w;
    r.cfg = cfg;
    r.enabled = true;
    return r;
}

QVector<CaseRow> defaultCases() {
    QVector<CaseRow> out;
    out.push_back(defaultRow("baseline",               1.0, 2.0,  5.0, 0.5, 2.0));
    out.push_back(defaultRow("low damping",            1.0, 0.5,  5.0, 0.5, 2.0));
    out.push_back(defaultRow("high damping",           1.0, 5.0,  5.0, 0.5, 2.0));
    out.push_back(defaultRow("stiffer spring",         1.0, 2.0, 12.0, 0.5, 2.0));
    out.push_back(defaultRow("near natural frequency", 1.0, 0.5,  5.0, 0.5, 2.2));
    return out;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("MSD forced response (Qt6) — %1")
                       .arg(msd_core_version()));
    resize(1320, 800);

    cases_ = defaultCases();

    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(50);
    connect(debounce_, &QTimer::timeout, this, &MainWindow::onRun);

    buildUi();
    refreshCaseList(0);
    onRun();
}

void MainWindow::buildUi() {
    auto* central = new QWidget; setCentralWidget(central);

    // ===== left: case list =====
    auto* leftW = new QWidget;
    auto* ll = new QVBoxLayout(leftW);
    ll->setContentsMargins(6, 6, 6, 6); ll->setSpacing(4);
    ll->addWidget(new QLabel(QStringLiteral("Cases")));
    list_ = new QListWidget;
    ll->addWidget(list_, 1);
    auto* btnRow = new QHBoxLayout;
    addBtn_ = new QPushButton(QStringLiteral("Add"));
    dupBtn_ = new QPushButton(QStringLiteral("Duplicate"));
    rmBtn_  = new QPushButton(QStringLiteral("Remove"));
    resetBtn_ = new QPushButton(QStringLiteral("Reset"));
    btnRow->addWidget(addBtn_); btnRow->addWidget(dupBtn_);
    btnRow->addWidget(rmBtn_); btnRow->addWidget(resetBtn_);
    ll->addLayout(btnRow);

    connect(list_, &QListWidget::currentRowChanged, this, &MainWindow::onCaseSelected);
    connect(list_, &QListWidget::itemChanged, this, &MainWindow::onCaseChecked);
    connect(addBtn_,  &QPushButton::clicked, this, &MainWindow::onAddCase);
    connect(dupBtn_,  &QPushButton::clicked, this, &MainWindow::onDuplicateCase);
    connect(rmBtn_,   &QPushButton::clicked, this, &MainWindow::onRemoveCase);
    connect(resetBtn_, &QPushButton::clicked, this, &MainWindow::onResetCases);

    // ===== centre: editor =====
    auto* midW = new QWidget;
    auto* ml = new QVBoxLayout(midW);
    ml->setContentsMargins(6, 6, 6, 6); ml->setSpacing(6);

    auto* nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(QStringLiteral("Name")));
    nameEdit_ = new QComboBox; nameEdit_->setEditable(true);
    nameRow->addWidget(nameEdit_, 1);
    ml->addLayout(nameRow);

    auto* plant = new QGroupBox(QStringLiteral("Plant"));
    {
        auto* f = new QFormLayout(plant);
        edM_ = makeDbl(1e-4, 1000.0, 1.0, 0.05, 4);
        edC_ = makeDbl(0.0,  1000.0, 2.0, 0.05, 4);
        edK_ = makeDbl(0.0,  1e6,    5.0, 0.1,  4);
        f->addRow(QStringLiteral("m [kg]"),    edM_);
        f->addRow(QStringLiteral("c [N·s/m]"), edC_);
        f->addRow(QStringLiteral("k [N/m]"),   edK_);
    }
    ml->addWidget(plant);

    auto* force = new QGroupBox(QStringLiteral("External force f(t)=F·sin(ω·t)"));
    {
        auto* f = new QFormLayout(force);
        edF_ = makeDbl(-1000.0, 1000.0, 0.5, 0.05, 4);
        edW_ = makeDbl(0.0,    1000.0, 2.0, 0.05, 4);
        f->addRow(QStringLiteral("F"), edF_);
        f->addRow(QStringLiteral("ω"), edW_);
    }
    ml->addWidget(force);

    auto* init = new QGroupBox(QStringLiteral("Initial state"));
    {
        auto* f = new QFormLayout(init);
        edX0_ = makeDbl(-100.0, 100.0, 0.0, 0.01, 4);
        edV0_ = makeDbl(-100.0, 100.0, 0.0, 0.01, 4);
        f->addRow(QStringLiteral("x0"), edX0_);
        f->addRow(QStringLiteral("v0"), edV0_);
    }
    ml->addWidget(init);

    auto* derived = new QGroupBox(QStringLiteral("Derived"));
    {
        auto* f = new QFormLayout(derived);
        lblWn_   = new QLabel(QStringLiteral("—"));
        lblZeta_ = new QLabel(QStringLiteral("—"));
        lblXend_ = new QLabel(QStringLiteral("—"));
        lblVend_ = new QLabel(QStringLiteral("—"));
        lblXmax_ = new QLabel(QStringLiteral("—"));
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        for (auto* w : {lblWn_, lblZeta_, lblXend_, lblVend_, lblXmax_})
            w->setFont(mono);
        f->addRow(QStringLiteral("ωn [rad/s]"),     lblWn_);
        f->addRow(QStringLiteral("ζ"),              lblZeta_);
        f->addRow(QStringLiteral("x(final) [m]"),   lblXend_);
        f->addRow(QStringLiteral("v(final) [m/s]"), lblVend_);
        f->addRow(QStringLiteral("max |x| [m]"),    lblXmax_);
    }
    ml->addWidget(derived);
    ml->addStretch(1);

    for (auto* w : {edM_, edC_, edK_, edF_, edW_, edX0_, edV0_})
        connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::onEditorChanged);
    connect(nameEdit_, &QComboBox::editTextChanged, this, &MainWindow::onNameChanged);

    // ===== right: plot + sampling =====
    auto* rightW = new QWidget;
    auto* rl = new QVBoxLayout(rightW);
    rl->setContentsMargins(6, 6, 6, 6); rl->setSpacing(4);
    plot_ = new OverlayPlot;
    plot_->setTitle(QStringLiteral("Mass-Spring-Damper Forced Response — Parameter Sweep"));
    plot_->setAxisLabels(QStringLiteral("t [s]"), QStringLiteral("x [m]"));
    rl->addWidget(plot_, 1);

    auto* botRow = new QHBoxLayout;
    botRow->addWidget(new QLabel(QStringLiteral("dt [s]")));
    edDt_ = makeDbl(1e-6, 1.0, 0.001, 0.0005, 6);
    botRow->addWidget(edDt_);
    botRow->addWidget(new QLabel(QStringLiteral("stop [s]")));
    edStop_ = makeDbl(0.1, 1000.0, 10.0, 0.5, 3);
    botRow->addWidget(edStop_);
    cbForce_ = new QCheckBox(QStringLiteral("show force (dotted)"));
    botRow->addWidget(cbForce_);
    botRow->addStretch(1);
    runBtn_     = new QPushButton(QStringLiteral("Run"));
    saveCsvBtn_ = new QPushButton(QStringLiteral("Save CSV…"));
    botRow->addWidget(runBtn_); botRow->addWidget(saveCsvBtn_);
    rl->addLayout(botRow);

    connect(edDt_,   QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double){ debounce_->start(); });
    connect(edStop_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double){ debounce_->start(); });
    connect(cbForce_, &QCheckBox::toggled, this, [this](bool){ refreshPlot(); });
    connect(runBtn_, &QPushButton::clicked, this, &MainWindow::onRun);
    connect(saveCsvBtn_, &QPushButton::clicked, this, &MainWindow::onSaveCsv);

    // ===== assemble =====
    auto* split = new QSplitter(Qt::Horizontal);
    split->addWidget(leftW); split->addWidget(midW); split->addWidget(rightW);
    split->setSizes({260, 320, 740});
    split->setStretchFactor(0, 0); split->setStretchFactor(1, 0); split->setStretchFactor(2, 1);

    auto* outer = new QVBoxLayout(central); outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(split, 1);

    statusLbl_ = new QLabel(QStringLiteral("Ready"));
    statusLbl_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    statusBar()->addWidget(statusLbl_);
}

// ---- case-list management ----
void MainWindow::refreshCaseList(int selectIndex) {
    list_->blockSignals(true);
    list_->clear();
    for (int i = 0; i < cases_.size(); ++i) {
        auto* it = new QListWidgetItem(cases_[i].name);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(cases_[i].enabled ? Qt::Checked : Qt::Unchecked);
        list_->addItem(it);
    }
    list_->blockSignals(false);
    if (selectIndex >= 0 && selectIndex < list_->count())
        list_->setCurrentRow(selectIndex);
    else if (list_->count() > 0)
        list_->setCurrentRow(0);
}

void MainWindow::onCaseSelected(int row) {
    loadCaseIntoEditor(row);
}

void MainWindow::onCaseChecked(QListWidgetItem* item) {
    const int row = list_->row(item);
    if (row >= 0 && row < cases_.size()) {
        cases_[row].enabled = (item->checkState() == Qt::Checked);
        refreshPlot();
    }
}

void MainWindow::loadCaseIntoEditor(int row) {
    if (row < 0 || row >= cases_.size()) return;
    const CaseRow& r = cases_[row];
    loadingEditor_ = true;
    nameEdit_->setEditText(r.name);
    edM_->setValue(r.cfg.m); edC_->setValue(r.cfg.c); edK_->setValue(r.cfg.k);
    edF_->setValue(r.cfg.force_amplitude); edW_->setValue(r.cfg.force_omega);
    edX0_->setValue(r.cfg.x0); edV0_->setValue(r.cfg.v0);
    loadingEditor_ = false;
    updateDerivedLabels(row);
}

void MainWindow::onEditorChanged() {
    if (loadingEditor_) return;
    const int row = list_->currentRow();
    if (row < 0 || row >= cases_.size()) return;
    CaseRow& r = cases_[row];
    r.cfg.m = edM_->value(); r.cfg.c = edC_->value(); r.cfg.k = edK_->value();
    r.cfg.force_amplitude = edF_->value(); r.cfg.force_omega = edW_->value();
    r.cfg.x0 = edX0_->value(); r.cfg.v0 = edV0_->value();
    debounce_->start();
}

void MainWindow::onNameChanged(const QString& text) {
    if (loadingEditor_) return;
    const int row = list_->currentRow();
    if (row < 0 || row >= cases_.size()) return;
    cases_[row].name = text;
    if (auto* it = list_->item(row)) {
        list_->blockSignals(true); it->setText(text); list_->blockSignals(false);
    }
    refreshPlot();
}

void MainWindow::onAddCase() {
    cases_.push_back(defaultRow(
        QStringLiteral("case %1").arg(cases_.size() + 1).toUtf8().constData(),
        1.0, 2.0, 5.0, 0.5, 2.0));
    refreshCaseList(cases_.size() - 1);
    onRun();
}

void MainWindow::onDuplicateCase() {
    const int row = list_->currentRow();
    if (row < 0 || row >= cases_.size()) return;
    CaseRow dup = cases_[row];
    dup.name = cases_[row].name + QStringLiteral(" (copy)");
    cases_.insert(row + 1, dup);
    refreshCaseList(row + 1);
    onRun();
}

void MainWindow::onRemoveCase() {
    const int row = list_->currentRow();
    if (row < 0 || cases_.size() <= 1) return;
    cases_.removeAt(row);
    refreshCaseList(std::min(row, static_cast<int>(cases_.size()) - 1));
    refreshPlot();
}

void MainWindow::onResetCases() {
    cases_ = defaultCases();
    refreshCaseList(0);
    onRun();
}

// ---- simulation ----
void MainWindow::onRun() {
    MsdSamplingConfig sp;
    sp.dt   = edDt_->value();
    sp.stop = edStop_->value();

    simT_.clear(); simX_.clear(); simV_.clear(); simForce_.clear();
    simFinalX_.clear(); simFinalV_.clear(); simMaxAbsX_.clear();
    simT_.reserve(cases_.size()); simX_.reserve(cases_.size());
    simV_.reserve(cases_.size()); simForce_.reserve(cases_.size());

    int n_samples = 0;
    int failures = 0;
    for (const auto& r : cases_) {
        MsdSimulation* sim = msd_core_simulate(&r.cfg, &sp);
        if (!sim) {
            simT_.push_back({}); simX_.push_back({}); simV_.push_back({});
            simForce_.push_back({});
            simFinalX_.push_back(0); simFinalV_.push_back(0); simMaxAbsX_.push_back(0);
            ++failures;
            continue;
        }
        const int n = msd_core_sim_length(sim);
        n_samples = n;
        std::vector<double> t(n), x(n), v(n), force(n);
        msd_core_sim_copy_time    (sim, t.data(),     n);
        msd_core_sim_copy_position(sim, x.data(),     n);
        msd_core_sim_copy_velocity(sim, v.data(),     n);
        msd_core_sim_copy_force   (sim, force.data(), n);
        QVector<double> qt, qx, qv, qf;
        qt.reserve(n); qx.reserve(n); qv.reserve(n); qf.reserve(n);
        for (int i = 0; i < n; ++i) { qt.push_back(t[i]); qx.push_back(x[i]); qv.push_back(v[i]); qf.push_back(force[i]); }
        simT_.push_back(qt); simX_.push_back(qx); simV_.push_back(qv); simForce_.push_back(qf);
        simFinalX_.push_back(msd_core_sim_final_position(sim));
        simFinalV_.push_back(msd_core_sim_final_velocity(sim));
        simMaxAbsX_.push_back(msd_core_sim_max_abs_position(sim));
        msd_core_free_simulation(sim);
    }

    if (failures > 0)
        statusLbl_->setText(QStringLiteral("Simulation failed for %1 case(s)").arg(failures));
    else
        statusLbl_->setText(QStringLiteral("OK — %1 cases, %2 samples each")
                                .arg(cases_.size()).arg(n_samples));

    updateDerivedLabels(list_->currentRow());
    refreshPlot();
}

void MainWindow::updateDerivedLabels(int row) {
    if (row < 0 || row >= cases_.size()) return;
    double wn, zeta;
    msd_core_derived(&cases_[row].cfg, &wn, &zeta);
    lblWn_  ->setText(QString::number(wn, 'f', 4));
    lblZeta_->setText(QString::number(zeta, 'f', 4));
    if (row < simFinalX_.size() && !simX_[row].isEmpty()) {
        lblXend_->setText(QString::asprintf("%+.6f", simFinalX_[row]));
        lblVend_->setText(QString::asprintf("%+.6f", simFinalV_[row]));
        lblXmax_->setText(QString::asprintf("%.6f",  simMaxAbsX_[row]));
    } else {
        lblXend_->setText(QStringLiteral("—"));
        lblVend_->setText(QStringLiteral("—"));
        lblXmax_->setText(QStringLiteral("—"));
    }
}

void MainWindow::refreshPlot() {
    QVector<OverlayPlot::Series> series;
    const bool showForce = cbForce_->isChecked();
    for (int i = 0; i < cases_.size(); ++i) {
        if (!cases_[i].enabled || i >= simX_.size() || simX_[i].isEmpty()) continue;
        const QColor col = kCycle[i % (int)(sizeof(kCycle) / sizeof(kCycle[0]))];
        series.push_back(OverlayPlot::Series{
            makeLabel(cases_[i].name, cases_[i].cfg), col,
            simT_[i], simX_[i], false});
        if (showForce) {
            series.push_back(OverlayPlot::Series{
                QStringLiteral("force (%1)").arg(cases_[i].name), col,
                simT_[i], simForce_[i], true});
        }
    }
    plot_->setSeries(series, edStop_->value());
}

void MainWindow::onSaveCsv() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save sweep CSV"),
        QStringLiteral("msd_sweep.csv"),
        QStringLiteral("CSV (*.csv);;All files (*)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&f);

    // header
    out << "time_s";
    for (const auto& r : cases_) {
        const QString safe = QString(r.name).replace(QChar(','), QChar('_'));
        out << "," << safe << "__x," << safe << "__v," << safe << "__f";
    }
    out << "\n";

    // rows
    const int n = simT_.isEmpty() ? 0 : simT_[0].size();
    for (int i = 0; i < n; ++i) {
        out << QString::number(simT_[0][i], 'g', 12);
        for (int c = 0; c < cases_.size(); ++c) {
            if (i < simX_[c].size())
                out << "," << QString::number(simX_[c][i], 'g', 12)
                    << "," << QString::number(simV_[c][i], 'g', 12)
                    << "," << QString::number(simForce_[c][i], 'g', 12);
            else
                out << ",,,";
        }
        out << "\n";
    }
    statusBar()->showMessage(QStringLiteral("Saved: %1").arg(path), 5000);
}

}  // namespace msd_qt
