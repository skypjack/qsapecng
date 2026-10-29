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


#ifndef SPICE_CIRCUIT_H
#define SPICE_CIRCUIT_H


#include "parser/parser.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>


namespace sapecng
{


// Writes the circuit as a SPICE netlist for an AC analysis (ngspice,
// LTspice, Xyce ...). Export only: there is no SPICE parser.
//
// Every part keeps its numeric value; the elements SPICE does not have
// are built from the ones it has:
//   conductance       a resistor of 1/G
//   ideal op-amp      a VCVS with a very large gain
//   real op-amp       gain A0 with one pole at GBW / A0 (a VCCS, R and C)
//   ideal transformer a VCVS and a CCCS sensing the secondary current
//   mutual inductance two inductors and a K coupling
//   ammeter, CCCS / CCVS control  a 0 V source the current is read from
//   R, L, C with parasitics       the parasitics as their own elements
// Each output (Out marker, voltmeter, ammeter) gets a .print line.
class spice_builder: public abstract_builder
{

public:
  spice_builder(
      std::basic_ostream<
          std::char_traits< char >::char_type
        >& stream
    ): stream_(stream) { }

  // the analysis written at the end (default: .ac dec 100 1 1MEG)
  void set_analysis(const std::string& line) { analysis_ = line; }
  // a comment line for the header (e.g. the schematic's file name)
  void set_title(const std::string& title) { title_ = title; }
  // instead of the analysis and .print lines: a .control block running
  // these commands and printing every output (scripts, tests)
  void set_control(const std::string& commands) { control_ = commands; }
  // the gain of the VCVS standing in for an ideal op-amp (default 1e8)
  void set_ideal_gain(double gain) { idealGain_ = gain; }

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
    );

  void add_dual_component(
      abstract_builder::dual_component_type c_type,
      std::string name,
      double value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void add_quad_component(
      abstract_builder::quad_component_type c_type,
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
    );

  void end_userdef_component(
      std::string name,
      std::map<std::string,std::string> props =
        std::map<std::string,std::string>()
    );

  void flush();

private:
  // a unique element name starting with the SPICE letter
  std::string element(char letter, const std::string& name);
  // an internal node (parasitics, op-amp pole, sense sources)
  std::string internal();
  static std::string node(unsigned int n);
  static std::string number(double x);
  void line(const std::string& text) { lines_.push_back(text); }
  // a 0 V source from a to b: the current through it can be read
  std::string sense(const std::string& name, const std::string& a,
    const std::string& b);

  std::basic_ostream<
      std::char_traits< char >::char_type
    >& stream_;

  std::vector<std::string> lines_;
  // one per output: what to print (v(3), v(2,1), i(V_AM1)) and its name
  std::vector< std::pair<std::string, std::string> > prints_;
  std::vector<std::string> notes_;     // header comments
  std::set<std::string> used_;         // element names, upper case
  std::vector<std::string> prefix_;    // sub-circuit instance names
  unsigned int internal_ = 0;
  unsigned int outs_ = 0;
  std::string analysis_ = ".ac dec 100 1 1MEG";
  std::string title_;
  std::string control_;
  double idealGain_ = 1e8;

};


}


#endif // SPICE_CIRCUIT_H
