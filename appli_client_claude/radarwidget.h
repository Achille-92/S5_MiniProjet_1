#ifndef RADARWIDGET_H
#define RADARWIDGET_H

#include <QVector>
#include <QWidget>

#include "scanpoint.h"

// Affiche un balayage sous forme de radar en demi-cercle.
// Convention : 0° à droite, 90° vers le haut, 180° à gauche.
class RadarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RadarWidget(QWidget *parent = nullptr);

    void setPoints(const QVector<ScanPoint> &points);
    void clear();

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double rangeCm() const;   // portée affichée, multiple de 4 anneaux

    QVector<ScanPoint> m_points;
};

#endif // RADARWIDGET_H
