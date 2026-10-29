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


#include "parser/spice_circuit.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <locale>
#include <sstream>


namespace sapecng
{


namespace
{

double prop(std::map<std::string,std::string>& props, const char* key)
{
  std::map<std::string,std::string>::const_iterator i = props.find(key);
  if(i == props.end())
    return 0.;
  std::istringstream in(i->second);
  in.imbue(std::locale::classic());
  double v = 0.;
  in >> v;
  return in.fail() ? 0. : v;
}

const double pi = 3.14159265358979323846;


}


std::string spice_builder::node(unsigned int n)
{
  std::ostringstream s;
  // 0 is the ground in SPICE too; the nodes inside a part built from
  // others (abstract_builder::internal_node) get short names
  if(n >= 0x40000000u)
    s << "t" << (n - 0x40000000u + 1);
  else
    s << n;
  return s.str();
}


std::string spice_builder::number(double x)
{
  std::ostringstream s;
  s.imbue(std::locale::classic());
  s.precision(12);
  s << x;
  return s.str();
}


std::string spice_builder::internal()
{
  std::ostringstream s;
  s << "n_" << ++internal_;
  return s.str();
}


std::string spice_builder::element(char letter, const std::string& name)
{
  std::string base;
  for(const std::string& p : prefix_)
    base += p + "_";
  base += name;
  // SPICE names: no blanks; the first letter is the element type
  for(char& c : base)
    if(std::isspace(static_cast<unsigned char>(c)) || c == '.' || c == ',')
      c = '_';
  if(base.empty() || std::toupper(static_cast<unsigned char>(base[0])) != letter)
    base = std::string(1, letter) + (base.empty() ? std::string() : "_" + base);

  std::string unique = base;
  for(int n = 2; ; ++n) {
    std::string upper = unique;
    std::transform(upper.begin(), upper.end(), upper.begin(),
      [](unsigned char c) { return char(std::toupper(c)); });
    if(used_.insert(upper).second)
      return unique;
    std::ostringstream s;
    s << base << "_" << n;
    unique = s.str();
  }
}


std::string spice_builder::sense(const std::string& name,
  const std::string& a, const std::string& b)
{
  const std::string v = element('V', name);
  line(v + " " + a + " " + b + " 0");
  return v;
}


void spice_builder::add_out_component(
    unsigned int v,
    std::map<std::string,std::string> props
  )
{
  ++outs_;
  std::string name = props["name"];
  if(name.empty()) {
    std::ostringstream s;
    s << "Out" << outs_;
    name = s.str();
  }
  prints_.push_back(std::make_pair("v(" + node(v) + ")", name));
  // its load, a resistor to the ground
  if(prop(props, "load") > 0.)
    line(element('R', "load_" + name) + " " + node(v) + " 0 "
      + number(prop(props, "load")));
}


void spice_builder::add_dual_component(
    abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string,std::string> props
  )
{
  // the program's pin order (va, vb): a source sets V(vb) - V(va), a
  // current source drives its value into va
  const std::string a = node(va), b = node(vb);
  const bool real = props["ideal"] == "0";

  switch(c_type)
  {
  case abstract_builder::R:
    {
      // a 0 ohm resistor (e.g. the ports of a sub-circuit) is a short
      if(value == 0.) {
        sense(name.empty() ? std::string("short") : name, a, b);
        break;
      }
      std::string end = b;
      if(real && prop(props, "ls") > 0.) {
        end = internal();
        line(element('L', "Ls_" + name) + " " + end + " " + b + " "
          + number(prop(props, "ls")));
      }
      line(element('R', name) + " " + a + " " + end + " " + number(value));
      if(real && prop(props, "cp") > 0.)
        line(element('C', "Cp_" + name) + " " + a + " " + b + " "
          + number(prop(props, "cp")));
      break;
    }
  case abstract_builder::G:
    line(element('R', name) + " " + a + " " + b + " " + number(1. / value)
      + "    ; conductance " + number(value) + " S");
    break;
  case abstract_builder::L:
    {
      std::string end = b;
      if(real && prop(props, "rs") > 0.) {
        end = internal();
        line(element('R', "Rs_" + name) + " " + end + " " + b + " "
          + number(prop(props, "rs")));
      }
      line(element('L', name) + " " + a + " " + end + " " + number(value));
      if(real && prop(props, "cp") > 0.)
        line(element('C', "Cp_" + name) + " " + a + " " + b + " "
          + number(prop(props, "cp")));
      break;
    }
  case abstract_builder::C:
    {
      // a - ESR - ESL - (C || leakage) - b
      std::string from = a;
      if(real && prop(props, "esr") > 0.) {
        const std::string m = internal();
        line(element('R', "Resr_" + name) + " " + from + " " + m + " "
          + number(prop(props, "esr")));
        from = m;
      }
      if(real && prop(props, "esl") > 0.) {
        const std::string m = internal();
        line(element('L', "Lesl_" + name) + " " + from + " " + m + " "
          + number(prop(props, "esl")));
        from = m;
      }
      line(element('C', name) + " " + from + " " + b + " " + number(value));
      if(real && prop(props, "rp") > 0.)
        line(element('R', "Rp_" + name) + " " + from + " " + b + " "
          + number(prop(props, "rp")));
      break;
    }
  case abstract_builder::V:
    {
      // internal resistance: the source to an internal node, Rs to the pin
      std::string plus = b;
      if(real && prop(props, "rs") > 0.) {
        plus = internal();
        line(element('R', "Rs_" + name) + " " + plus + " " + b + " "
          + number(prop(props, "rs")));
      }
      line(element('V', name) + " " + plus + " " + a + " DC 0 AC "
        + number(value));
      break;
    }
  case abstract_builder::I:
    line(element('I', name) + " " + b + " " + a + " DC 0 AC " + number(value));
    break;
  case abstract_builder::VM:
    {
      ++outs_;
      // V(b) - V(a); "v(0,n)" (the ground first) is not printed by every
      // SPICE: then a unity-gain VCVS gives the reading its own node
      const std::string label = name.empty() ? std::string("voltmeter") : name;
      std::string what;
      if(va == 0)
        what = "v(" + b + ")";
      else if(vb == 0) {
        const std::string x = internal();
        line(element('E', "read_" + label) + " " + x + " 0 " + b + " " + a
          + " 1    ; " + label + " reading");
        what = "v(" + x + ")";
      } else
        what = "v(" + b + "," + a + ")";
      prints_.push_back(std::make_pair(what, label));
      break;
    }
  case abstract_builder::AM:
    {
      ++outs_;
      const std::string v = sense(name.empty() ? std::string("AM") : name, a, b);
      prints_.push_back(std::make_pair("i(" + v + ")",
        name.empty() ? std::string("ammeter") : name));
      break;
    }
  default:
    break;
  }
}


void spice_builder::add_quad_component(
    abstract_builder::quad_component_type c_type,
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
  // the program's pin order: output (va, vb), control (vac, vbc)
  const std::string a = node(va), b = node(vb), c = node(vac), d = node(vbc);

  switch(c_type)
  {
  case abstract_builder::VCCS:
    line(element('G', name) + " " + b + " " + a + " " + d + " " + c + " "
      + number(value));
    break;
  case abstract_builder::VCVS:
    line(element('E', name) + " " + b + " " + a + " " + d + " " + c + " "
      + number(value));
    break;
  case abstract_builder::CCCS:
    {
      const std::string s = sense("sense_" + name, c, d);
      line(element('F', name) + " " + b + " " + a + " " + s + " "
        + number(value));
      break;
    }
  case abstract_builder::CCVS:
    {
      const std::string s = sense("sense_" + name, c, d);
      line(element('H', name) + " " + b + " " + a + " " + s + " "
        + number(value));
      break;
    }
  case abstract_builder::AO:
    {
      const double gbw = prop(props, "gbw");
      if(props["ideal"] != "0") {
        line(element('E', name) + " " + b + " " + a + " " + d + " " + c + " "
          + number(idealGain_) + "    ; ideal op-amp");
      } else if(gbw <= 0. || value <= 0.) {
        line(element('E', name) + " " + b + " " + a + " " + d + " " + c + " "
          + number(value) + "    ; op-amp, open-loop gain");
      } else {
        // A0 / (1 + s A0 / (2 pi GBW)): A0 (V(d) - V(c)) into 1 ohm || C
        const std::string x = internal();
        line(element('G', "pole_" + name) + " 0 " + x + " " + d + " " + c
          + " " + number(value) + "    ; op-amp: A0 = " + number(value)
          + ", GBW = " + number(gbw) + " Hz");
        line(element('R', "pole_" + name) + " " + x + " 0 1");
        line(element('C', "pole_" + name) + " " + x + " 0 "
          + number(value / (2. * pi * gbw)));
        line(element('E', name) + " " + b + " " + a + " " + x + " 0 1");
      }
      break;
    }
  case abstract_builder::n:
    {
      // V(b) - V(a) = n (V(d) - V(c)); the current into the (c, d) side
      // is n times the one through (a, b)
      const std::string x = internal();
      line(element('E', name) + " " + b + " " + x + " " + d + " " + c + " "
        + number(value) + "    ; ideal transformer, ratio " + number(value));
      const std::string s = sense("sense_" + name, x, a);
      line(element('F', name) + " " + c + " " + d + " " + s + " "
        + number(value));
      break;
    }
  case abstract_builder::K:
    {
      const double lp = prop(props, "lp:value"), ls = prop(props, "ls:value");
      const std::string p = element('L', props["lp:name"].empty()
        ? "p_" + name : props["lp:name"]);
      const std::string q = element('L', props["ls:name"].empty()
        ? "s_" + name : props["ls:name"]);
      line(p + " " + a + " " + b + " " + number(lp));
      line(q + " " + c + " " + d + " " + number(ls));
      const double k = (lp > 0. && ls > 0.) ? value / std::sqrt(lp * ls) : 0.;
      line(element('K', name) + " " + p + " " + q + " " + number(k)
        + "    ; M = " + number(value) + " H");
      break;
    }
  default:
    break;
  }
}


void spice_builder::begin_userdef_component(
    std::string name,
    std::map<std::string,std::string> props
  )
{
  prefix_.push_back(name.empty() ? std::string("X") : name);
}


void spice_builder::end_userdef_component(
    std::string name,
    std::map<std::string,std::string> props
  )
{
  if(!prefix_.empty())
    prefix_.pop_back();
}


void spice_builder::flush()
{
  stream_ << "* " << (title_.empty() ? std::string("circuit") : title_)
          << " - SPICE netlist written by QSapecNG\n";
  stream_ << "* AC analysis: sources carry their value as AC amplitude\n";
  for(const std::string& l : lines_)
    stream_ << l << "\n";
  if(prints_.empty())
    stream_ << "* no output: place an Out marker, a voltmeter or an ammeter\n";
  if(control_.empty()) {
    stream_ << analysis_ << "\n";
    for(const auto& p : prints_)
      stream_ << ".print ac " << p.first << "    ; " << p.second << "\n";
  } else {
    stream_ << ".control\n" << control_ << "\n";
    if(!prints_.empty()) {
      stream_ << "print";
      for(const auto& p : prints_)
        stream_ << " " << p.first;
      stream_ << "\n";
    }
    stream_ << ".endc\n";
  }
  stream_ << ".end\n";
  stream_.flush();
}


}
