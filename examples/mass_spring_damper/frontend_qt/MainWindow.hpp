// MainWindow.hpp — Qt6 GUI for the MSD parameter-sweep demo.

#ifndef MSD_QT_MAINWINDOW_HPP
#define MSD_QT_MAINWINDOW_HPP

#include "msd_core.h"

#include <QMainWindow>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;

namespace msd_qt {

class OverlayPlot;

// One row of the case table — the parameters plus a name + enabled flag.
struct CaseRow {
    QString name;
    MsdCase cfg;
    bool    enabled = true;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onCaseSelected(int row);
    void onCaseChecked(QListWidgetItem* item);
    void onEditorChanged();
    void onNameChanged(const QString& text);
    void onAddCase();
    void onDuplicateCase();
    void onRemoveCase();
    void onResetCases();
    void onRun();
    void onSaveCsv();

private:
    void buildUi();
    void refreshCaseList(int selectIndex = -1);
    void loadCaseIntoEditor(int row);
    void updateDerivedLabels(int row);
    void refreshPlot();

    // Case table.
    QVector<CaseRow> cases_;
    QVector<QVector<double>> simT_, simX_, simV_;
    QVector<QVector<double>> simForce_;
    QVector<double>          simFinalX_, simFinalV_, simMaxAbsX_;

    QListWidget* list_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* dupBtn_ = nullptr;
    QPushButton* rmBtn_  = nullptr;
    QPushButton* resetBtn_ = nullptr;

    QComboBox*       nameEdit_ = nullptr;
    QDoubleSpinBox*  edM_ = nullptr;
    QDoubleSpinBox*  edC_ = nullptr;
    QDoubleSpinBox*  edK_ = nullptr;
    QDoubleSpinBox*  edF_ = nullptr;
    QDoubleSpinBox*  edW_ = nullptr;
    QDoubleSpinBox*  edX0_ = nullptr;
    QDoubleSpinBox*  edV0_ = nullptr;

    QLabel* lblWn_   = nullptr;
    QLabel* lblZeta_ = nullptr;
    QLabel* lblXend_ = nullptr;
    QLabel* lblVend_ = nullptr;
    QLabel* lblXmax_ = nullptr;

    QDoubleSpinBox*  edDt_   = nullptr;
    QDoubleSpinBox*  edStop_ = nullptr;
    QCheckBox*       cbForce_ = nullptr;
    QPushButton*     runBtn_  = nullptr;
    QPushButton*     saveCsvBtn_ = nullptr;

    OverlayPlot* plot_ = nullptr;
    QLabel*      statusLbl_ = nullptr;
    QTimer*      debounce_ = nullptr;

    bool loadingEditor_ = false;
};

}  // namespace msd_qt

#endif
