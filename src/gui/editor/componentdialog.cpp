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


#include "gui/editor/componentdialog.h"
#include "gui/units.h"

#include "gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.h"
#include "gui/qtsolutions/qtpropertyeditor/qtpropertymanager.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>


namespace qsapecng
{


namespace
{


// What the value of each component type means, for the field label.
struct ValueMeaning
{
  QString label;
  QString unit;
};


ValueMeaning valueMeaning(SchematicScene::SupportedItemType type)
{
  switch(type)
  {
  case SchematicScene::ResistorItemType:
    return { QObject::tr("Resistance"), SchematicScene::valueUnit(type) };
  case SchematicScene::ConductanceItemType:
    return { QObject::tr("Conductance"), SchematicScene::valueUnit(type) };
  case SchematicScene::InductorItemType:
    return { QObject::tr("Inductance"), SchematicScene::valueUnit(type) };
  case SchematicScene::CapacitorItemType:
    return { QObject::tr("Capacitance"), SchematicScene::valueUnit(type) };
  case SchematicScene::VoltageSourceItemType:
  case SchematicScene::SingleVoltageSourceItemType:
    return { QObject::tr("Voltage"), SchematicScene::valueUnit(type) };
  case SchematicScene::CurrentSourceItemType:
    return { QObject::tr("Current"), SchematicScene::valueUnit(type) };
  case SchematicScene::VCVSItemType:
  case SchematicScene::CCCSItemType:
    return { QObject::tr("Gain"), SchematicScene::valueUnit(type) };
  case SchematicScene::VCCSItemType:
    return { QObject::tr("Transconductance"), SchematicScene::valueUnit(type) };
  case SchematicScene::CCVSItemType:
    return { QObject::tr("Transresistance"), SchematicScene::valueUnit(type) };
  case SchematicScene::TransformerItemType:
    return { QObject::tr("Turns ratio"), SchematicScene::valueUnit(type) };
  case SchematicScene::MutualInductanceItemType:
    return { QObject::tr("Mutual inductance"), SchematicScene::valueUnit(type) };
  default:
    return { QObject::tr("Value"), SchematicScene::valueUnit(type) };
  }
}


}


void applyPropertyValue(QtProperty* property, const QVariant& value)
{
  QtAbstractPropertyManager* manager = property->propertyManager();

  if(QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(manager))
    spm->setValue(property, value.toString());
  else if(QtBoolPropertyManager* bpm =
      qobject_cast<QtBoolPropertyManager*>(manager))
    bpm->setValue(property, value.toBool());
}


ComponentDialog::ComponentDialog(
    SchematicScene::SupportedItemType type,
    QtProperty* properties,
    QWidget* parent
  ) : QDialog(parent), symbolicProperty_(0), symbolic_(0)
{
  setWindowTitle(tr("%1 %2")
    .arg(SchematicScene::itemNameByType(type), properties->valueText()));

  QHash<QString, QtProperty*> sub;
  foreach(QtProperty* prop, properties->subProperties())
    sub.insert(prop->propertyName(), prop);

  QFormLayout* form = new QFormLayout;
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  addTextField(properties, tr("Name:"), false, QString(), form);

  const ValueMeaning meaning = valueMeaning(type);
  QLineEdit* valueEdit = 0;
  const QString valueKey =
    (type == SchematicScene::MutualInductanceItemType) ? "M" : "Value";

  if(type == SchematicScene::MutualInductanceItemType) {
    // the two coupled inductors first, then their mutual inductance
    if(sub.contains("lp:name"))
      addTextField(sub.value("lp:name"), tr("Primary (Lp) name:"),
        false, QString(), form);
    if(sub.contains("lp:value"))
      addTextField(sub.value("lp:value"), tr("Primary inductance:"),
        true, QStringLiteral("H"), form);
    if(sub.contains("ls:name"))
      addTextField(sub.value("ls:name"), tr("Secondary (Ls) name:"),
        false, QString(), form);
    if(sub.contains("ls:value"))
      addTextField(sub.value("ls:value"), tr("Secondary inductance:"),
        true, QStringLiteral("H"), form);
  }

  if(type == SchematicScene::OpAmplItemType && sub.contains("Ideal")) {
    // ideal (nullor) or open-loop gain with an optional GBW
    QtBoolPropertyManager* bpm =
      qobject_cast<QtBoolPropertyManager*>(sub.value("Ideal")->propertyManager());
    idealProperty_ = sub.value("Ideal");
    ideal_ = new QCheckBox(tr("Ideal (infinite gain and bandwidth)"));
    ideal_->setChecked(bpm && bpm->value(idealProperty_));
    form->addRow(QString(), ideal_);

    if(QLineEdit* gain = addTextField(sub.value("Value"),
        tr("Open-loop gain:"), true, QString(), form))
    {
      gain->setToolTip(tr("A0 from the datasheet: 100dB, 200V/mV, 1e5V/V "
                          "or a plain number (100k)"));
      realOpAmpFields_ << gain;
    }
    if(sub.contains("GBW")) {
      if(QLineEdit* gbw = addTextField(sub.value("GBW"), tr("GBW:"), true,
          QStringLiteral("Hz"), form))
      {
        gbw->setToolTip(tr("Gain-bandwidth product, e.g. 1MEG or 10M "
                           "(0: no bandwidth limit)"));
        gbw->setPlaceholderText(tr("0 = no limit"));
        realOpAmpFields_ << gbw;
      }
    }
    connect(ideal_, &QCheckBox::toggled,
      this, &ComponentDialog::updateOpAmpFields);
  } else if(type == SchematicScene::MultiTransformerItemType) {
    // turns ratios; ideal, or the primary inductance and the coupling
    if(QLineEdit* n2 = addTextField(sub.value("n2"),
        tr("Turns ratio N2 / N1:"), true, QString(), form))
      n2->setToolTip(tr("Turns of the upper secondary over those of the "
                        "primary"));
    if(QLineEdit* n3 = addTextField(sub.value("n3"),
        tr("Turns ratio N3 / N1:"), true, QString(), form))
      n3->setToolTip(tr("Turns of the lower secondary over those of the "
                        "primary"));
    QtBoolPropertyManager* bpm =
      qobject_cast<QtBoolPropertyManager*>(sub.value("Ideal")->propertyManager());
    idealProperty_ = sub.value("Ideal");
    ideal_ = new QCheckBox(tr("Ideal (turns ratios only)"));
    ideal_->setChecked(bpm && bpm->value(idealProperty_));
    form->addRow(QString(), ideal_);
    if(QLineEdit* lp = addTextField(sub.value("Lp"),
        tr("Primary inductance:"), true, QStringLiteral("H"), form))
    {
      lp->setToolTip(tr("Inductance of the primary; a secondary has it "
                        "times its turns ratio squared"));
      realOpAmpFields_ << lp;
    }
    if(QLineEdit* k = addTextField(sub.value("k"), tr("Coupling k:"), true,
        QString(), form))
    {
      k->setToolTip(tr("Coupling between every two windings, above 0 and "
                       "up to 1 (1: no leakage)"));
      realOpAmpFields_ << k;
    }
    connect(ideal_, &QCheckBox::toggled,
      this, &ComponentDialog::updateOpAmpFields);
  } else if(sub.contains(valueKey)) {
    valueEdit = addTextField(sub.value(valueKey), meaning.label + ":", true,
      meaning.unit, form);

    // R, L, C: ideal or with parasitics
    const QList<SchematicScene::Parasitic> extra =
      SchematicScene::parasitics(type);
    if(!extra.isEmpty() && sub.contains("Ideal")) {
      QtBoolPropertyManager* bpm = qobject_cast<QtBoolPropertyManager*>(
        sub.value("Ideal")->propertyManager());
      idealProperty_ = sub.value("Ideal");
      const bool source = type == SchematicScene::VoltageSourceItemType
        || type == SchematicScene::SingleVoltageSourceItemType;
      ideal_ = new QCheckBox(source ? tr("Ideal (no internal resistance)")
                                    : tr("Ideal (no parasitics)"));
      ideal_->setChecked(bpm && bpm->value(idealProperty_));
      form->addRow(QString(), ideal_);
      foreach(const SchematicScene::Parasitic& par, extra) {
        if(!sub.contains(par.property))
          continue;
        if(QLineEdit* e = addTextField(sub.value(par.property),
            par.property + ":", true, par.unit, form))
        {
          e->setToolTip(par.description + tr(" (0 = none)"));
          e->setPlaceholderText(tr("0 = none"));
          realOpAmpFields_ << e;
        }
      }
      connect(ideal_, &QCheckBox::toggled,
        this, &ComponentDialog::updateOpAmpFields);
    }
  }

  if(sub.contains("Symbolic")) {
    QtBoolPropertyManager* bpm =
      qobject_cast<QtBoolPropertyManager*>(
        sub.value("Symbolic")->propertyManager());
    if(bpm) {
      symbolicProperty_ = sub.value("Symbolic");
      symbolic_ = new QCheckBox(type == SchematicScene::OpAmplItemType
        ? tr("Keep the open-loop gain symbolic (by name) in the result")
        : tr("Keep symbolic in the result"));
      if(type == SchematicScene::OpAmplItemType)
        realOpAmpFields_ << symbolic_;
      symbolic_->setToolTip(
        tr("Checked: the component appears by name in the Result.\n"
           "Unchecked: its numeric value is used there instead.\n"
           "(The Raw section always lists every component by name.)"));
      symbolic_->setChecked(bpm->value(symbolicProperty_));
      form->addRow(QString(), symbolic_);
    }
  }

  buttons_ = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

  QVBoxLayout* layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(buttons_);

  setMinimumWidth(320);
  updateOpAmpFields();
  validate();

  if(!fields_.isEmpty()) {
    // start in the value field when there is one (the usual edit)
    QLineEdit* first = valueEdit ? valueEdit
      : fields_.size() > 1 ? fields_.last().edit : fields_.first().edit;
    first->setFocus();
    first->selectAll();
  }
}


QLineEdit* ComponentDialog::addTextField(
    QtProperty* property,
    const QString& label,
    bool isValue,
    const QString& unit,
    QFormLayout* form
  )
{
  QtStringPropertyManager* spm =
    qobject_cast<QtStringPropertyManager*>(property->propertyManager());
  if(!spm)
    return 0;

  QLineEdit* edit = new QLineEdit(
    isValue ? units::formatForEdit(spm->value(property))
            : spm->value(property));
  if(isValue) {
    edit->setValidator(new units::ValueValidator(unit, edit));
    edit->setToolTip(tr("Number with an optional prefix: 4.7k, 4k7, 100n, "
                        "2.2u (or µ), 1meg, 1e-6"));
  } else {
    // names are used as symbols in the results: no spaces
    edit->setValidator(new QRegularExpressionValidator(
      QRegularExpression(QStringLiteral("\\S+")), edit));
  }
  connect(edit, &QLineEdit::textChanged, this, &ComponentDialog::validate);

  QWidget* field = edit;
  if(!unit.isEmpty()) {
    QWidget* box = new QWidget;
    QHBoxLayout* row = new QHBoxLayout(box);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(edit, 1);
    row->addWidget(new QLabel(unit));
    field = box;
  }

  form->addRow(label, field);
  fields_.append({ property, edit, isValue, unit });
  return edit;
}


void ComponentDialog::updateOpAmpFields()
{
  if(!ideal_)
    return;
  foreach(QWidget* w, realOpAmpFields_)
    w->setEnabled(!ideal_->isChecked());
  validate();
}


void ComponentDialog::validate()
{
  bool ok = true;
  foreach(const Field& f, fields_)
    ok = ok && (f.edit->hasAcceptableInput() || !f.edit->isEnabled());
  buttons_->button(QDialogButtonBox::Ok)->setEnabled(ok);
}


QList<PropertyChange> ComponentDialog::changes() const
{
  QList<PropertyChange> result;

  foreach(const Field& f, fields_) {
    QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(f.property->propertyManager());
    const QString oldValue = spm->value(f.property);
    QString newValue = f.edit->text().trimmed();
    if(f.isValue) {
      // stored as a plain number; unchanged unless the number differs
      double v = 0., old = 0.;
      if(!units::parse(newValue, f.unit, &v))
        continue;
      if(units::parse(oldValue, QString(), &old) && v == old)
        continue;
      newValue = units::toPlain(v);
    }
    if(newValue != oldValue)
      result.append({ f.property, oldValue, newValue });
  }

  if(ideal_) {
    QtBoolPropertyManager* bpm = qobject_cast<QtBoolPropertyManager*>(
      idealProperty_->propertyManager());
    const bool oldValue = bpm->value(idealProperty_);
    if(ideal_->isChecked() != oldValue)
      result.append({ idealProperty_, oldValue, ideal_->isChecked() });
  }

  if(symbolic_) {
    QtBoolPropertyManager* bpm =
      qobject_cast<QtBoolPropertyManager*>(
        symbolicProperty_->propertyManager());
    const bool oldValue = bpm->value(symbolicProperty_);
    if(symbolic_->isChecked() != oldValue)
      result.append({ symbolicProperty_, oldValue, symbolic_->isChecked() });
  }

  return result;
}


}
