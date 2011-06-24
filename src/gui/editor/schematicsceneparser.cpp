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


#include "gui/editor/schematicsceneparser.h"
#include "gui/editor/component.h"
#include "gui/editor/label.h"
#include "gui/editor/wire.h"
#include "gui/editor/item.h"

#include "gui/qtsolutions/qtpropertyeditor/QtBoolPropertyManager"
#include "gui/qtsolutions/qtpropertyeditor/QtStringPropertyManager"
#include "gui/qtsolutions/qtpropertyeditor/QtDoublePropertyManager"
#include "gui/qtsolutions/qtpropertyeditor/QtProperty"

#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtGui/QGraphicsItem>

#include <boost/assign.hpp>
#include <memory>


namespace qsapecng
{


SchematicSceneParser::SchematicSceneParser(
    const SchematicScene& scene
  ): discard_(0)
{
  setupMap();
  items_ = scene.activeItems();
}


SchematicSceneParser::SchematicSceneParser(
    const SchematicScene& scene,
    const QList<QGraphicsItem*>& items
  ): discard_(0)
{
  setupMap();
  foreach(QGraphicsItem* gItem, items) {
    Item* item = qgraphicsitem_cast<Item*>(gItem);
    if(item && scene.activeItems().contains(item))
      items_.push_back(item);
  }
}


SchematicSceneParser::SchematicSceneParser(
    const SchematicScene& scene,
    QGraphicsItem* gItem
  ): discard_(0)
{
  setupMap();
  Item* item = qgraphicsitem_cast<Item*>(gItem);
  if(item && scene.activeItems().contains(item))
    items_.push_back(item);
}


void SchematicSceneParser::parse(
    sapecng::abstract_builder& builder
  )
{
  foreach(Item* item, items_)
    parse_item(builder, item);

  builder.flush();
}


void SchematicSceneParser::storeItemData(
    std::map<std::string, std::string>& props,
    SchematicScene::SupportedItemType type,
    Item* item
  )
{
  QPointF position = item->pos();
  std::string x_pos = QString::number(position.x()).toStdString();
  std::string y_pos = QString::number(position.y()).toStdString();

  std::string mirrored = (item->mirrored() ? "1" : "0");
  std::string rot_factor =
    QString::number(item->angle() / 90).toStdString();

  props["type"] = QString::number(type).toStdString();
  props["x"] = x_pos;
  props["y"] = y_pos;
  props["mirrored"] = mirrored;
  props["angle"] = rot_factor;
}


void SchematicSceneParser::storeLabel(
    std::map<std::string, std::string>& props,
    Component* component
  )
{
  if(!component->label()->toPlainText().trimmed().isEmpty()) {
    QPointF pos = component->label()->pos();
    props["x_label"] = QString::number(pos.x()).toStdString();
    props["y_label"] = QString::number(pos.y()).toStdString();
  }
}


void SchematicSceneParser::setupMap()
{
  dualMap_[SchematicScene::CapacitorItemType] = sapecng::abstract_builder::C;
  dualMap_[SchematicScene::ConductanceItemType] = sapecng::abstract_builder::G;
  dualMap_[SchematicScene::CurrentSourceItemType] =
    sapecng::abstract_builder::I;
  dualMap_[SchematicScene::ResistorItemType] = sapecng::abstract_builder::R;
  dualMap_[SchematicScene::InductorItemType] = sapecng::abstract_builder::L;
  dualMap_[SchematicScene::VoltageSourceItemType] =
    sapecng::abstract_builder::V;

  dualMap_[SchematicScene::VoltmeterItemType] = sapecng::abstract_builder::VM;
  dualMap_[SchematicScene::AmmeterItemType] = sapecng::abstract_builder::AM;

  quadMap_[SchematicScene::CCCSItemType] = sapecng::abstract_builder::CCCS;
  quadMap_[SchematicScene::CCVSItemType] = sapecng::abstract_builder::CCVS;
  quadMap_[SchematicScene::VCCSItemType] = sapecng::abstract_builder::VCCS;
  quadMap_[SchematicScene::VCVSItemType] = sapecng::abstract_builder::VCVS;
  quadMap_[SchematicScene::OpAmplItemType] = sapecng::abstract_builder::AO;
  quadMap_[SchematicScene::TransformerItemType] = sapecng::abstract_builder::n;
  quadMap_[SchematicScene::MutualInductanceItemType] =
    sapecng::abstract_builder::K;
}


void SchematicSceneParser::parse_item(
    sapecng::abstract_builder& builder,
    Item* item
  )
{
  SchematicScene::SupportedItemType type = SchematicScene::itemType(item);
  std::map<std::string, std::string> props;
  storeItemData(props, type, item);

  QtProperty* properties = SchematicScene::itemProperties(item);

  QHash<QString, QString> subproperties;
  if(properties) {
    subproperties.insert("__NAME", properties->valueText());
    foreach(QtProperty* prop, properties->subProperties())
      subproperties.insert(prop->propertyName(), prop->valueText());
  }

  switch(type)
  {
  case SchematicScene::GroundItemType:
  case SchematicScene::PortItemType:
    {
      // add as unknow 
      Component* component = static_cast<Component*>(item);
      props["node"] = QString::number(component->nodes().front()).toStdString();

      builder.add_unknow_component(props);

      break;
    }
  case SchematicScene::LabelItemType:
    {
      // add as unknow 
      Label* label = static_cast<Label*>(item);
      props["text"] = label->text().toStdString();

      builder.add_unknow_component(props);

      break;
    }
  case SchematicScene::WireItemType:
    {
      Wire* wire = static_cast<Wire*>(item);
      props["orientation"] =
        QString::number(wire->orientation()).toStdString();
      props["to_x"] =
        QString::number(wire->toPoint().x()).toStdString();
      props["to_y"] =
        QString::number(wire->toPoint().y()).toStdString();
      props["conn"] =
        QString::number(wire->isJunctionsConnected()).toStdString();

      builder.add_wire_component(props);

      break;
    }
  case SchematicScene::OutItemType:
    {
      if(!discard_) {
        Component* out = static_cast<Component*>(item);

        builder.add_out_component(out->nodes().front(), props);
      }

      break;
    }
  case SchematicScene::VoltmeterItemType:
  case SchematicScene::AmmeterItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();

      builder.add_dual_component(
        dualMap_[type], "", 1., true,
        nodes.at(1), nodes.at(0),
        props
      );

      break;
    }
  case SchematicScene::CapacitorItemType:
  case SchematicScene::ConductanceItemType:
  case SchematicScene::InductorItemType:
  case SchematicScene::ResistorItemType:
  case SchematicScene::CurrentSourceItemType:
  case SchematicScene::VoltageSourceItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);

      builder.add_dual_component(
        dualMap_[type], subproperties.value("__NAME").toStdString(),
        subproperties.value("Value").toDouble(),
        QVariant(subproperties.value("Symbolic")).toBool(),
        nodes.at(1), nodes.at(0),
        props
      );

      break;
    }
  case SchematicScene::CCCSItemType:
  case SchematicScene::CCVSItemType:
  case SchematicScene::VCCSItemType:
  case SchematicScene::VCVSItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);

      builder.add_quad_component(
        quadMap_[type], subproperties.value("__NAME").toStdString(),
        subproperties.value("Value").toDouble(),
        QVariant(subproperties.value("Symbolic")).toBool(),
        nodes.at(1), nodes.at(0), nodes.at(3), nodes.at(2),
        props
      );

      break;
    }
  case SchematicScene::OpAmplItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);

      builder.add_quad_component(
        quadMap_[type], subproperties.value("__NAME").toStdString(), 1., false,
        SchematicScene::Ground, nodes.at(2), nodes.at(1), nodes.at(0), props
      );

      break;
    }
  case SchematicScene::TransformerItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);

      builder.add_quad_component(
        quadMap_[type], subproperties.value("__NAME").toStdString(),
        subproperties.value("Value").toDouble(),
        QVariant(subproperties.value("Symbolic")).toBool(),
        nodes.at(3), nodes.at(2), nodes.at(1), nodes.at(0),
        props
      );

      break;
    }
  case SchematicScene::MutualInductanceItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);

      props["lp:name"] = subproperties.value("lp:name").toStdString();
      props["lp:value"] = subproperties.value("lp:value").toStdString();
      props["ls:name"] = subproperties.value("ls:name").toStdString();
      props["ls:value"] = subproperties.value("ls:value").toStdString();

      builder.add_quad_component(
        quadMap_[type], subproperties.value("__NAME").toStdString(),
        subproperties.value("Value").toDouble(),
        QVariant(subproperties.value("Symbolic")).toBool(),
        nodes.at(3), nodes.at(2), nodes.at(1), nodes.at(0),
        props
      );

      break;
    }
  case SchematicScene::UserDefItemType:
    {
      Component* component = static_cast<Component*>(item);
      storeLabel(props, component);

      builder.begin_userdef_component(
          subproperties.value("__NAME").toStdString(),
          props
        );

      ++discard_;

      QPointer<qsapecng::SchematicScene> scene =
        item->data(101).value< QPointer<qsapecng::SchematicScene> >();

      foreach(Item* item, scene->activeItems())
        parse_item(builder, item);

      --discard_;

      builder.end_userdef_component(
          subproperties.value("__NAME").toStdString(),
          props
        );

      // outer-to-inner port buffer resistors
      QVector<int> nodes = component->nodes();
      QVector<int> ports = scene->ports();

      if(nodes.size() == ports.size()) {
        for(int i = 0; i < nodes.size(); ++i)
          builder.add_dual_component(
            dualMap_[SchematicScene::ResistorItemType],
            "", .0, false, nodes.at(i), ports.at(i),
            boost::assign::map_list_of("discard", "1")
          );
      } else {
        // something strange happens - port-to-ground short circuits
        foreach(int node, nodes)
          builder.add_dual_component(
            dualMap_[SchematicScene::ResistorItemType],
            "", .0, false, node, SchematicScene::Ground,
            boost::assign::map_list_of("discard", "1")
          );
      }

      break;
    }
  default:
    break;
  }
}


void SchematicSceneBuilder::add_circuit_properties(
  std::map<std::string, std::string> map)
{
}


void SchematicSceneBuilder::add_circuit_property(
  std::string name, std::string value)
{
}


void SchematicSceneBuilder::add_wire_component(
    std::map<std::string, std::string> props
  )
{
  if(!discard(props)) {
    Wire* item = static_cast<Wire*>(
      SchematicScene::itemByType(SchematicScene::WireItemType));

    bool ok;

    double to_x = QString::fromStdString(props["to_x"]).toDouble(&ok);
    if(!ok)
      to_x = SchematicScene::GridStep;

    double to_y = QString::fromStdString(props["to_y"]).toDouble(&ok);
    if(!ok)
      to_y = SchematicScene::GridStep;

    bool conn = QString::fromStdString(props["conn"]).toInt(&ok);
    if(!ok)
      conn = false;

    item->setWire(QLineF(QPointF(0, 0), QPointF(to_x, to_y)));
    item->setConnectedJunctions(conn);

    scene_->addSupportedItem(item, false);
    grid_coordinate(item, props);
    items_.push_back(item);
  }
}


void SchematicSceneBuilder::add_out_component(
    unsigned int v,
    std::map<std::string, std::string> props
  )
{
  if(!discard(props)) {
    Item* item = static_cast<Item*>(
      SchematicScene::itemByType(SchematicScene::OutItemType));

    scene_->addSupportedItem(item, false);
    grid_coordinate(item, props);
    mirror_and_rotate(item, props);
    items_.push_back(item);
  }
}


void SchematicSceneBuilder::add_dual_component(
    sapecng::abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string, std::string> props
  )
{
  if(!discard(props)) {
    Component* component = 0;

    switch(c_type)
    {
    case sapecng::abstract_builder::R:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::ResistorItemType));
        break;
      }
    case sapecng::abstract_builder::G:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::ConductanceItemType));
        break;
      }
    case sapecng::abstract_builder::L:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::InductorItemType));
        break;
      }
    case sapecng::abstract_builder::C:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::CapacitorItemType));
        break;
      }
    case sapecng::abstract_builder::V:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::VoltageSourceItemType));
        break;
      }
    case sapecng::abstract_builder::I:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::CurrentSourceItemType));
        break;
      }
    case sapecng::abstract_builder::VM:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::VoltmeterItemType));
        break;
      }
    case sapecng::abstract_builder::AM:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::AmmeterItemType));
        break;
      }
    default:
      break;
    }

    if(component) {
      scene_->addSupportedItem(component, false);
      grid_coordinate(component, props);
      setup_properties(component, name, value, symbolic, props);
      mirror_and_rotate(component, props);
      adjust_label(component, props);
      items_.push_back(component);
    }
  }
}


void SchematicSceneBuilder::add_quad_component(
    sapecng::abstract_builder::quad_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    unsigned int vac,
    unsigned int vbc,
    std::map<std::string, std::string> props
  )
{
  if(!discard(props)) {
    Component* component = 0;

    switch(c_type)
    {
    case sapecng::abstract_builder::VCCS:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::VCCSItemType));
        break;
      }
    case sapecng::abstract_builder::VCVS:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::VCVSItemType));
        break;
      }
    case sapecng::abstract_builder::CCCS:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::CCCSItemType));
        break;
      }
    case sapecng::abstract_builder::CCVS:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::CCVSItemType));
        break;
      }
    case sapecng::abstract_builder::AO:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::OpAmplItemType));
        break;
      }
    case sapecng::abstract_builder::n:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::TransformerItemType));
        break;
      }
    case sapecng::abstract_builder::K:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(
            SchematicScene::MutualInductanceItemType));
        break;
      }
    default:
      break;
    }

    if(component) {
      scene_->addSupportedItem(component, false);
      grid_coordinate(component, props);
      setup_properties(component, name, value, symbolic, props);
      mirror_and_rotate(component, props);
      adjust_label(component, props);
      items_.push_back(component);
    }
  }
}


void SchematicSceneBuilder::add_unknow_component(
    std::map<std::string, std::string> props
  )
{
  if(!discard(props)) {
    bool ok;

    SchematicScene::SupportedItemType type =
      (SchematicScene::SupportedItemType)
        QString::fromStdString(props["type"]).toInt(&ok);

    if(!ok)
      type = SchematicScene::NilItemType;

    switch(type)
    {
    case SchematicScene::GroundItemType:
    case SchematicScene::PortItemType:
      {
        Item* item = static_cast<Item*>(
          SchematicScene::itemByType(type));

        scene_->addSupportedItem(item, false);
        grid_coordinate(item, props);
        mirror_and_rotate(item, props);
        items_.push_back(item);

        break;
      }
    case SchematicScene::LabelItemType:
      {
        Label* item = static_cast<Label*>(
          SchematicScene::itemByType(SchematicScene::LabelItemType));

        scene_->addSupportedItem(item, false);
        grid_coordinate(item, props);
        mirror_and_rotate(item, props);
        item->setText(QString::fromStdString(props["text"]));
        items_.push_back(item);

        break;
      }
    default:
      break;
    }
  }
}


void SchematicSceneBuilder::begin_userdef_component(
    std::string name,
    std::map<std::string,std::string> props
  )
{
  QPair< SchematicScene*, QList<QGraphicsItem*> > pair(scene_, items_);
  
  stack_.push(pair);
  scene_ = new SchematicScene;
  items_.clear();
}


void SchematicSceneBuilder::end_userdef_component(
    std::string name,
    std::map<std::string,std::string> props
  )
{
  int size = scene_->size();
  QByteArray md5 = stack_.back().first->registerUserDef(*scene_);

  Component* component = static_cast<Component*>(
    SchematicScene::itemByType(SchematicScene::UserDefItemType));
  
  component->setPath(SchematicScene::userDefPath(size));
  component->addNodes(SchematicScene::userDefNodes(size));
  component->setData(99, md5);
  
  QPointer<qsapecng::SchematicScene> smart(scene_);
  component->setData(101, qVariantFromValue(smart));
  
  QPair< SchematicScene*, QList<QGraphicsItem*> > pair = stack_.pop();
  
  items_.clear();
  items_.append(pair.second);
  scene_ = pair.first;
  
  scene_->addSupportedItem(component, false);
  grid_coordinate(component, props);
  setup_properties(component, name,
    /* foo */ 0, /* foo */ false, /* foo */ props);
  mirror_and_rotate(component, props);
  adjust_label(component, props);
  items_.push_back(component);
}


void SchematicSceneBuilder::flush()
{
  QPointF offset = QPointF(0, 0);
  if(offset_ != QPointF(0, 0)) {
    foreach(QGraphicsItem* item, items_)
      offset += item->pos();

    offset /= items_.size();
  }

  QPointF realOffset = offset_ - offset;
  foreach(QGraphicsItem* item, items_)
    item->setPos(scene_->closestGridPoint(item->pos() + realOffset));
  
  scene_->addItems(items_);
}


bool SchematicSceneBuilder::discard(std::map<std::string,std::string> props) {
  bool ok;

  int discard = QString::fromStdString(props["discard"]).toInt(&ok);
  if(!ok)
    discard = 0;
  
  return discard;
}


void SchematicSceneBuilder::grid_coordinate(
    Item* item, std::map<std::string, std::string> props
  )
{
  bool ok;

  double x = QString::fromStdString(props["x"]).toDouble(&ok);
  if(!ok)
    x = 0;

  double y = QString::fromStdString(props["y"]).toDouble(&ok);
  if(!ok)
    y = 0;

  item->setPos(scene_->closestGridPoint(QPointF(x, y)));
}


void SchematicSceneBuilder::mirror_and_rotate(
  Item* item, std::map<std::string, std::string> props)
{
  bool ok;

  bool mirrored = QString::fromStdString(props["mirrored"]).toInt(&ok);
  if(!ok)
    mirrored = false;

  int rot_factor = QString::fromStdString(props["angle"]).toInt(&ok);
  if(!ok)
    rot_factor = 0;

  if(mirrored)
    item->mirror();

  for(int i = 0; i < rot_factor; ++i)
    item->rotate();
}


void SchematicSceneBuilder::adjust_label(
  Component* cmp, std::map<std::string, std::string> props)
{
  bool xOk, yOk;

  double xLab = QString::fromStdString(props["x_label"]).toDouble(&xOk);
  double yLab = QString::fromStdString(props["y_label"]).toDouble(&yOk);

  if(xOk && yOk)
    cmp->label()->setPos(QPointF(xLab, yLab));
}


void SchematicSceneBuilder::setup_properties(
    Item* item,
    std::string name,
    double value,
    bool symbolic,
    std::map<std::string, std::string> props
  )
{
  QtProperty* properties = SchematicScene::itemProperties(item);

  QHash<QString, QtProperty*> subproperties;
  if(properties) {
    subproperties.insert("__NAME", properties);
    foreach(QtProperty* prop, properties->subProperties())
      subproperties.insert(prop->propertyName(), prop);
  }

  if(subproperties.contains("__NAME")) {
    QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(
        subproperties.value("__NAME")->propertyManager());
      if(spm)
        spm->setValue(
            subproperties.value("__NAME"),
            QString::fromStdString(name)
          );
  }

  if(subproperties.contains("Value")) {
    QtDoublePropertyManager* dpm =
      qobject_cast<QtDoublePropertyManager*>(
        subproperties.value("Value")->propertyManager());
      if(dpm)
        dpm->setValue(
            subproperties.value("Value"),
            value
          );
  }

  if(subproperties.contains("Symbolic")) {
    QtBoolPropertyManager* bpm =
      qobject_cast<QtBoolPropertyManager*>(
        subproperties.value("Symbolic")->propertyManager());
      if(bpm)
        bpm->setValue(
            subproperties.value("Symbolic"),
            symbolic
          );
  }

  if(subproperties.contains("lp:name")) {
    QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(
        subproperties.value("lp:name")->propertyManager());
      if(spm)
        spm->setValue(
            subproperties.value("lp:name"),
            QString::fromStdString(props["lp:name"])
          );
  }

  if(subproperties.contains("lp:value")) {
    QtDoublePropertyManager* dpm =
      qobject_cast<QtDoublePropertyManager*>(
        subproperties.value("lp:value")->propertyManager());
      if(dpm)
        dpm->setValue(
            subproperties.value("lp:value"),
            QString::fromStdString(props["lp:value"]).toDouble()
          );
  }

  if(subproperties.contains("ls:name")) {
    QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(
        subproperties.value("ls:name")->propertyManager());
      if(spm)
        spm->setValue(
            subproperties.value("ls:name"),
            QString::fromStdString(props["ls:name"])
          );
  }

  if(subproperties.contains("ls:value")) {
    QtDoublePropertyManager* dpm =
      qobject_cast<QtDoublePropertyManager*>(
        subproperties.value("ls:value")->propertyManager());
      if(dpm)
        dpm->setValue(
            subproperties.value("ls:value"),
            QString::fromStdString(props["ls:value"]).toDouble()
          );
  }
}


}
