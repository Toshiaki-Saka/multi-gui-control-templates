// Widgets.cpp — multi-series LinePlot.

#include "Widgets.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <limits>

namespace tdof_qt {

LinePlot::LinePlot(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 280);
}

void LinePlot::setTitle(const QString& s) { title_ = s; update(); }
void LinePlot::setAxisLabels(const QString& x, const QString& y) {
    xLabel_ = x; yLabel_ = y; update();
}
void LinePlot::setSeries(const QVector<Series>& series) {
    series_ = series; update();
}
void LinePlot::clearSeries() { series_.clear(); update(); }

void LinePlot::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), Qt::white);

    const int marginL = 56;
    const int marginR = 16;
    const int marginT = title_.isEmpty() ? 14 : 28;
    const int marginB = 40;
    const QRectF plot(marginL, marginT,
                      width()  - marginL - marginR,
                      height() - marginT - marginB);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // Compute data ranges over all series.
    double xMin =  std::numeric_limits<double>::infinity();
    double xMax = -std::numeric_limits<double>::infinity();
    double yMin =  std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    for (const auto& s : series_) {
        for (double x : s.xs) { xMin = std::min(xMin, x); xMax = std::max(xMax, x); }
        for (double y : s.ys) { yMin = std::min(yMin, y); yMax = std::max(yMax, y); }
    }
    if (!std::isfinite(xMin)) { xMin = 0; xMax = 1; }
    if (!std::isfinite(yMin)) { yMin = 0; yMax = 1; }
    if (xMax - xMin < 1e-12) { xMax = xMin + 1; }
    if (yMax - yMin < 1e-12) { yMin -= 0.5; yMax += 0.5; }
    const double yPad = 0.08 * (yMax - yMin);
    yMin -= yPad; yMax += yPad;

    auto xToPx = [&](double x) {
        return plot.left() + (x - xMin) / (xMax - xMin) * plot.width();
    };
    auto yToPx = [&](double y) {
        return plot.top() + (1.0 - (y - yMin) / (yMax - yMin)) * plot.height();
    };

    // Grid + box.
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

    // Tick labels.
    {
        QFont f = g.font(); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetrics fm(f);
        for (int i = 0; i <= 4; ++i) {
            const double fr = i / 4.0;
            const double xv = xMin + fr * (xMax - xMin);
            const double yv = yMin + (1.0 - fr) * (yMax - yMin);
            const QString sx = QString::number(xv, 'g', 3);
            const QString sy = QString::number(yv, 'g', 3);
            g.drawText(QPointF(plot.left() + fr * plot.width()
                                   - fm.horizontalAdvance(sx) / 2.0,
                               plot.bottom() + fm.ascent() + 3), sx);
            g.drawText(QPointF(plot.left() - fm.horizontalAdvance(sy) - 5,
                               plot.top() + fr * plot.height()
                                   + fm.ascent() / 2 - 2), sy);
        }
    }

    // Title + axis labels.
    if (!title_.isEmpty()) {
        QFont f = g.font(); f.setBold(true); f.setPointSizeF(10.0); g.setFont(f);
        g.drawText(QRectF(0, 4, width(), 20), Qt::AlignCenter, title_);
    }
    {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(8.5); g.setFont(f);
        if (!xLabel_.isEmpty()) {
            g.drawText(QRectF(plot.left(), height() - 18, plot.width(), 16),
                       Qt::AlignCenter, xLabel_);
        }
        if (!yLabel_.isEmpty()) {
            g.save();
            g.translate(14, plot.center().y());
            g.rotate(-90);
            g.drawText(QRectF(-60, -8, 120, 16), Qt::AlignCenter, yLabel_);
            g.restore();
        }
    }

    // Curves.
    for (const auto& s : series_) {
        if (s.xs.size() < 2 || s.xs.size() != s.ys.size()) continue;
        QPainterPath path;
        path.moveTo(xToPx(s.xs[0]), yToPx(s.ys[0]));
        for (int i = 1; i < s.xs.size(); ++i) {
            path.lineTo(xToPx(s.xs[i]), yToPx(s.ys[i]));
        }
        g.setPen(QPen(s.color, 1.8));
        g.drawPath(path);
    }

    // Legend (top-right inside the plot).
    {
        QFont f = g.font(); f.setPointSizeF(8.5); g.setFont(f);
        QFontMetrics fm(f);
        int lw = 0;
        for (const auto& s : series_) lw = std::max(lw, fm.horizontalAdvance(s.label));
        const int boxW = lw + 38;
        const int boxH = series_.size() * 16 + 8;
        const QRectF box(plot.right() - boxW - 6, plot.top() + 6, boxW, boxH);
        g.setBrush(QColor(255, 255, 255, 220));
        g.setPen(QPen(QColor(180, 180, 180), 0.8));
        g.drawRect(box);
        int yy = static_cast<int>(box.top()) + 12;
        for (const auto& s : series_) {
            g.setPen(QPen(s.color, 2.4));
            g.drawLine(QPointF(box.left() + 6, yy), QPointF(box.left() + 26, yy));
            g.setPen(Qt::black);
            g.drawText(QPointF(box.left() + 30, yy + 4), s.label);
            yy += 16;
        }
    }
}

}  // namespace tdof_qt
