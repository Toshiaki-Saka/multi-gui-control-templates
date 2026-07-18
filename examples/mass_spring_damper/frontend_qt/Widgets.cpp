// Widgets.cpp — OverlayPlot.

#include "Widgets.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <limits>

namespace msd_qt {

namespace {

double mapX(double v, double lo, double hi, double pxLo, double pxHi) {
    if (hi <= lo) return pxLo;
    return pxLo + (v - lo) / (hi - lo) * (pxHi - pxLo);
}
double mapY(double v, double lo, double hi, double pxLo, double pxHi) {
    if (hi <= lo) return pxLo;
    return pxHi - (v - lo) / (hi - lo) * (pxHi - pxLo);
}
double niceStep(double range, int target_ticks) {
    if (range <= 0) return 1.0;
    const double raw = range / std::max(1, target_ticks);
    const double exp = std::pow(10.0, std::floor(std::log10(raw)));
    const double n   = raw / exp;
    double mult;
    if      (n < 1.5) mult = 1.0;
    else if (n < 3.5) mult = 2.0;
    else if (n < 7.5) mult = 5.0;
    else              mult = 10.0;
    return mult * exp;
}

}  // namespace

OverlayPlot::OverlayPlot(QWidget* parent) : QWidget(parent) {
    setMinimumSize(420, 320);
}

void OverlayPlot::setSeries(const QVector<Series>& s, double xMax) {
    series_ = s;
    xMax_   = xMax > 0 ? xMax : 1.0;
    update();
}
void OverlayPlot::setTitle(const QString& s)                       { title_  = s; update(); }
void OverlayPlot::setAxisLabels(const QString& x, const QString& y) { xLabel_ = x; yLabel_ = y; update(); }

void OverlayPlot::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), Qt::white);

    const int marginL = 64;
    const int marginR = 16;
    const int marginT = title_.isEmpty() ? 16 : 28;
    const int marginB = 38;
    const QRectF plot(marginL, marginT,
                      width()  - marginL - marginR,
                      height() - marginT - marginB);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // Y-range.
    double yMin =  std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    for (const auto& s : series_) {
        for (double y : s.ys) {
            yMin = std::min(yMin, y); yMax = std::max(yMax, y);
        }
    }
    if (!std::isfinite(yMin)) { yMin = -1; yMax = 1; }
    if (yMax - yMin < 1e-12) { yMin -= 0.5; yMax += 0.5; }
    const double padY = 0.05 * (yMax - yMin);
    yMin -= padY; yMax += padY;

    const double xMin = 0.0;
    const double xMax = xMax_;
    const double stepX = niceStep(xMax - xMin, 6);
    const double stepY = niceStep(yMax - yMin, 6);

    // Grid + frame.
    g.setPen(QPen(QColor(230, 230, 230), 1.0));
    for (double xv = std::ceil(xMin / stepX) * stepX; xv <= xMax; xv += stepX) {
        const double px = mapX(xv, xMin, xMax, plot.left(), plot.right());
        g.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
    }
    for (double yv = std::ceil(yMin / stepY) * stepY; yv <= yMax; yv += stepY) {
        const double py = mapY(yv, yMin, yMax, plot.top(), plot.bottom());
        g.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
    }
    g.setPen(QPen(Qt::black, 1.0));
    g.drawRect(plot);

    // Tick labels.
    {
        QFont f = g.font(); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetrics fm(f);
        // Tiny epsilon used to snap "essentially zero" tick values to 0
        // so we don't render labels like "-2.776e-17" — a normal hazard
        // when std::ceil(min/step)*step lands a hair below zero.
        const double yEps = (yMax - yMin) * 1e-9;
        const double xEps = (xMax - xMin) * 1e-9;
        for (double xv = std::ceil(xMin / stepX) * stepX; xv <= xMax; xv += stepX) {
            double v = (std::fabs(xv) < xEps) ? 0.0 : xv;
            const QString s = QString::number(v, 'g', 4);
            const double px = mapX(xv, xMin, xMax, plot.left(), plot.right());
            g.drawText(QPointF(px - fm.horizontalAdvance(s) / 2.0,
                               plot.bottom() + fm.ascent() + 2), s);
        }
        for (double yv = std::ceil(yMin / stepY) * stepY; yv <= yMax; yv += stepY) {
            double v = (std::fabs(yv) < yEps) ? 0.0 : yv;
            const QString s = QString::number(v, 'g', 4);
            const double py = mapY(yv, yMin, yMax, plot.top(), plot.bottom());
            g.drawText(QPointF(plot.left() - fm.horizontalAdvance(s) - 4,
                               py + fm.ascent() / 2 - 2), s);
        }
    }

    // Title + axes.
    if (!title_.isEmpty()) {
        QFont f = g.font(); f.setBold(true); f.setPointSizeF(10.5); g.setFont(f);
        g.drawText(QRectF(0, 6, width(), 18), Qt::AlignCenter, title_);
    }
    if (!xLabel_.isEmpty()) {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(9.0); g.setFont(f);
        g.drawText(QRectF(plot.left(), height() - 18, plot.width(), 16),
                   Qt::AlignCenter, xLabel_);
    }
    if (!yLabel_.isEmpty()) {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(9.0); g.setFont(f);
        g.save();
        g.translate(14, plot.center().y()); g.rotate(-90);
        g.drawText(QRectF(-80, -8, 160, 16), Qt::AlignCenter, yLabel_);
        g.restore();
    }

    // Series.
    for (const auto& s : series_) {
        const int n = std::min(s.xs.size(), s.ys.size());
        if (n < 2) continue;
        QPainterPath path;
        path.moveTo(mapX(s.xs[0], xMin, xMax, plot.left(), plot.right()),
                    mapY(s.ys[0], yMin, yMax, plot.top(), plot.bottom()));
        for (int i = 1; i < n; ++i) {
            path.lineTo(mapX(s.xs[i], xMin, xMax, plot.left(), plot.right()),
                        mapY(s.ys[i], yMin, yMax, plot.top(), plot.bottom()));
        }
        QPen pen(s.color, 1.6);
        if (s.dotted) { pen.setStyle(Qt::DotLine); pen.setWidthF(1.2); }
        g.setPen(pen); g.drawPath(path);
    }

    // Legend.
    QVector<const Series*> labelled;
    for (const auto& s : series_) if (!s.label.isEmpty()) labelled.push_back(&s);
    if (!labelled.isEmpty()) {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetrics fm(f);
        int lw = 0;
        for (const auto* s : labelled) lw = std::max(lw, fm.horizontalAdvance(s->label));
        const int boxW = lw + 38;
        const int boxH = 4 + 13 * labelled.size();
        const QRectF box(plot.left() + 6, plot.top() + 6, boxW, boxH);
        g.setBrush(QColor(255, 255, 255, 220));
        g.setPen(QPen(QColor(180, 180, 180), 0.8));
        g.drawRect(box);
        int yy = static_cast<int>(box.top()) + 11;
        for (const auto* s : labelled) {
            QPen pen(s->color, 2.0);
            if (s->dotted) pen.setStyle(Qt::DotLine);
            g.setPen(pen);
            g.drawLine(QPointF(box.left() + 6,  yy),
                       QPointF(box.left() + 26, yy));
            g.setPen(Qt::black);
            g.drawText(QPointF(box.left() + 30, yy + 4), s->label);
            yy += 13;
        }
    }
}

}  // namespace msd_qt
