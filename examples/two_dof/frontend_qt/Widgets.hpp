// Widgets.hpp — multi-curve line plot for the 2-DOF comparison demo.

#ifndef TDOF_QT_WIDGETS_HPP
#define TDOF_QT_WIDGETS_HPP

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

namespace tdof_qt {

// A simple multi-series XY line plot with auto-scaling, a legend,
// axis ticks, and a title.
class LinePlot : public QWidget {
    Q_OBJECT
public:
    explicit LinePlot(QWidget* parent = nullptr);

    void setTitle(const QString& s);
    void setAxisLabels(const QString& x, const QString& y);

    // Replace all series. Each series is (label, colour, xs, ys).
    struct Series {
        QString         label;
        QColor          color;
        QVector<double> xs;
        QVector<double> ys;
    };
    void setSeries(const QVector<Series>& series);
    void clearSeries();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Series> series_;
    QString title_, xLabel_, yLabel_;
};

}  // namespace tdof_qt

#endif
