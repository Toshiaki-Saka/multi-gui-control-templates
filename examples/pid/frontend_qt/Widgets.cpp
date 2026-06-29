// Widgets.cpp — ResponsePlot.

#include "Widgets.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <limits>

namespace pid_qt {

ResponsePlot::ResponsePlot(QWidget* parent) : QWidget(parent) {
    setMinimumSize(600, 320);
}

void ResponsePlot::setData(const QVector<double>& ts,
                           const QVector<double>& thetas,
                           double target,
                           double finalTheta,
                           double xMax)
{
    ts_ = ts; thetas_ = thetas; target_ = target;
    finalTheta_ = finalTheta;
    xMax_ = xMax > 0 ? xMax : 1.0;
    update();
}

void ResponsePlot::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), Qt::white);

    const int marginL = 60;
    const int marginR = 16;
    const int marginT = 30;
    const int marginB = 38;
    const QRectF plot(marginL, marginT,
                      width()  - marginL - marginR,
                      height() - marginT - marginB);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // Y range: include data + target + start with 20 unit pad each side.
    double yMin =  std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    for (double v : thetas_) { yMin = std::min(yMin, v); yMax = std::max(yMax, v); }
    if (!std::isfinite(yMin)) { yMin = 0; yMax = 1; }
    yMin = std::min(yMin, target_) - 20.0;
    yMax = std::max(yMax, target_) + 20.0;
    if (yMax - yMin < 1e-9) yMax = yMin + 1.0;

    auto xToPx = [&](double x) {
        return plot.left() + (x / xMax_) * plot.width();
    };
    auto yToPx = [&](double y) {
        return plot.top() + (1.0 - (y - yMin) / (yMax - yMin)) * plot.height();
    };

    // Grid (5x5 lines, matching the CLAUDE.md guideline for Avalonia
    // so the two frontends look the same).
    g.setPen(QPen(QColor(230, 230, 230), 1.0));
    for (int i = 0; i <= 4; ++i) {
        const double f = i / 4.0;
        g.drawLine(QPointF(plot.left(), plot.top() + f * plot.height()),
                   QPointF(plot.right(), plot.top() + f * plot.height()));
        g.drawLine(QPointF(plot.left() + f * plot.width(), plot.top()),
                   QPointF(plot.left() + f * plot.width(), plot.bottom()));
    }
    g.setPen(QPen(Qt::black, 1.0));
    g.drawRect(plot);

    // Tick labels (5 each axis).
    {
        QFont f = g.font(); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetrics fm(f);
        for (int i = 0; i <= 4; ++i) {
            const double fr = i / 4.0;
            const double xv = fr * xMax_;
            const double yv = yMin + (1.0 - fr) * (yMax - yMin);
            const QString sx = QString::number(xv, 'g', 4);
            const QString sy = QString::number(yv, 'g', 4);
            g.drawText(QPointF(plot.left() + fr * plot.width()
                                   - fm.horizontalAdvance(sx) / 2.0,
                               plot.bottom() + fm.ascent() + 4), sx);
            g.drawText(QPointF(plot.left() - fm.horizontalAdvance(sy) - 5,
                               plot.top() + fr * plot.height()
                                   + fm.ascent() / 2 - 2), sy);
        }
    }

    // Title shows final theta.
    {
        QFont f = g.font(); f.setBold(true); f.setPointSizeF(10.0); g.setFont(f);
        g.drawText(QRectF(0, 4, width(), 18), Qt::AlignCenter,
                   QStringLiteral("final theta = %1").arg(finalTheta_, 0, 'f', 3));
    }
    {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(9.0); g.setFont(f);
        g.drawText(QRectF(plot.left(), height() - 18, plot.width(), 16),
                   Qt::AlignCenter, QStringLiteral("t"));
        g.save();
        g.translate(14, plot.center().y()); g.rotate(-90);
        g.drawText(QRectF(-60, -8, 120, 16), Qt::AlignCenter,
                   QStringLiteral("theta"));
        g.restore();
    }

    // Target line (red dashed).
    {
        QPen pen(QColor(214, 40, 40), 1.4);
        pen.setStyle(Qt::DashLine);
        g.setPen(pen);
        g.drawLine(QPointF(plot.left(),  yToPx(target_)),
                   QPointF(plot.right(), yToPx(target_)));
    }

    // Response curve (blue).
    if (ts_.size() >= 2 && ts_.size() == thetas_.size()) {
        QPainterPath path;
        path.moveTo(xToPx(ts_[0]), yToPx(thetas_[0]));
        for (int i = 1; i < ts_.size(); ++i) {
            path.lineTo(xToPx(ts_[i]), yToPx(thetas_[i]));
        }
        g.setPen(QPen(QColor(31, 119, 180), 1.6));
        g.drawPath(path);
    }

    // Legend.
    {
        QFont f = g.font(); f.setPointSizeF(8.5); g.setFont(f);
        QFontMetrics fm(f);
        const QString sA = QStringLiteral("Target");
        const QString sB = QStringLiteral("PID");
        const int lw = std::max(fm.horizontalAdvance(sA),
                                fm.horizontalAdvance(sB));
        const int boxW = lw + 40;
        const int boxH = 38;
        const QRectF box(plot.right() - boxW - 6,
                         plot.bottom() - boxH - 6, boxW, boxH);
        g.setBrush(QColor(255, 255, 255, 220));
        g.setPen(QPen(QColor(180, 180, 180), 0.8));
        g.drawRect(box);

        int yy = static_cast<int>(box.top()) + 12;
        QPen pen(QColor(214, 40, 40), 2.0);
        pen.setStyle(Qt::DashLine);
        g.setPen(pen);
        g.drawLine(QPointF(box.left() + 6, yy),
                   QPointF(box.left() + 26, yy));
        g.setPen(Qt::black);
        g.drawText(QPointF(box.left() + 30, yy + 4), sA);
        yy += 16;
        g.setPen(QPen(QColor(31, 119, 180), 2.4));
        g.drawLine(QPointF(box.left() + 6, yy),
                   QPointF(box.left() + 26, yy));
        g.setPen(Qt::black);
        g.drawText(QPointF(box.left() + 30, yy + 4), sB);
    }
}

}  // namespace pid_qt
