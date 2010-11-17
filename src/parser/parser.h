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

  virtual void flush() = 0;

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
