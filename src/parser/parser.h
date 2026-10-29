/*
    SapecNG - Next Generation Symbolic Analysis Program for Electric Circuit
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


#ifndef PARSER_H
#define PARSER_H


#include <string>
#include <vector>
#include <map>


namespace sapecng
{


class abstract_builder
{

public:
  enum const_vertex
    { GROUND = 0, };

  enum dual_component_type
    {
      R,  // Resistor component type
      G,  // Admittance component type
      L,  // Inductor component type
      C,  // Capacitor component type
      V,  // Voltage source component type
      I,  // Current source component type
      VM,  // Voltmeter component type
      AM,  // Ammeter component type
    };

  enum quad_component_type
    {
      VCCS,  // Voltage controlled current source component type
      VCVS,  // Voltage controlled voltage source component type
      CCCS,  // Current controlled current source component type
      CCVS,  // Current controlled voltage source component type
      AO,  // Operational amplifier component type
      n,  // Ideal transformer component type
      K,  // Mutual inductance component type
    };

  enum userdef_component_type
    {
      X,  // User defined component type
    };

public:
  virtual ~abstract_builder() { }

  virtual void add_circuit_properties(
    std::map<std::string,std::string> map) = 0;
  virtual void add_circuit_property(
    std::string name, std::string value) = 0;

  virtual void add_wire_component(
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) = 0;

  virtual void add_out_component(
      unsigned int v,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) = 0;

  virtual void add_dual_component(
      dual_component_type c_type,
      std::string name,
      double value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) = 0;

  virtual void add_quad_component(
      quad_component_type c_type,
      std::string name,
      double value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      unsigned int vac,
      unsigned int vbc,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) = 0;

  virtual void add_unknow_component(
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) = 0;

  virtual void begin_userdef_component(
      std::string name,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) = 0;

  virtual void end_userdef_component(
      std::string name,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    ) = 0;

  virtual void flush() = 0;

  // A transformer with several windings. windings: the (start, end) nodes
  // of each, the start being the dotted end; ratios: the turns of each
  // winding over those of the first (ratios[0] = 1); lp: the inductance of
  // the first winding and k the coupling between every two windings
  // (0 < k <= 1), or lp <= 0 for an ideal transformer (turns only).
  //
  // By default it is built from the other elements, exactly: an ideal
  // transformer of ratio n_i from the first winding to every other one
  // and, when lp > 0, the magnetizing inductance k lp across the first
  // winding and a leakage inductance (1 - k) lp n_i^2 in series with each
  // winding. Then L_i = lp n_i^2 and M_ij = k sqrt(L_i L_j). A builder that
  // keeps the part whole (a file writer, the schematic) overrides it.
  virtual void add_transformer(
      std::string name,
      std::vector< std::pair<unsigned int, unsigned int> > windings,
      std::vector<double> ratios,
      double lp,
      double k,
      bool symbolic,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    )
  {
    if(windings.empty() || ratios.size() != windings.size())
      return;
    const std::map<std::string,std::string> none;
    const bool coupled = lp > 0.;
    const bool leaky = coupled && k < 1.;

    // the first winding: leakage in series, then the magnetizing
    // inductance; the ideal transformers start from its inner node
    unsigned int primary = windings[0].first;
    if(leaky) {
      const unsigned int m = internal_node();
      add_dual_component(L, name + ".Lk1", (1. - k) * lp, symbolic,
        windings[0].first, m, none);
      primary = m;
    }
    if(coupled)
      add_dual_component(L, name + ".Lm", k * lp, symbolic,
        primary, windings[0].second, none);

    for(std::size_t i = 1; i < windings.size(); ++i) {
      const std::string index = std::to_string(i + 1);
      unsigned int start = windings[i].first;
      if(leaky) {
        const unsigned int x = internal_node();
        add_dual_component(L, name + ".Lk" + index,
          (1. - k) * lp * ratios[i] * ratios[i], symbolic, start, x, none);
        start = x;
      }
      // V(start) - V(end) = n (V(primary) - V(primary end)); the turns
      // balance the currents into the dotted ends
      add_quad_component(n, name + ".n" + index, ratios[i], symbolic,
        windings[i].second, start, windings[0].second, primary, none);
    }
  }

protected:
  // a node inside a part built from others (add_transformer): numbered
  // far above the netlist's own nodes
  unsigned int internal_node() { return internalNode_++; }

private:
  unsigned int internalNode_ = 0x40000000u;

};



class abstract_parser
{

public:
  abstract_parser() { }
  virtual ~abstract_parser() { }

  virtual void parse(abstract_builder& builder) = 0;

};



}


#endif // PARSER_H
