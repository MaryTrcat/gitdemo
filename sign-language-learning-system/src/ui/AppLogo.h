#pragma once

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <QtGlobal>

inline QImage createAppLogoHandGlyphImage(int size)
{
    const int imageSize = qMax(size, 16);
    QImage image(imageSize, imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(imageSize / 96.0, imageSize / 96.0);
    painter.translate(5.0, 5.0);
    painter.scale(0.92, 0.92);

    const QColor handFill(255, 205, 92);
    const QColor handHighlight(255, 221, 129);
    const QColor handStroke(222, 166, 48);
    QRadialGradient fillGradient(QPointF(34, 25), 42);
    fillGradient.setColorAt(0.0, handHighlight);
    fillGradient.setColorAt(0.72, handFill);
    fillGradient.setColorAt(1.0, QColor(246, 190, 72));

    QPainterPath hand;
    hand.moveTo(16, 27);
    hand.cubicTo(14, 20, 22, 17, 25, 24);
    hand.lineTo(31, 42);
    hand.lineTo(31, 29);
    hand.cubicTo(31, 22, 40, 22, 41, 29);
    hand.lineTo(41, 39);
    hand.lineTo(43, 28);
    hand.cubicTo(44, 21, 53, 22, 54, 29);
    hand.lineTo(54, 43);
    hand.lineTo(56, 13);
    hand.cubicTo(56, 5, 68, 5, 68, 13);
    hand.lineTo(68, 45);
    hand.cubicTo(71, 39, 79, 34, 86, 37);
    hand.cubicTo(91, 39, 91, 44, 86, 47);
    hand.cubicTo(79, 51, 73, 57, 70, 64);
    hand.cubicTo(65, 73, 55, 78, 41, 78);
    hand.cubicTo(25, 78, 16, 69, 15, 55);
    hand.lineTo(15, 44);
    hand.cubicTo(16, 38, 18, 34, 16, 27);
    hand.closeSubpath();

    painter.setBrush(fillGradient);
    painter.setPen(QPen(handStroke, 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(hand);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(handStroke, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    QPainterPath foldedLeft;
    foldedLeft.moveTo(31, 31);
    foldedLeft.cubicTo(33, 24, 42, 24, 42, 32);
    foldedLeft.lineTo(42, 44);
    foldedLeft.cubicTo(42, 51, 33, 51, 32, 44);
    foldedLeft.lineTo(31, 31);
    painter.drawPath(foldedLeft);

    QPainterPath foldedRight;
    foldedRight.moveTo(43, 31);
    foldedRight.cubicTo(45, 24, 54, 24, 54, 32);
    foldedRight.lineTo(54, 44);
    foldedRight.cubicTo(53, 51, 44, 51, 43, 44);
    foldedRight.lineTo(43, 31);
    painter.drawPath(foldedRight);

    QPainterPath palmCrease;
    palmCrease.moveTo(56, 62);
    palmCrease.cubicTo(58, 52, 63, 46, 70, 43);
    painter.setPen(QPen(QColor(224, 171, 54, 190), 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(palmCrease);

    painter.setPen(QPen(QColor(255, 232, 151, 130), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath softHighlight;
    softHighlight.moveTo(23, 26);
    softHighlight.cubicTo(24, 33, 27, 40, 29, 47);
    softHighlight.moveTo(59, 13);
    softHighlight.lineTo(59, 39);
    painter.drawPath(softHighlight);

    return image;
}
