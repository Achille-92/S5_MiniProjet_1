#include "radarwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

constexpr int kMargin = 28;          // place pour les étiquettes autour du demi-cercle
constexpr int kRings = 4;
constexpr double kRangeStepCm = 40.0;   // la portée est arrondie à un multiple de 40 cm
constexpr int kAngleStepDeg = 30;

QPointF polar(const QPointF &center, double radius, int angleDeg)
{
    const double a = qDegreesToRadians(static_cast<double>(angleDeg));
    return {center.x() + radius * qCos(a), center.y() - radius * qSin(a)};
}

} // namespace

RadarWidget::RadarWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
}

QSize RadarWidget::minimumSizeHint() const
{
    return {260, 160};
}

QSize RadarWidget::sizeHint() const
{
    return {480, 260};
}

void RadarWidget::setPoints(const QVector<ScanPoint> &points)
{
    m_points = points;
    update();
}

void RadarWidget::clear()
{
    m_points.clear();
    update();
}

double RadarWidget::rangeCm() const
{
    double maxCm = 0.0;
    for (const ScanPoint &p : m_points) {
        if (p.hasEcho && p.cm > maxCm) {
            maxCm = p.cm;
        }
    }
    return qMax(kRangeStepCm, qCeil(maxCm / kRangeStepCm) * kRangeStepCm);
}

void RadarWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF area = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    const double radius = qMin(area.width() / 2.0, area.height());
    if (radius < 20.0) {
        return;
    }
    const QPointF center(area.center().x(), area.bottom());
    const double range = rangeCm();

    const QColor textColor = palette().color(QPalette::Text);
    QColor gridColor = textColor;
    gridColor.setAlpha(70);
    const QColor traceColor(46, 160, 67);

    // Anneaux de distance
    painter.setPen(QPen(gridColor, 1));
    for (int i = 1; i <= kRings; ++i) {
        const double r = radius * i / kRings;
        painter.drawArc(QRectF(center.x() - r, center.y() - r, 2 * r, 2 * r), 0, 180 * 16);
        // Étiquette sous la ligne de base, pour ne pas gêner celles des angles
        painter.setPen(textColor);
        painter.drawText(QRectF(center.x() + r - 30, center.y() + 6, 60, 16), Qt::AlignCenter,
                         tr("%1 cm").arg(range * i / kRings, 0, 'f', 0));
        painter.setPen(QPen(gridColor, 1));
    }

    // Rayons tous les 30° et ligne de base
    painter.drawLine(QPointF(center.x() - radius, center.y()), QPointF(center.x() + radius, center.y()));
    for (int a = 0; a <= 180; a += kAngleStepDeg) {
        painter.setPen(QPen(gridColor, 1));
        painter.drawLine(center, polar(center, radius, a));
        painter.setPen(textColor);
        const QPointF label = polar(center, radius + 14, a);
        painter.drawText(QRectF(label.x() - 18, label.y() - 8, 36, 16), Qt::AlignCenter, tr("%1°").arg(a));
    }

    // Mesures : trait reliant les échos, un point par mesure, un repère sur le
    // bord pour les angles sans écho
    QPainterPath trace;
    bool traceStarted = false;
    for (const ScanPoint &p : m_points) {
        if (!p.hasEcho) {
            const QPointF rim = polar(center, radius, p.angle);
            painter.setPen(QPen(gridColor, 2));
            painter.drawLine(polar(center, radius - 6, p.angle), rim);
            traceStarted = false;
            continue;
        }
        const QPointF pt = polar(center, qMin(radius, radius * p.cm / range), p.angle);
        if (traceStarted) {
            trace.lineTo(pt);
        } else {
            trace.moveTo(pt);
            traceStarted = true;
        }
    }
    painter.setPen(QPen(traceColor, 2));
    painter.drawPath(trace);

    painter.setPen(Qt::NoPen);
    painter.setBrush(traceColor);
    for (const ScanPoint &p : m_points) {
        if (p.hasEcho) {
            painter.drawEllipse(polar(center, qMin(radius, radius * p.cm / range), p.angle), 3.5, 3.5);
        }
    }
}
