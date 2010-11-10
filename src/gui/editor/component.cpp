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


#include "gui/editor/component.h"
#include "gui/editor/graphicsnode.h"
#include "gui/editor/wire.h"

#include <QtGui/QGraphicsScene>
#include <QtGui/QPainter>


namespace qsapecng
{


Component::Component(
    QGraphicsItem* parent,
    SchematicScene* scene
  ) : Item(parent, scene)
{
  label_ = new PropertyTextItem("", this);
}


Component::Component(
    QPainterPath path,
    QGraphicsItem* parent,
    SchematicScene* scene
  ) : Item(parent, scene), path_(path)
{
  label_ = new PropertyTextItem("", this);
}


Component::Component(
    QPainterPath path,
    QList<QPointF> nodes,
    QGraphicsItem* parent,
    SchematicScene* scene
  ) : Item(parent, scene), path_(path)
{
  label_ = new PropertyTextItem("", this);

  foreach(QPointF node, nodes)
    createNode(node);
}


Component::Component(
    QPainterPath path,
    QPointF node,
    QGraphicsItem* parent,
    SchematicScene* scene
  ) : Item(parent, scene), path_(path), label_(0)
{
  label_ = new PropertyTextItem("", this);

  createNode(node);
}


Component::~Component()
{
  foreach(GraphicsNode* node, nodeList_) {
    if(node->scene())
      node->scene()->removeItem(node);

    delete node;
  }

  nodeList_.clear();
}


QRectF Component::boundingRect() const
{
  qreal offset = penWidth();
  QRectF rect = path_.boundingRect();

  rect.setX(rect.x() - offset);
  rect.setY(rect.y() - offset);
  rect.setWidth(rect.width() + 2 * offset);
  rect.setHeight(rect.height() + 2 * offset);

  return rect;
}


void Component::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget
  )
{
  painter->save();

  if(isSelected()) {
    painter->save();

    painter->setPen(defaultPen_);
    double opacity = painter->opacity();
    painter->setOpacity(opacity / OpacityFactor);
    painter->drawRect(boundingRect());

    painter->restore();
  }

  painter->setPen(itemPen_);
  painter->drawPath(path_);

  painter->restore();
}


void Component::mirror()
{
  if(!angle() || angle() == 180) {
    if(label_)
      label_->scale(-1, 1);
    foreach(GraphicsNode* node, nodeList_)
      node->scale(-1, 1);
  } else {
    if(label_)
      label_->scale(1, -1);
    foreach(GraphicsNode* node, nodeList_)
      node->scale(1, -1);
  }

  Item::mirror();
}


void Component::rotate()
{
  foreach(GraphicsNode* node, nodeList_)
    node->rotate(-90);

  if(label_)
    label_->rotate(-90);

  Item::rotate();
}


void Component::addNodes(QList<QPointF> nodes)
{
  foreach(QPointF node, nodes)
    createNode(node);
}


void Component::addNodes(QPointF node)
{
  createNode(node);
}


QVector<int> Component::nodes() const
{
  QVector<int> values;
  foreach(GraphicsNode* node, nodeList_)
    values.push_back(node->constNode()->value());

  return values;
}


int Component::propagate(int value)
{
  foreach(GraphicsNode* node, nodeList_) {
    if(!node->node()->isValid()) {
      node->node()->setValue(value++);

      foreach(GraphicsNode* setnode, node->itemSet())
	if(setnode->owner()->itemType() == Item::Wire)
	  static_cast<qsapecng::Wire*>(setnode->owner())->propagate(value);

      node->showNodeValue();
    }
  }

  return value;
}


void Component::invalidate()
{
  foreach(GraphicsNode* node, nodeList_) {
    node->node()->setDirty();
    node->hideNodeValue();
  }
}


void Component::focusInEvent(QFocusEvent* event)
{
  update();
  Item::focusInEvent(event);
}


void Component::focusOutEvent(QFocusEvent* event)
{
  update();
  Item::focusOutEvent(event);
}


QVariant Component::itemChange(GraphicsItemChange change, const QVariant& value)
{
  if(change == ItemPositionHasChanged
        || change == ItemTransformHasChanged
        || change == ItemSceneHasChanged
      )
    foreach(GraphicsNode* node, nodeList_)
      node->updateItemSet();

  return Item::itemChange(change, value);
}


void Component::createNode(const QPointF& point)
{
  GraphicsNode* node = new GraphicsNode(this);
  node->setOwner(this);
  node->setPos(point);
  nodeList_.append(node);
}


}
