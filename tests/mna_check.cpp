// Cross-checks the symbolic engine against an independent numeric solver.
//
// The same circuit is fed to sapecng::circuit_builder (two-graph symbolic
// analysis) and to a modified nodal analysis (MNA) builder written here
// from scratch; the engine's numeric transfer function (metacircuit
// "digit", a ratio of polynomials in s) is evaluated at s = j 2 pi f and
// compared with the MNA node voltage at the Out node.
//
// usage:
//   mna_check file <circuit.info|circuit.crc>   one circuit
//   mna_check file-numeric <circuit>             the same, the numeric
//                                                solve (any size)
//   mna_check roots-file <circuit.info>          the numeric result's
//                                                poles and zeros
//   mna_check fuzz <count> [seed]                random circuits
//   mna_check bench                              solve time vs. size
//   mna_check reference <count> [seed] [ext]     random circuits, the
//                                                solver against the old one
//   mna_check reference-file <circuit>...        the same, on files
//   mna_check numeric <count> [seed] [ext]       random circuits, the
//                                                numeric solve against the
//                                                tree solve's digit result
//   mna_check numeric-file <circuit>...          the same, on files

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "model/circuit.h"
#include "model/metacircuit.h"
#include "parser/crc_circuit.h"
#include "parser/ir_circuit.h"
#include "parser/spice_circuit.h"
#include "functor/functor.hpp"
#include "reference_solver.h"

namespace sapecng {
std::list< std::pair< double, double > > rpoly_adapter(const std::map< int, double >& p);
}

using cplx = std::complex<double>;
using props_t = std::map<std::string, std::string>;
using sapecng::abstract_builder;

namespace {

const double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- recorder

struct Element
{
  bool quad = false;
  int type = 0;
  std::string name;
  double value = 0.;
  bool symbolic = false;
  unsigned int n[4] = { 0, 0, 0, 0 };
  props_t props;
  // a transformer (add_transformer): windings, turns ratios, lp and k
  // (value: lp, props "k"); the pins are in windings, not in n
  bool transformer = false;
  std::vector< std::pair<unsigned int, unsigned int> > windings;
  std::vector<double> ratios;
};

double prop(const props_t& p, const char* key);

struct Netlist
{
  std::vector<Element> elements;
  unsigned int out = 0;
  // several Out markers (when not empty): out is outs[outIndex], the one
  // the engine is asked for; the others, and any voltmeter or ammeter,
  // must not change it
  std::vector<unsigned int> outs;
  unsigned int outIndex = 0;

  // the engine's output index: meters come first (replayed with the
  // elements), then the Out markers
  unsigned int engineOutput() const
  {
    unsigned int meters = 0;
    for(const Element& e : elements)
      if(!e.quad && (e.type == abstract_builder::VM || e.type == abstract_builder::AM))
        ++meters;
    return meters + (outs.empty() ? 0 : outIndex);
  }

  void replay(abstract_builder& b) const
  {
    for(const Element& e : elements) {
      if(e.transformer)
        b.add_transformer(e.name, e.windings, e.ratios, e.value,
          prop(e.props, "k"), e.symbolic, e.props);
      else if(e.quad)
        b.add_quad_component(
          static_cast<abstract_builder::quad_component_type>(e.type),
          e.name, e.value, e.symbolic, e.n[0], e.n[1], e.n[2], e.n[3], e.props);
      else
        b.add_dual_component(
          static_cast<abstract_builder::dual_component_type>(e.type),
          e.name, e.value, e.symbolic, e.n[0], e.n[1], e.props);
    }
    if(!outs.empty())
      for(unsigned int o : outs)
        b.add_out_component(o);
    else if(out)
      b.add_out_component(out);
    b.flush();
  }

  std::string describe() const
  {
    static const char* dual[] = { "R", "G", "L", "C", "V", "I", "VM", "AM" };
    static const char* quad[] = { "VCCS", "VCVS", "CCCS", "CCVS", "AO", "n", "K" };
    std::ostringstream s;
    for(const Element& e : elements) {
      if(e.transformer) {
        s << "  T " << e.name << "  lp=" << e.value << "  windings";
        for(std::size_t i = 0; i < e.windings.size(); ++i)
          s << " (" << e.windings[i].first << "," << e.windings[i].second
            << " n=" << e.ratios[i] << ")";
        for(const auto& p : e.props)
          s << "  " << p.first << "=" << p.second;
        s << "\n";
        continue;
      }
      s << "  " << (e.quad ? quad[e.type] : dual[e.type]) << " " << e.name
        << " = " << e.value << "  nodes";
      for(int i = 0; i < (e.quad ? 4 : 2); ++i)
        s << " " << e.n[i];
      for(const auto& p : e.props)
        s << "  " << p.first << "=" << p.second;
      s << "\n";
    }
    s << "  out " << out;
    if(!outs.empty()) {
      s << "  (outputs";
      for(unsigned int o : outs)
        s << " " << o;
      s << ", engine output " << engineOutput() << ")";
    }
    s << "\n";
    return s.str();
  }
};

class Recorder: public abstract_builder
{
public:
  Netlist net;

  void add_circuit_properties(props_t) override { }
  void add_circuit_property(std::string, std::string) override { }
  void add_wire_component(props_t) override { }
  void add_out_component(unsigned int v, props_t) override
    { if(!net.out) net.out = v; }
  void add_dual_component(dual_component_type t, std::string name,
      double value, bool symbolic, unsigned int va, unsigned int vb,
      props_t props) override
  {
    Element e;
    e.type = t; e.name = name; e.value = value; e.symbolic = symbolic;
    e.n[0] = va; e.n[1] = vb; e.props = props;
    net.elements.push_back(e);
  }
  void add_quad_component(quad_component_type t, std::string name,
      double value, bool symbolic, unsigned int va, unsigned int vb,
      unsigned int vac, unsigned int vbc, props_t props) override
  {
    Element e;
    e.quad = true;
    e.type = t; e.name = name; e.value = value; e.symbolic = symbolic;
    e.n[0] = va; e.n[1] = vb; e.n[2] = vac; e.n[3] = vbc; e.props = props;
    net.elements.push_back(e);
  }
  void add_transformer(std::string name,
      std::vector< std::pair<unsigned int, unsigned int> > windings,
      std::vector<double> ratios, double lp, double k, bool symbolic,
      props_t props) override
  {
    Element e;
    e.transformer = true;
    e.name = name; e.value = lp; e.symbolic = symbolic;
    e.windings = windings; e.ratios = ratios;
    e.props = props;
    std::ostringstream ks;
    ks.precision(17);
    ks << k;
    e.props["k"] = ks.str();
    net.elements.push_back(e);
  }
  void add_unknow_component(props_t) override { }
  void begin_userdef_component(std::string, props_t) override { }
  void end_userdef_component(std::string, props_t) override { }
  void flush() override { }
};

// ---------------------------------------------------------------- MNA

// Conventions (those of the GUI, which passes the pins in this order):
//   dual (va, vb):   V: V(vb) - V(va) = value
//                    I: value flows through the source from va to vb
//   VCCS (va, vb, vac, vbc): value * (V(vbc) - V(vac)) through the output
//                    from va to vb
//   VCVS:            V(vb) - V(va) = value * (V(vbc) - V(vac))
//   CCCS / CCVS:     control: a short from vac to vbc carrying i_c
//                    CCCS: value * i_c from va to vb; CCVS: V(vb) - V(va) = value * i_c
//   AO (nullor):     V(vac) = V(vbc), output current free between va, vb
// Signs that turn out to be the other way round are calibrated in main().
struct Signs
{
  double I = 1, VCCS = 1, VCVS = 1, CCCS = 1, CCVS = 1;
  double nV = 1, nI = 1, K = 1;
};

double prop(const props_t& p, const char* key)
{
  auto i = p.find(key);
  return i == p.end() ? 0. : std::atof(i->second.c_str());
}

bool real(const Element& e)
{
  auto i = e.props.find("ideal");
  return i != e.props.end() && i->second == "0";
}

class Mna
{
public:
  Mna(const Netlist& net, const Signs& sg): net_(net), sg_(sg)
  {
    // unknowns for the nodes in use (any numbering), ground is 0
    std::map<unsigned int, int> ids;
    auto use = [&](unsigned int n) {
        if(n && !ids.count(n)) { const int k = int(ids.size()) + 1; ids[n] = k; }
      };
    use(net.out);
    for(const Element& e : net.elements) {
      if(e.transformer) {
        for(const auto& w : e.windings) { use(w.first); use(w.second); }
        continue;
      }
      for(int i = 0; i < (e.quad ? 4 : 2); ++i)
        use(e.n[i]);
    }
    ids_ = ids;
    nodes_ = unsigned(ids.size());
  }

  // V(out) at frequency f; false when the circuit is singular
  bool solve(double f, cplx* result) const
  {
    // long double: a reference a few digits better than the engine
    using lc = std::complex<long double>;
    const lc s(0., 2. * kPi * f);

    // count branch currents
    int extra = 0;
    for(const Element& e : net_.elements) {
      if(e.transformer) { extra += int(e.windings.size()); continue; }
      if(!e.quad && (e.type == abstract_builder::V || e.type == abstract_builder::AM))
        ++extra;
      if(e.quad) {
        switch(e.type) {
        case abstract_builder::VCVS: extra += 1; break;
        case abstract_builder::CCCS: extra += 1; break;
        case abstract_builder::CCVS: extra += 2; break;
        case abstract_builder::AO: extra += 1; break;
        case abstract_builder::n: extra += 2; break;
        case abstract_builder::K: extra += 2; break;
        default: break;
        }
      }
    }
    const int N = int(nodes_) + extra;
    std::vector<std::vector<lc>> A(N, std::vector<lc>(N + 1, 0.));
    auto idx = [this](unsigned int node) { return ids_.at(node) - 1; };
    auto addY = [&](unsigned int a, unsigned int b, lc y) {
        if(a) A[idx(a)][idx(a)] += y;
        if(b) A[idx(b)][idx(b)] += y;
        if(a && b) { A[idx(a)][idx(b)] -= y; A[idx(b)][idx(a)] -= y; }
      };
    // a current i (unknown k) leaving node a and entering node b
    auto addBranch = [&](unsigned int a, unsigned int b, int k, double gain) {
        if(a) A[idx(a)][k] += gain;
        if(b) A[idx(b)][k] -= gain;
      };
    auto addVoltage = [&](int row, unsigned int p, unsigned int m, double g) {
        if(p) A[row][idx(p)] += g;
        if(m) A[row][idx(m)] -= g;
      };

    int k = int(nodes_);
    for(const Element& e : net_.elements) {
      if(e.transformer) {
        // its own model, not the engine's decomposition: winding currents
        // I_i into the dotted ends; coupled: V_i = s sum_j M_ij I_j with
        // L_i = lp n_i^2, M_ij = k lp n_i n_j; ideal: V_i = n_i V_1 and
        // sum_i n_i I_i = 0
        const int N = int(e.windings.size());
        const long double lp = e.value, kc = prop(e.props, "k");
        for(int i = 0; i < N; ++i)
          addBranch(e.windings[i].first, e.windings[i].second, k + i, 1.);
        if(lp > 0) {
          for(int i = 0; i < N; ++i) {
            addVoltage(k + i, e.windings[i].first, e.windings[i].second, 1.);
            for(int j = 0; j < N; ++j) {
              const long double m = (i == j ? 1.L : kc) * lp
                * e.ratios[i] * e.ratios[j];
              A[k + i][k + j] -= s * m;
            }
          }
        } else {
          for(int i = 1; i < N; ++i) {
            addVoltage(k + i - 1, e.windings[i].first, e.windings[i].second, 1.);
            addVoltage(k + i - 1, e.windings[0].first, e.windings[0].second,
              -e.ratios[i]);
          }
          for(int i = 0; i < N; ++i)
            A[k + N - 1][k + i] += e.ratios[i];
        }
        k += N;
        continue;
      }
      const unsigned int a = e.n[0], b = e.n[1], c = e.n[2], d = e.n[3];
      const double v = e.value;
      if(!e.quad) {
        switch(e.type) {
        case abstract_builder::R:
        case abstract_builder::L:
        case abstract_builder::C:
          if(real(e)) {
            // independent model of the parasitics: series parts as
            // impedances in one chain, parallel parts as admittances
            lc zs = 0.;          // series impedance of the parasitics
            lc main;             // the part itself (admittance)
            lc yAcrossMain = 0.; // leakage across the part only (C)
            lc yAcrossAll = 0.;  // across both terminals
            if(e.type == abstract_builder::R) {
              main = 1.L / (long double)v;
              zs = s * (long double)prop(e.props, "ls");
              yAcrossAll = s * (long double)prop(e.props, "cp");
            } else if(e.type == abstract_builder::L) {
              main = 1.L / (s * (long double)v);
              zs = (long double)prop(e.props, "rs");
              yAcrossAll = s * (long double)prop(e.props, "cp");
            } else {
              main = s * (long double)v;
              zs = (long double)prop(e.props, "esr")
                 + s * (long double)prop(e.props, "esl");
              const double rp = prop(e.props, "rp");
              if(rp > 0) yAcrossMain = 1.L / (long double)rp;
            }
            const lc zMain = 1.L / (main + yAcrossMain);
            addY(a, b, 1.L / (zs + zMain) + yAcrossAll);
          } else if(e.type == abstract_builder::R) addY(a, b, lc(1.L / v));
          else if(e.type == abstract_builder::L) addY(a, b, 1.L / (s * (long double)v));
          else addY(a, b, s * (long double)v);
          break;
        case abstract_builder::G: addY(a, b, lc(v)); break;
        case abstract_builder::V:
          // current k flows through the source from vb (+) out ... into vb
          addBranch(b, a, k, 1.);
          addVoltage(k, b, a, 1.);
          A[k][N] += v;
          ++k;
          break;
        case abstract_builder::AM:
          // an ideal ammeter is a short: a 0 V source (a voltmeter is
          // open and adds nothing)
          addBranch(b, a, k, 1.);
          addVoltage(k, b, a, 1.);
          ++k;
          break;
        case abstract_builder::I:
          // value from va to vb through the source: leaves va's node? the
          // source pushes current out of its vb terminal into the node
          if(a) A[idx(a)][N] -= sg_.I * v;
          if(b) A[idx(b)][N] += sg_.I * v;
          break;
        default: break;
        }
      } else {
        switch(e.type) {
        case abstract_builder::VCCS:
          // g (V(d) - V(c)) leaving node a, entering node b
                    if(a) { if(d) A[idx(a)][idx(d)] += sg_.VCCS * v;
                  if(c) A[idx(a)][idx(c)] -= sg_.VCCS * v; }
          if(b) { if(d) A[idx(b)][idx(d)] -= sg_.VCCS * v;
                  if(c) A[idx(b)][idx(c)] += sg_.VCCS * v; }
          break;
        case abstract_builder::VCVS:
          addBranch(b, a, k, 1.);
          addVoltage(k, b, a, 1.);
          addVoltage(k, d, c, -sg_.VCVS * v);
          ++k;
          break;
        case abstract_builder::CCCS:
          // control short c -> d carrying i_c (unknown k)
          addBranch(c, d, k, 1.);
          addVoltage(k, c, d, 1.);
          // output value * i_c leaving a, entering b
          addBranch(a, b, k, sg_.CCCS * v);
          ++k;
          break;
        case abstract_builder::CCVS:
          addBranch(c, d, k, 1.);
          addVoltage(k, c, d, 1.);
          addBranch(b, a, k + 1, 1.);
          addVoltage(k + 1, b, a, 1.);
          A[k + 1][k] -= sg_.CCVS * v;
          k += 2;
          break;
        case abstract_builder::AO:
          if(real(e)) {
            // V(b) - V(a) = A(s) (V(d) - V(c)), A = A0 / (1 + s A0 / (2 pi GBW))
            const double gbw = prop(e.props, "gbw");
            lc gain = (long double)v;
            if(gbw > 0 && v > 0)
              gain = (long double)v / (1.L + s * (long double)v / (2.L * kPi * gbw));
            addBranch(b, a, k, 1.);
            addVoltage(k, b, a, 1.);
            if(d) A[k][idx(d)] -= (long double)sg_.VCVS * gain;
            if(c) A[k][idx(c)] += (long double)sg_.VCVS * gain;
            ++k;
            break;
          }
          // nullor: V(c) = V(d); free output current from a to b
          addBranch(b, a, k, 1.);
          addVoltage(k, c, d, 1.);
          ++k;
          break;
        case abstract_builder::n:
          // ideal transformer, ratio v: port B (a, b), port A (c, d)
          //   V(b) - V(a) = nV * v (V(d) - V(c));  i_A = nI * v * i_B
          addBranch(b, a, k, 1.);              // i_B
          addBranch(d, c, k + 1, 1.);          // i_A
          addVoltage(k, b, a, 1.);
          addVoltage(k, d, c, -sg_.nV * v);
          A[k + 1][k + 1] += 1.;
          A[k + 1][k] -= sg_.nI * v;
          k += 2;
          break;
        case abstract_builder::K:
        {
          // coupled coils Lp (a, b) and Ls (c, d), mutual inductance v
          const double lp = prop(e.props, "lp:value");
          const double ls = prop(e.props, "ls:value");
          addBranch(a, b, k, 1.);              // i_p from a to b
          addBranch(c, d, k + 1, 1.);          // i_s from c to d
          addVoltage(k, a, b, 1.);
          A[k][k] -= s * (long double)lp;
          A[k][k + 1] -= (long double)sg_.K * s * (long double)v;
          addVoltage(k + 1, c, d, 1.);
          A[k + 1][k + 1] -= s * (long double)ls;
          A[k + 1][k] -= (long double)sg_.K * s * (long double)v;
          k += 2;
          break;
        }
        default: break;
        }
      }
    }

    // Gaussian elimination with partial pivoting
    // equilibrate rows (element values span many decades), then a
    // pivot that is tiny against its row means a singular system
    for(int i = 0; i < N; ++i) {
      long double m = 0.;
      for(int j = 0; j < N; ++j)
        m = std::max(m, std::abs(A[i][j]));
      if(m == 0.)
        return false;
      for(int j = 0; j <= N; ++j)
        A[i][j] /= m;
    }
    const long double scale = 1.;
    for(int col = 0; col < N; ++col) {
      int piv = col;
      for(int r = col + 1; r < N; ++r)
        if(std::abs(A[r][col]) > std::abs(A[piv][col]))
          piv = r;
      if(std::abs(A[piv][col]) < 1e-14L * scale)
        return false;
      std::swap(A[piv], A[col]);
      for(int r = 0; r < N; ++r) {
        if(r == col) continue;
        const lc factor = A[r][col] / A[col][col];
        if(factor == lc(0.)) continue;
        for(int j = col; j <= N; ++j)
          A[r][j] -= factor * A[col][j];
      }
    }
    if(!net_.out) return false;
    const lc r = A[idx(net_.out)][N] / A[idx(net_.out)][idx(net_.out)];
    *result = cplx(double(r.real()), double(r.imag()));
    return std::isfinite(result->real()) && std::isfinite(result->imag());
  }

private:
  const Netlist& net_;
  Signs sg_;
  unsigned int nodes_;
  std::map<unsigned int, int> ids_;
};

// ---------------------------------------------------------------- engine

struct EngineResult
{
  bool ok = false;
  bool singular = false;
  sapecng::metacircuit::expression num, den;
  std::string text;
};

EngineResult solveEngine(const Netlist& net, int output = -1,
  bool numeric = false)
{
  EngineResult r;
  sapecng::circuit circ;
  sapecng::circuit_builder builder(circ,
    output < 0 ? net.engineOutput() : unsigned(output));
  net.replay(builder);
  sapecng::metacircuit meta;
  if(numeric)
    meta.numeric(circ);   // no trees listed: fast on any circuit
  else
    meta(circ);
  r.singular = meta.singular();
  r.num = meta.digit().first;
  r.den = meta.digit().second;
  r.text = sapecng::metacircuit::as_string(r.num) + " / "
    + sapecng::metacircuit::as_string(r.den);
  r.ok = true;
  return r;
}

cplx evalPoly(const sapecng::metacircuit::expression& e, cplx s, bool* numeric)
{
  cplx sum = 0.;
  for(const auto& deg : e)
    for(const auto& term : deg.second) {
      if(!term.second.empty())
        *numeric = false;    // a symbol left in the "digit" result
      sum += term.first * std::pow(s, deg.first);
    }
  return sum;
}

bool evalEngine(const EngineResult& r, double f, cplx* h)
{
  const cplx s(0., 2. * kPi * f);
  bool numeric = true;
  const cplx n = evalPoly(r.num, s, &numeric);
  const cplx d = evalPoly(r.den, s, &numeric);
  if(!numeric || d == cplx(0.))
    return false;
  *h = n / d;
  return true;
}

// ---------------------------------------------------------------- compare

enum Verdict { Match, Mismatch, Singular, EngineZero, FalseSingular,
               BothSingular };

Verdict compare(const Netlist& net, const Signs& sg, std::string* detail,
  bool numeric = false)
{
  EngineResult er = solveEngine(net, -1, numeric);
  Mna mna(net, sg);
  static const double freqs[] = { 3.7, 159.2, 12345., 7.7e5 };
  if(er.singular) {
    cplx hm;
    for(double f : freqs)
      if(mna.solve(f, &hm)) {
        *detail = "engine says singular, MNA solves it: " + er.text;
        return FalseSingular;
      }
    return BothSingular;
  }
  bool anySingular = false;
  for(double f : freqs) {
    cplx hm, he;
    const bool mOk = mna.solve(f, &hm);
    if(!mOk) { anySingular = true; continue; }
    if(!evalEngine(er, f, &he)) {
      std::ostringstream s;
      s << "engine gave no usable result (" << er.text << ")";
      *detail = s.str();
      return EngineZero;
    }
    const double err = std::abs(he - hm) / std::max(std::abs(hm), 1e-12);
    // an exact zero comes out of MNA as rounding noise far below the
    // source level (checked with an exact solve)
    // the reference is only a few digits better than the engine where
    // long double has more precision than double (not with MSVC)
    if(err > 1e-6 && std::abs(he - hm) > 1e-9) {
      std::ostringstream s;
      s.precision(10);
      s << "f = " << f << ": engine " << he << ", MNA " << hm
        << " (rel. error " << err << ")\n  engine: " << er.text;
      *detail = s.str();
      return Mismatch;
    }
  }
  return anySingular ? Singular : Match;
}

// ---------------------------------------------------------------- random

struct Gen
{
  std::mt19937 rng;
  explicit Gen(unsigned seed): rng(seed) { }
  int uni(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
  double logu(double lo, double hi)
  {
    return std::exp(std::uniform_real_distribution<double>(
      std::log(lo), std::log(hi))(rng));
  }
};

Netlist randomCircuit(Gen& g, bool active, bool extended = false)
{
  Netlist net;
  const int nodes = g.uni(2, 6);
  int count[16] = { 0 };
  auto name = [&](const char* p, int t) {
      return std::string(p) + std::to_string(++count[t]);
    };
  auto passive = [&](unsigned a, unsigned b) {
      Element e;
      e.n[0] = a; e.n[1] = b;
      e.symbolic = g.uni(0, 1);
      switch(g.uni(0, 3)) {
      case 0: e.type = abstract_builder::R; e.value = g.logu(10, 1e5); e.name = name("R", 0); break;
      case 1: e.type = abstract_builder::G; e.value = g.logu(1e-5, 0.1); e.name = name("G", 1); break;
      case 2: e.type = abstract_builder::L; e.value = g.logu(1e-6, 1e-1); e.name = name("L", 2); break;
      default: e.type = abstract_builder::C; e.value = g.logu(1e-12, 1e-5); e.name = name("C", 3); break;
      }
      if(extended && e.type != abstract_builder::G && g.uni(0, 2) == 0) {
        // a real part with some parasitics
        e.props["ideal"] = "0";
        auto maybe = [&](const char* key, double lo, double hi) {
            if(g.uni(0, 2))
              e.props[key] = std::to_string(g.logu(lo, hi));
          };
        if(e.type == abstract_builder::R) { maybe("ls", 1e-9, 1e-6); maybe("cp", 1e-13, 1e-10); }
        if(e.type == abstract_builder::L) { maybe("rs", 0.01, 10); maybe("cp", 1e-13, 1e-10); }
        if(e.type == abstract_builder::C) { maybe("esr", 0.001, 1); maybe("esl", 1e-10, 1e-7); maybe("rp", 1e6, 1e9); }
      }
      net.elements.push_back(e);
    };

  // the source: V1 from ground (-) to node 1 (+)
  Element v;
  v.type = abstract_builder::V; v.name = "V1"; v.value = g.logu(0.1, 10);
  v.symbolic = true; v.n[0] = 0; v.n[1] = 1;
  net.elements.push_back(v);

  // a random spanning tree over 0..nodes keeps everything connected
  for(int i = 1; i <= nodes; ++i) {
    const unsigned other = unsigned(g.uni(0, i - 1));
    passive(other, unsigned(i));
  }
  // extra passives
  const int extra = g.uni(0, nodes + 1);
  for(int i = 0; i < extra; ++i) {
    unsigned a = unsigned(g.uni(0, nodes)), b = unsigned(g.uni(0, nodes));
    if(a != b) passive(a, b);
  }
  // controlled sources
  if(active) {
    const int nc = g.uni(1, 2);
    for(int i = 0; i < nc; ++i) {
      Element e;
      e.quad = true;
      e.type = g.uni(0, extended ? 6 : 3);    // VCCS ... CCVS, AO, n, K
      static const char* names[] = { "Gm", "E", "F", "H", "A", "T", "K" };
      e.name = name(names[e.type], 4 + e.type);
      e.symbolic = g.uni(0, 1);
      e.value = (e.type == 0) ? g.logu(1e-4, 1e-1)
              : (e.type == 3) ? g.logu(10, 1e4) : g.logu(0.1, 10);
      if(e.type == abstract_builder::AO) {
        if(g.uni(0, 1)) {
          e.props["ideal"] = "0";
          e.value = g.logu(1e3, 1e6);
          if(g.uni(0, 2))
            e.props["gbw"] = std::to_string(g.logu(1e5, 1e8));
        } else {
          e.value = 1;
        }
      }
      if(e.type == abstract_builder::K) {
        // real coils: well below 1 H
        const double lp = g.logu(1e-6, 0.5), ls = g.logu(1e-6, 0.5);
        e.value = g.uni(1, 9) / 10. * std::sqrt(lp * ls);
        e.props["lp:name"] = "Lp" + e.name; e.props["lp:value"] = std::to_string(lp);
        e.props["ls:name"] = "Ls" + e.name; e.props["ls:value"] = std::to_string(ls);
      }
      for(int j = 0; j < 4; ++j)
        e.n[j] = unsigned(g.uni(0, nodes));
      if(e.n[0] == e.n[1] || e.n[2] == e.n[3])
        continue;
      net.elements.push_back(e);
    }
  }
  // now and then a transformer with three windings: ideal, perfectly
  // coupled or with leakage
  if(extended && g.uni(0, 7) == 0) {
    Element t;
    t.transformer = true;
    t.name = "T1";
    t.symbolic = g.uni(0, 1);
    // ideal (half), perfectly coupled (a third), leaky (a sixth: three more
    // inductors make the circuit slow to solve symbolically)
    const int roll = g.uni(0, 5);
    const int kind = roll < 3 ? 0 : roll < 5 ? 1 : 2;
    t.value = kind == 0 ? 0. : g.logu(1e-5, 1e-1);
    t.props["k"] = kind == 2 ? std::to_string(g.uni(50, 99) / 100.) : "1";
    bool ok = true;
    for(int i = 0; i < 3; ++i) {
      const unsigned a = unsigned(g.uni(0, nodes)), b = unsigned(g.uni(0, nodes));
      if(a == b) ok = false;
      t.windings.push_back(std::make_pair(a, b));
      t.ratios.push_back(i == 0 ? 1. : g.logu(0.2, 5.));
    }
    if(ok)
      net.elements.push_back(t);
  }
  // now and then a current source too
  if(extended && g.uni(0, 3) == 0) {
    Element i;
    i.type = abstract_builder::I; i.name = "I1"; i.value = g.logu(1e-4, 1e-2);
    i.symbolic = true;
    i.n[0] = unsigned(g.uni(0, nodes)); i.n[1] = unsigned(g.uni(0, nodes));
    if(i.n[0] != i.n[1])
      net.elements.push_back(i);
  }
  net.out = unsigned(g.uni(2 <= nodes ? 2 : 1, nodes));
  if(extended && g.uni(0, 2) == 0) {
    // more outputs: Out markers on other nodes and now and then an ideal
    // voltmeter or ammeter; the engine is asked for net.out
    const int more = g.uni(1, 2);
    net.outs.push_back(net.out);
    for(int i = 0; i < more; ++i)
      net.outs.push_back(unsigned(g.uni(1, nodes)));
    std::swap(net.outs[0], net.outs[g.uni(0, int(net.outs.size()) - 1)]);
    for(unsigned int i = 0; i < net.outs.size(); ++i)
      if(net.outs[i] == net.out) { net.outIndex = i; break; }
    if(g.uni(0, 1)) {
      Element m;
      m.type = g.uni(0, 1) ? abstract_builder::VM : abstract_builder::AM;
      m.n[0] = unsigned(g.uni(0, nodes));
      m.n[1] = unsigned(g.uni(0, nodes));
      if(m.n[0] != m.n[1]) {
        m.name = m.type == abstract_builder::VM ? "VM1" : "AM1";
        m.value = 1.;
        net.elements.push_back(m);
      }
    }
  }
  if(std::getenv("SPARSE_NODES")) {
    // netlists written by hand: any node numbers (1, 7, 42 ...)
    std::vector<unsigned> map(nodes + 1, 0);
    std::vector<unsigned> used;
    for(int i = 1; i <= nodes; ++i) {
      unsigned id;
      do { id = unsigned(g.uni(1, 60)); }
      while(std::find(used.begin(), used.end(), id) != used.end());
      used.push_back(id);
      map[i] = id;
    }
    for(Element& e : net.elements)
      for(int j = 0; j < 4; ++j)
        e.n[j] = map[e.n[j]];
    net.out = map[net.out];
    for(unsigned int& o : net.outs)
      o = map[o];
  }
  return net;
}

// a passive ladder (bench): R in series, C to ground, n sections
Netlist ladder(int sections)
{
  Netlist net;
  Element v;
  v.type = abstract_builder::V; v.name = "V1"; v.value = 1; v.symbolic = true;
  v.n[0] = 0; v.n[1] = 1;
  net.elements.push_back(v);
  for(int i = 1; i <= sections; ++i) {
    Element r;
    r.type = abstract_builder::R; r.name = "R" + std::to_string(i);
    r.value = 1000; r.symbolic = true; r.n[0] = unsigned(i); r.n[1] = unsigned(i + 1);
    net.elements.push_back(r);
    Element c;
    c.type = abstract_builder::C; c.name = "C" + std::to_string(i);
    c.value = 1e-7; c.symbolic = true; c.n[0] = unsigned(i + 1); c.n[1] = 0;
    net.elements.push_back(c);
  }
  net.out = unsigned(sections + 1);
  return net;
}

// a tiny circuit to calibrate the sign of one element type
Signs calibrate()
{
  Signs sg;
  auto base = [](Netlist& net) {
      Element v; v.type = abstract_builder::V; v.name = "V1"; v.value = 1;
      v.n[0] = 0; v.n[1] = 1; net.elements.push_back(v);
      Element r; r.type = abstract_builder::R; r.name = "R1"; r.value = 1000;
      r.n[0] = 1; r.n[1] = 2; net.elements.push_back(r);
      Element r2; r2.type = abstract_builder::R; r2.name = "R2"; r2.value = 2000;
      r2.n[0] = 2; r2.n[1] = 0; net.elements.push_back(r2);
      Element rl; rl.type = abstract_builder::R; rl.name = "RL"; rl.value = 500;
      rl.n[0] = 3; rl.n[1] = 0; net.elements.push_back(rl);
      net.out = 3;
    };
  struct Case { int type; double value; double* sign; const char* name; };
  Case cases[] = {
    { abstract_builder::VCCS, 0.01, &sg.VCCS, "VCCS" },
    { abstract_builder::VCVS, 3., &sg.VCVS, "VCVS" },
    { abstract_builder::CCCS, 3., &sg.CCCS, "CCCS" },
    { abstract_builder::CCVS, 300., &sg.CCVS, "CCVS" },
  };
  for(Case& c : cases) {
    Netlist net;
    base(net);
    Element e; e.quad = true; e.type = c.type; e.name = "X1"; e.value = c.value;
    // output into node 3 from ground; control across R2 (or through it)
    e.n[0] = 0; e.n[1] = 3;
    if(c.type == abstract_builder::CCCS || c.type == abstract_builder::CCVS) {
      // the control short is put in series with R2: 2 -> 4, R2 then 4 -> 0
      net.elements[2].n[0] = 4;
      e.n[2] = 2; e.n[3] = 4;
    } else {
      e.n[2] = 0; e.n[3] = 2;
    }
    net.elements.push_back(e);
    std::string detail;
    for(double sign : { 1., -1. }) {
      *c.sign = sign;
      if(compare(net, sg, &detail) == Match)
        break;
      if(sign < 0)
        std::cout << "calibration: no sign of " << c.name << " matches: "
                  << detail << "\n";
    }
    std::cout << "convention " << c.name << ": sign " << *c.sign << "\n";
  }
  // ideal transformer (ratio 2) and coupled coils (integer henries: the
  // engine reads the coil values as integers, see the K tests)
  {
    bool found = false;
    for(double a : { 1., -1. }) {
      for(double b : { 1., -1. }) {
        sg.nV = a; sg.nI = b;
        Netlist net;
        base(net);
        Element e; e.quad = true; e.type = abstract_builder::n; e.name = "T1";
        e.value = 2; e.n[0] = 0; e.n[1] = 3; e.n[2] = 0; e.n[3] = 2;
        net.elements.push_back(e);
        std::string detail;
        if(compare(net, sg, &detail) == Match) { found = true; break; }
      }
      if(found) break;
    }
    std::cout << "convention n: " << (found ? "" : "NONE MATCHES, last ")
              << "signs " << sg.nV << " " << sg.nI << "\n";
  }
  {
    bool found = false;
    for(double sign : { 1., -1. }) {
      sg.K = sign;
      Netlist net;
      base(net);
      Element e; e.quad = true; e.type = abstract_builder::K; e.name = "K1";
      e.value = 1; e.n[0] = 2; e.n[1] = 0; e.n[2] = 3; e.n[3] = 0;
      e.props["lp:name"] = "Lp"; e.props["lp:value"] = "2";
      e.props["ls:name"] = "Ls"; e.props["ls:value"] = "3";
      net.elements.push_back(e);
      std::string detail;
      if(compare(net, sg, &detail) == Match) { found = true; break; }
      if(sign < 0) std::cout << "K: " << detail << "\n";
    }
    std::cout << "convention K: " << (found ? "" : "NONE MATCHES, last ")
              << "sign " << sg.K << "\n";
  }

  // current source: 1 mA into node 2 of the divider
  {
    Netlist net;
    base(net);
    net.elements.erase(net.elements.begin());     // no V1
    Element i; i.type = abstract_builder::I; i.name = "I1"; i.value = 1e-3;
    i.n[0] = 0; i.n[1] = 2; net.elements.push_back(i);
    net.out = 2;
    std::string detail;
    for(double sign : { 1., -1. }) {
      sg.I = sign;
      if(compare(net, sg, &detail) == Match) break;
      if(sign < 0)
        std::cout << "calibration: no sign of I matches: " << detail << "\n";
    }
    std::cout << "convention I: sign " << sg.I << "\n";
  }
  return sg;
}

// ---------------------------------------------------------------- SPICE

// The circuit through spice_builder and ngspice at frequency f: every
// output's value, in the order of the outputs. Empty when ngspice failed.
std::vector<cplx> runSpice(const Netlist& net, double f, std::string* text,
  double idealGain = 1e8)
{
  std::ostringstream cmd;
  cmd.precision(12);
  cmd << "set numdgt=12\nac lin 1 " << f << " " << f;
  std::ostringstream deck;
  sapecng::spice_builder builder(deck);
  builder.set_title("mna_check");
  builder.set_control(cmd.str());
  builder.set_ideal_gain(idealGain);
  net.replay(builder);
  *text = deck.str();

  const std::string cir = "mna_check_spice.cir", log = "mna_check_spice.log";
  { std::ofstream out(cir); out << *text; }
  std::remove(log.c_str());
  const std::string run = "ngspice -b " + cir + " > " + log + " 2>&1";
  std::vector<cplx> values;
  if(std::system(run.c_str()) != 0)
    return values;
  std::ifstream in(log);
  std::string line;
  while(std::getline(in, line)) {
    // "v(4) = 4.999998212179e-01,-5.00000000000e-01"
    const std::size_t eq = line.find(" = ");
    if(eq == std::string::npos || line.find('(') > eq)
      continue;
    std::istringstream num(line.substr(eq + 3));
    num.imbue(std::locale::classic());
    double re = 0., im = 0.;
    char comma = 0;
    if(num >> re >> comma >> im && comma == ',')
      values.push_back(cplx(re, im));
  }
  return values;
}

}  // namespace


int main(int argc, char** argv)
{
  const std::string mode = argc > 1 ? argv[1] : "fuzz";

  // the solver against the one it replaced (tests/reference_solver.h)
  if(mode == "numeric" || mode == "numeric-file"
      || mode == "reference" || mode == "reference-file") {
    const bool numeric = mode.compare(0, 7, "numeric") == 0;
    int bad = 0, same = 0, total = 0;
    long long trees = 0;
    auto one = [&](const sapecng::circuit& circ, const std::string& what) {
        bool text = false;
        long long n = 0;
        const std::string d = numeric ? reference::check_numeric(circ, &text)
                                      : reference::check(circ, &text, &n);
        ++total;
        trees += n;
        if(!d.empty()) {
          ++bad;
          if(bad <= 5)
            std::cout << what << ": " << d << "\n";
        } else if(text) {
          ++same;
        }
      };
    if(mode == "reference-file" || mode == "numeric-file") {
      for(int a = 2; a < argc; ++a) {
        const std::string file(argv[a]);
        std::ifstream in(file.c_str());
        sapecng::circuit circ;
        sapecng::circuit_builder builder(circ);
        if(file.size() > 4 && file.substr(file.size() - 4) == ".crc") {
          sapecng::crc_parser parser(in);
          parser.parse(builder);
        } else {
          sapecng::info_parser parser(in);
          parser.parse(builder);
        }
        one(circ, file);
      }
    } else {
      const int count = argc > 2 ? std::atoi(argv[2]) : 200;
      Gen g(argc > 3 ? unsigned(std::atoi(argv[3])) : 1u);
      const bool ext = argc > 4;
      for(int i = 0; i < count; ++i) {
        const Netlist net = randomCircuit(g, i % 2 == 1, ext);
        sapecng::circuit circ;
        sapecng::circuit_builder builder(circ, net.engineOutput());
        net.replay(builder);
        one(circ, "case " + std::to_string(i) + "\n" + net.describe());
      }
    }
    std::cout << total << " circuits (" << trees << " trees): "
      << total - bad << (numeric ? " the same as the tree solve's digit ("
                                  : " the same as the old solver (") << same
      << " printed identically), " << bad << " different\n";
    return bad ? 1 : 0;
  }

  if(mode == "bench") {
    const int maxSections = argc > 2 ? std::atoi(argv[2]) : 12;
    for(int n = 1; n <= maxSections; ++n) {
      const Netlist net = ladder(n);
      const auto t0 = std::chrono::steady_clock::now();
      EngineResult r = solveEngine(net);
      const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
      size_t terms = 0;
      for(const auto& d : r.den) terms += d.second.size();
      std::cout << "ladder " << n << " sections (" << 2 * n + 1
                << " elements): " << ms << " ms, " << terms
                << " denominator terms\n" << std::flush;
      if(ms > 20000) break;
    }
    return 0;
  }

  if(mode == "response") {
    // the plots' frequency response (sapecng::magnitude, phase) against MNA
    // over 1 Hz ... 10 MHz, 20 points per decade
    const Signs sgr = calibrate();
    const int count = argc > 2 ? std::atoi(argv[2]) : 300;
    Gen g(argc > 3 ? unsigned(std::atoi(argv[3])) : 1u);
    double worst = 0.; int compared = 0;
    for(int i = 0; i < count + 10; ++i) {
      const Netlist net = i < count ? randomCircuit(g, i % 2 == 1, true)
                                    : ladder(i - count + 3);
      EngineResult er = solveEngine(net);
      if(er.singular) continue;
      const std::map<std::string, double> none;
      auto mag = sapecng::magnitude(std::make_pair(1., 1e7), 20., true)(er.num, er.den, none);
      auto ph = sapecng::phase(std::make_pair(1., 1e7), 20., true)(er.num, er.den, none);
      Mna mna(net, sgr);
      for(size_t k = 0; k < mag.first.size(); ++k) {
        cplx hm;
        if(!mna.solve(mag.first[k], &hm)) continue;
        const cplx hf = std::polar(mag.second[k], ph.second[k]);
        const double err = std::abs(hf - hm) / std::max(std::abs(hm), 1e-9);
        ++compared;
        if(err > worst) {
          worst = err;
          if(err > 1e-6)
            std::cout << "case " << i << " f = " << mag.first[k] << ": plot " << hf
                      << " MNA " << hm << " rel. error " << err << "\n";
        }
      }
    }
    std::cout << compared << " points compared, worst relative error " << worst << "\n";
    return worst > 1e-6 ? 1 : 0;
  }

  if(mode == "roots") {
    // poles and zeros (rpoly) against their polynomials: every root must
    // be one (small residual) and none may be missing
    const int count = argc > 2 ? std::atoi(argv[2]) : 500;
    Gen g(argc > 3 ? unsigned(std::atoi(argv[3])) : 1u);
    int checked = 0, missing = 0, bad = 0;
    double worst = 0.;
    for(int i = 0; i < count + 8; ++i) {
      // the last ones: RC ladders of order 2 ... 9 (real, spread poles)
      const Netlist net = i < count ? randomCircuit(g, i % 2 == 1, true)
                                    : ladder(i - count + 2);
      EngineResult er = solveEngine(net);
      if(er.singular) continue;
      for(const auto* e : { &er.num, &er.den }) {
        std::map<int, double> p;
        for(const auto& deg : *e)
          for(const auto& t : deg.second)
            p[deg.first] += t.first;
        int degree = 0;
        for(const auto& c : p) if(c.second != 0.) degree = std::max(degree, c.first);
        const auto roots = sapecng::rpoly_adapter(p);
        ++checked;
        if(int(roots.size()) != degree) {
          ++missing;
          if(missing <= 3)
            std::cout << "case " << i << ": " << roots.size() << " roots for degree "
                      << degree << ": " << er.text << "\n";
        }
        for(const auto& r : roots) {
          const cplx z(r.first, r.second);
          cplx v = 0.; double mag = 0.;
          for(const auto& c : p) {
            v += c.second * std::pow(z, c.first);
            mag += std::abs(c.second) * std::pow(std::abs(z), c.first);
          }
          const double res = mag > 0 ? std::abs(v) / mag : 0.;
          worst = std::max(worst, res);
          if(res > 1e-6) {
            ++bad;
            if(bad <= 3)
              std::cout << "case " << i << ": root " << z << " residual " << res
                        << " in " << er.text << "\n";
          }
        }
      }
    }
    std::cout << checked << " polynomials: " << missing << " with roots missing, "
              << bad << " inaccurate roots; worst relative residual " << worst << "\n";
    return (missing || bad) ? 1 : 0;
  }

  const Signs sg = calibrate();

  if(mode == "spice") {
    // the SPICE export, checked by ngspice on random circuits: mna_check
    // spice <count> <seed>; skipped without ngspice
    if(std::system("ngspice -v > /dev/null 2>&1") != 0) {
      std::cout << "ngspice not found: SPICE export not checked\n";
      return 0;
    }
    const int count = argc > 2 ? std::atoi(argv[2]) : 100;
    Gen g(argc > 3 ? unsigned(std::atoi(argv[3])) : 1u);
    int match = 0, mismatch = 0, skipped = 0, failed = 0;
    for(int i = 0; i < count; ++i) {
      const Netlist net = randomCircuit(g, i % 2 == 1, true);
      const EngineResult er = solveEngine(net);
      if(er.singular) { ++skipped; continue; }
      bool ok = true, ran = true, beyond = false;
      bool idealOpAmp = false;
      for(const Element& e : net.elements)
        idealOpAmp = idealOpAmp || (e.quad && e.type == abstract_builder::AO
          && !real(e));
      std::string detail, deck;
      // every output: the Out markers and the meters (their signs)
      unsigned int outputs = 0;
      for(const Element& e : net.elements)
        if(!e.quad && (e.type == abstract_builder::VM || e.type == abstract_builder::AM))
          ++outputs;
      outputs += net.outs.empty() ? (net.out ? 1 : 0) : unsigned(net.outs.size());
      // the engine once per output (the frequencies only evaluate it)
      std::vector<EngineResult> engine;
      for(unsigned int k = 0; k < outputs; ++k)
        engine.push_back(solveEngine(net, int(k)));
      for(double f : { 159.2, 12345. }) {
        const std::vector<cplx> out = runSpice(net, f, &deck);
        if(out.size() != outputs) { ran = false; break; }
        // with an ideal op-amp: the same at a tenth of the gain; where the
        // two differ the circuit is limited by the gain (small loop gain),
        // not a matter of the export
        std::vector<cplx> lower;
        if(idealOpAmp) {
          std::string unused;
          lower = runSpice(net, f, &unused, 1e7);
          if(lower.size() != outputs) { ran = false; break; }
        }
        for(unsigned int k = 0; k < outputs; ++k) {
          const EngineResult& ek = engine[k];
          if(ek.singular)
            continue;
          cplx he;
          if(!evalEngine(ek, f, &he)) { ran = false; break; }
          // relative; an absolute floor where the engine's value is (near)
          // zero: an ideal op-amp is a gain of 1e8 in SPICE, good to about
          // 1e-8 of the signal, and no finite gain follows a result that
          // is itself huge
          if(idealOpAmp && (std::abs(he) > 1e4
              || std::abs(lower[k] - out[k]) > 1e-6 * std::max(std::abs(out[k]), 1e-3)))
          { beyond = true; break; }
          const double err = std::abs(out[k] - he) / std::max(std::abs(he), 1e-3);
          if(!(err < 1e-4)) {
            std::ostringstream s;
            s << "f = " << f << ": engine " << he << ", ngspice " << out[k]
              << " (output " << k << " of " << out.size() << ")";
            detail = s.str();
            ok = false;
            break;
          }
        }
        if(!ok || !ran || beyond)
          break;
      }
      if(beyond) {
        ++skipped;
      } else if(!ran) {
        ++failed;
        if(failed <= 3)
          std::cout << "case " << i << ": ngspice gave no result\n"
                    << net.describe() << deck << "\n";
      } else if(ok) {
        ++match;
      } else {
        ++mismatch;
        if(mismatch <= 5)
          std::cout << "case " << i << ": " << detail << "\n"
                    << net.describe() << deck << "\n";
      }
    }
    std::cout << count << " circuits: " << match << " match ngspice, "
              << mismatch << " mismatch, " << failed << " without ngspice "
              << "result, " << skipped << " skipped (singular, or beyond a "
              << "finite op-amp gain)\n";
    return mismatch ? 1 : 0;
  }

  // the numeric result of a circuit, its poles and zeros: every coefficient
  // a finite double, as many roots as the degree, each one a root (the
  // polynomial there small against its terms). Big circuits: coefficients
  // spanning 300 decades and more overflowed, and the root finder hung
  if(mode == "roots-file" && argc > 2) {
    std::ifstream in(argv[2]);
    sapecng::circuit circ;
    sapecng::circuit_builder builder(circ);
    sapecng::info_parser parser(in);
    parser.parse(builder);
    sapecng::metacircuit meta;
    meta.numeric(circ);
    const auto digit = meta.digit();
    bool ok = true;
    int pi = 0;
    for(const sapecng::metacircuit::expression* e : { &digit.first, &digit.second }) {
      const char* what = pi++ ? "denominator" : "numerator";
      std::map<int, double> c;
      for(const auto& d : *e)
        for(const auto& t : d.second)
          c[d.first] += t.first;
      for(const auto& kv : c)
        if(!std::isfinite(kv.second)) {
          std::cout << what << ": coefficient of s^" << kv.first << " not finite\n";
          ok = false;
        }
      int low = c.empty() ? 0 : c.begin()->first;
      for(const auto& kv : c)
        if(kv.second != 0.) { low = kv.first; break; }
      const int degree = c.empty() ? 0 : c.rbegin()->first;
      const auto roots = sapecng::rpoly_adapter(c);
      std::cout << what << ": degree " << degree << ", " << roots.size() << " roots\n";
      if(int(roots.size()) != degree)
        ok = false;
      // |P(r)| against the sum of |terms|, both scaled by the largest
      double worst = 0.;
      for(const auto& r : roots) {
        const std::complex<long double> z((long double)r.first, (long double)r.second);
        if(std::abs(z) == 0.L)
          continue;   // a root at the origin (a power of s)
        const long double lz = std::log(std::abs(z)), az = std::arg(z);
        long double top = -HUGE_VALL;
        for(const auto& kv : c)
          if(kv.second != 0.)
            top = std::max(top, std::log(std::fabs((long double)kv.second)) + kv.first * lz);
        std::complex<long double> v = 0;
        long double sum = 0;
        for(const auto& kv : c) {
          if(kv.second == 0.)
            continue;
          const long double m = std::exp(std::log(std::fabs((long double)kv.second)) + kv.first * lz - top);
          v += std::polar(kv.second < 0 ? -m : m, kv.first * az);
          sum += m;
        }
        worst = std::max(worst, double(std::abs(v) / sum));
      }
      (void)low;
      std::cout << "  largest residual " << worst << "\n";
      if(!(worst < 1e-6))
        ok = false;
    }
    std::cout << (ok ? "ok\n" : "FAILED\n");
    return ok ? 0 : 1;
  }

  if((mode == "file" || mode == "file-numeric") && argc > 2) {
    std::ifstream in(argv[2]);
    const std::string file(argv[2]);
    std::unique_ptr<sapecng::abstract_parser> parser;
    if(file.size() > 4 && file.substr(file.size() - 4) == ".crc")
      parser.reset(new sapecng::crc_parser(in));
    else
      parser.reset(new sapecng::info_parser(in));
    Recorder rec;
    try {
      parser->parse(rec);
    } catch(...) {
      // a malformed file is rejected (the program reports it on opening)
      std::cout << file << ": rejected by the parser\n";
      return 0;
    }
    std::string detail;
    const Verdict v = compare(rec.net, sg, &detail, mode == "file-numeric");
    static const char* names[] = { "match", "MISMATCH", "singular (MNA only)",
      "ENGINE WITHOUT RESULT", "WRONGLY SINGULAR", "singular (both)" };
    std::cout << file << ": " << names[v] << "\n";
    if(v == Mismatch || v == EngineZero || v == FalseSingular)
      std::cout << detail << "\n" << rec.net.describe();
    return (v == Mismatch || v == EngineZero || v == FalseSingular) ? 1 : 0;
  }

  if(mode == "case" && argc > 3) {
    // replay one fuzz case: mna_check case <seed> <index>
    Gen g(unsigned(std::atoi(argv[2])));
    const int index = std::atoi(argv[3]);
    Netlist net;
    const bool ext = argc > 4;
    for(int i = 0; i <= index; ++i)
      net = randomCircuit(g, i % 2 == 1, ext);
    std::cout << net.describe();
    EngineResult er = solveEngine(net);
    std::cout << "engine: " << er.text << "\n";
    Mna mna(net, sg);
    for(double f : { 3.7, 159.2, 12345., 7.7e5 }) {
      cplx hm, he;
      const bool m = mna.solve(f, &hm);
      const bool e = evalEngine(er, f, &he);
      std::cout << "f = " << f << ": engine " << (e ? he : cplx(NAN))
                << "  MNA " << (m ? hm : cplx(NAN)) << "\n";
    }
    return 0;
  }

  // the reference needs extended precision to judge the engine (element
  // values span many decades); where long double is double (MSVC) the
  // random circuits are skipped: the engine code is the same everywhere
  // and checked on the other platforms
  if(sizeof(long double) <= sizeof(double)) {
    std::cout << "no extended precision here: random circuits skipped\n";
    return 0;
  }

  const bool extended = (mode == "fuzz2");
  const int count = argc > 2 ? std::atoi(argv[2]) : 500;
  const unsigned seed = argc > 3 ? unsigned(std::atoi(argv[3])) : 1u;
  Gen g(seed);
  int match = 0, mismatch = 0, singular = 0, zero = 0, falseSing = 0, bothSing = 0;
  int transformerMatch = 0;
  for(int i = 0; i < count; ++i) {
    const Netlist net = randomCircuit(g, i % 2 == 1, extended);
    std::string detail;
    const auto started = std::chrono::steady_clock::now();
    struct Timer {
      std::chrono::steady_clock::time_point t; int i; const Netlist& n;
      ~Timer() {
        const double s = std::chrono::duration<double>(
          std::chrono::steady_clock::now() - t).count();
        if(std::getenv("SLOW") && s > std::atof(std::getenv("SLOW")))
          std::cout << "slow case " << i << ": " << s << " s\n" << n.describe();
      }
    } timer{ started, i, net };
    bool hasTransformer = false;
    for(const Element& e : net.elements)
      hasTransformer = hasTransformer || e.transformer;
    switch(compare(net, sg, &detail)) {
    case Match: ++match; if(hasTransformer) ++transformerMatch; break;
    case Singular:
      ++singular;
      if(std::getenv("SHOW_SINGULAR") && singular <= 3) {
        EngineResult er = solveEngine(net);
        std::cout << "case " << i << " singular for MNA, engine " << er.text
                  << "\n" << net.describe();
      }
      break;
    case EngineZero:
      ++zero;
      if(zero <= 3)
        std::cout << "case " << i << ": " << detail << "\n" << net.describe();
      break;
    case FalseSingular:
      ++falseSing;
      if(falseSing <= 3)
        std::cout << "case " << i << ": " << detail << "\n" << net.describe();
      break;
    case BothSingular: ++bothSing; break;
    case Mismatch:
      ++mismatch;
      if(mismatch <= 5)
        std::cout << "case " << i << ": " << detail << "\n" << net.describe();
      break;
    }
  }
  std::cout << count << " circuits: " << match << " match, " << mismatch
            << " mismatch, " << zero << " engine without result, "
            << singular << " singular for MNA only (engine finite), "
            << bothSing << " singular for both, " << falseSing
            << " wrongly singular\n";
  if(extended)
    std::cout << "  (" << transformerMatch << " of the matches have a "
              << "three-winding transformer)\n";
  return (mismatch || zero || falseSing) ? 1 : 0;
}
