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


#include "parser/crc_circuit.h"


namespace sapecng
{


void crc_builder::add_out_component(
    unsigned int v,
    std::map<std::string, std::string> props
  )
{
  out_ = v;
}


void crc_builder::add_dual_component(
    abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string, std::string> props
  )
{
  std::vector<unsigned int> nodes;
  nodes.push_back(va);
  nodes.push_back(vb);

  switch(c_type)
  {
  case abstract_builder::R:
    {
      if(name.size() == 0 || name.at(0) != 'R')
        name = "R_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::G:
    {
      if(name.size() == 0 || name.at(0) != 'G')
        name = "G_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::L:
    {
      if(name.size() == 0 || name.at(0) != 'L')
        name = "L_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::C:
    {
      if(name.size() == 0 || name.at(0) != 'C')
        name = "C_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::V:
    {
      if(name.size() == 0 || name.at(0) != 'V')
        name = "V_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::I:
    {
      if(name.size() == 0 || name.at(0) != 'I')
        name = "I_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  default:
    break;
  }
}


void crc_builder::add_quad_component(
    abstract_builder::quad_component_type c_type,
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
  std::vector<unsigned int> nodes;
  nodes.push_back(va);
  nodes.push_back(vb);
  nodes.push_back(vac);
  nodes.push_back(vbc);

  switch(c_type)
  {
  case abstract_builder::VCCS:
    {
      if(name.size() == 0 || name.at(0) != 'H')
        name = "H_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::VCVS:
    {
      if(name.size() == 0 || name.at(0) != 'E')
        name = "E_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::CCCS:
    {
      if(name.size() == 0 || name.at(0) != 'F')
        name = "F_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::CCVS:
    {
      if(name.size() == 0 || name.at(0) != 'Y')
        name = "Y_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::AO:
    {
      if(name.size() == 0 || name.at(0) != 'A')
        name = "A_" + name;

      stream_ << name << " ";
      for(std::vector<unsigned int>::size_type i = 0; i < nodes.size(); ++i)
        stream_ << nodes[i] << " ";
      stream_ << std::endl;

      break;
    }
  case abstract_builder::n:
    {
      if(name.size() == 0 || name.at(0) != 'n')
        name = "n_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::K:
    {
      if(name.size() == 0 || name.at(0) != 'K')
        name = "K_" + name;

      std::vector<unsigned int> lp_nodes, ls_nodes;
      lp_nodes.push_back(va);
      lp_nodes.push_back(vb);
      ls_nodes.push_back(vac);
      ls_nodes.push_back(vbc);

      add_item(
        props["lp:name"], strtol(props["lp:value"].c_str(), 0, 10),
        symbolic, lp_nodes);
      add_item(
        props["ls:name"], strtol(props["ls:value"].c_str(), 0, 10),
        symbolic, ls_nodes);

      stream_ << name << " " << props["lp:name"] << " " << props["ls:name"];
      stream_ << " " << value << " " << symbolic << std::endl;

      break;
    }
  default:
    break;
  }
}


void crc_builder::flush()
{
  stream_ << ".OUT " << out_ << std::endl;
  stream_ << ".END" << std::endl;
}


void crc_builder::add_item(
    std::string name,
    double value,
    bool symbolic,
    std::vector<unsigned int> nodes
  )
{
  stream_ << name << " ";
  for(std::vector<unsigned int>::size_type i = 0; i < nodes.size(); ++i)
    stream_ << nodes[i] << " ";
  stream_ << value << " ";
  stream_ << symbolic << std::endl;
}


}
