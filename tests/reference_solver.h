// The solver as it was before the new tree search (QSapecNG up to the
// multi-core work): the trees from boost-sapecng/mrt.hpp, the signs by
// elimination, the terms added up in the order found. The tests check the
// solver against it, term for term.

#ifndef REFERENCE_SOLVER_H
#define REFERENCE_SOLVER_H

#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <iterator>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "boost-sapecng/mrt.hpp"
#include "model/circuit.h"
#include "model/metacircuit.h"
#include "model/treesearch.h"
#include "boost-sapecng/mrt_pool.hpp"
#include "boost-sapecng/mrt_progress.hpp"


namespace sapecng
{

// reaches metacircuit::simplify (a friend of metacircuit)
struct metacircuit_test_access
{
  static bool simplify(metacircuit& m,
      std::pair<metacircuit::expression, metacircuit::expression>& p)
    { return m.simplify(p); }
};

}


namespace reference
{

typedef sapecng::metacircuit::expression expression;
typedef sapecng::metacircuit::basic_expression basic_expression;
typedef sapecng::metacircuit::degree_expression degree_expression;

struct result
{
  std::pair<expression, expression> raw, digit, mixed;
  bool singular = false;
  std::vector< std::vector<bool> > trees;   // valid ones, in order
  long long invalid = 0;                    // sets with sign 0
};

inline int get_sign(size_t vertices, const sapecng::Graph& graph,
  const std::vector<sapecng::circuit::edge_descriptor>& order,
  const std::vector<bool>& inL)
{
  size_t row = vertices;
  size_t col = vertices - 1;
  std::vector<int> matrix(row * col, 0);
  for(std::vector<bool>::size_type i = 0, j = 0; i < inL.size(); ++i) {
    if(inL[i]) {
      matrix[col * source(order[i], graph) + j] = -1;
      matrix[col * target(order[i], graph) + j] = +1;
      ++j;
    }
  }
  int det = 1;
  for(size_t ofs = 0; ofs < col; ++ofs) {
    for(size_t iter = ofs; iter < row; ++iter) {
      if(matrix[iter * col + ofs] != 0) {
        if(iter != ofs) {
          for(size_t cnt = ofs; cnt < col; ++cnt) {
            int swap = matrix[iter * col + cnt];
            matrix[iter * col + cnt] = matrix[ofs * col + cnt];
            matrix[ofs * col + cnt] = swap;
          }
          det *= -1;
        }
        for(iter = ofs + 1; iter < row; ++iter) {
          if(matrix[iter * col + ofs] != 0) {
            int weight = -1 * matrix[ofs * col + ofs] / matrix[iter * col + ofs];
            for(size_t cnt = ofs; cnt < col; ++cnt)
              matrix[iter * col + cnt] += matrix[ofs * col + cnt] * weight;
          }
        }
        iter = row;
      }
    }
  }
  for(size_t iter = 0; iter < col; ++iter)
    det *= matrix[iter * col + iter];
  return det;
}

typedef std::map< std::pair< int, sapecng::symbol_list >,
  degree_expression::iterator > term_index;

inline void append(int deg, expression& e, const basic_expression& be,
    term_index& index)
{
  const term_index::key_type key(deg, be.second);
  term_index::iterator found = index.find(key);
  if(found != index.end()) {
    found->second->first += be.first;
  } else {
    degree_expression& terms = e[deg];
    terms.push_back(be);
    index.insert(std::make_pair(key, std::prev(terms.end())));
  }
}

inline result solve(const sapecng::circuit& circuit, bool skipInvalid)
{
  using namespace sapecng;
  result r;
  if(num_edges(circuit.vGraph()) != num_edges(circuit.iGraph())
      || num_vertices(circuit.vGraph()) != num_vertices(circuit.iGraph()))
    return r;

  const Graph& refG = circuit.iGraph();
  const std::vector<circuit::edge_descriptor>& refO = circuit.iGraph_order();
  std::vector<bool> forcedIn(num_edges(refG), false);
  auto FM = get(boost::edge_type, refG);
  for(std::vector<bool>::size_type i = 0; i < forcedIn.size(); ++i)
    if(FM[ refO[i] ] == sapecng::F)
      forcedIn[i] = true;

  typedef std::vector<bool> Seq;
  typedef std::vector< std::vector<bool> > Coll;
  Coll coll;
  boost::mrt_two_graphs_common_spanning_trees
      < sapecng::Graph, std::vector< sapecng::circuit::edge_descriptor >,
        boost::tree_collector< Coll, Seq >, std::vector< bool > >
    (circuit.iGraph(), circuit.iGraph_order(), circuit.vGraph(),
      circuit.vGraph_order(), boost::tree_collector< Coll, Seq >(coll),
      forcedIn);

  term_index rawNum, rawDen, digitNum, digitDen, mixedNum, mixedDen;
  for(Coll::size_type i = 0; i < coll.size(); ++i) {
    int degree = 0;
    basic_expression raw_rep(1., sapecng::symbol_list());
    basic_expression digit_rep(1., sapecng::symbol_list());
    basic_expression mixed_rep(1., sapecng::symbol_list());
    Seq inL = coll[i];

    int sign = 1;
    sign *= get_sign(num_vertices(circuit.iGraph()), circuit.iGraph(),
      circuit.iGraph_order(), inL);
    sign *= get_sign(num_vertices(circuit.vGraph()), circuit.vGraph(),
      circuit.vGraph_order(), inL);
    if(sign != 0) {
      r.trees.push_back(inL);
    } else {
      ++r.invalid;
      if(skipInvalid)
        continue;
    }

    raw_rep.first *= sign;
    digit_rep.first *= sign;
    mixed_rep.first *= sign;

    bool numerator_part = false;
    bool denominator_part = false;
    for(Seq::size_type j = 0; j < inL.size(); ++j) {
      if(inL[j] && (FM[ refO[j] ] == sapecng::GREF))
        numerator_part = true;
      if(inL[j] && (FM[ refO[j] ] == sapecng::YREF))
        denominator_part = true;
      if((inL[j] && FM[ refO[j] ] == sapecng::Y)
          || (!inL[j] && FM[ refO[j] ] == sapecng::Z))
      {
        if(get(boost::edge_symbolic, refG)[ refO[j] ]) {
          if(get(boost::edge_name, refG)[ refO[j] ].size())
            mixed_rep.second.push_back(get(boost::edge_name, refG)[ refO[j] ]);
        } else {
          mixed_rep.first *= get(boost::edge_weight, refG)[ refO[j] ];
        }
        digit_rep.first *= get(boost::edge_weight, refG)[ refO[j] ];
        if(get(boost::edge_name, refG)[ refO[j] ].size())
          raw_rep.second.push_back(get(boost::edge_name, refG)[ refO[j] ]);
        else
          raw_rep.first *= get(boost::edge_weight, refG)[ refO[j] ];
        degree += get(boost::edge_degree, refG)[ refO[j] ];
      }
    }
    raw_rep.second.sort();
    digit_rep.second.sort();
    mixed_rep.second.sort();
    if(numerator_part) {
      append(degree, r.raw.first, raw_rep, rawNum);
      append(degree, r.digit.first, digit_rep, digitNum);
      append(degree, r.mixed.first, mixed_rep, mixedNum);
    } else if(denominator_part) {
      append(degree, r.raw.second, raw_rep, rawDen);
      append(degree, r.digit.second, digit_rep, digitDen);
      append(degree, r.mixed.second, mixed_rep, mixedDen);
    }
  }

  sapecng::metacircuit m;
  const bool rawOk = sapecng::metacircuit_test_access::simplify(m, r.raw);
  const bool digitOk = sapecng::metacircuit_test_access::simplify(m, r.digit);
  sapecng::metacircuit_test_access::simplify(m, r.mixed);
  r.singular = !rawOk || !digitOk;
  return r;
}


// the same terms in the same order, the coefficients equal to `tolerance`
// (the solver sums them exactly, the reference in the order found); a
// description of the first difference, or "" if none
inline std::string compare(const expression& a, const expression& b,
  double tolerance)
{
  if(a.size() != b.size())
    return "different powers of s";
  for(auto ia = a.begin(), ib = b.begin(); ia != a.end(); ++ia, ++ib) {
    if(ia->first != ib->first)
      return "different powers of s";
    if(ia->second.size() != ib->second.size())
      return "different number of terms at s^" + std::to_string(ia->first);
    for(auto ta = ia->second.begin(), tb = ib->second.begin();
        ta != ia->second.end(); ++ta, ++tb)
    {
      if(ta->second != tb->second)
        return "different terms (or order) at s^" + std::to_string(ia->first);
      const double x = ta->first, y = tb->first;
      const double scale = std::max(std::fabs(x), std::fabs(y));
      if(std::fabs(x - y) > tolerance * scale)
        return "coefficient " + std::to_string(x) + " vs "
          + std::to_string(y) + " at s^" + std::to_string(ia->first);
    }
  }
  return "";
}



// the trees of the new search, with where each lies in it
struct tree_recorder
{
  std::vector< std::pair<sapecng::tree_position, std::vector<bool> > > trees;
  tree_recorder part() const { return tree_recorder(); }
  void merge(tree_recorder&& o)
    { for(auto& t : o.trees) trees.push_back(std::move(t)); }
  void tree(const std::vector<bool>& inL, const sapecng::tree_position& at)
    { trees.emplace_back(at, inL); }
};

inline std::vector< std::vector<bool> > new_trees(
  const sapecng::circuit& circuit, unsigned threads)
{
  using namespace sapecng;
  const Graph& iG = circuit.iGraph();
  const Graph& vG = circuit.vGraph();
  tree_graphs g;
  g.vertices = int(num_vertices(iG));
  g.edges = int(num_edges(iG));
  for(int j = 0; j < g.edges; ++j) {
    g.iu.push_back(int(source(circuit.iGraph_order()[j], iG)));
    g.iv.push_back(int(target(circuit.iGraph_order()[j], iG)));
    g.vu.push_back(int(source(circuit.vGraph_order()[j], vG)));
    g.vv.push_back(int(target(circuit.vGraph_order()[j], vG)));
  }
  std::vector<bool> forced(g.edges, false);
  auto FM = get(boost::edge_type, iG);
  for(int j = 0; j < g.edges; ++j)
    forced[j] = FM[ circuit.iGraph_order()[j] ] == sapecng::F;

  tree_recorder rec;
  tree_search<tree_recorder> search(g, threads ? 1 : 0);
  if(threads == 0) {
    search.run(forced, rec, nullptr);
  } else {
    boost::mrt_pool pool{ threads };
    std::atomic<bool> cancel(false);
    std::atomic<int> permille(0);
    std::atomic<long long> total(0);
    boost::mrt_progress progress;
    progress.cancel = &cancel;
    progress.permille = &permille;
    progress.pool = &pool;
    progress.total = &total;
    search.run(forced, rec, &progress);
  }
  std::sort(rec.trees.begin(), rec.trees.end(),
    [](const std::pair<tree_position, std::vector<bool> >& a,
       const std::pair<tree_position, std::vector<bool> >& b) {
      return tree_before(a.first, b.first);
    });
  std::vector< std::vector<bool> > out;
  for(auto& t : rec.trees)
    out.push_back(std::move(t.second));
  return out;
}


// the solver against the reference on one circuit: the same valid trees
// in the same order (on 0 and 3 threads, these handing on every part they
// can), the same terms in the same order
// with the same coefficients (to 1e-9), the same singular verdict. Returns
// "" or what differs; *sameText: the printed results are identical
inline std::string check(const sapecng::circuit& circuit, bool* sameText,
  long long* trees = nullptr, bool skipInvalid = true)
{
  const result ref = solve(circuit, skipInvalid);
  if(trees)
    *trees = (long long)ref.trees.size();
  if(new_trees(circuit, 0) != ref.trees)
    return "different trees (one thread)";
  if(new_trees(circuit, 3) != ref.trees)
    return "different trees (three threads)";

  sapecng::metacircuit meta;
  meta(circuit);
  if(meta.singular() != ref.singular)
    return "different singular verdict";
  const char* what[6] = { "raw numerator", "raw denominator",
    "digit numerator", "digit denominator", "mixed numerator",
    "mixed denominator" };
  const expression* theirs[6] = { &ref.raw.first, &ref.raw.second,
    &ref.digit.first, &ref.digit.second, &ref.mixed.first,
    &ref.mixed.second };
  // raw(), digit(), mixed() return copies: keep them alive
  const std::pair<expression, expression> raw = meta.raw(),
    digit = meta.digit(), mixed = meta.mixed();
  const expression* mine[6] = { &raw.first, &raw.second, &digit.first,
    &digit.second, &mixed.first, &mixed.second };
  bool same = true;
  for(int k = 0; k < 6; ++k) {
    const std::string d = compare(*mine[k], *theirs[k], 1e-9);
    if(!d.empty())
      return std::string(what[k]) + ": " + d;
    same = same && sapecng::metacircuit::as_string(*mine[k])
      == sapecng::metacircuit::as_string(*theirs[k]);
  }
  if(sameText)
    *sameText = same;
  return "";
}



// the numeric solve (metacircuit::numeric) against the tree solve's digit
// result: the same singular verdict and the same transfer function, to
// 1e-9 at a few frequencies. *sameText: printed identically
inline std::string check_numeric(const sapecng::circuit& circuit,
  bool* sameText)
{
  sapecng::metacircuit full, fast;
  full(circuit);
  fast.numeric(circuit);
  if(full.singular() != fast.singular())
    return "different singular verdict";
  if(sameText)
    *sameText = sapecng::metacircuit::as_string(full.digit().first)
        == sapecng::metacircuit::as_string(fast.digit().first)
      && sapecng::metacircuit::as_string(full.digit().second)
        == sapecng::metacircuit::as_string(fast.digit().second);
  if(full.singular())
    return "";
  auto eval = [](const expression& e, std::complex<long double> s) {
      std::complex<long double> v = 0;
      for(const auto& d : e)
        for(const auto& t : d.second)
          v += (long double)t.first * std::pow(s, d.first);
      return v;
    };
  for(double f : { 0.01, 3.7, 159.2, 12345., 7.7e5, 3.1e7, 2.2e9 }) {
    const std::complex<long double> s(0.L, 2.L * 3.14159265358979323846L * f);
    const std::complex<long double> a = eval(full.digit().first, s)
      / eval(full.digit().second, s);
    const std::complex<long double> b = eval(fast.digit().first, s)
      / eval(fast.digit().second, s);
    const long double scale = std::max(std::abs(a), std::abs(b));
    if(!(std::abs(a - b) <= 1e-9L * scale) && scale > 0)
      return "H(j 2 pi " + std::to_string(f) + ") differs";
  }
  return "";
}

}


#endif // REFERENCE_SOLVER_H
