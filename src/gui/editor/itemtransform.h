/*
    QSapecNG - Qt based SapecNG GUI front-end
    Copyright (C) 2009, Michele Caini

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/


#ifndef ITEMTRANSFORM_H
#define ITEMTRANSFORM_H


#include <QGraphicsItem>
#include <QTransform>


namespace qsapecng
{


// Replacements for the QGraphicsItem::scale(sx, sy), rotate(angle) and
// translate(dx, dy) members that were removed after Qt 4. They behave
// exactly like the Qt 4 versions: the operation is combined with the
// item's current transform.

inline void scaleItem(QGraphicsItem* item, qreal sx, qreal sy)
  { item->setTransform(QTransform::fromScale(sx, sy), true); }

inline void rotateItem(QGraphicsItem* item, qreal angle)
  { item->setTransform(QTransform().rotate(angle), true); }

inline void translateItem(QGraphicsItem* item, qreal dx, qreal dy)
  { item->setTransform(QTransform::fromTranslate(dx, dy), true); }


}


#endif // ITEMTRANSFORM_H
