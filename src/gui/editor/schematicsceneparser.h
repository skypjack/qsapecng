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


#ifndef SCHEMATICSCENEPARSER_H
#define SCHEMATICSCENEPARSER_H


#include "parser/parser.h"
#include "gui/editor/schematicscene.h"
#include "gui/editor/item.h"

#include <QPair>
#include <QStack>
#include <QPointF>
#include <QList>

#include <vector>
#include <map>


class QGraphicsItem;


namespace qsapecng
{


class SchematicSceneParser: public sapecng::abstract_parser
{

public:
  SchematicSceneParser(const SchematicScene& scene);
  SchematicSceneParser(
      const SchematicScene& scene,
      const QList<QGraphicsItem*>& items
    );
  SchematicSceneParser(
      const SchematicScene& scene,
      QGraphicsItem* gItem
    );

  void parse(sapecng::abstract_builder& builder);

  // solving: leave out parts not connected (through other parts) to the
  // ground; they would make the circuit unsolvable (result 0)
  inline void setSkipFloating(bool skip) { skipFloating_ = skip; }

private:
  QSet<Item*> floatingParts() const;
  QMap<QString, QString> props_;   // the schematic's own properties
  void storeItemData(
      std::map<std::string,std::string>& props,
      SchematicScene::SupportedItemType type,
      Item* item
    );
  void storeLabel(
      std::map<std::string,std::string>& props,
      Component* component
    );
  void setupMap();
  void parse_item(
    sapecng::abstract_builder& builder,
    Item* item
  );

private:
  QMap<SchematicScene::SupportedItemType,
    sapecng::abstract_builder::dual_component_type> dualMap_;
  QMap<SchematicScene::SupportedItemType,
    sapecng::abstract_builder::quad_component_type> quadMap_;

  QList<Item*> items_;
  int discard_;
  bool skipFloating_ = false;
  bool warned_ = false;   // floating parts reported (first parse)

};




// Draws a netlist (a .crc file: parts and the nodes they join, no
// drawing) as a schematic. Every part sits in one row, each with its own
// lanes: a pin goes sideways to its lane, the lane up to the bus of its
// node (one bus per node, above the row) or down to a ground symbol. Out
// markers sit at the right end of their bus. The drawing is a start, to be
// tidied by hand; the circuit is the netlist's.
class NetlistSceneBuilder: public sapecng::abstract_builder
{

public:
  NetlistSceneBuilder(SchematicScene& scene): scene_(&scene) { }

  void add_circuit_properties(std::map<std::string,std::string> map) { }
  void add_circuit_property(std::string name, std::string value) { }

  void add_wire_component(
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) { }

  void add_out_component(
      unsigned int v,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) { outs_.push_back(v); }

  void add_dual_component(
      sapecng::abstract_builder::dual_component_type c_type,
      std::string name,
      double value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void add_quad_component(
      sapecng::abstract_builder::quad_component_type c_type,
      std::string name,
      double value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      unsigned int vac,
      unsigned int vbc,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void add_unknow_component(
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) { }

  void begin_userdef_component(
      std::string name,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) { }

  void end_userdef_component(
      std::string name,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) { }

  // lays the parts out and puts them in the scene
  void flush();

private:
  struct Part {
    bool quad;
    int type;
    std::string name;
    double value;
    bool symbolic;
    std::vector<unsigned int> nodes;   // va, vb (, vac, vbc)
    std::map<std::string,std::string> props;
  };

  SchematicScene* scene_;
  std::vector<Part> parts_;
  std::vector<unsigned int> outs_;

};



class SchematicSceneBuilder: public sapecng::abstract_builder
{

public:
  SchematicSceneBuilder(SchematicScene& scene)
    : scene_(&scene), offset_(QPointF(0, 0)) { }

  SchematicSceneBuilder(SchematicScene& scene, const QPointF& offset)
    : scene_(&scene), offset_(offset) { }

  void add_circuit_properties(std::map<std::string,std::string> map);
  void add_circuit_property(std::string name, std::string value);

  void add_wire_component(
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void add_out_component(
      unsigned int v,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void add_dual_component(
      sapecng::abstract_builder::dual_component_type c_type,
      std::string name,
      double value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void add_quad_component(
      sapecng::abstract_builder::quad_component_type c_type,
      std::string name,
      double value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      unsigned int vac,
      unsigned int vbc,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void add_unknow_component(
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void begin_userdef_component(
      std::string name,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void end_userdef_component(
      std::string name,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  // the three-winding transformer, kept whole
  void add_transformer(
      std::string name,
      std::vector< std::pair<unsigned int, unsigned int> > windings,
      std::vector<double> ratios,
      double lp,
      double k,
      bool symbolic,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void flush();

  inline QList<QGraphicsItem*> items() const { return items_; }

private:
  bool discard(std::map<std::string,std::string> props);
  void grid_coordinate(Item* item, std::map<std::string,std::string> props);
  void mirror_and_rotate(Item* item, std::map<std::string,std::string> props);
  void adjust_label(Component* cmp, std::map<std::string,std::string> props);
  void setup_properties(
      Item* item,
      std::string name,
      double value,
      bool symbolic,
      std::map<std::string,std::string> props
    );

  QStack< QPair< SchematicScene*, QList<QGraphicsItem*> > > stack_;
  QList<QGraphicsItem*> items_;
  SchematicScene* scene_;
  QPointF offset_;

};



}


#endif // SCHEMATICSCENEPARSER_H
