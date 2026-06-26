// Widgets.hpp — single-curve response plot with a dashed target line.

#ifndef PID_QT_WIDGETS_HPP
#define PID_QT_WIDGETS_HPP

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>

namespace pid_qt {

class ResponsePlot : public QWidget {
    Q_OBJECT
public:
    explicit ResponsePlot(QWidget* parent = nullptr);

    void setData(const QVector<double>& ts,
                 const QVector<double>& thetas,
                 double target,
                 double finalTheta,
                 double xMax);   // x-axis upper bound (e.g. time_length)

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<double> ts_, thetas_;
    double target_     = 0.0;
    double finalTheta_ = 0.0;
    double xMax_       = 1.0;
};

}  // namespace pid_qt

#endif
