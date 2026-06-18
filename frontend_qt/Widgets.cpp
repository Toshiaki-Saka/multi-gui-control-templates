// Widgets.cpp — XyPlot + LinePlot.

#include "Widgets.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <limits>

namespace track_qt {

namespace {

// Map a data range [lo, hi] onto pixel range [pxLo, pxHi] (used for x).
double mapX(double v, double lo, double hi, double pxLo, double pxHi) {
    if (hi <= lo) return pxLo;
    return pxLo + (v - lo) / (hi - lo) * (pxHi - pxLo);
}
// Map [lo, hi] onto [pxHi, pxLo] (y flipped because screen Y grows down).
double mapY(double v, double lo, double hi, double pxLo, double pxHi) {
    if (hi <= lo) return pxLo;
    return pxHi - (v - lo) / (hi - lo) * (pxHi - pxLo);
}

// "Nice" ticks: choose a step that's 1/2/5 times a power of 10, give
// or take, so the axis labels are clean.
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

// ===========================================================================
// XyPlot
// ===========================================================================
XyPlot::XyPlot(QWidget* parent) : QWidget(parent) {
    setMinimumSize(420, 420);
}

void XyPlot::setData(const QVector<double>& rx, const QVector<double>& ry,
                     const QVector<double>& ax, const QVector<double>& ay) {
    refX_ = rx; refY_ = ry; actX_ = ax; actY_ = ay;
    update();
}

void XyPlot::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), Qt::white);

    const int marginL = 60;
    const int marginR = 16;
    const int marginT = 32;
    const int marginB = 38;

    // Combined data range, padded.
    double xMin =  std::numeric_limits<double>::infinity();
    double xMax = -std::numeric_limits<double>::infinity();
    double yMin =  std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    auto extend = [&](const QVector<double>& xs, const QVector<double>& ys) {
        const int n = std::min(xs.size(), ys.size());
        for (int i = 0; i < n; ++i) {
            xMin = std::min(xMin, xs[i]); xMax = std::max(xMax, xs[i]);
            yMin = std::min(yMin, ys[i]); yMax = std::max(yMax, ys[i]);
        }
    };
    extend(refX_, refY_); extend(actX_, actY_);
    if (!std::isfinite(xMin)) { xMin = 0; xMax = 1; yMin = 0; yMax = 1; }
    const double padX = 0.05 * std::max(1e-6, xMax - xMin);
    const double padY = 0.05 * std::max(1e-6, yMax - yMin);
    xMin -= padX; xMax += padX; yMin -= padY; yMax += padY;

    // Equal aspect: stretch the smaller-range axis to match the larger.
    const double dx = xMax - xMin;
    const double dy = yMax - yMin;
    const double availW = width()  - marginL - marginR;
    const double availH = height() - marginT - marginB;
    if (availW <= 0 || availH <= 0) return;
    const double scaleX = availW / dx;
    const double scaleY = availH / dy;
    const double scale  = std::min(scaleX, scaleY);
    const double usedW  = dx * scale;
    const double usedH  = dy * scale;
    const double pxL    = marginL + (availW - usedW) / 2.0;
    const double pxR    = pxL + usedW;
    const double pxT    = marginT + (availH - usedH) / 2.0;
    const double pxB    = pxT + usedH;

    const QRectF plot(pxL, pxT, usedW, usedH);

    // Gridlines.
    const double stepX = niceStep(dx, 6);
    const double stepY = niceStep(dy, 6);
    g.setPen(QPen(QColor(230, 230, 230), 1.0));
    for (double xv = std::ceil(xMin / stepX) * stepX; xv <= xMax; xv += stepX) {
        const double px = mapX(xv, xMin, xMax, pxL, pxR);
        g.drawLine(QPointF(px, pxT), QPointF(px, pxB));
    }
    for (double yv = std::ceil(yMin / stepY) * stepY; yv <= yMax; yv += stepY) {
        const double py = mapY(yv, yMin, yMax, pxT, pxB);
        g.drawLine(QPointF(pxL, py), QPointF(pxR, py));
    }
    g.setPen(QPen(Qt::black, 1.0));
    g.drawRect(plot);

    // Tick labels.
    {
        QFont f = g.font(); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetrics fm(f);
        for (double xv = std::ceil(xMin / stepX) * stepX; xv <= xMax; xv += stepX) {
            const QString s = QString::number(xv, 'g', 4);
            const double px = mapX(xv, xMin, xMax, pxL, pxR);
            g.drawText(QPointF(px - fm.horizontalAdvance(s) / 2.0,
                               pxB + fm.ascent() + 2), s);
        }
        for (double yv = std::ceil(yMin / stepY) * stepY; yv <= yMax; yv += stepY) {
            const QString s = QString::number(yv, 'g', 4);
            const double py = mapY(yv, yMin, yMax, pxT, pxB);
            g.drawText(QPointF(pxL - fm.horizontalAdvance(s) - 4,
                               py + fm.ascent() / 2 - 2), s);
        }
    }

    // Title + axis labels.
    {
        QFont f = g.font(); f.setBold(true); f.setPointSizeF(10.0); g.setFont(f);
        g.drawText(QRectF(0, 6, width(), 18), Qt::AlignCenter,
                   QStringLiteral("Path tracking — XY"));
    }
    g.drawText(QRectF(0, height() - 16, width(), 14),
               Qt::AlignCenter, QStringLiteral("X [m]"));
    g.save();
    g.translate(14, (pxT + pxB) / 2.0); g.rotate(-90);
    g.drawText(QRectF(-60, -8, 120, 16), Qt::AlignCenter,
               QStringLiteral("Y [m]"));
    g.restore();

    // Reference path (dashed blue).
    auto drawPolyline = [&](const QVector<double>& xs, const QVector<double>& ys,
                            const QPen& pen) {
        const int n = std::min(xs.size(), ys.size());
        if (n < 2) return;
        QPainterPath p;
        p.moveTo(mapX(xs[0], xMin, xMax, pxL, pxR),
                 mapY(ys[0], yMin, yMax, pxT, pxB));
        for (int i = 1; i < n; ++i) {
            p.lineTo(mapX(xs[i], xMin, xMax, pxL, pxR),
                     mapY(ys[i], yMin, yMax, pxT, pxB));
        }
        g.setPen(pen); g.drawPath(p);
    };
    {
        QPen p(QColor(31, 119, 180), 1.4);
        p.setStyle(Qt::DashLine);
        drawPolyline(refX_, refY_, p);
    }
    drawPolyline(actX_, actY_, QPen(QColor(255, 127, 14), 1.6));

    // Start/End markers.
    auto drawMarker = [&](double x, double y, const QColor& c) {
        const QPointF p(mapX(x, xMin, xMax, pxL, pxR),
                        mapY(y, yMin, yMax, pxT, pxB));
        g.setBrush(c); g.setPen(QPen(Qt::black, 0.8));
        g.drawEllipse(p, 4.0, 4.0);
    };
    if (!actX_.isEmpty()) {
        drawMarker(actX_.first(), actY_.first(), QColor(31, 119, 180));
        drawMarker(actX_.last(),  actY_.last(),  QColor(255, 127, 14));
    }

    // Legend.
    {
        QFont f = g.font(); f.setPointSizeF(8.5); f.setBold(false); g.setFont(f);
        QFontMetrics fm(f);
        const QStringList items = {QStringLiteral("Reference path"),
                                   QStringLiteral("Actual path"),
                                   QStringLiteral("Start"),
                                   QStringLiteral("End")};
        int lw = 0;
        for (const auto& s : items) lw = std::max(lw, fm.horizontalAdvance(s));
        const int boxW = lw + 38;
        const int boxH = 4 + 14 * items.size();
        const QRectF box(plot.left() + 6, plot.top() + 6, boxW, boxH);
        g.setBrush(QColor(255, 255, 255, 220));
        g.setPen(QPen(QColor(180, 180, 180), 0.8));
        g.drawRect(box);

        const double xLine0 = box.left() + 6;
        const double xLine1 = box.left() + 26;
        int yy = static_cast<int>(box.top()) + 12;
        QPen p1(QColor(31, 119, 180), 2.0); p1.setStyle(Qt::DashLine);
        g.setPen(p1);
        g.drawLine(QPointF(xLine0, yy), QPointF(xLine1, yy));
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[0]);
        yy += 14;
        g.setPen(QPen(QColor(255, 127, 14), 2.4));
        g.drawLine(QPointF(xLine0, yy), QPointF(xLine1, yy));
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[1]);
        yy += 14;
        g.setBrush(QColor(31, 119, 180));
        g.setPen(QPen(Qt::black, 0.6));
        g.drawEllipse(QPointF((xLine0 + xLine1) / 2.0, yy), 3.5, 3.5);
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[2]);
        yy += 14;
        g.setBrush(QColor(255, 127, 14));
        g.setPen(QPen(Qt::black, 0.6));
        g.drawEllipse(QPointF((xLine0 + xLine1) / 2.0, yy), 3.5, 3.5);
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[3]);
    }
}

// ===========================================================================
// LinePlot
// ===========================================================================
LinePlot::LinePlot(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 180);
}

void LinePlot::setTitle(const QString& s)                  { title_  = s; update(); }
void LinePlot::setAxisLabels(const QString& x, const QString& y)
                                                           { xLabel_ = x; yLabel_ = y; update(); }
void LinePlot::setSeries(const QVector<Series>& s)         { series_ = s; update(); }
void LinePlot::setHorizontalLines(const QVector<double>& l) { hLines_ = l; update(); }

void LinePlot::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), Qt::white);

    const int marginL = 56;
    const int marginR = 12;
    const int marginT = title_.isEmpty() ? 12 : 22;
    const int marginB = 32;
    const QRectF plot(marginL, marginT,
                      width()  - marginL - marginR,
                      height() - marginT - marginB);
    if (plot.width() <= 0 || plot.height() <= 0) return;

    // Data ranges.
    double xMin =  std::numeric_limits<double>::infinity();
    double xMax = -std::numeric_limits<double>::infinity();
    double yMin =  std::numeric_limits<double>::infinity();
    double yMax = -std::numeric_limits<double>::infinity();
    for (const auto& s : series_) {
        for (double v : s.xs) { xMin = std::min(xMin, v); xMax = std::max(xMax, v); }
        for (double v : s.ys) { yMin = std::min(yMin, v); yMax = std::max(yMax, v); }
    }
    for (double v : hLines_) { yMin = std::min(yMin, v); yMax = std::max(yMax, v); }
    if (!std::isfinite(xMin)) { xMin = 0; xMax = 1; yMin = 0; yMax = 1; }
    if (xMax - xMin < 1e-12) xMax = xMin + 1;
    if (yMax - yMin < 1e-12) yMax = yMin + 1;
    const double padY = 0.05 * (yMax - yMin);
    yMin -= padY; yMax += padY;

    const double stepX = niceStep(xMax - xMin, 5);
    const double stepY = niceStep(yMax - yMin, 5);

    // Grid.
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
        for (double xv = std::ceil(xMin / stepX) * stepX; xv <= xMax; xv += stepX) {
            const QString s = QString::number(xv, 'g', 4);
            const double px = mapX(xv, xMin, xMax, plot.left(), plot.right());
            g.drawText(QPointF(px - fm.horizontalAdvance(s) / 2.0,
                               plot.bottom() + fm.ascent() + 2), s);
        }
        for (double yv = std::ceil(yMin / stepY) * stepY; yv <= yMax; yv += stepY) {
            const QString s = QString::number(yv, 'g', 4);
            const double py = mapY(yv, yMin, yMax, plot.top(), plot.bottom());
            g.drawText(QPointF(plot.left() - fm.horizontalAdvance(s) - 4,
                               py + fm.ascent() / 2 - 2), s);
        }
    }

    // Title + axes.
    if (!title_.isEmpty()) {
        QFont f = g.font(); f.setBold(true); f.setPointSizeF(9.5); g.setFont(f);
        g.drawText(QRectF(0, 4, width(), 16), Qt::AlignCenter, title_);
    }
    if (!xLabel_.isEmpty()) {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(8.5); g.setFont(f);
        g.drawText(QRectF(plot.left(), height() - 14, plot.width(), 14),
                   Qt::AlignCenter, xLabel_);
    }
    if (!yLabel_.isEmpty()) {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(8.5); g.setFont(f);
        g.save();
        g.translate(12, plot.center().y()); g.rotate(-90);
        g.drawText(QRectF(-80, -8, 160, 16), Qt::AlignCenter, yLabel_);
        g.restore();
    }

    // Horizontal limit lines (dashed).
    for (double v : hLines_) {
        const double py = mapY(v, yMin, yMax, plot.top(), plot.bottom());
        QPen p(QColor(0, 0, 0, 120), 0.8);
        p.setStyle(Qt::DashLine);
        g.setPen(p);
        g.drawLine(QPointF(plot.left(), py), QPointF(plot.right(), py));
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
        if (s.dashed) pen.setStyle(Qt::DashLine);
        g.setPen(pen); g.drawPath(path);
    }

    // Legend (only if more than one labelled series).
    {
        QVector<const Series*> labelled;
        for (const auto& s : series_) if (!s.label.isEmpty()) labelled.push_back(&s);
        if (labelled.size() >= 2) {
            QFont f = g.font(); f.setBold(false); f.setPointSizeF(8.5); g.setFont(f);
            QFontMetrics fm(f);
            int lw = 0;
            for (const auto* s : labelled) lw = std::max(lw, fm.horizontalAdvance(s->label));
            const int boxW = lw + 40;
            const int boxH = 4 + 14 * labelled.size();
            const QRectF box(plot.right() - boxW - 6,
                             plot.top()   + 6, boxW, boxH);
            g.setBrush(QColor(255, 255, 255, 220));
            g.setPen(QPen(QColor(180, 180, 180), 0.8));
            g.drawRect(box);
            int yy = static_cast<int>(box.top()) + 12;
            for (const auto* s : labelled) {
                QPen pen(s->color, 2.0);
                if (s->dashed) pen.setStyle(Qt::DashLine);
                g.setPen(pen);
                g.drawLine(QPointF(box.left() + 6,  yy),
                           QPointF(box.left() + 26, yy));
                g.setPen(Qt::black);
                g.drawText(QPointF(box.left() + 30, yy + 4), s->label);
                yy += 14;
            }
        }
    }
}

}  // namespace track_qt
