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


#include "gui/theme.h"
#include "gui/editor/wire.h"
#include "gui/editor/graphicsnode.h"
#include "gui/editor/schematicscene.h"

#include <QStringList>
#include <QPainter>
#include <QColor>


namespace qsapecng
{


Wire::Wire(
      QGraphicsItem* parent,
      SchematicScene* scene
    )
  : Item(parent, scene), connectedJunctions_(false), mod_(false)
{
  setAcceptDrops(false);
  setWire(QLineF(0, 0, 0, 0));
  itemPen_.setColor(Qt::black);
}


Wire::Wire(
      const QLineF& wire,
      bool connectedJunctions,
      QGraphicsItem* parent,
      SchematicScene* scene
    )
  : Item(parent, scene), connectedJunctions_(connectedJunctions), mod_(false)
{
  setAcceptDrops(false);
  setWire(wire);
  itemPen_.setColor(Qt::black);
}


Wire::~Wire()
{
  foreach(GraphicsNode* node, nodes_) {
    if(node->scene())
      node->scene()->removeItem(node);

    delete node;
  }

  nodes_.clear();
  taps_.clear();
}


QRectF Wire::boundingRect() const
{
  qreal offset = penWidth() + COLOR_BORDER;

  if(orientation_ == Qt::Horizontal)
    return
      QRectF(0 - offset, 0 - offset,
        path_.boundingRect().width() + 2 * offset,
        SchematicScene::GridStep / 2 + 2 * offset);
  else
    return
      QRectF(0 - offset, 0 - offset,
        SchematicScene::GridStep / 2 + 2 * offset,
        path_.boundingRect().height() + 2 * offset);
}


void Wire::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget
  )
{
  painter->save();

  if(isSelected()) {
    painter->save();

    QPen selectionPen = defaultPen_;
    selectionPen.setColor(theme::sheetForeground());
    painter->setPen(selectionPen);
    double opacity = painter->opacity();
    painter->setOpacity(opacity / OpacityFactor);
    painter->drawRect(boundingRect());

    painter->restore();
  }

  QPen pen = itemPen_;
  pen.setColor(net_ >= 0 ? theme::netColor(net_) : theme::sheetForeground());
  painter->setPen(pen);
  painter->drawPath(path_);

  painter->restore();
}


Qt::Orientation Wire::orientation() const
{
  return orientation_;
}


void Wire::setWire(const QLineF& wire)
{
  if(qAbs(wire.angle()) == 0 || qAbs(wire.angle()) == 180) {
    orientation_ = Qt::Horizontal;
  } else if(qAbs(wire.angle()) == 90 || qAbs(wire.angle()) == 270) {
    orientation_ = Qt::Vertical;
  } else {
    if(qAbs(wire.dx()) > qAbs(wire.dy())) {
      orientation_ = Qt::Horizontal;
      wire.p2().setY(wire.p1().y());
    } else {
      orientation_ = Qt::Vertical;
      wire.p2().setX(wire.p1().x());
    }
  }

  QLineF realWire = wire.translated(-wire.p1());
  if(realWire.length() < SchematicScene::GridStep) {
    if(orientation_ == Qt::Horizontal)
      realWire.setLine(0, 0, SchematicScene::GridStep, 0);
    else
      realWire.setLine(0, 0, 0, SchematicScene::GridStep);
  }

  QPainterPath path;
  path.moveTo(0, 0);
  path.lineTo(
    qAbs(realWire.x2()),
    qAbs(realWire.y2())
  );

  path_ = path;
  refreshNodeList();
}


QPointF Wire::fromPoint() const
{
  return QPointF(0, 0);
}


QPointF Wire::toPoint() const
{
  if(orientation_ == Qt::Horizontal)
    return QPointF(path_.boundingRect().width(), 0);
  else
    return QPointF(0, path_.boundingRect().height());
}


bool Wire::isJunctionsConnected() const
{
  return connectedJunctions_;
}


void Wire::setConnectedJunctions(bool state)
{
  connectedJunctions_ = state;
  refreshNodeList();
}


void Wire::rotate()
{
  qreal tmp;
  QPointF from = fromPoint();
  QPointF to = toPoint();

  tmp = from.x();
  from.setX(from.y());
  from.setY(tmp);

  tmp = to.x();
  to.setX(to.y());
  to.setY(tmp);

  bool selected = isSelected();
  setVisible(false);
  setWire(QLineF(from, to));
  setVisible(true);
  setSelected(selected);
}


void Wire::rotateBack()
{
  rotate();
}


void Wire::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if(schematicScene())
    schematicScene()->modifyWire(this);
  else
    Item::mouseDoubleClickEvent(event);
}


void Wire::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  QList<QGraphicsItem*> collidingItemList = collidingItems();

  foreach(GraphicsNode* node, nodes_)
    collidingItemList.removeAll(node);

  foreach(QGraphicsItem* graphicsItem, collidingItemList) {
    Item* item = qgraphicsitem_cast<Item*>(graphicsItem);
    if(item && item->itemType() == Item::Wire) {

      Wire* wire =
        static_cast<Wire*>(graphicsItem);

      // only wires lying on top of this one (after a drag): merely
      // touching ones stay apart, the point between them may be a
      // junction (a click used to glue a split wire back together)
      if(wire->orientation() == orientation_ && schematicScene()) {
        const QPointF a1 = mapToScene(fromPoint()), a2 = mapToScene(toPoint());
        const QPointF b1 = wire->mapToScene(wire->fromPoint());
        const QPointF b2 = wire->mapToScene(wire->toPoint());
        const bool horizontal = orientation_ == Qt::Horizontal;
        const bool sameLine = horizontal ? qAbs(a1.y() - b1.y()) < 0.5
                                         : qAbs(a1.x() - b1.x()) < 0.5;
        const qreal lo = horizontal ? qMax(qMin(a1.x(), a2.x()), qMin(b1.x(), b2.x()))
                                    : qMax(qMin(a1.y(), a2.y()), qMin(b1.y(), b2.y()));
        const qreal hi = horizontal ? qMin(qMax(a1.x(), a2.x()), qMax(b1.x(), b2.x()))
                                    : qMin(qMax(a1.y(), a2.y()), qMax(b1.y(), b2.y()));
        if(sameLine && hi - lo > 0.5)
          schematicScene()->joinWires(this, wire);
      }
    }
  }

  Item::mouseReleaseEvent(event);
}


QVariant Wire::itemChange(GraphicsItemChange change, const QVariant& value)
{
  if(change == ItemPositionHasChanged
        || change == ItemTransformHasChanged
        || change == ItemSceneHasChanged
      ) {
    foreach(GraphicsNode* node, nodes_)
      node->updateItemSet();
  }

  return Item::itemChange(change, value);
}


Wire* Wire::joined(Wire* wire)
{
  bool positionChanged = false;

  QPointF wireFromPoint = mapFromItem(wire, wire->fromPoint());
  QPointF wireToPoint = mapFromItem(wire, wire->toPoint());

  switch(orientation_)
  {
  case Qt::Horizontal:
    if(fromPoint().x() < wireFromPoint.x())
      wireFromPoint = fromPoint();
    else
      positionChanged = true;

    if(toPoint().x() > wireToPoint.x())
      wireToPoint = toPoint();

    wireFromPoint.setY(fromPoint().y());
    wireToPoint.setY(toPoint().y());

    break;
  case Qt::Vertical:
    if(fromPoint().y() < wireFromPoint.y())
      wireFromPoint = fromPoint();
    else
      positionChanged = true;

    if(toPoint().y() > wireToPoint.y())
      wireToPoint = toPoint();

    wireFromPoint.setX(fromPoint().x());
    wireToPoint.setX(toPoint().x());

    break;
  }

  Wire* joined = static_cast<Wire*>(
    SchematicScene::itemByType(SchematicScene::WireItemType));

  joined->setWire(QLineF(wireFromPoint, wireToPoint));
  joined->setConnectedJunctions(connectedJunctions_);
  if(positionChanged)
    joined->setPos(mapToScene(wireFromPoint));
  else
    joined->setPos(mapToScene(fromPoint()));

  return joined;
}


int Wire::propagate(int value)
{
  if(mod_)
    return value;

  mod_ = true;

  int act = SchematicScene::Ground;
  bool found = false;

  foreach(GraphicsNode* node, nodes_) {
    if(node->node()->isValid()) {
      act = node->node()->value();
      found = true;
    }
  }

  if(!found)
    act = value++;

  foreach(GraphicsNode* node, nodes_) {
    node->node()->setValue(act);

    foreach(GraphicsNode* setnode, node->itemSet())
      if(setnode->owner()->itemType() == Item::Wire)
        static_cast<Wire*>(setnode->owner())->propagate(act);

  }

  if(!found)
    nodes_.front()->showNodeValue();

  // the colour itself is looked up at paint time (follows the theme)
  net_ = act;
  itemPen_.setWidth(itemPen_.width() + COLOR_BORDER);
  update();

  return value;
}


void Wire::invalidate()
{
  foreach(GraphicsNode* node, nodes_) {
    node->node()->setDirty();
    node->hideNodeValue();
  }

  if(mod_) {
    itemPen_.setWidth(itemPen_.width() - COLOR_BORDER);
    net_ = -1;
    mod_ = false;
    update();
  }
}


void Wire::refreshNodeList()
{
  foreach(GraphicsNode* node, nodes_) {
    if(node->scene())
      node->scene()->removeItem(node);

    delete node;
  }

  nodes_.clear();
  taps_.clear();

  if(connectedJunctions_) {
    if(orientation_ == Qt::Horizontal) {
      int size =
        qAbs((toPoint().x() - fromPoint().x()) / SchematicScene::GridStep) + 1;

      for(int x = 0; x < size; ++x) {
        GraphicsNode* node = new GraphicsNode(this);
        node->setOwner(this);
        node->setPos(QPointF(x * SchematicScene::GridStep, 0));
        nodes_.append(node);
      }
    } else {
      int size =
        qAbs((toPoint().y() - fromPoint().y()) / SchematicScene::GridStep) + 1;

      for(int y = 0; y < size; ++y) {
        GraphicsNode* node = new GraphicsNode(this);
        node->setOwner(this);
        node->setPos(QPointF(0, y * SchematicScene::GridStep));
        nodes_.append(node);
      }
    }
  } else {
    GraphicsNode* from = new GraphicsNode(this);
    GraphicsNode* to = new GraphicsNode(this);
    from->setOwner(this);
    to->setOwner(this);
    from->setPos(fromPoint());
    to->setPos(toPoint());
    nodes_.append(from);
    nodes_.append(to);
  }
}


bool Wire::updateTaps()
{
  QGraphicsScene* sc = scene();
  if(!sc || connectedJunctions_)
    return false;

  // interior grid points of the segment, in local coordinates
  QList<QPointF> wanted;
  const QPointF a = fromPoint();
  const QPointF b = toPoint();
  const int step = SchematicScene::GridStep;

  QList<QPointF> interior;
  if(orientation_ == Qt::Horizontal) {
    const qreal lo = qMin(a.x(), b.x()), hi = qMax(a.x(), b.x());
    for(qreal x = lo + step; x < hi - step / 2.; x += step)
      interior.append(QPointF(x, a.y()));
  } else {
    const qreal lo = qMin(a.y(), b.y()), hi = qMax(a.y(), b.y());
    for(qreal y = lo + step; y < hi - step / 2.; y += step)
      interior.append(QPointF(a.x(), y));
  }

  foreach(const QPointF& p, interior) {
    const QPointF sp = mapToScene(p);
    foreach(QGraphicsItem* graphicsItem, sc->items(sp)) {
      GraphicsNode* other = qgraphicsitem_cast<GraphicsNode*>(graphicsItem);
      if(!other || other->parentItem() == this || other->isTap())
        continue;
      if(QLineF(other->scenePos(), sp).length() < 0.5) {
        wanted.append(p);
        break;
      }
    }
  }

  QList<QPointF> current;
  foreach(GraphicsNode* tap, taps_)
    current.append(tap->pos());
  if(current == wanted)
    return false;

  foreach(GraphicsNode* tap, taps_) {
    nodes_.removeAll(tap);
    if(tap->scene())
      tap->scene()->removeItem(tap);
    delete tap;
  }
  taps_.clear();

  foreach(const QPointF& p, wanted) {
    GraphicsNode* tap = new GraphicsNode(this);
    tap->setOwner(this);
    tap->setTap(true);
    tap->setPos(p);
    tap->updateItemSet();
    nodes_.append(tap);
    taps_.append(tap);
  }

  return true;
}


}
