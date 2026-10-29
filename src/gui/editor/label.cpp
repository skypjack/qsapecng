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
#include "gui/editor/label.h"
#include <QGraphicsSceneMouseEvent>
#include "gui/editor/schematicscene.h"

#include <QPainter>


namespace qsapecng
{


Label::Label(QGraphicsItem* parent, SchematicScene* scene)
  : Item(parent, scene)
{
  setFlags(
    QGraphicsItem::ItemIsMovable
    | QGraphicsItem::ItemIsSelectable
    | QGraphicsItem::ItemIsFocusable
  );

  label_.setParentItem(this);
  label_.setDefaultTextColor(theme::sheetForeground());
  label_.setFont(theme::schematicFont(10));
  label_.setPlainText(QObject::tr("Text"));
  // the text is not edited in place: a click selects and drags the
  // label, a double-click edits it (SchematicScene::editLabelText)
  label_.setTextInteractionFlags(Qt::NoTextInteraction);
  label_.setAcceptedMouseButtons(Qt::NoButton);
  label_.setPos(0, 0);
}


QRectF Label::boundingRect() const
{
  qreal offset = penWidth();

  QRectF rect(
      0 - offset,
      0 - offset,
      label_.boundingRect().bottomRight().x() + 2 * offset,
      label_.boundingRect().bottomRight().y() + 2 * offset
    );

  return rect;
}


void Label::paint(
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
    painter->setOpacity(opacity / 2.0);
    painter->drawRect(boundingRect());

    painter->restore();
  }

  // follow the sheet's ink (theme switch); setting it repaints, so only
  // when it changed
  if(label_.defaultTextColor() != theme::sheetForeground())
    label_.setDefaultTextColor(theme::sheetForeground());
  painter->restore();
}


QString Label::text() const
{
  return label_.toPlainText();
}


void Label::setText(const QString& label)
{
  label_.setPlainText(label);
}


void Label::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if(schematicScene())
    schematicScene()->editLabelText(this);
  event->accept();
}


qreal Label::penWidth() const
{
  return
    (defaultPen_.width() > itemPen_.width())
      ? defaultPen_.width() / 2
      : itemPen_.width() / 2;
}


}
