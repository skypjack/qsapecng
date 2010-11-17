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


#ifndef CRC_CIRCUIT_H
#define CRC_CIRCUIT_H


#include "parser/parser.h"
#include "exception/sapecngexception.h"

#include <iostream>


namespace sapecng
{


class crc_builder: public abstract_builder
{

public:
  crc_builder(
      std::ostream& stream
    ): stream_(stream), out_(0) { }

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

  void flush();

private:
  void add_item(
      std::string name,
      double value,
      bool symbolic,
      std::vector<unsigned int> nodes
    );

private:
  std::ostream& stream_;
  unsigned int out_;

};



}


#endif // CRC_CIRCUIT_H
