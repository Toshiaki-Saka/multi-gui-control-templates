// Widgets.hpp — overlay line plot for the MSD sweep.

#ifndef MSD_QT_WIDGETS_HPP
#define MSD_QT_WIDGETS_HPP

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

namespace msd_qt {

// Multi-series time-series plot with a legend.
class OverlayPlot : public QWidget {
    Q_OBJECT
public:
    explicit OverlayPlot(QWidget* parent = nullptr);

    struct Series {
        QString         label;
        QColor          color;
        QVector<double> xs;
        QVector<double> ys;
        bool            dotted = false;
    };
    void setSeries(const QVector<Series>& series, double xMax);
    void setTitle(const QString& s);
    void setAxisLabels(const QString& x, const QString& y);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Series> series_;
    double          xMax_   = 1.0;
    QString         title_, xLabel_, yLabel_;
};

}  // namespace msd_qt

#endif
