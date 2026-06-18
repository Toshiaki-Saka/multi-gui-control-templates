// Widgets.hpp — plotting widgets for the path-tracking demo.

#ifndef TRACK_QT_WIDGETS_HPP
#define TRACK_QT_WIDGETS_HPP

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

namespace track_qt {

// 2-D map with the reference path (dashed) and the actual path (solid)
// drawn at equal aspect.
class XyPlot : public QWidget {
    Q_OBJECT
public:
    explicit XyPlot(QWidget* parent = nullptr);

    void setData(const QVector<double>& refX, const QVector<double>& refY,
                 const QVector<double>& actX, const QVector<double>& actY);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<double> refX_, refY_, actX_, actY_;
};

// Multi-series time-series plot used for the states / errors / inputs
// panels.
class LinePlot : public QWidget {
    Q_OBJECT
public:
    explicit LinePlot(QWidget* parent = nullptr);

    void setTitle(const QString& s);
    void setAxisLabels(const QString& x, const QString& y);

    struct Series {
        QString         label;
        QColor          color;
        QVector<double> xs;
        QVector<double> ys;
        bool            dashed = false;
    };
    void setSeries(const QVector<Series>& series);
    // Optional dashed horizontal reference lines (constraint limits).
    void setHorizontalLines(const QVector<double>& levels);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Series> series_;
    QVector<double> hLines_;
    QString title_, xLabel_, yLabel_;
};

}  // namespace track_qt

#endif
