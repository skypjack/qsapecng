/*
    QSapecNG - Qt based SapecNG GUI front-end
    Copyright (C) 2009, Michele Caini

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "gui/editor/itemtransform.h"
#include "gui/editor/item.h"
#include "gui/editor/schematicscene.h"

#include <QTimer>
#include <QGraphicsSceneMouseEvent>


namespace qsapecng
{


Item::Item(QGraphicsItem* parent, SchematicScene* scene)
  : QGraphicsItem(parent)
{
  // Qt 4 accepted the scene in the QGraphicsItem constructor
  if(scene && !parent)
    scene->addItem(this);

  setFlags(
    QGraphicsItem::ItemIsMovable
    | QGraphicsItem::ItemIsSelectable
    | QGraphicsItem::ItemIsFocusable
    | QGraphicsItem::ItemSendsGeometryChanges
  );

  angle_ = 0;
  mirrored_ = false;

  defaultPen_ =
    QPen(
        Qt::SolidPattern,
        0,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin
      );
  itemPen_ =
    QPen(
        Qt::SolidPattern,
        1,
        Qt::SolidLine,
        Qt::RoundCap,
        Qt::RoundJoin
      );
}


void Item::mirror()
{
  mirrored_ = !mirrored_;
  scaleItem(this, -1, 1);
  translateItem(this, -pivotRect().width(), 0);
}


void Item::rotate()
{
  QTransform transform;

  switch(angle_)
  {
  case 90:
    angle_ = 180;
    transform.rotate(180);
    transform.translate(-pivotRect().width(), -pivotRect().height());
    break;
  case 180:
    angle_ = 270;
    transform.rotate(270);
    transform.translate(-pivotRect().width(), 0);
    break;
  case 270:
    angle_ = 0;
    transform.rotate(360);
    transform.translate(0, 0);
    break;
  case 0:
  default:
    angle_ = 90;
    transform.rotate(90);
    transform.translate(0, -pivotRect().height());
  }

  setTransform(transform);

  if(mirrored_) {
    scaleItem(this, -1, 1);
    translateItem(this, -pivotRect().width(), 0);
  }
}


void Item::rotateBack()
{
  rotate();
  rotate();
  rotate();
}


bool Item::mirrored() const
{
  return mirrored_;
}


bool Item::rotated() const
{
  return (angle_ != 0);
}


uint Item::angle() const
{
  return angle_;
}


SchematicScene* Item::schematicScene() const
{
  if(!scene())
    return 0;

  return qobject_cast<SchematicScene*>(scene());
}


void Item::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  // double-click edits the parameters (rotation: R key or Rotate action)
  if(event->button() == Qt::LeftButton && schematicScene()
      && (SchematicScene::itemProperties(this)
          || SchematicScene::isOutput(this)))
  {
    event->accept();
    // open the dialog once the mouse event has been handled, so the view
    // does not keep this item as mouse grabber while the dialog is up
    SchematicScene* scene = schematicScene();
    QGraphicsItem* item = this;
    QTimer::singleShot(0, scene, [scene, item]() {
      if(scene->items().contains(item))
        scene->editItemProperties(item);
    });
  } else {
    QGraphicsItem::mouseDoubleClickEvent(event);
  }
}


void Item::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  QPointF offset =
      (event->pos() - event->buttonDownPos(Qt::LeftButton)) * transform()
    - QPoint(transform().m31(), transform().m32());

  offset.setX(
        offset.toPoint().x()
      - (offset.toPoint().x() % SchematicScene::GridStep)
    );
  offset.setY(
        offset.toPoint().y()
      - (offset.toPoint().y() % SchematicScene::GridStep)
    );

  if(schematicScene()) {
    schematicScene()->dragSelectedBy(offset);
  } else {
    if(scene()) {
      QList<QGraphicsItem*> items = scene()->selectedItems();
      foreach(QGraphicsItem* item, items)
        item->moveBy(offset.x(), offset.y());
    }
  }

  event->accept();
}


qreal Item::penWidth() const
{
  return
    (defaultPen_.width() > itemPen_.width())
      ? defaultPen_.width() / 2
      : itemPen_.width() / 2;
}


}
