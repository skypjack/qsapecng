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
#include "gui/qlogger.h"
#include <functional>
#include "gui/editor/component.h"
#include "gui/editor/label.h"
#include "gui/editor/wire.h"
#include "gui/editor/item.h"

#include "gui/qtsolutions/qtpropertyeditor/QtBoolPropertyManager"
#include "gui/qtsolutions/qtpropertyeditor/QtStringPropertyManager"
#include "gui/qtsolutions/qtpropertyeditor/QtProperty"

#include <QPointF>
#include <QPointer>
#include <QGraphicsItem>

#ifndef Q_MOC_RUN
#include <boost/assign.hpp>
#endif

#include <memory>
#include <cmath>
#include <algorithm>
#include <QHash>


namespace qsapecng
{


SchematicSceneParser::SchematicSceneParser(
    const SchematicScene& scene
  ): discard_(0)
{
  setupMap();
  items_ = scene.activeItems();
  props_ = scene.circuitProperties();
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
  if(!props_.isEmpty()) {
    std::map<std::string, std::string> map;
    for(auto i = props_.constBegin(); i != props_.constEnd(); ++i)
      map[i.key().toStdString()] = i.value().toStdString();
    builder.add_circuit_properties(map);
  }

  const QSet<Item*> floating = skipFloating_ ? floatingParts() : QSet<Item*>();
  foreach(Item* item, items_) {
    if(floating.contains(item)) {
      QtProperty* properties = SchematicScene::itemProperties(item);
      // once, not for every output the circuit is built for
      if(!warned_)
        QLogger::warning(QObject::tr("%1 is not connected to the rest of the "
        "circuit and was left out").arg(properties ? properties->valueText()
                                                   : QObject::tr("A part")));
      continue;
    }
    parse_item(builder, item);
  }
  warned_ = true;

  builder.flush();
}


QSet<Item*> SchematicSceneParser::floatingParts() const
{
  // nets joined by parts (union-find); a part none of whose nets reaches
  // the ground net floats
  QHash<int, int> parent;
  std::function<int(int)> root = [&](int n) {
      if(!parent.contains(n))
        parent.insert(n, n);
      int r = n;
      while(parent.value(r) != r)
        r = parent.value(r);
      parent[n] = r;
      return r;
    };
  auto join = [&](int a, int b) { parent[root(a)] = root(b); };

  QList<Item*> parts;
  bool grounded = false;
  foreach(Item* item, items_) {
    const SchematicScene::SupportedItemType type = SchematicScene::itemType(item);
    switch(type) {
    case SchematicScene::GroundItemType:
      grounded = true;
      break;
    case SchematicScene::WireItemType:
    case SchematicScene::LabelItemType:
    case SchematicScene::OutItemType:
    case SchematicScene::PortItemType:
      break;
    default:
      if(Component* c = dynamic_cast<Component*>(item)) {
        const QVector<int> nodes = c->nodes();
        if(nodes.isEmpty())
          break;
        parts.append(item);
        for(int i = 1; i < nodes.size(); ++i)
          join(nodes.at(i), nodes.at(0));
        // referenced to the ground inside the part
        if(type == SchematicScene::SingleVoltageSourceItemType
            || type == SchematicScene::OpAmplItemType)
          join(nodes.at(0), SchematicScene::Ground);
      }
      break;
    }
  }

  QSet<Item*> floating;
  if(!grounded)
    return floating;    // nothing to measure against: leave it to the solver
  const int ground = root(SchematicScene::Ground);
  foreach(Item* item, parts)
    if(root(static_cast<Component*>(item)->nodes().at(0)) != ground)
      floating.insert(item);
  return floating;
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
  // only a label the user moved; the others are placed automatically
  // (the anchor is the label centre in the component's own coordinates)
  if(component->hasCustomLabelPosition()) {
    QPointF anchor = component->labelAnchor();
    props["label_cx"] = QString::number(anchor.x()).toStdString();
    props["label_cy"] = QString::number(anchor.y()).toStdString();
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
        props["name"] = SchematicScene::outputName(out).toStdString();
        if(SchematicScene::outputLoad(out) > 0.)
          props["load"] = QString::number(SchematicScene::outputLoad(out),
            'g', 12).toStdString();

        builder.add_out_component(out->nodes().front(), props);
      }

      break;
    }
  case SchematicScene::VoltmeterItemType:
  case SchematicScene::AmmeterItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();

      // reversed polarity: the other way round (and so in the file)
      const bool reversed = component->isReversed();
      if(reversed)
        props["reversed"] = "1";
      builder.add_dual_component(
        dualMap_[type], SchematicScene::outputName(component).toStdString(),
        1., true,
        reversed ? nodes.at(0) : nodes.at(1),
        reversed ? nodes.at(1) : nodes.at(0),
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

      // a real R, L or C: the solver adds the parasitics
      if(subproperties.contains("Ideal")
          && !QVariant(subproperties.value("Ideal")).toBool())
      {
        props["ideal"] = "0";
        foreach(const SchematicScene::Parasitic& par,
            SchematicScene::parasitics(type))
          props[par.key] = QString::number(
            subproperties.value(par.property).toDouble(), 'g', 12).toStdString();
      }

      builder.add_dual_component(
        dualMap_[type], subproperties.value("__NAME").toStdString(),
        subproperties.value("Value").toDouble(),
        QVariant(subproperties.value("Symbolic")).toBool(),
        nodes.at(1), nodes.at(0),
        props
      );

      break;
    }
  case SchematicScene::SingleVoltageSourceItemType:
    {
      // saved as a voltage source from the ground (-) to the pin (+); the
      // "type" property brings the one-pin symbol back on loading
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);
      // internal resistance
      if(subproperties.contains("Ideal")
          && !QVariant(subproperties.value("Ideal")).toBool())
      {
        props["ideal"] = "0";
        foreach(const SchematicScene::Parasitic& par,
            SchematicScene::parasitics(type))
          props[par.key] = QString::number(
            subproperties.value(par.property).toDouble(), 'g', 12).toStdString();
      }

      builder.add_dual_component(
        sapecng::abstract_builder::V,
        subproperties.value("__NAME").toStdString(),
        subproperties.value("Value").toDouble(),
        QVariant(subproperties.value("Symbolic")).toBool(),
        SchematicScene::Ground, nodes.at(0),
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

      // ideal: a nullor; otherwise the solver models open-loop gain A0
      // (Value) and, when GBW > 0, a single pole at GBW / A0
      const bool ideal = !subproperties.contains("Ideal")
        || QVariant(subproperties.value("Ideal")).toBool();
      double a0 = 1.;
      bool symbolic = false;
      if(!ideal) {
        a0 = subproperties.value("Value").toDouble();
        symbolic = QVariant(subproperties.value("Symbolic")).toBool();
        props["ideal"] = "0";
        props["gbw"] = QString::number(
          subproperties.value("GBW").toDouble(), 'g', 12).toStdString();
      }

      builder.add_quad_component(
        quadMap_[type], subproperties.value("__NAME").toStdString(), a0,
        symbolic,
        SchematicScene::Ground, nodes.at(2), nodes.at(1), nodes.at(0), props
      );

      break;
    }
  case SchematicScene::MultiTransformerItemType:
    {
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);

      const bool ideal = !subproperties.contains("Ideal")
        || QVariant(subproperties.value("Ideal")).toBool();
      const double lp = subproperties.value("Lp").toDouble();
      double k = subproperties.value("k").toDouble();
      if(!(k > 0.) || k > 1.)
        k = 1.;
      // the inductance is kept while the part is ideal
      props["ideal"] = ideal ? "1" : "0";
      props["lp"] = QString::number(lp, 'g', 15).toStdString();

      std::vector< std::pair<unsigned int, unsigned int> > windings;
      for(int i = 0; i + 1 < nodes.size(); i += 2)
        windings.push_back(std::make_pair(unsigned(nodes.at(i)),
                                          unsigned(nodes.at(i + 1))));
      std::vector<double> ratios;
      ratios.push_back(1.);
      ratios.push_back(subproperties.value("n2").toDouble());
      ratios.push_back(subproperties.value("n3").toDouble());

      builder.add_transformer(
        subproperties.value("__NAME").toStdString(), windings, ratios,
        ideal ? 0. : lp, k, QVariant(subproperties.value("Symbolic")).toBool(),
        props);

      break;
    }
  case SchematicScene::NullorItemType:
    {
      // the solver's ideal op-amp element (a nullor) on four free pins:
      // norator (output) and nullator (input)
      Component* component = static_cast<Component*>(item);
      QVector<int> nodes = component->nodes();
      storeLabel(props, component);

      builder.add_quad_component(
        sapecng::abstract_builder::AO,
        subproperties.value("__NAME").toStdString(), 1., false,
        nodes.at(3), nodes.at(2), nodes.at(1), nodes.at(0), props
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
  for(const auto& p : map)
    add_circuit_property(p.first, p.second);
}


void SchematicSceneBuilder::add_circuit_property(
  std::string name, std::string value)
{
  // the top schematic's only (not those of sub-circuits)
  if(stack_.isEmpty())
    scene_->setCircuitProperty(QString::fromStdString(name),
      QString::fromStdString(value));
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
    // its name (older files have none: Out1, Out2 ... are given)
    item->setData(SchematicScene::OutputNameKey,
      QString::fromStdString(props["name"]));
    item->setData(SchematicScene::OutputLoadKey,
      QString::fromStdString(props["load"]).toDouble());

    scene_->addSupportedItem(item, false);
    grid_coordinate(item, props);
    mirror_and_rotate(item, props);
    items_.push_back(item);
  }
}


void SchematicSceneBuilder::add_transformer(
    std::string name,
    std::vector< std::pair<unsigned int, unsigned int> > windings,
    std::vector<double> ratios,
    double lp,
    double k,
    bool symbolic,
    std::map<std::string, std::string> props
  )
{
  // the schematic has the three-winding part only
  if(discard(props) || windings.size() != 3 || ratios.size() != 3)
    return;

  Component* component = static_cast<Component*>(
    SchematicScene::itemByType(SchematicScene::MultiTransformerItemType));
  if(!component)
    return;

  if(!props.count("ideal"))
    props["ideal"] = lp > 0. ? "0" : "1";
  scene_->addSupportedItem(component, false);
  grid_coordinate(component, props);
  setup_properties(component, name, lp, symbolic, props);

  // turns ratios, inductance (also kept for an ideal part) and coupling
  if(QtProperty* properties = SchematicScene::itemProperties(component)) {
    auto number = [](double x) { return QString::number(x, 'g', 15); };
    double inductance = lp;
    if(props.count("lp"))
      inductance = QString::fromStdString(props["lp"]).toDouble();
    const QHash<QString, QString> values = {
      { QStringLiteral("n2"), number(ratios[1]) },
      { QStringLiteral("n3"), number(ratios[2]) },
      { QStringLiteral("k"), number(k) } };
    foreach(QtProperty* p, properties->subProperties()) {
      QtStringPropertyManager* spm =
        qobject_cast<QtStringPropertyManager*>(p->propertyManager());
      if(!spm)
        continue;
      if(values.contains(p->propertyName()))
        spm->setValue(p, values.value(p->propertyName()));
      else if(p->propertyName() == QLatin1String("Lp") && inductance > 0.)
        spm->setValue(p, number(inductance));
    }
  }

  mirror_and_rotate(component, props);
  adjust_label(component, props);
  items_.push_back(component);
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
        // a one-pin (to ground) source is a V with its own "type"
        const bool single = QString::fromStdString(props["type"]).toInt()
          == SchematicScene::SingleVoltageSourceItemType;
        component = static_cast<Component*>(SchematicScene::itemByType(
          single ? SchematicScene::SingleVoltageSourceItemType
                 : SchematicScene::VoltageSourceItemType));
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
        if(component && props["reversed"] == "1")
          component->setReversed(true);
        break;
      }
    case sapecng::abstract_builder::AM:
      {
        component = static_cast<Component*>(
          SchematicScene::itemByType(SchematicScene::AmmeterItemType));
        if(component && props["reversed"] == "1")
          component->setReversed(true);
        break;
      }
    default:
      break;
    }

    if(component) {
      if(c_type == sapecng::abstract_builder::VM
          || c_type == sapecng::abstract_builder::AM)
        component->setData(SchematicScene::OutputNameKey,
          QString::fromStdString(name));
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
        // a nullor is an op-amp element with its own "type"
        const bool nullor = QString::fromStdString(props["type"]).toInt()
          == SchematicScene::NullorItemType;
        component = static_cast<Component*>(SchematicScene::itemByType(
          nullor ? SchematicScene::NullorItemType
                 : SchematicScene::OpAmplItemType));
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
  component->setData(101, QVariant::fromValue(smart));
  
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

  // older files store the label's top-left corner (x_label/y_label) for
  // the old placement: ignored, those labels get the automatic placement
  double xLab = QString::fromStdString(props["label_cx"]).toDouble(&xOk);
  double yLab = QString::fromStdString(props["label_cy"]).toDouble(&yOk);

  if(xOk && yOk)
    cmp->setLabelAnchor(QPointF(xLab, yLab));
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
      if(spm) {
        QString wanted = QString::fromStdString(name);
        // pasting: a copy gets a name of its own (R2 -> R3)
        if(offset_ != QPointF(0, 0))
          wanted = scene_->uniqueName(SchematicScene::itemType(item), wanted, item);
        spm->setValue(subproperties.value("__NAME"), wanted);
      }
  }

  if(subproperties.contains("Value")) {
    QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(
        subproperties.value("Value")->propertyManager());
      if(spm)
        spm->setValue(
            subproperties.value("Value"),
            QString::number(value)
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

  if(subproperties.contains("Ideal") && !subproperties.contains("GBW")) {
    // R, L, C: ideal unless saved with parasitics
    const bool ideal = props["ideal"] != "0";
    if(QtBoolPropertyManager* bpm = qobject_cast<QtBoolPropertyManager*>(
        subproperties.value("Ideal")->propertyManager()))
      bpm->setValue(subproperties.value("Ideal"), ideal);
    foreach(const SchematicScene::Parasitic& par,
        SchematicScene::parasitics(SchematicScene::itemType(item)))
    {
      QtProperty* pp = subproperties.value(par.property);
      if(!pp || !props.count(par.key))
        continue;
      if(QtStringPropertyManager* spm =
          qobject_cast<QtStringPropertyManager*>(pp->propertyManager()))
        spm->setValue(pp, QString::number(
          QString::fromStdString(props[par.key]).toDouble()));
    }
  } else if(subproperties.contains("Ideal")) {
    // op-amp: older files have no "ideal" property (always ideal)
    const bool ideal = props["ideal"] != "0";
    QtBoolPropertyManager* bpm =
      qobject_cast<QtBoolPropertyManager*>(
        subproperties.value("Ideal")->propertyManager());
    if(bpm)
      bpm->setValue(subproperties.value("Ideal"), ideal);

    QtStringPropertyManager* spm = qobject_cast<QtStringPropertyManager*>(
      subproperties.value("Value")->propertyManager());
    if(spm && ideal)
      spm->setValue(subproperties.value("Value"), "100000");
    if(spm && subproperties.contains("GBW"))
      spm->setValue(subproperties.value("GBW"),
        props.count("gbw") ? QString::number(
          QString::fromStdString(props["gbw"]).toDouble()) : QString("1000000"));
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
    QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(
        subproperties.value("lp:value")->propertyManager());
      if(spm)
        spm->setValue(
            subproperties.value("lp:value"),
            QString::number(
              QString::fromStdString(props["lp:value"]).toDouble())
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
    QtStringPropertyManager* spm =
      qobject_cast<QtStringPropertyManager*>(
        subproperties.value("ls:value")->propertyManager());
      if(spm)
        spm->setValue(
            subproperties.value("ls:value"),
            QString::number(
              QString::fromStdString(props["ls:value"]).toDouble())
          );
  }
}


void NetlistSceneBuilder::add_dual_component(
    sapecng::abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string,std::string> props
  )
{
  Part part = { false, int(c_type), name, value, symbolic, { va, vb }, props };
  parts_.push_back(part);
}


void NetlistSceneBuilder::add_quad_component(
    sapecng::abstract_builder::quad_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    unsigned int vac,
    unsigned int vbc,
    std::map<std::string,std::string> props
  )
{
  // an op-amp element with its fourth node off the ground has four free
  // pins: a nullor
  if(c_type == sapecng::abstract_builder::AO && va != 0)
    props["type"] = QString::number(
      SchematicScene::NullorItemType).toStdString();
  Part part = { true, int(c_type), name, value, symbolic,
    { va, vb, vac, vbc }, props };
  parts_.push_back(part);
}


void NetlistSceneBuilder::flush()
{
  const qreal g = SchematicScene::GridStep;
  auto down = [g](qreal v) { return std::floor(v / g) * g; };
  auto up = [g](qreal v) { return std::ceil(v / g) * g; };

  SchematicSceneBuilder builder(*scene_);

  // lane: the x of its vertical wire (the pin's own x: straight up or down)
  struct Pin { unsigned int node; QPointF at; qreal lane; };
  QList<Pin> pins;
  QList<QRectF> bodies;
  qreal cursor = 0;

  for(const Part& part : parts_) {
    std::map<std::string,std::string> props = part.props;
    props["x"] = "0";
    props["y"] = "0";
    const int before = builder.items().size();
    if(part.quad)
      builder.add_quad_component(
        sapecng::abstract_builder::quad_component_type(part.type),
        part.name, part.value, part.symbolic, part.nodes[0], part.nodes[1],
        part.nodes[2], part.nodes[3], props);
    else
      builder.add_dual_component(
        sapecng::abstract_builder::dual_component_type(part.type),
        part.name, part.value, part.symbolic, part.nodes[0], part.nodes[1],
        props);
    if(builder.items().size() == before)
      continue;
    Component* component = dynamic_cast<Component*>(builder.items().last());
    if(!component)
      continue;

    // the node of each pin, in the order the schematic gives them
    const QList<QPointF> local = component->pinPoints();
    const std::vector<unsigned int>& n = part.nodes;
    QList<unsigned int> nodeOf;
    if(!part.quad)
      nodeOf = { n[1], n[0] };
    else
      switch(part.type)
      {
      case sapecng::abstract_builder::VCCS:
      case sapecng::abstract_builder::VCVS:
      case sapecng::abstract_builder::CCCS:
      case sapecng::abstract_builder::CCVS:
        nodeOf = { n[1], n[0], n[3], n[2] };
        break;
      default:   // AO, n, K: from the last node back
        nodeOf = { n[3], n[2], n[1], n[0] };
        break;
      }
    if(nodeOf.size() > local.size())
      nodeOf = nodeOf.mid(0, local.size());

    // two pins: upright, the ground (if any) at the bottom
    if(!part.quad && local.size() == 2) {
      if(qAbs(component->mapToScene(local.at(0)).y()
          - component->mapToScene(local.at(1)).y()) < 0.5)
        component->rotate();
      const bool firstOnTop = component->mapToScene(local.at(0)).y()
        < component->mapToScene(local.at(1)).y();
      const unsigned int topNode = nodeOf.at(firstOnTop ? 0 : 1);
      const unsigned int bottomNode = nodeOf.at(firstOnTop ? 1 : 0);
      if(topNode == 0 && bottomNode != 0) {
        component->rotate();
        component->rotate();
      }
    }

    // where the part is, and which pins go out on either side
    auto layout = [&](QRectF* rect, QList<Pin>* out) {
        // the pins' extent: the body is in it (the label is left out)
        QList<QPointF> at;
        for(const QPointF& p : local)
          at.append(component->mapToScene(p));
        qreal x0 = at.first().x(), x1 = x0, y0 = at.first().y(), y1 = y0;
        for(const QPointF& p : at) {
          x0 = qMin(x0, p.x());
          x1 = qMax(x1, p.x());
          y0 = qMin(y0, p.y());
          y1 = qMax(y1, p.y());
        }
        *rect = QRectF(QPointF(x0, y0), QPointF(x1, y1));
        if(rect->width() < g)
          rect->adjust(-g, 0, g, 0);
        out->clear();
        // a lone pin on top going up, or at the bottom going to the
        // ground, takes the straight way
        auto alone = [&](int i, qreal y) {
            for(int j = 0; j < at.size(); ++j)
              if(j != i && qAbs(at.at(j).y() - y) < 0.5)
                return false;
            return qAbs(at.at(i).y() - y) < 0.5;
          };
        QList<int> left, right;
        for(int i = 0; i < nodeOf.size(); ++i) {
          if((nodeOf.at(i) != 0 && alone(i, rect->top()))
              || (nodeOf.at(i) == 0 && alone(i, rect->bottom()))) {
            out->append({ nodeOf.at(i), at.at(i), at.at(i).x() });
            continue;
          }
          const qreal dx = at.at(i).x() - rect->center().x();
          // a pin in the middle: the side with fewer (the left first:
          // the name and value are on the right)
          if(dx < -0.5 || (qAbs(dx) <= 0.5 && left.size() <= right.size()))
            left.append(i);
          else
            right.append(i);
        }
        // the top pin gets the lane nearest the part: the stubs below it
        // pass under its lane
        auto byY = [&](int a, int b) { return at.at(a).y() < at.at(b).y(); };
        std::sort(left.begin(), left.end(), byY);
        std::sort(right.begin(), right.end(), byY);
        for(int k = 0; k < left.size(); ++k)
          out->append({ nodeOf.at(left.at(k)), at.at(left.at(k)),
            down(rect->left() - 1) - k * g });
        for(int k = 0; k < right.size(); ++k)
          out->append({ nodeOf.at(right.at(k)), at.at(right.at(k)),
            up(rect->right() + 1) + k * g });
      };

    // the room it takes, its name and value included
    auto room = [&]() {
        return component->mapRectToScene(
          component->boundingRect() | component->childrenBoundingRect());
      };
    QRectF rect;
    QList<Pin> own;
    layout(&rect, &own);
    qreal leftmost = qMin(rect.left(), room().left());
    for(const Pin& p : own)
      leftmost = qMin(leftmost, p.lane);
    component->setPos(component->pos()
      + QPointF(up(cursor - leftmost), 0));
    layout(&rect, &own);

    qreal rightmost = qMax(rect.right(), room().right());
    for(const Pin& p : own)
      rightmost = qMax(rightmost, p.lane);
    cursor = up(rightmost) + 3 * g;
    pins << own;
    bodies << rect;
  }

  if(bodies.isEmpty()) {
    builder.flush();
    return;
  }

  qreal top = bodies.first().top(), bottom = bodies.first().bottom();
  for(const QRectF& r : bodies) {
    top = qMin(top, r.top());
    bottom = qMax(bottom, r.bottom());
  }

  // one bus per node, node 1 at the top
  QList<unsigned int> nets;
  for(const Pin& p : pins)
    if(p.node != 0 && !nets.contains(p.node))
      nets.append(p.node);
  std::sort(nets.begin(), nets.end());
  QHash<unsigned int, qreal> busY;
  for(int i = 0; i < nets.size(); ++i)
    busY.insert(nets.at(i), down(top) - 3 * g * (nets.size() - i));
  const qreal groundY = up(bottom) + 2 * g;

  auto wire = [&](QPointF a, QPointF b) {
      if(QLineF(a, b).length() < 0.5)
        return;
      const QPointF from(qMin(a.x(), b.x()), qMin(a.y(), b.y()));
      std::map<std::string,std::string> props;
      props["x"] = QString::number(from.x()).toStdString();
      props["y"] = QString::number(from.y()).toStdString();
      props["to_x"] = QString::number(qAbs(b.x() - a.x())).toStdString();
      props["to_y"] = QString::number(qAbs(b.y() - a.y())).toStdString();
      props["conn"] = "0";
      builder.add_wire_component(props);
    };

  QHash<unsigned int, QList<qreal> > taps;
  for(const Pin& p : pins) {
    const QPointF corner(p.lane, p.at.y());
    wire(p.at, corner);
    if(p.node == 0) {
      wire(corner, QPointF(p.lane, groundY));
      taps[0].append(p.lane);
    } else {
      wire(corner, QPointF(p.lane, busY.value(p.node)));
      taps[p.node].append(p.lane);
    }
  }

  // the buses, cut at every tap (a tap is a wire end), and the ground
  for(auto i = taps.begin(); i != taps.end(); ++i) {
    QList<qreal>& xs = i.value();
    std::sort(xs.begin(), xs.end());
    const qreal y = i.key() == 0 ? groundY : busY.value(i.key());
    for(int k = 0; k + 1 < xs.size(); ++k)
      wire(QPointF(xs.at(k), y), QPointF(xs.at(k + 1), y));
  }
  if(taps.contains(0)) {
    std::map<std::string,std::string> props;
    props["type"] = QString::number(SchematicScene::GroundItemType).toStdString();
    props["x"] = QString::number(taps.value(0).first()).toStdString();
    props["y"] = QString::number(groundY).toStdString();
    builder.add_unknow_component(props);
    // its pin on the bus
    if(Component* ground = dynamic_cast<Component*>(builder.items().last()))
      if(!ground->pinPoints().isEmpty())
        ground->setPos(ground->pos() + QPointF(taps.value(0).first(), groundY)
          - ground->mapToScene(ground->pinPoints().first()));
  }

  // the outputs, past everything on the right
  for(unsigned int v : outs_) {
    if(v == 0 || !taps.contains(v))
      continue;
    const qreal y = busY.value(v);
    const QPointF end(cursor, y);
    wire(QPointF(taps.value(v).last(), y), end);
    std::map<std::string,std::string> props;
    builder.add_out_component(v, props);
    if(Component* out = dynamic_cast<Component*>(builder.items().last()))
      if(!out->pinPoints().isEmpty())
        out->setPos(out->pos() + end - out->mapToScene(out->pinPoints().first()));
  }

  builder.flush();
}


}
