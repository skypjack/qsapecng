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


#ifndef COMPONENTDIALOG_H
#define COMPONENTDIALOG_H


#include "gui/editor/schematicscene.h"

#include <QDialog>
#include <QList>
#include <QVariant>


class QCheckBox;
class QDialogButtonBox;
class QFormLayout;
class QLineEdit;
class QtProperty;


namespace qsapecng
{


// One property edit made in a ComponentDialog: string properties hold a
// QString, the "Symbolic" flag a bool.
struct PropertyChange
{
  QtProperty* property;
  QVariant oldValue;
  QVariant newValue;
};


// Applies a property value through its QtStringPropertyManager or
// QtBoolPropertyManager, so labels, the property browser and the
// "modified" state follow as for any other edit.
void applyPropertyValue(QtProperty* property, const QVariant& value);


// Form for the parameters of a component (name, value, symbolic flag and,
// for a mutual inductance, the two coupled inductors), opened by
// double-clicking the component.
class ComponentDialog: public QDialog
{

  Q_OBJECT

public:
  ComponentDialog(
      SchematicScene::SupportedItemType type,
      QtProperty* properties,
      QWidget* parent = 0
    );

  // the edits to apply (only properties whose value changed)
  QList<PropertyChange> changes() const;

private slots:
  void validate();
  void updateOpAmpFields();

private:
  struct Field
  {
    QtProperty* property;
    QLineEdit* edit;
    bool isValue;
    QString unit;
  };

  QLineEdit* addTextField(QtProperty* property, const QString& label,
      bool isValue, const QString& unit, QFormLayout* form);

  QList<Field> fields_;
  QtProperty* symbolicProperty_;
  QCheckBox* symbolic_;
  QtProperty* idealProperty_ = 0;   // op-amp only
  QCheckBox* ideal_ = 0;
  QList<QWidget*> realOpAmpFields_;
  QDialogButtonBox* buttons_;

};


}


#endif // COMPONENTDIALOG_H
