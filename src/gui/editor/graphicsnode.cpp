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
#include "gui/editor/graphicsnode.h"
#include "gui/editor/wire.h"

#include <QGraphicsTextItem>
#include <QGraphicsScene>
#include <QPainter>


namespace qsapecng
{


GraphicsNode::GraphicsNode(
      QGraphicsItem* parent,
      QGraphicsScene* scene
    )
  : QGraphicsItem(parent)
{
  // Qt 4 accepted the scene in the QGraphicsItem constructor
  if(scene && !parent)
    scene->addItem(this);

  owner_ = qgraphicsitem_cast<Item*>(parent);

  dataPointer_ = new Node;
  colliding_ = false;
  label_ = 0;

  path_.addEllipse(
    QRectF(
      QPointF(-Radius, -Radius),
      QPointF(Radius, Radius)
    )
  );

  setAcceptDrops(false);
  setFlags(QGraphicsItem::GraphicsItemFlags());
}


GraphicsNode::~GraphicsNode()
{
  foreach(GraphicsNode* item, itemSet_)
    item->detach(this);

  dataPointer_.detach();
  itemSet_.clear();
}


void GraphicsNode::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget
  )
{
  // a dot only marks a junction: three or more branches meeting. A wire
  // end or a pin is one branch, a point inside a wire two (it goes on);
  // plain bends, wire ends on pins and touching pins get no dot.
  if(isColliding() && branchCount() >= 3) {
    painter->save();
    painter->setBrush(QBrush(theme::sheetForeground(), Qt::SolidPattern));
    painter->setPen(QPen(theme::sheetForeground(), 0));
    painter->drawPath(path_);
    painter->restore();
  }
}


int GraphicsNode::branches() const
{
  if(isTap())
    return 2;
  if(Wire* wire = dynamic_cast<Wire*>(owner_)) {
    const QPointF p = pos();
    if(QLineF(p, wire->fromPoint()).length() > 0.5
        && QLineF(p, wire->toPoint()).length() > 0.5)
      return 2;   // grid point inside a "wire cross" wire
  }
  return 1;
}


int GraphicsNode::branchCount() const
{
  int count = branches();
  foreach(GraphicsNode* other, itemSet_)
    count += other->branches();
  return count;
}


QRectF GraphicsNode::boundingRect() const
{
  return path_.boundingRect();
}


QPainterPath GraphicsNode::shape() const
{
  return path_;
}


void GraphicsNode::attach(GraphicsNode* item)
{
  if(item && item != this) {
    setNode(item->node());
    itemSet_.insert(item);
    colliding_ = true;
    update();   // the dot may appear (one more branch)
  }
}


void GraphicsNode::detach(GraphicsNode* item)
{
  itemSet_.remove(item);
  colliding_ = !itemSet_.isEmpty();
  update();
}


void GraphicsNode::setNode(Node* node)
{
  dataPointer_ = node;
}


const Node* GraphicsNode::constNode() const
{
  return dataPointer_.constData();
}


Node* GraphicsNode::node() const
{
  return dataPointer_.data();
}


void GraphicsNode::updateItemSet()
{
  foreach(GraphicsNode* item, itemSet_)
    item->detach(this);

  dataPointer_.detach();
  itemSet_.clear();

  QList<QGraphicsItem*> collidingItemList = collidingItems();
  foreach(QGraphicsItem* graphicsItem, collidingItemList) {
    GraphicsNode* item =
      qgraphicsitem_cast<GraphicsNode*>(graphicsItem);

    if(item) {
      item->attach(this);
      itemSet_.insert(item);
    }
  }

  colliding_ = !itemSet_.isEmpty();
  update();
}


const QSet<GraphicsNode*>& GraphicsNode::itemSet() const
{
  return itemSet_;
}


bool GraphicsNode::isColliding() const
{
  return colliding_;
}


QVariant GraphicsNode::itemChange(GraphicsItemChange change, const QVariant& value)
{
  if(change == ItemPositionHasChanged)
    updateItemSet();

  return QGraphicsItem::itemChange(change, value);
}


void GraphicsNode::showNodeValue()
{
  // the ground net (0) is already marked by its ground symbols
  if(node()->value() == 0) {
    hideNodeValue();
    return;
  }

  if(!label_) {
    label_ = new QGraphicsTextItem(this);
    label_->setDefaultTextColor(theme::sheetForeground());
    label_->setFont(theme::schematicFont(9));
  }

  label_->setPlainText(QString::number(node()->value()));
  updateLabelPosition();
  label_->setVisible(true);
}


void GraphicsNode::setDisplayedValue(int number)
{
  if(!label_)
    return;
  label_->setPlainText(QString::number(number));
  updateLabelPosition();
}


void GraphicsNode::hideNodeValue()
{
  if(label_)
    label_->setVisible(false);
}


void GraphicsNode::setOwner(Item* owner)
{
  owner_ = owner;
}


Item* GraphicsNode::owner() const
{
  return owner_;
}


void GraphicsNode::updateLabelPosition()
{
  // the first free spot of eight around the node: free of wires (their
  // lines, not their boxes), parts and other labels
  const QRectF text = label_->boundingRect();
  const QPointF centre = boundingRect().center();
  const qreal dx = text.width() / 2 + 1, dy = text.height() / 2 - 1;
  const QPointF offsets[] = {
    QPointF(dx, -dy), QPointF(-dx, -dy), QPointF(dx, dy), QPointF(-dx, dy),
    QPointF(0, -dy - 3), QPointF(dx + 2, 0), QPointF(-dx - 2, 0), QPointF(0, dy + 3)
  };

  auto blocked = [this](const QRectF& sceneRect) {
      if(!scene())
        return false;
      const QRectF probe = sceneRect.adjusted(3, 3, -3, -3);
      foreach(QGraphicsItem* item, scene()->items(probe)) {
        if(item == label_ || item == this || !item->isVisible())
          continue;
        if(Wire* wire = dynamic_cast<Wire*>(item)) {
          const QRectF line = QRectF(wire->mapToScene(wire->fromPoint()),
            wire->mapToScene(wire->toPoint())).normalized().adjusted(-1, -1, 1, 1);
          if(line.intersects(probe))
            return true;
        } else if(dynamic_cast<QGraphicsTextItem*>(item)) {
          return true;     // another label
        } else if(dynamic_cast<Item*>(item)) {
          return true;     // a part
        }
      }
      return false;
    };

  for(const QPointF& offset : offsets) {
    const QPointF pos = centre + offset - text.center();
    label_->setPos(pos);
    if(!blocked(label_->sceneBoundingRect()))
      return;
  }
  label_->setPos(centre + offsets[0] - text.center());
}


}
