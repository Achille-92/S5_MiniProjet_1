#ifndef SCANPOINT_H
#define SCANPOINT_H

#include <QMetaType>
#include <QVector>

// Une mesure du balayage, côté C++ (équivalent de scan_point_t du module C).
struct ScanPoint
{
    int angle = 0;          // degrés
    double cm = 0.0;        // distance, significative seulement si hasEcho
    bool hasEcho = false;   // false si la carte a répondu TIMEOUT pour cet angle
};

Q_DECLARE_METATYPE(ScanPoint)
Q_DECLARE_METATYPE(QVector<ScanPoint>)

#endif // SCANPOINT_H
