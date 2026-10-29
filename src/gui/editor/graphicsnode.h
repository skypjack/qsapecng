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


#ifndef GRAPHICSNODE_H
#define GRAPHICSNODE_H


#include "gui/editor/inode.h"
#include "gui/editor/item.h"

#include <QGraphicsItem>
#include <QGraphicsTextItem>
#include <QPainterPath>

#include <QExplicitlySharedDataPointer>
#include <QSet>


namespace qsapecng
{


class GraphicsNode: public QGraphicsItem
{

public:
  enum { Type = UserType + 128 };
  enum { Radius = 1 };

public:
  GraphicsNode(
    QGraphicsItem* parent = 0,
    QGraphicsScene* scene = 0
  );
  virtual ~GraphicsNode();

  inline int type() const { return GraphicsNode::Type; }

  void paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget
  );

  QRectF boundingRect() const;
  QPainterPath shape() const;

  void attach(GraphicsNode* item);
  void detach(GraphicsNode* item);

  void setNode(Node* node);
  const Node* constNode() const;
  Node* node() const;

  void updateItemSet();
  const QSet<GraphicsNode*>& itemSet() const;
  bool isColliding() const;

  // a tap is a connection point added in the middle of a wire
  inline bool isTap() const { return tap_; }

  // wires / pins meeting here (see paint)
  int branchCount() const;
  // 2 for a point inside a wire, 1 for a wire end or a pin
  int branches() const;
  inline void setTap(bool tap) { tap_ = tap; }

  void showNodeValue();
  // the label shown for its net, if this node carries it
  inline bool showsNodeValue() const { return label_ && label_->isVisible(); }
  void setDisplayedValue(int number);
  void hideNodeValue();

  void setOwner(Item* owner);
  Item* owner() const;

protected:
  virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value);

private:
  void updateLabelPosition();

private:
  Item* owner_;

  bool colliding_;
  QSet<GraphicsNode*> itemSet_;
  bool tap_ = false;
  QExplicitlySharedDataPointer<Node> dataPointer_;

  QGraphicsTextItem* label_;
  QPainterPath path_;

};


}


#endif // GRAPHICSNODE_H
