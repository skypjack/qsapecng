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


#include "model/circuit.h"
#include <cstring>

#include <cmath>
#include <sstream>
#include <locale>

#include <algorithm>


namespace sapecng
{


namespace
{

// a number from a property (C locale; "1e-3", "0.001"); 0 when not one.
// The coil values of a mutual inductance used to be read with strtol, as
// integers: 0.001 H became 0 H
double to_double(const std::string& text)
{
  std::istringstream in(text);
  in.imbue(std::locale::classic());
  double v = 0.;
  in >> v;
  return in.fail() ? 0. : v;
}

}


circuit_builder::circuit_builder(circuit& circuit, unsigned int output)
  : circuit_(circuit), has_block_(false), output_(output), prefix_("")
{
  circuit_.props_.clear();
  circuit_.outputNames_.clear();

  if(circuit_.iG_)
    (circuit_.iG_)->clear();
  else
    circuit_.iG_ = new Graph;

  if(circuit_.vG_)
    (circuit_.vG_)->clear();
  else
    circuit_.vG_ = new Graph;

  circuit_.vG_o_.clear();
  circuit_.iG_o_.clear();

  replace_requested(GROUND);
  circuit_.out_ = GROUND;

  circuit_.reference_ = boost::add_vertex(*(circuit_.iG_));
  requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
}


void circuit_builder::add_circuit_properties(
  std::map<std::string, std::string> map)
{
  std::map<std::string, std::string>::const_iterator i;
  for(i = map.begin(); i != map.end(); ++i)
    circuit_.props_[i->first] = i->second;
}


void circuit_builder::add_circuit_property(
  std::string name, std::string value)
{
  circuit_.props_[name] = value;
}


void circuit_builder::add_wire_component(
    std::map<std::string, std::string> props
  )
{ }


unsigned int circuit_builder::node(unsigned int external)
{
  // nodes are graph vertices: numbered 1, 2, 3 ... in order of appearance,
  // whatever the numbers in the netlist. A gap (an unused number, e.g. a
  // net with only a loose wire) used to be an isolated vertex, and a
  // circuit with one had no spanning tree: "singular", result 0
  if(external == GROUND)
    return GROUND;
  std::map<unsigned int, unsigned int>::const_iterator i = nodes_.find(external);
  if(i != nodes_.end())
    return i->second;
  const unsigned int id = (unsigned int)(nodes_.size()) + 1;
  nodes_[external] = id;
  return id;
}


void circuit_builder::add_out_component(
    unsigned int v,
    std::map<std::string, std::string> props
  )
{
  v = node(v);
  replace_requested(v);
  // a load on the output: a resistor to the ground (unnamed: numeric)
  if(props.count("load")) {
    double load = 0.;
    std::istringstream in(props["load"]);
    in >> load;
    if(load > 0. && std::isfinite(load)) {
      std::map<std::string, std::string> none;
      add_edge(v, GROUND, v, GROUND, Z, "", load, 0, false, none);
    }
  }
  add_output(v, props["name"], "Out");
}


void circuit_builder::add_output(circuit::vertex_descriptor vertex,
  const std::string& name, const char* prefix)
{
  // the vertex may still move (replace_requested); out_ is chosen in flush
  outputs_.push_back(vertex);
  std::string shown = name;
  if(shown.empty()) {
    unsigned int n = 0;
    for(const std::string& other : circuit_.outputNames_)
      if(other.compare(0, std::strlen(prefix), prefix) == 0)
        ++n;
    shown = prefix + std::to_string(n + 1);
  }
  circuit_.outputNames_.push_back(shown);
}


void circuit_builder::add_dual_component(
    abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string, std::string> props
  )
{
  add_dual(c_type, name, value, symbolic, node(va), node(vb), props);
}


void circuit_builder::add_dual(
    abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string, std::string> props
  )
{
  replace_requested(va <= vb ? va : vb);
  replace_requested(va > vb ? va : vb);

  // a voltage source with internal resistance: the ideal source to an
  // internal node, Rs from there to the + pin (unnamed: numeric)
  if(c_type == abstract_builder::V && props.count("ideal")
      && props["ideal"] == "0")
  {
    double rs = 0.;
    if(props.count("rs")) {
      std::istringstream in(props["rs"]);
      in >> rs;
    }
    props.erase("ideal");
    props.erase("rs");
    if(rs > 0. && std::isfinite(rs)) {
      std::map<std::string, std::string> none;
      circuit::vertex_descriptor n = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
      add_edge(n, vb, n, vb, Z, "", rs, 0, false, none);
      vb = n;
    }
  }

  if((c_type == abstract_builder::R || c_type == abstract_builder::L
      || c_type == abstract_builder::C)
    && props.count("ideal") && props["ideal"] == "0")
  {
    add_real_passive(c_type, name, value, symbolic, va, vb, props);
    return;
  }

  switch(c_type)
  {
  case abstract_builder::R:
    {
      add_edge(
          va, vb, va, vb, Z,
          prefix_ + name, value,
          0, symbolic, props
        );
      break;
    }
  case abstract_builder::G:
    {
      add_edge(
          va, vb, va, vb, Y,
          prefix_ + name, value,
          0, symbolic, props
        );
      break;
    }
  case abstract_builder::L:
    {
      add_edge(
          va, vb, va, vb, Z,
          prefix_ + name, value,
          1, symbolic, props
        );
      break;
    }
  case abstract_builder::C:
    {
      add_edge(
          va, vb, va, vb, Y,
          prefix_ + name, value,
          1, symbolic, props
        );
      break;
    }
  case abstract_builder::V:
    {
      circuit::vertex_descriptor vdesc = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

      add_edge(
          vdesc, GROUND, GROUND, circuit_.reference_,
          Y, prefix_ + name, value, 0, symbolic, props
        );
      add_edge(
          vdesc, GROUND, vb, va,
          Y, "", -1., 0, false, props
        );
      add_edge(
          vb, va, GROUND, vdesc,
          F, "", 1., 0, true, props
        );

      break;
    }
  case abstract_builder::I:
    {
      add_edge(
          vb, va, GROUND, circuit_.reference_,
          Y, prefix_ + name, value, 0, symbolic, props
        );

      break;
    }
  case abstract_builder::VM:
    {
      circuit::vertex_descriptor vdesc = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
      circuit::vertex_descriptor v_ = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

      add_edge(
          vdesc, vb, vb, va, Y,
          "", -1., 0, false, props
        );
      add_edge(
          vdesc, vb, GROUND, v_, Y,
          "", -1., 0, false, props
        );
      add_edge(
          GROUND, v_, vb, vdesc, F,
          "", 1., 0, true, props
        );

      add_output(v_, name, "VM");

      break;
    }
  case abstract_builder::AM:
    {
      circuit::vertex_descriptor v_ = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

      add_edge(
          va, vb, GROUND, v_, Z,
          "", -1., 0, false, props
        );
      add_edge(
          GROUND, v_, vb, va, F,
          "", 1., 0, true, props
        );

      add_output(v_, name, "AM");

      break;
    }
  default:
    break;
  }
}


void circuit_builder::add_real_passive(
    abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string, std::string> props
  )
{
  // R, L or C with parasitics (props, SI units; missing or 0: none):
  //   R: "ls" series inductance, "cp" parallel capacitance
  //   L: "rs" series (winding) resistance, "cp" parallel capacitance
  //   C: "esr", "esl" in series, "rp" leakage across the capacitance
  // The series parts form a chain va - ... - main part - vb through
  // internal nodes; the parasitics are unnamed (numeric) so the result
  // keeps only the component's own symbol.
  props.erase("ideal");
  std::map<std::string, std::string> none;
  auto take = [&props](const char* key) {
      double v = 0.;
      std::map<std::string, std::string>::iterator i = props.find(key);
      if(i != props.end()) {
        std::istringstream in(i->second);
        in >> v;
        props.erase(i);
      }
      return (v > 0. && std::isfinite(v)) ? v : 0.;
    };

  struct Part { EdgeType type; double value; int degree; };
  std::vector<Part> series;
  double cp = 0., rp = 0.;
  EdgeType type = Z;
  int degree = 0;
  switch(c_type)
  {
  case abstract_builder::R:
    series.push_back({ Z, take("ls"), 1 });
    cp = take("cp");
    type = Z; degree = 0;
    break;
  case abstract_builder::L:
    series.push_back({ Z, take("rs"), 0 });
    cp = take("cp");
    type = Z; degree = 1;
    break;
  default:  // C
    series.push_back({ Z, take("esr"), 0 });
    series.push_back({ Z, take("esl"), 1 });
    rp = take("rp");
    type = Y; degree = 1;
    break;
  }
  props.erase("cp"); props.erase("ls"); props.erase("rs");
  props.erase("esr"); props.erase("esl"); props.erase("rp");

  circuit::vertex_descriptor cur = va;
  for(const Part& part : series) {
    if(!(part.value > 0.))
      continue;
    circuit::vertex_descriptor n = boost::add_vertex(*(circuit_.iG_));
    requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
    add_edge(cur, n, cur, n, part.type, "", part.value, part.degree,
      false, none);
    cur = n;
  }

  add_edge(cur, vb, cur, vb, type, prefix_ + name, value, degree,
    symbolic, props);
  if(rp > 0.)
    add_edge(cur, vb, cur, vb, Y, "", 1. / rp, 0, false, none);
  if(cp > 0.)
    add_edge(va, vb, va, vb, Y, "", cp, 1, false, none);
}


void circuit_builder::add_real_opamp(
    std::string name,
    double a0,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    unsigned int vac,
    unsigned int vbc,
    std::map<std::string, std::string> props
  )
{
  // output (vb, ref va) = A(s) * control voltage (vac, vbc), with
  // A(s) = A0 / (1 + s / wp), wp = 2 pi GBW / A0. Without a GBW it is a
  // plain VCVS of gain A0 (named after the op-amp, so A0 can stay
  // symbolic); with one, the VCVS drives an internal RC pole followed by
  // a unity-gain buffer. Internal parts are unnamed: they stay numeric
  // and do not show up in the results.
  props.erase("ideal");
  double gbw = 0.;
  if(props.count("gbw")) {
    std::istringstream in(props["gbw"]);
    in >> gbw;
    props.erase("gbw");
  }

  if(!(gbw > 0.) || !(a0 > 0.)) {
    add_quad(abstract_builder::VCVS, name, a0, symbolic,
      va, vb, vac, vbc, props);
    return;
  }

  const double wp = 2. * 4. * std::atan(1.) * gbw / a0;

  // two internal nodes: gain stage output and pole node
  circuit::vertex_descriptor n1 = boost::add_vertex(*(circuit_.iG_));
  requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
  circuit::vertex_descriptor n2 = boost::add_vertex(*(circuit_.iG_));
  requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

  // edges are added directly: add_dual/quad_component would treat the
  // internal nodes as requested ones and move their edges away
  std::map<std::string, std::string> none;
  add_vcvs_edges(name, a0, symbolic, va, n1, vac, vbc, props);
  add_edge(n1, n2, n1, n2, Y, "", 1., 0, false, none);            // G = 1 S
  add_edge(n2, va, n2, va, Y, "", 1. / wp, 1, false, none);       // C
  add_vcvs_edges("", 1., false, va, vb, va, n2, none);            // buffer
}


void circuit_builder::add_vcvs_edges(
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
  // as abstract_builder::VCVS in add_quad_component
  circuit::vertex_descriptor vdesc = boost::add_vertex(*(circuit_.iG_));
  requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

  add_edge(
      vdesc, vbc, vbc, vac, Y,
      name.empty() ? std::string() : prefix_ + name, value,
      0, symbolic, props
    );
  add_edge(
      vdesc, vbc, vb, va, Y,
      "", -1., 0, false, props
    );
  add_edge(
      vb, va, vbc, vdesc, F,
      "", 1., 0, true, props
    );
}


void circuit_builder::add_quad_component(
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
  add_quad(c_type, name, value, symbolic,
    node(va), node(vb), node(vac), node(vbc), props);
}


void circuit_builder::add_quad(
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
  std::vector<circuit::vertex_descriptor> nv;
  nv.push_back(va);
  nv.push_back(vb);
  nv.push_back(vac);
  nv.push_back(vbc);
  std::sort(nv.begin(), nv.end());

  replace_requested(nv.at(0));
  replace_requested(nv.at(1));
  replace_requested(nv.at(2));
  replace_requested(nv.at(3));

  switch(c_type)
  {
  case abstract_builder::VCCS:
    {
      add_edge(
          vb, va, vbc, vac, Y,
          prefix_ + name, value,
          0, symbolic, props
        );

      break;
    }
  case abstract_builder::VCVS:
    {
      circuit::vertex_descriptor vdesc = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

      add_edge(
          vdesc, vbc, vbc, vac, Y,
          prefix_ + name, value,
          0, symbolic, props
        );
      add_edge(
          vdesc, vbc, vb, va, Y,
          "", -1., 0, false, props
        );
      add_edge(
          vb, va, vbc, vdesc, F,
          "", 1., 0, true, props
        );

      break;
    }
  case abstract_builder::CCCS:
    {
      circuit::vertex_descriptor vdesc = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

      add_edge(
          vac, vbc, GROUND, vdesc, Z,
          prefix_ + name, value,
          0, symbolic, props
        );
      add_edge(
          GROUND, vdesc, vbc, vac, F,
          "", 1., 0, true, props
        );
      add_edge(
          va, vb, GROUND, vdesc, Y,
          "", -1., 0, false, props
        );

      break;
    }
  case abstract_builder::CCVS:
    {
      add_edge(
          vac, vbc, vb, va, Z,
          prefix_ + name, value,
          0, symbolic, props
        );
      add_edge(
          vb, va, vbc, vac, F,
          "", 1., 0, true, props
        );

      break;
    }
  case abstract_builder::AO:
    {
      // ideal op-amp (nullor) unless the GUI says otherwise
      if(props.count("ideal") && props["ideal"] == "0") {
        add_real_opamp(name, value, symbolic, va, vb, vac, vbc, props);
        break;
      }

      add_edge(
          vb, va, vbc, vac, F,
          "", 1., 0, true, props
        );

      break;
    }
  case abstract_builder::n:
    {
      circuit::vertex_descriptor vvdesc = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
      circuit::vertex_descriptor ivdesc = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
      circuit::vertex_descriptor fix = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

      add_edge(
          vvdesc, vbc, vbc, vac, Y,
          prefix_ + name, value,
          0, symbolic, props
        );
      add_edge(
          vvdesc, vbc, vb, fix, Y,
          "", -1., 0, false, props
        );
      add_edge(
          vb, fix, vbc, vvdesc, F,
          "", 1., 0, true, props
        );

      add_edge(
          fix, va, GROUND, ivdesc, Z,
          prefix_ + name, value,
          0, symbolic, props
        );
      add_edge(
          GROUND, ivdesc, va, fix, F,
          "", 1., 0, true, props
        );
      add_edge(
          vbc, vac, GROUND, ivdesc, Y,
          "", -1., 0, false, props
        );

      break;
    }
  case abstract_builder::K:
    {
      circuit::vertex_descriptor afix = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
      circuit::vertex_descriptor bfix = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
      circuit::vertex_descriptor cfix = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));
      circuit::vertex_descriptor dfix = boost::add_vertex(*(circuit_.iG_));
      requested_.push_front(boost::add_vertex(*(circuit_.vG_)));

      add_edge(
          va, afix, va, afix, Z, prefix_ + props["lp:name"],
          to_double(props["lp:value"]),
          1, symbolic, props
        );
      add_edge(
          vac, bfix, vac, bfix, Z, prefix_ + props["ls:name"],
          to_double(props["ls:value"]),
          1, symbolic, props
        );

      add_edge(
          dfix, bfix, vb, cfix, Z,
          prefix_ + name, value,
          1, symbolic, props
        );
      add_edge(
          vb, cfix, bfix, dfix, F,
          "", 1., 1, true, props
        );

      add_edge(
          cfix, afix, vbc, dfix, Z,
          prefix_ + name, value,
          1, symbolic, props
        );
      add_edge(
          vbc, dfix, afix, cfix, F,
          "", 1., 1, true, props
        );

      break;
    }
  default:
    break;
  }
}


void circuit_builder::flush()
{
  circuit_.out_ = GROUND;
  if(output_ < outputs_.size())
    circuit_.out_ = outputs_[output_];
  try_add_block();
}


void circuit_builder::try_add_block()
{
  if(!has_block_ && circuit_.out_) {
    add_edge(
        GROUND, circuit_.reference_,
        GROUND, circuit_.reference_,
        YREF, "", 1., 0, false,
        std::map<std::string, std::string>()
      );
    add_edge(
        circuit_.reference_, GROUND,
        GROUND, circuit_.out_,
        GREF, "", 1., 0, false,
        std::map<std::string, std::string>()
      );

    has_block_ = true;
  }
}


void circuit_builder::replace_requested(circuit::vertex_descriptor vertex)
{
  boost::remove_edge(
      boost::add_edge(vertex, vertex, *(circuit_.iG_)).first,
      *(circuit_.iG_)
    );
  boost::remove_edge(
      boost::add_edge(vertex, vertex, *(circuit_.vG_)).first,
      *(circuit_.vG_)
    );

  if(std::find(requested_.begin(), requested_.end(), vertex)
      != requested_.end())
  {

    circuit::vertex_descriptor ireq = boost::add_vertex(*(circuit_.iG_));
    circuit::vertex_descriptor vreq = boost::add_vertex(*(circuit_.vG_));

    if(ireq != vreq) {
      boost::remove_vertex(ireq, *(circuit_.iG_));
      boost::remove_vertex(vreq, *(circuit_.vG_));
      return;
    }

    circuit::edge_iterator icurr, iend, vcurr, vend;
    boost::tie(icurr, iend) = boost::edges(*(circuit_.iG_));
    boost::tie(vcurr, vend) = boost::edges(*(circuit_.vG_));

    while(icurr != iend && vcurr != vend) {
      circuit::edge_descriptor iedge = *icurr;
      circuit::edge_descriptor vedge = *vcurr;

      if(boost::source(iedge, *(circuit_.iG_)) == vertex
          || boost::target(iedge, *(circuit_.iG_)) == vertex
          || boost::source(vedge, *(circuit_.vG_)) == vertex
          || boost::target(vedge, *(circuit_.vG_)) == vertex
        )
      {
        add_edge(
          (iedge.m_source == vertex ? ireq : iedge.m_source),
          (iedge.m_target == vertex ? ireq : iedge.m_target),
          (vedge.m_source == vertex ? vreq : vedge.m_source),
          (vedge.m_target == vertex ? vreq : vedge.m_target),
          get(boost::edge_type, *(circuit_.iG_), iedge),
          get(boost::edge_name, *(circuit_.iG_), iedge),
          get(boost::edge_weight, *(circuit_.iG_), iedge),
          get(boost::edge_degree, *(circuit_.iG_), iedge),
          get(boost::edge_symbolic, *(circuit_.iG_), iedge),
          get(boost::edge_any, *(circuit_.iG_), iedge)
        );

        boost::remove_edge(iedge, *(circuit_.iG_));
        boost::remove_edge(vedge, *(circuit_.vG_));
        circuit_.iG_o_.erase(
            std::find(circuit_.iG_o_.begin(), circuit_.iG_o_.end(), iedge)
          );
        circuit_.vG_o_.erase(
            std::find(circuit_.vG_o_.begin(), circuit_.vG_o_.end(), vedge)
          );

        boost::tie(icurr, iend) = boost::edges(*(circuit_.iG_));
        boost::tie(vcurr, vend) = boost::edges(*(circuit_.vG_));
      } else {
        ++icurr;
        ++vcurr;
      }
    }

    if(circuit_.reference_ == vertex)
      circuit_.reference_ = ireq;
    // a meter's output vertex moved too
    for(circuit::vertex_descriptor& o : outputs_)
      if(o == vertex)
        o = ireq;

    requested_.remove(vertex);
    requested_.push_front(ireq);

  }
}


void circuit_builder::add_edge(
      circuit::vertex_descriptor ivb,
      circuit::vertex_descriptor iva,
      circuit::vertex_descriptor vvb,
      circuit::vertex_descriptor vva,
      EdgeType type,
      std::string name,
      double value,
      int degree,
      bool symbolic,
      std::map<std::string, std::string> props
  )
{
  circuit::edge_descriptor iedesc =
    boost::add_edge(iva, ivb, *circuit_.iG_).first;
  circuit::edge_descriptor vedesc =
    boost::add_edge(vva, vvb, *circuit_.vG_).first;
  circuit_.iG_o_.push_back(iedesc);
  circuit_.vG_o_.push_back(vedesc);

  put(boost::edge_type, *circuit_.iG_, iedesc, type);
  put(boost::edge_name, *circuit_.iG_, iedesc, name);
  put(boost::edge_weight, *circuit_.iG_, iedesc, value);
  put(boost::edge_degree, *circuit_.iG_, iedesc, degree);
  put(boost::edge_symbolic, *circuit_.iG_, iedesc, symbolic);
  put(boost::edge_any, *circuit_.iG_, iedesc, props);

  put(boost::edge_type, *circuit_.vG_, vedesc, type);
  put(boost::edge_name, *circuit_.iG_, vedesc, name);
  put(boost::edge_weight, *circuit_.iG_, vedesc, value);
  put(boost::edge_degree, *circuit_.iG_, vedesc, degree);
  put(boost::edge_symbolic, *circuit_.iG_, vedesc, symbolic);
  put(boost::edge_any, *circuit_.iG_, vedesc, props);
}


}
