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


#ifndef PROPERTYTEXTITEM_H
#define PROPERTYTEXTITEM_H


#include "gui/theme.h"
#include <QGraphicsTextItem>

#include <functional>

#include "gui/qtsolutions/qtpropertyeditor/QtProperty"


namespace qsapecng
{


// Label of a component: its name and, when enabled in the settings
// (theme::showComponentValues()), its value on a second line. The text is
// rebuilt from the component's properties whenever one of them changes.
class PropertyTextItem: public QGraphicsTextItem
{

  Q_OBJECT

public:
  PropertyTextItem(QGraphicsItem* parent = 0);
  PropertyTextItem(const QString& text, QGraphicsItem* parent = 0);

  // name property of the component (its sub-properties hold the value)
  inline void setProperty(QtProperty* property)
    { property_ = property; refreshText(); }
  // unit shown after the value, e.g. "F" (empty: plain number)
  inline void setValueUnit(const QString& unit)
    { unit_ = unit; refreshText(); }

  // rebuild the text from the properties and the current settings
  void refreshText();

  // called when the user drags the label (not when the program moves it)
  std::function<void()> onMoved;
  // the automatic centre of the label (parent coordinates): a dragged
  // label close to it snaps there, as if never moved
  std::function<QPointF()> automaticCentre;

public slots:
  void valueChanged(QtProperty* property, const QString& value);

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value)
    override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
  void init();

  QtProperty* property_;
  QString unit_;
  bool dragging_ = false;   // snap to the grid only while the user drags

};


}


#endif // PROPERTYTEXTITEM_H
