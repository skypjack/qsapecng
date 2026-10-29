/*
    QSapecNG - Qt based SapecNG GUI front-end
    Copyright (C) 2009, Michele Caini
    Copyright (C) 2026, Wojciech Kaczmarski

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


#include "gui/editor/propertytextitem.hpp"
#include <QLineF>
#include "gui/units.h"
#include "gui/editor/schematicscene.h"

#include <QGraphicsSceneMouseEvent>
#include <cmath>

#include "gui/qtsolutions/qtpropertyeditor/qtpropertymanager.h"

#include <QTextDocument>
#include <QTextOption>



namespace qsapecng
{


PropertyTextItem::PropertyTextItem(QGraphicsItem* parent)
  : QGraphicsTextItem(parent), property_(0)
{
  init();
}


PropertyTextItem::PropertyTextItem(const QString& text, QGraphicsItem* parent)
  : QGraphicsTextItem(text, parent), property_(0)
{
  init();
}


void PropertyTextItem::init()
{
  setFont(theme::schematicFont(10));
  // the sheet follows the theme: don't inherit the widget text colour
  setDefaultTextColor(theme::sheetForeground());
  setTextInteractionFlags(Qt::NoTextInteraction);
  setFlags(QGraphicsItem::ItemIsMovable
    | QGraphicsItem::ItemSendsGeometryChanges);

  // tight box (the label is centred on its anchor) and centred lines
  document()->setDocumentMargin(1);
  QTextOption option = document()->defaultTextOption();
  option.setAlignment(Qt::AlignHCenter);
  document()->setDefaultTextOption(option);
}


void PropertyTextItem::refreshText()
{
  if(!property_)
    return;

  QString text = property_->valueText();

  // op-amp: nothing for an ideal one, "G=200k" (and the GBW) otherwise
  QtProperty* ideal = 0;
  QtProperty* gbw = 0;
  foreach(QtProperty* sub, property_->subProperties()) {
    if(sub->propertyName() == QLatin1String("Ideal")) ideal = sub;
    if(sub->propertyName() == QLatin1String("GBW")) gbw = sub;
  }
  // (R, L and C also have "Ideal"; only an op-amp has a GBW)
  if(ideal && gbw && theme::showComponentValues()) {
    if(ideal->valueText() != tr("True")
        && ideal->valueText() != QLatin1String("True"))
    {
      foreach(QtProperty* sub, property_->subProperties())
        if(sub->propertyName() == QLatin1String("Value"))
          text += QStringLiteral("\nG=") + units::format(sub->valueText(), "");
      if(gbw && gbw->valueText().toDouble() > 0)
        text += QStringLiteral("\n") + units::format(gbw->valueText(), "Hz");
    }
  } else if(theme::showComponentValues()) {
    // the value of a component is its "Value" (or, for a mutual
    // inductance, "M") sub-property
    foreach(QtProperty* sub, property_->subProperties()) {
      const QString name = sub->propertyName();
      if(name == QLatin1String("Value") || name == QLatin1String("M")) {
        QString value = units::format(sub->valueText(), unit_);
        if(name == QLatin1String("M"))
          value.prepend(QStringLiteral("M = "));
        text += QLatin1Char('\n') + value;
        break;
      }
    }
    // a voltage source with an internal resistance: "Rs = 50 Ω" below
    if(unit_ == QLatin1String("V") && ideal
        && ideal->valueText() != tr("True")
        && ideal->valueText() != QLatin1String("True"))
    {
      foreach(QtProperty* sub, property_->subProperties()) {
        if(sub->propertyName() == QLatin1String("Rs")
            && sub->valueText().toDouble() > 0)
          text += QStringLiteral("\nRs = ")
            + units::format(sub->valueText(), QStringLiteral("Ω"));
      }
    }
  }

  if(text != toPlainText()) {
    setPlainText(text);
    // centred lines need a text width
    setTextWidth(-1);
    setTextWidth(document()->idealWidth());
  }
}


void PropertyTextItem::valueChanged(QtProperty* property, const QString&)
{
  if(!property_)
    return;
  if(property == property_ || property_->subProperties().contains(property))
    refreshText();
}


void PropertyTextItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  dragging_ = true;
  QGraphicsTextItem::mousePressEvent(event);
}


void PropertyTextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  dragging_ = false;
  QGraphicsTextItem::mouseReleaseEvent(event);
}


QVariant PropertyTextItem::itemChange(
    GraphicsItemChange change, const QVariant& value)
{
  // dragged by the user: the centre of the label goes to a grid point
  if(change == ItemPositionChange && dragging_ && parentItem()) {
    const QPointF pos = value.toPointF();
    const QPointF centre = pos + transform().map(boundingRect().center());
    const QPointF onSheet = parentItem()->mapToScene(centre);
    const qreal step = SchematicScene::GridStep;
    // near its automatic place (centred on the part): there
    if(automaticCentre) {
      const QPointF home = automaticCentre();
      if(QLineF(onSheet, parentItem()->mapToScene(home)).length() < 0.8 * step)
        return pos + (home - centre);
    }
    const QPointF snapped(std::round(onSheet.x() / step) * step,
                          std::round(onSheet.y() / step) * step);
    return pos + (parentItem()->mapFromScene(snapped) - centre);
  }

  if(change == ItemPositionHasChanged && onMoved)
    onMoved();
  return QGraphicsTextItem::itemChange(change, value);
}


}
