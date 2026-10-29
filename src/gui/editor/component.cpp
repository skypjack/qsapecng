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
#include "gui/editor/itemtransform.h"
#include "gui/editor/component.h"
#include <QTransform>
#include <QLineF>
#include "gui/editor/graphicsnode.h"
#include "gui/editor/wire.h"

#include <QGraphicsScene>
#include <QPainter>
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>


namespace qsapecng
{


Component::Component(
    QGraphicsItem* parent,
    SchematicScene* scene
  ) : Item(parent, scene)
{
  label_ = new PropertyTextItem("", this);
  setupLabel();
}


Component::Component(
    QPainterPath path,
    QGraphicsItem* parent,
    SchematicScene* scene
  ) : Item(parent, scene), path_(path)
{
  label_ = new PropertyTextItem("", this);
  setupLabel();
}


Component::Component(
    QPainterPath path,
    QList<QPointF> nodes,
    QGraphicsItem* parent,
    SchematicScene* scene
  ) : Item(parent, scene), path_(path)
{
  label_ = new PropertyTextItem("", this);
  setupLabel();

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
  setupLabel();

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

  // captions may sit beside the symbol (Lp, Ls of a mutual inductance);
  // a square box covers them in any rotation
  for(const auto& c : captions_)
    rect = rect.united(QRectF(c.first - QPointF(12, 12), QSizeF(24, 24)));

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

    QPen selectionPen = defaultPen_;
    selectionPen.setColor(theme::sheetForeground());
    painter->setPen(selectionPen);
    double opacity = painter->opacity();
    painter->setOpacity(opacity / OpacityFactor);
    painter->drawRect(boundingRect());

    painter->restore();
  }

  if(!tag_.isEmpty()) {
    // filled tag with an upright caption
    // same ink as the rest of the sheet
    const QColor border = theme::sheetForeground();
    const QColor fill = theme::sheetBackground();
    const QColor text = theme::sheetForeground();

    QPen pen = itemPen_;
    pen.setColor(border);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);
    painter->setBrush(fill);
    painter->drawPath(path_);

    // the text box: the tag without its pointed end
    const QRectF box = path_.boundingRect().adjusted(7, 0, 0, 0);
    const QTransform t = transform();
    const QTransform upright =
      QTransform(t.m11(), t.m12(), t.m21(), t.m22(), 0, 0).inverted();
    // whole pixels (at 100 %): a caption centred on a half pixel is
    // smeared and looks grey
    painter->translate(QPointF(qRound(box.center().x()), qRound(box.center().y())));
    painter->setTransform(upright, true);
    // upright now; a tag standing on end (rotated a quarter turn) has its
    // caption along it, read from the bottom up
    if(qAbs(t.m11()) < 0.5)
      painter->rotate(-90);
    const QRectF r(-box.width() / 2, -box.height() / 2, box.width(), box.height());
    // the schematic font, sized in pixels so the caption fits the tag the
    // same way on every platform (point sizes differ with the screen DPI)
    QFont font = theme::schematicFont(10);
    font.setPixelSize(qMax(6, qRound(box.height() * 0.7)));
    painter->setFont(font);
    painter->setPen(text);
    painter->drawText(r, Qt::AlignCenter, tag_);

    painter->restore();
    return;
  }

  QPen pen = itemPen_;
  pen.setColor(theme::sheetForeground());
  painter->setPen(pen);
  painter->drawPath(path_);

  // upright drawings inside the symbol (the meters' V and A)
  if(!glyphs_.isEmpty()) {
    const QTransform t = transform();
    const QTransform upright =
      QTransform(t.m11(), t.m12(), t.m21(), t.m22(), 0, 0).inverted();
    for(const auto& g : glyphs_) {
      painter->save();
      painter->translate(g.first);
      painter->setTransform(upright, true);
      painter->drawPath(g.second);
      painter->restore();
    }
  }

  // small upright captions inside the symbol (e.g. Lp / Ls)
  if(!captions_.isEmpty()) {
    const QTransform t = transform();
    const QTransform upright =
      QTransform(t.m11(), t.m12(), t.m21(), t.m22(), 0, 0).inverted();
    QFont font = theme::schematicFont(8);
    font.setPixelSize(8);
    painter->setFont(font);
    painter->setPen(theme::sheetForeground());
    for(const auto& c : captions_) {
      painter->save();
      painter->translate(c.first);
      painter->setTransform(upright, true);
      painter->drawText(QRectF(-12, -6, 24, 12), Qt::AlignCenter, c.second);
      painter->restore();
    }
  }

  painter->restore();
}


void Component::setReversed(bool reversed)
{
  if(reversed == reversed_ || nodeList_.size() != 2)
    return;
  reversed_ = reversed;
  // mirror across the perpendicular bisector of the pins
  const QPointF a = nodeList_.at(0)->pos(), b = nodeList_.at(1)->pos();
  const QPointF mid = (a + b) / 2;
  QTransform t;
  if(qAbs(a.x() - b.x()) < 0.5)      // pins one above the other
    t = QTransform(1, 0, 0, -1, 0, 2 * mid.y());
  else
    t = QTransform(-1, 0, 0, 1, 2 * mid.x(), 0);
  prepareGeometryChange();
  path_ = t.map(path_);
  // the upright marks (+ and -) change places too
  for(auto& g : glyphs_)
    g.first = t.map(g.first);
  update();
}


void Component::addGlyph(const QPointF& centre, const QPainterPath& glyph)
{
  glyphs_.append(qMakePair(centre, glyph));
  update();
}


void Component::addCaption(const QPointF& centre, const QString& text)
{
  captions_.append(qMakePair(centre, text));
  update();
}


void Component::setTag(const QString& text, const QRectF& pivot)
{
  tag_ = text;
  pivot_ = pivot;
  update();
}


QRectF Component::pivotRect() const
{
  if(pivot_.isNull())
    return boundingRect();
  const qreal offset = penWidth();
  return pivot_.adjusted(-offset, -offset, offset, offset);
}


void Component::mirror()
{
  if(!angle() || angle() == 180) {
    foreach(GraphicsNode* node, nodeList_)
      scaleItem(node, -1, 1);
  } else {
    foreach(GraphicsNode* node, nodeList_)
      scaleItem(node, 1, -1);
  }

  Item::mirror();
}


void Component::rotate()
{
  foreach(GraphicsNode* node, nodeList_)
    rotateItem(node, -90);

  Item::rotate();
}


void Component::addNodes(QList<QPointF> nodes)
{
  foreach(QPointF node, nodes)
    createNode(node);
  updateLabelPosition();
}


void Component::addNodes(QPointF node)
{
  createNode(node);
  updateLabelPosition();
}


QList<QPointF> Component::pinPoints() const
{
  QList<QPointF> points;
  foreach(GraphicsNode* node, nodeList_)
    points.append(node->pos());
  return points;
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
      ) {
    foreach(GraphicsNode* node, nodeList_)
      node->updateItemSet();
  }

  if(change == ItemTransformHasChanged)
    updateLabelPosition();   // rotated or mirrored

  return Item::itemChange(change, value);
}


void Component::setupLabel()
{
  // re-centre when the text changes (rename, value display)
  QObject::connect(label_->document(), &QTextDocument::contentsChanged,
    label_, [this]() { updateLabelPosition(); });
  // ...and when its box changes size (text width set after the text)
  QObject::connect(label_->document()->documentLayout(),
    &QAbstractTextDocumentLayout::documentSizeChanged,
    label_, [this]() { updateLabelPosition(); });

  label_->automaticCentre = [this]() { return automaticLabelCentre(); };
  label_->onMoved = [this]() {
    if(positioningLabel_)
      return;
    // dragged by the user: remember its centre (back on the automatic
    // place: automatic again)
    const QPointF centre = label_->pos()
      + label_->transform().map(label_->boundingRect().center());
    labelMoved_ = QLineF(centre, automaticLabelCentre()).length() > 0.5;
    labelAnchor_ = centre;
  };
}


QPointF Component::automaticLabelCentre() const
{
  const QTransform t = transform();
  const QTransform orientation(t.m11(), t.m12(), t.m21(), t.m22(), 0, 0);
  return defaultLabelAnchor(label_->boundingRect().size(), orientation);
}


void Component::resetLabelPosition()
{
  labelMoved_ = false;
  updateLabelPosition();
}


void Component::setLabelAnchor(const QPointF& anchor)
{
  labelAnchor_ = anchor;
  labelMoved_ = true;
  updateLabelPosition();
}


QPointF Component::defaultLabelAnchor(
    const QSizeF& textSize, const QTransform& orientation) const
{
  // the drawn body: some symbols carry invisible points (tiny rectangles)
  // that pad their box for rotation; the label goes beside what is drawn
  QRectF body;
  foreach(const QPolygonF& poly, path_.toSubpathPolygons()) {
    const QRectF r = poly.boundingRect();
    if(r.width() < 1e-6 && r.height() < 1e-6)
      continue;
    body = body.isNull() ? r : body.united(r);
  }
  if(body.isNull())
    body = path_.boundingRect();
  // the centre across the terminals stays that of the full symbol (the
  // label is centred on the part)
  const QPointF centre = body.center();

  // beside the body, perpendicular to the terminals: to the right of a
  // symbol whose two terminals are one above the other (sources, meters),
  // above every other symbol
  QPointF side(0, -1);
  qreal halfBody = centre.y() - body.top();
  if(nodeList_.size() == 2
      && qFuzzyCompare(nodeList_.at(0)->pos().x() + 1.,
                       nodeList_.at(1)->pos().x() + 1.))
  {
    side = QPointF(1, 0);
    halfBody = body.right() - centre.x();
  }

  // the text is upright on the sheet: its extent along the side direction
  // depends on how the component is oriented
  const QPointF onSheet = orientation.map(side);
  const qreal halfText = qAbs(onSheet.x()) * textSize.width() / 2.
                       + qAbs(onSheet.y()) * textSize.height() / 2.;

  // the text box has a little air of its own (document margin, line
  // spacing): a small gap is enough above a part; beside a round symbol
  // (sources, meters) a little more, or the first letter touches it
  const qreal gap = side.x() != 0. ? 3. : 1.;
  return centre + side * (halfBody + gap + halfText);
}


void Component::updateLabelPosition()
{
  if(!label_ || positioningLabel_)
    return;

  // orientation of the component (rotation/mirroring, no translation)
  const QTransform t = transform();
  const QTransform orientation(t.m11(), t.m12(), t.m21(), t.m22(), 0, 0);
  // keep the text upright and unmirrored on the sheet
  const QTransform upright = orientation.inverted();

  const QRectF text = label_->boundingRect();
  const QPointF anchor = labelMoved_
    ? labelAnchor_ : defaultLabelAnchor(text.size(), orientation);

  positioningLabel_ = true;
  label_->setTransform(upright);
  label_->setPos(anchor - upright.map(text.center()));
  positioningLabel_ = false;
}


void Component::createNode(const QPointF& point)
{
  GraphicsNode* node = new GraphicsNode(this);
  node->setOwner(this);
  node->setPos(point);
  nodeList_.append(node);
}


}
