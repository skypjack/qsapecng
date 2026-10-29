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
#include "gui/sidebarmodel.h"

#include <QHash>

#include "gui/editor/schematicscene.h"

#include <QMap>
#include <QMimeData>


namespace qsapecng
{


SideBarModel::SideBarModel(QObject* parent)
  : QAbstractListModel(parent)
{
  setupSideBar();
}


int SideBarModel::rowCount(const QModelIndex& parent) const
{
  return componentList_.size();
}


QVariant SideBarModel::data(const QModelIndex& index, int role) const
{
  if(index.isValid())
    return (componentList_.at(index.row()))[role];

  return QVariant();
}


QVariant SideBarModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role
  ) const
{
  if(orientation != Qt::Horizontal)
    return QVariant();

  return tr("Components");
}


Qt::ItemFlags SideBarModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags defaultFlags = QAbstractListModel::flags(index);

  if(index.isValid())
    return Qt::ItemIsDragEnabled | defaultFlags;
  else
    return defaultFlags;
}


Qt::DropActions SideBarModel::supportedDropActions() const
{
  return Qt::IgnoreAction;
}


QStringList SideBarModel::mimeTypes() const
{
  QStringList types;
  types << "application/qsapecng-itemtype";

  return types;
}


QMimeData* SideBarModel::mimeData(const QModelIndexList& indexes) const
{
  QMimeData* mimeData = new QMimeData;

  QModelIndex index;
  if(!indexes.isEmpty())
    index = indexes.first();

  if(index.isValid()) {
    int type = (componentList_.at(index.row())[Qt::UserRole]).toInt();
    QByteArray encodedData = QByteArray::number(type);
    mimeData->setData("application/qsapecng-itemtype", encodedData);
  }

  return mimeData;
}


void SideBarModel::setupSideBar()
{
  QMap<int, QVariant> resistorItemData;
  resistorItemData[Qt::DisplayRole] = tr("Resistor");
  resistorItemData[Qt::DecorationRole] =
    theme::symbolPixmap(theme::resistorSymbolName(), QSize(32, 32));
  resistorItemData[Qt::ToolTipRole] = tr("Resistor Item");
  resistorItemData[Qt::UserRole] = SchematicScene::ResistorItemType;
  componentList_.append(resistorItemData);

  QMap<int, QVariant> conductanceItemData;
  conductanceItemData[Qt::DisplayRole] = tr("Conductance");
  conductanceItemData[Qt::DecorationRole] =
    theme::symbolPixmap(theme::resistorSymbolName(), QSize(32, 32));
  conductanceItemData[Qt::ToolTipRole] = tr("Conductance Item");
  conductanceItemData[Qt::UserRole] = SchematicScene::ConductanceItemType;
  componentList_.append(conductanceItemData);

  QMap<int, QVariant> inductorItemData;
  inductorItemData[Qt::DisplayRole] = tr("Inductor");
  inductorItemData[Qt::DecorationRole] = theme::symbolPixmap("inductor", QSize(32, 32));
  inductorItemData[Qt::ToolTipRole] = tr("Inductor Item");
  inductorItemData[Qt::UserRole] = SchematicScene::InductorItemType;
  componentList_.append(inductorItemData);

  QMap<int, QVariant> capacitorItemData;
  capacitorItemData[Qt::DisplayRole] = tr("Capacitor");
  capacitorItemData[Qt::DecorationRole] = theme::symbolPixmap("capacitor", QSize(32, 32));
  capacitorItemData[Qt::ToolTipRole] = tr("Capacitor Item");
  capacitorItemData[Qt::UserRole] = SchematicScene::CapacitorItemType;
  componentList_.append(capacitorItemData);

  QMap<int, QVariant> voltageSourceItemData;
  voltageSourceItemData[Qt::DisplayRole] = tr("Voltage Source");
  voltageSourceItemData[Qt::DecorationRole] =
    theme::symbolPixmap("voltage", QSize(32, 32));
  voltageSourceItemData[Qt::ToolTipRole] = tr("Voltage Source Item");
  voltageSourceItemData[Qt::UserRole] = SchematicScene::VoltageSourceItemType;
  componentList_.append(voltageSourceItemData);

  QMap<int, QVariant> singleSourceItemData;
  singleSourceItemData[Qt::DisplayRole] = tr("Voltage Source (1-pin)");
  singleSourceItemData[Qt::DecorationRole] =
    theme::symbolPixmap("voltage-1pin", QSize(32, 32));
  singleSourceItemData[Qt::ToolTipRole] =
    tr("Voltage source with one pin; the other end is the ground");
  singleSourceItemData[Qt::UserRole] =
    SchematicScene::SingleVoltageSourceItemType;
  componentList_.append(singleSourceItemData);

  QMap<int, QVariant> currentSourceItemData;
  currentSourceItemData[Qt::DisplayRole] = tr("Current Source");
  currentSourceItemData[Qt::DecorationRole] =
    theme::symbolPixmap("current", QSize(32, 32));
  currentSourceItemData[Qt::ToolTipRole] = tr("Current Source Item");
  currentSourceItemData[Qt::UserRole] = SchematicScene::CurrentSourceItemType;
  componentList_.append(currentSourceItemData);

  QMap<int, QVariant> vcvsItemData;
  vcvsItemData[Qt::DisplayRole] = tr("VCVS");
  vcvsItemData[Qt::DecorationRole] = theme::symbolPixmap("vcvs", QSize(32, 32));
  vcvsItemData[Qt::ToolTipRole] = tr("VCVS Item");
  vcvsItemData[Qt::UserRole] = SchematicScene::VCVSItemType;
  componentList_.append(vcvsItemData);

  QMap<int, QVariant> ccvsItemData;
  ccvsItemData[Qt::DisplayRole] = tr("CCVS");
  ccvsItemData[Qt::DecorationRole] = theme::symbolPixmap("ccvs", QSize(32, 32));
  ccvsItemData[Qt::ToolTipRole] = tr("CCVS Item");
  ccvsItemData[Qt::UserRole] = SchematicScene::CCVSItemType;
  componentList_.append(ccvsItemData);

  QMap<int, QVariant> vccsItemData;
  vccsItemData[Qt::DisplayRole] = tr("VCCS");
  vccsItemData[Qt::DecorationRole] = theme::symbolPixmap("vccs", QSize(32, 32));
  vccsItemData[Qt::ToolTipRole] = tr("VCCS Item");
  vccsItemData[Qt::UserRole] = SchematicScene::VCCSItemType;
  componentList_.append(vccsItemData);

  QMap<int, QVariant> cccsItemData;
  cccsItemData[Qt::DisplayRole] = tr("CCCS");
  cccsItemData[Qt::DecorationRole] = theme::symbolPixmap("cccs", QSize(32, 32));
  cccsItemData[Qt::ToolTipRole] = tr("CCCS Item");
  cccsItemData[Qt::UserRole] = SchematicScene::CCCSItemType;
  componentList_.append(cccsItemData);

  QMap<int, QVariant> opAmplItemData;
  opAmplItemData[Qt::DisplayRole] = tr("Operational Amplifier");
  opAmplItemData[Qt::DecorationRole] = theme::symbolPixmap("opampl", QSize(32, 32));
  opAmplItemData[Qt::ToolTipRole] = tr("Operational Amplifier Item");
  opAmplItemData[Qt::UserRole] = SchematicScene::OpAmplItemType;
  componentList_.append(opAmplItemData);

  QMap<int, QVariant> nullorItemData;
  nullorItemData[Qt::DisplayRole] = tr("Nullor");
  nullorItemData[Qt::DecorationRole] = theme::symbolPixmap("nullor", QSize(32, 32));
  nullorItemData[Qt::ToolTipRole] = tr("Nullor: a nullator (no voltage, no "
    "current) paired with a norator (any voltage and current) - an ideal "
    "op-amp with all four pins free");
  nullorItemData[Qt::UserRole] = SchematicScene::NullorItemType;
  componentList_.append(nullorItemData);

  QMap<int, QVariant> transformerItemData;
  transformerItemData[Qt::DisplayRole] = tr("Ideal Transformer");
  transformerItemData[Qt::DecorationRole] =
    theme::symbolPixmap("transformer", QSize(32, 32));
  transformerItemData[Qt::ToolTipRole] = tr("Ideal Transformer Item");
  transformerItemData[Qt::UserRole] = SchematicScene::TransformerItemType;
  componentList_.append(transformerItemData);

  QMap<int, QVariant> multiTransformerItemData;
  multiTransformerItemData[Qt::DisplayRole] = tr("Three-Winding Transformer");
  multiTransformerItemData[Qt::DecorationRole] =
    theme::symbolPixmap("transformer3", QSize(32, 32));
  multiTransformerItemData[Qt::ToolTipRole] = tr("A primary and two "
    "secondaries on one core: ideal (turns ratios), or with the primary "
    "inductance and a coupling. Wire the secondaries in series for a "
    "centre tap");
  multiTransformerItemData[Qt::UserRole] = SchematicScene::MultiTransformerItemType;
  componentList_.append(multiTransformerItemData);

  QMap<int, QVariant> mutualInductanceItemData;
  mutualInductanceItemData[Qt::DisplayRole] = tr("Mutual Inductance");
  mutualInductanceItemData[Qt::DecorationRole] =
    theme::symbolPixmap("mutualinductance", QSize(32, 32));
  mutualInductanceItemData[Qt::ToolTipRole] = tr("Mutual Inductance Item");
  mutualInductanceItemData[Qt::UserRole] = SchematicScene::MutualInductanceItemType;
  componentList_.append(mutualInductanceItemData);

  // the meters: outputs read across or through a branch
  QMap<int, QVariant> voltmeterItemData;
  voltmeterItemData[Qt::DisplayRole] = tr("Voltmeter");
  voltmeterItemData[Qt::DecorationRole] =
    theme::symbolPixmap("voltmeter", QSize(32, 32));
  voltmeterItemData[Qt::ToolTipRole] =
    tr("Voltmeter: an output, the voltage between its pins");
  voltmeterItemData[Qt::UserRole] = SchematicScene::VoltmeterItemType;
  componentList_.append(voltmeterItemData);

  QMap<int, QVariant> ammeterItemData;
  ammeterItemData[Qt::DisplayRole] = tr("Ammeter");
  ammeterItemData[Qt::DecorationRole] =
    theme::symbolPixmap("ammeter", QSize(32, 32));
  ammeterItemData[Qt::ToolTipRole] =
    tr("Ammeter: an output, the current through it");
  ammeterItemData[Qt::UserRole] = SchematicScene::AmmeterItemType;
  componentList_.append(ammeterItemData);
}


void SideBarModel::refreshSymbols()
{
  // re-tint every symbol (palette change) and follow the resistor style
  static const QHash<int, QString> names = {
    { SchematicScene::InductorItemType, "inductor" },
    { SchematicScene::CapacitorItemType, "capacitor" },
    { SchematicScene::VoltageSourceItemType, "voltage" },
    { SchematicScene::SingleVoltageSourceItemType, "voltage-1pin" },
    { SchematicScene::CurrentSourceItemType, "current" },
    { SchematicScene::VCVSItemType, "vcvs" },
    { SchematicScene::CCVSItemType, "ccvs" },
    { SchematicScene::VCCSItemType, "vccs" },
    { SchematicScene::CCCSItemType, "cccs" },
    { SchematicScene::OpAmplItemType, "opampl" },
    { SchematicScene::NullorItemType, "nullor" },
    { SchematicScene::MultiTransformerItemType, "transformer3" },
    { SchematicScene::TransformerItemType, "transformer" },
    { SchematicScene::MutualInductanceItemType, "mutualinductance" },
    { SchematicScene::VoltmeterItemType, "voltmeter" },
    { SchematicScene::AmmeterItemType, "ammeter" }
  };

  for(int row = 0; row < componentList_.size(); ++row) {
    const int type = componentList_[row][Qt::UserRole].toInt();
    QString name = names.value(type);
    if(type == SchematicScene::ResistorItemType
        || type == SchematicScene::ConductanceItemType)
      name = theme::resistorSymbolName();
    if(name.isEmpty())
      continue;

    componentList_[row][Qt::DecorationRole] =
      theme::symbolPixmap(name, QSize(32, 32));
  }

  if(!componentList_.isEmpty())
    emit dataChanged(index(0), index(componentList_.size() - 1),
      { Qt::DecorationRole });
}


}
