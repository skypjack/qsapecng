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


#include "model/metacircuit.h"

#include "model/exactsum.h"
#include "model/numericsolve.h"
#include "model/treesearch.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <iterator>
#include <list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


namespace sapecng
{


std::string metacircuit::as_string(const expression& e)
{
  std::string e_str = "";

  if(e.size() != 0) {
    expression::const_reverse_iterator it = e.rbegin();

    e_str += as_string(it->second);
    if(it->first != 0) {
      e_str += " * s";
      if(it->first > 1)
        e_str += "^" + stringify(it->first);
    }

    for(++it; it != e.rend(); ++it) {
      e_str += " + " + as_string(it->second);
      if(it->first != 0) {
        e_str += " * s";
        if(it->first > 1)
          e_str += "^" + stringify(it->first);
      }
    }
  }

  if(e_str.size() == 0)
    e_str = "0";

  return e_str;
}


std::string metacircuit::stringify(double x)
{
  std::ostringstream o;
  if (!(o << x));
//     throw BadConversion("stringify(double)");
  return o.str();
} 


std::string metacircuit::as_string(const degree_expression& e)
{
  std::string e_str = "";

  if(e.size() != 0) {
    if(e.size() > 1)
      e_str += "( ";

    degree_expression::const_iterator it = e.begin();
    e_str += as_string(*(it++));

    for(; it != e.end(); ++it)
      e_str += " + " + as_string(*it);

    if(e.size() > 1)
      e_str += " )";
  }

  return e_str;
}


std::string metacircuit::as_string(const basic_expression& e)
{
  std::string e_str = "";

  int adjust_value = 1;
  if(e.first < 0) {
    adjust_value = -1;
    e_str += "( - ";
  }

  bool has_sym_part = (e.second.size() != 0);
  bool has_num_part = false;
  if(!has_sym_part || (e.first * adjust_value) != 1) {
    e_str += stringify(adjust_value * e.first);
    has_num_part = true;
  }

  symbol_list::const_iterator it = e.second.begin();

  if(!has_num_part)
    e_str += *(it++);

  for(; it != e.second.end(); ++it)
    e_str += " * " + *it;

  if(e.first < 0)
    e_str += " )";

  return e_str;
}


namespace
{


// what an edge adds to a term when it counts (Y in the tree, Z out of it)
struct edge_info
{
  EdgeType type = Z;
  double weight = 1.;
  int degree = 0;
  int name = -1;            // its name's number, -1: no name
  bool symbolic = false;
};


// det of the incidence matrix of a spanning tree (rows: the vertices,
// columns: its edges in order) without the last vertex's row: +-1. The
// elimination it replaces (get_sign) worked out the same: its row swaps
// and the zero sum of every column make up for whichever row it dropped.
// Each vertex but the last pairs with the edge towards the last one; the
// sign is the parity of that pairing times the entries met (+1 where the
// edge ends, -1 where it starts).
class tree_sign
{

public:
  int operator()(const tree_graphs& g, const std::vector<int>& u,
    const std::vector<int>& v, const std::vector<bool>& tree)
  {
    const int V = g.vertices;
    if(V < 2)
      return 1;
    if(int(head_.size()) != V) {
      head_.assign(V, -1);
      seen_.assign(V, 0);
      perm_.assign(V, 0);
      stack_.assign(V, 0);
      cu_.assign(V, 0);
      cv_.assign(V, 0);
      next_.assign(2 * V, 0);
      to_.assign(2 * V, 0);
      col_.assign(2 * V, 0);
    }
    const int root = V - 1;
    int n = 0;
    for(int j = 0; j < g.edges; ++j)
      if(tree[j]) {
        cu_[n] = u[j];
        cv_[n] = v[j];
        ++n;
      }
    for(int x = 0; x < V; ++x) {
      head_[x] = -1;
      seen_[x] = 0;
    }
    for(int k = 0; k < n; ++k) {
      to_[2 * k] = cv_[k];
      col_[2 * k] = k;
      next_[2 * k] = head_[cu_[k]];
      head_[cu_[k]] = 2 * k;
      to_[2 * k + 1] = cu_[k];
      col_[2 * k + 1] = k;
      next_[2 * k + 1] = head_[cv_[k]];
      head_[cv_[k]] = 2 * k + 1;
    }
    int sign = 1, top = 0;
    stack_[top++] = root;
    seen_[root] = 1;
    while(top) {
      const int x = stack_[--top];
      for(int a = head_[x]; a >= 0; a = next_[a]) {
        const int y = to_[a];
        if(seen_[y])
          continue;
        seen_[y] = 1;
        stack_[top++] = y;
        const int k = col_[a];
        perm_[y] = k;   // y < root: its row is row y
        if(cv_[k] != y)
          sign = -sign;
      }
    }
    // the parity of the pairing (rows 0 .. V-2 to columns 0 .. V-2)
    for(int x = 0; x < root; ++x)
      seen_[x] = 0;
    for(int x = 0; x < root; ++x) {
      if(seen_[x])
        continue;
      int length = 0;
      for(int y = x; !seen_[y]; y = perm_[y]) {
        seen_[y] = 1;
        ++length;
      }
      if(!(length & 1))
        sign = -sign;
    }
    return sign;
  }

private:
  std::vector<int> head_, seen_, perm_, stack_, cu_, cv_, next_, to_, col_;

};


// a term of a sum and where it first came from (to keep the order of the
// terms as found by a single thread)
// a term: its coefficient so far, and where its first tree lies (the
// first two words of its tree_position in place: a circuit of up to 126
// edges; the rest on the heap). A big result has millions of them
struct term_entry
{
  exact_sum sum;
  std::uint64_t at[2] = { 0, 0 };
  std::unique_ptr<std::uint64_t[]> rest;

  void set(const tree_position& p)
  {
    at[0] = p.size() > 0 ? p[0] : 0;
    at[1] = p.size() > 1 ? p[1] : 0;
    if(p.size() > 2) {
      rest.reset(new std::uint64_t[p.size() - 2]);
      std::copy(p.begin() + 2, p.end(), rest.get());
    }
  }
  std::uint64_t word(std::size_t i) const
    { return i < 2 ? at[i] : rest[i - 2]; }
};

// found earlier in the search (as tree_before)
inline bool before(const term_entry& a, const term_entry& b,
  std::size_t words)
{
  for(std::size_t i = 0; i < words; ++i)
    if(a.word(i) != b.word(i))
      return a.word(i) < b.word(i);
  return false;
}

typedef std::unordered_map<std::string, term_entry> term_table;


// More terms than the memory allows: the symbolic result is given up
// (metacircuit::operator() throws metacircuit::too_large)
struct term_count
{
  std::atomic<long long> terms{0};
  long long limit = 0;              // 0: none
  std::atomic<bool> exceeded{false};
  std::atomic<bool>* cancel = nullptr;   // the search's: the others stop
};


// the terms of the trees: raw, digit, mixed of the numerator (0 - 2) and
// of the denominator (3 - 5)
class term_sink
{

public:
  term_sink(const tree_graphs& g, const std::vector<edge_info>& edges,
      term_count* count)
    : g_(&g), edges_(&edges), count_(count)
    { words_ = std::size_t(g.edges + 2) / 64 + 1; }

  term_sink part() const
  {
    term_sink t(*g_, *edges_, count_);
    t.mixedIsRaw_ = mixedIsRaw_;
    return t;
  }

  // every mixed term is the raw one (no named component is given by
  // value, no symbolic one is nameless): one set of tables, not two
  void setMixedIsRaw(bool same) { mixedIsRaw_ = same; }

  void merge(term_sink&& other)
  {
    for(int k = 0; k < 6; ++k) {
      for(auto& kv : other.tables_[k]) {
        term_table::iterator it = tables_[k].find(kv.first);
        if(it == tables_[k].end()) {
          tables_[k].emplace(kv.first, std::move(kv.second));
        } else {
          it->second.sum.add(kv.second.sum);
          if(before(kv.second, it->second, words_)) {
            it->second.at[0] = kv.second.at[0];
            it->second.at[1] = kv.second.at[1];
            it->second.rest = std::move(kv.second.rest);
          }
        }
      }
      other.tables_[k].clear();
    }
  }

  void tree(const std::vector<bool>& inL, const tree_position& position)
  {
    const std::vector<edge_info>& edges = *edges_;
    int sign = 1;
    sign *= sign_(*g_, g_->iu, g_->iv, inL);
    sign *= sign_(*g_, g_->vu, g_->vv, inL);

    double raw = 1., digit = 1., mixed = 1.;
    raw *= sign;
    digit *= sign;
    mixed *= sign;

    bool numerator_part = false;
    bool denominator_part = false;
    int degree = 0;
    raw_.clear();
    mixed_.clear();
    for(std::size_t j = 0; j < edges.size(); ++j) {
      const edge_info& e = edges[j];
      if(inL[j] && e.type == sapecng::GREF)
        numerator_part = true;
      if(inL[j] && e.type == sapecng::YREF)
        denominator_part = true;
      if((inL[j] && e.type == sapecng::Y) || (!inL[j] && e.type == sapecng::Z)) {
        if(e.symbolic) {
          if(e.name >= 0)
            mixed_.push_back(e.name);
        } else {
          mixed *= e.weight;
        }
        digit *= e.weight;
        if(e.name >= 0)
          raw_.push_back(e.name);
        else
          raw *= e.weight;
        degree += e.degree;
      }
    }
    if(!numerator_part && !denominator_part)
      return;

    // the names are numbered in alphabetical order: sorted numbers are
    // sorted names
    std::sort(raw_.begin(), raw_.end());
    std::sort(mixed_.begin(), mixed_.end());
    const int base = numerator_part ? 0 : 3;
    add(tables_[base], degree, raw_, raw, position);
    none_.clear();
    add(tables_[base + 1], degree, none_, digit, position);
    if(!mixedIsRaw_)
      add(tables_[base + 2], degree, mixed_, mixed, position);
  }

  // the distinct terms kept
  long long size() const
  {
    long long n = 0;
    for(const term_table& t : tables_)
      n += (long long)t.size();
    return n;
  }

  // The sums, the terms in the order a single thread finds them; each
  // table is let go once used. Raw and mixed terms with the same names (all
  // of them when every component is symbolic) share one list of names
  void results(metacircuit::expression* out[6],
    const std::vector<std::string>& names)
  {
    std::vector<const std::string*> interned;
    for(const std::string& n : names)
      interned.push_back(symbol_list::intern(n));
    std::unordered_map<std::string, symbol_list> shared;   // by names
    for(int k : { 0, 3, 1, 4, 2, 5 }) {
      if(mixedIsRaw_ && (k == 2 || k == 5)) {
        // the raw terms again; a copy shares their lists of names
        *out[k] = *out[k == 2 ? 0 : 3];
        continue;
      }
      const bool raw = (k == 0 || k == 3), mixed = (k == 2 || k == 5);
      std::vector<term_table::value_type*> terms;
      terms.reserve(tables_[k].size());
      for(auto& kv : tables_[k])
        terms.push_back(&kv);
      const std::size_t words = words_;
      std::sort(terms.begin(), terms.end(),
        [words](const term_table::value_type* a,
                const term_table::value_type* b) {
          return before(a->second, b->second, words);
        });
      metacircuit::expression& e = *out[k];
      for(term_table::value_type* t : terms) {
        std::int16_t degree = 0;
        std::memcpy(&degree, t->first.data(), sizeof degree);
        const std::string ids = t->first.substr(sizeof degree);
        symbol_list symbols;
        std::unordered_map<std::string, symbol_list>::iterator found =
          mixed ? shared.find(ids) : shared.end();
        if(found != shared.end()) {
          symbols = found->second;
        } else {
          symbols.reserve(ids.size() / sizeof(std::uint16_t));
          for(std::size_t at = 0; at < ids.size(); at += sizeof(std::uint16_t)) {
            std::uint16_t id = 0;
            std::memcpy(&id, ids.data() + at, sizeof id);
            symbols.push_back_interned(interned[id]);
          }
          if(raw && !mixedIsRaw_)
            shared.emplace(ids, symbols);
        }
        e[degree].push_back(
          metacircuit::basic_expression(t->second.sum.value(), symbols));
      }
      term_table().swap(tables_[k]);   // its memory back now
    }
  }

private:
  // the key: the power of s and the names' numbers, 16 bits each
  void add(term_table& table, int degree, const std::vector<int>& ids,
    double value, const tree_position& position)
  {
    const std::int16_t d = std::int16_t(degree);
    key_.assign(reinterpret_cast<const char*>(&d), sizeof d);
    for(int id : ids) {
      const std::uint16_t u = std::uint16_t(id);
      key_.append(reinterpret_cast<const char*>(&u), sizeof u);
    }
    term_table::iterator it = table.find(key_);
    if(it == table.end()) {
      // the trees of one part come in order: this is its first
      term_entry entry;
      entry.set(position);
      it = table.emplace(key_, std::move(entry)).first;
      if(count_ && ++fresh_ >= 1024)
        flush();
    }
    it->second.sum.add(value);
  }

  // new terms to the count; over the limit: the search stops
  void flush()
  {
    if(!count_ || !fresh_)
      return;
    const long long n = count_->terms.fetch_add(fresh_) + fresh_;
    fresh_ = 0;
    if(count_->limit > 0 && n > count_->limit) {
      count_->exceeded.store(true);
      if(count_->cancel)
        count_->cancel->store(true);
      throw boost::mrt_cancelled();   // see metacircuit::operator()
    }
  }

  const tree_graphs* g_;
  const std::vector<edge_info>* edges_;
  term_count* count_ = nullptr;
  long long fresh_ = 0;   // new terms not counted yet
  bool mixedIsRaw_ = false;
  std::size_t words_ = 1;
  term_table tables_[6];
  tree_sign sign_;
  std::vector<int> raw_, mixed_, none_;
  std::string key_;

};


}


namespace
{

// the two graphs by edge number, and the edges every tree must have
bool tree_problem(const circuit& circuit, tree_graphs& graphs,
  std::vector<bool>& forcedIn)
{
  const Graph& iG = circuit.iGraph();
  const Graph& vG = circuit.vGraph();
  if(num_edges(vG) != num_edges(iG) || num_vertices(vG) != num_vertices(iG))
    return false;
  const std::vector<circuit::edge_descriptor>& iO = circuit.iGraph_order();
  const std::vector<circuit::edge_descriptor>& vO = circuit.vGraph_order();
  graphs.vertices = int(num_vertices(iG));
  graphs.edges = int(num_edges(iG));
  forcedIn.assign(graphs.edges, false);
  for(int j = 0; j < graphs.edges; ++j) {
    graphs.iu.push_back(int(source(iO[j], iG)));
    graphs.iv.push_back(int(target(iO[j], iG)));
    graphs.vu.push_back(int(source(vO[j], vG)));
    graphs.vv.push_back(int(target(vO[j], vG)));
    if(get(boost::edge_type, iG)[ iO[j] ] == sapecng::F)
      forcedIn[j] = true;
  }
  return true;
}


// the circuit as far as its terms go: the two graphs, every part's kind,
// power of s and name, and the value of the unnamed ones (the raw terms
// hold them). Not in it: the named parts' values, which are symbolic
std::string circuit_shape(const circuit& circuit, const tree_graphs& g)
{
  const Graph& refG = circuit.iGraph();
  const std::vector<circuit::edge_descriptor>& refO = circuit.iGraph_order();
  std::string s;
  auto put = [&s](const void* p, std::size_t n)
    { s.append(static_cast<const char*>(p), n); };
  put(&g.vertices, sizeof g.vertices);
  put(&g.edges, sizeof g.edges);
  for(int j = 0; j < g.edges; ++j) {
    const int at[6] = { g.iu[j], g.iv[j], g.vu[j], g.vv[j],
      int(get(boost::edge_type, refG)[ refO[j] ]),
      int(get(boost::edge_degree, refG)[ refO[j] ]) };
    put(at, sizeof at);
    const std::string& name = get(boost::edge_name, refG)[ refO[j] ];
    const std::uint32_t length = std::uint32_t(name.size());
    put(&length, sizeof length);
    s += name;
    if(name.empty()) {
      const double weight = get(boost::edge_weight, refG)[ refO[j] ];
      const char symbolic = get(boost::edge_symbolic, refG)[ refO[j] ];
      put(&weight, sizeof weight);
      put(&symbolic, sizeof symbolic);
    }
  }
  return s;
}

// a named part's value, and whether it stays a symbol (in mixed)
struct name_value
{
  double weight;
  bool symbolic;
};
typedef std::unordered_map<const std::string*, name_value> name_values;

// the circuit fits the terms of `cache`: its named parts' values. Every
// part of a name (an ideal transformer has two) has one value
bool fit_values(const circuit& circuit,
  const metacircuit::term_cache& cache, name_values& values)
{
  tree_graphs graphs;
  std::vector<bool> forcedIn;
  if(!tree_problem(circuit, graphs, forcedIn)
      || circuit_shape(circuit, graphs) != cache.shape)
    return false;
  const Graph& refG = circuit.iGraph();
  const std::vector<circuit::edge_descriptor>& refO = circuit.iGraph_order();
  values.clear();
  for(int j = 0; j < graphs.edges; ++j) {
    const EdgeType type = get(boost::edge_type, refG)[ refO[j] ];
    const std::string& name = get(boost::edge_name, refG)[ refO[j] ];
    if((type != sapecng::Y && type != sapecng::Z) || name.empty())
      continue;
    const name_value v = { get(boost::edge_weight, refG)[ refO[j] ],
      bool(get(boost::edge_symbolic, refG)[ refO[j] ]) };
    const auto at = values.emplace(symbol_list::intern(name), v);
    if(!at.second && (std::memcmp(&at.first->second.weight, &v.weight,
          sizeof v.weight) != 0 || at.first->second.symbolic != v.symbolic))
      return false;
  }
  return true;
}

}


bool metacircuit::fits(const circuit& circ, const term_cache& cache)
{
  name_values values;
  return fit_values(circ, cache, values);
}


bool metacircuit::reuse(const circuit& circ,
  const std::shared_ptr<const term_cache>& cache)
{
  name_values values;
  if(!cache || !fit_values(circ, *cache, values))
    return false;

  singular_ = false;
  raw_ = cache->raw;
  digit_.first.clear();
  digit_.second.clear();
  mixed_.first.clear();
  mixed_.second.clear();

  // each raw term (its number: the signs and the unnamed parts) times the
  // values put in: all of them (digit), those not symbolic (mixed). Mixed
  // terms in the order of their first raw term, as the trees give them
  std::vector<const std::string*> kept;
  std::string key;
  for(int side = 0; side < 2; ++side) {
    const expression& raw = side ? cache->raw.second : cache->raw.first;
    expression& digit = side ? digit_.second : digit_.first;
    expression& mixed = side ? mixed_.second : mixed_.first;
    for(const auto& degree : raw) {
      exact_sum digitSum;
      std::unordered_map<std::string, std::size_t> found;
      std::vector<std::pair<exact_sum, symbol_list>> terms;
      for(const basic_expression& term : degree.second) {
        double all = term.first, some = term.first;
        bool whole = true;   // every name stays
        kept.clear();
        for(symbol_list::const_iterator it = term.second.begin();
            it != term.second.end(); ++it) {
          const name_value& v = values.at(it.operator->());
          all *= v.weight;
          if(v.symbolic) {
            kept.push_back(it.operator->());
          } else {
            some *= v.weight;
            whole = false;
          }
        }
        digitSum.add(all);
        key.assign(reinterpret_cast<const char*>(kept.data()),
          kept.size() * sizeof(const std::string*));
        const auto at = found.emplace(key, terms.size());
        if(at.second) {
          symbol_list names;
          if(whole) {
            names = term.second;   // shares its list
          } else {
            names.reserve(kept.size());
            for(const std::string* n : kept)
              names.push_back_interned(n);
          }
          terms.emplace_back(exact_sum(), names);
        }
        terms[at.first->second].first.add(some);
      }
      digit[degree.first].push_back(
        basic_expression(digitSum.value(), symbol_list()));
      degree_expression& out = mixed[degree.first];
      for(auto& t : terms)
        out.push_back(basic_expression(t.first.value(), t.second));
    }
  }
  terms_ = cache;

  const bool rawOk = simplify(raw_);
  const bool digitOk = simplify(digit_);
  simplify(mixed_);
  singular_ = !rawOk || !digitOk;
  return true;
}


void metacircuit::operator()(const circuit& circuit)
{
  // clear data
  singular_ = false;
  terms_.reset();
  raw_.first.clear();
  raw_.second.clear();
  digit_.first.clear();
  digit_.second.clear();
  mixed_.first.clear();
  mixed_.second.clear();

  tree_graphs graphs;
  std::vector<bool> forcedIn;
  if(!tree_problem(circuit, graphs, forcedIn))
    return;

  const Graph& refG = circuit.iGraph();
  const std::vector<circuit::edge_descriptor>& refO = circuit.iGraph_order();

  // what each edge adds to a term
  std::vector<std::string> names;
  for(int j = 0; j < graphs.edges; ++j)
    if(get(boost::edge_name, refG)[ refO[j] ].size())
      names.push_back(get(boost::edge_name, refG)[ refO[j] ]);
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());

  std::vector<edge_info> edges(graphs.edges);
  for(int j = 0; j < graphs.edges; ++j) {
    edge_info& e = edges[j];
    e.type = get(boost::edge_type, refG)[ refO[j] ];
    e.weight = get(boost::edge_weight, refG)[ refO[j] ];
    e.degree = get(boost::edge_degree, refG)[ refO[j] ];
    e.symbolic = get(boost::edge_symbolic, refG)[ refO[j] ];
    const std::string& name = get(boost::edge_name, refG)[ refO[j] ];
    if(name.size())
      e.name = int(std::lower_bound(names.begin(), names.end(), name)
        - names.begin());
  }

  // every tree adds its terms as it is found (none is kept: a big
  // circuit has billions); the sums are exact, so neither the number of
  // threads nor the order they work in changes the result. Distinct terms
  // are counted: more than the progress allows (the memory) and the
  // symbolic result is given up
  boost::mrt_progress* progress = boost::mrt_current_progress();
  term_count count;
  count.limit = progress ? progress->term_limit : 0;
  count.cancel = progress ? progress->cancel : nullptr;
  {
    term_sink terms(graphs, edges, &count);
    bool same = true;
    for(const edge_info& e : edges)
      if((e.type == sapecng::Y || e.type == sapecng::Z)   // the others add nothing
          && ((e.name >= 0 && !e.symbolic) || (e.name < 0 && e.symbolic)))
        same = false;
    terms.setMixedIsRaw(same);
    tree_search<term_sink> search(graphs);
    try {
      search.run(forcedIn, terms, progress);
    } catch(const boost::mrt_cancelled&) {
      if(count.exceeded.load())
        throw too_large(count.terms.load());
      throw;
    }
    // the parts count as they go, in steps: the whole is checked too
    if(count.limit > 0 && terms.size() > count.limit)
      throw too_large(terms.size());

    metacircuit::expression* out[6] = { &raw_.first, &digit_.first,
      &mixed_.first, &raw_.second, &digit_.second, &mixed_.second };
    terms.results(out, names);
  }
  if(keep_) {
    std::shared_ptr<term_cache> cache = std::make_shared<term_cache>();
    cache->shape = circuit_shape(circuit, graphs);
    cache->raw = raw_;
    terms_ = cache;
  }

  const bool rawOk = simplify(raw_);
  const bool digitOk = simplify(digit_);
  simplify(mixed_);
  // structurally (raw) or for these values (digit)
  singular_ = !rawOk || !digitOk;
}


void metacircuit::numeric(const circuit& circuit)
{
  singular_ = false;
  terms_.reset();
  raw_.first.clear();
  raw_.second.clear();
  digit_.first.clear();
  digit_.second.clear();
  mixed_.first.clear();
  mixed_.second.clear();

  tree_graphs graphs;
  std::vector<bool> forcedIn;
  if(!tree_problem(circuit, graphs, forcedIn))
    return;

  const Graph& refG = circuit.iGraph();
  const std::vector<circuit::edge_descriptor>& refO = circuit.iGraph_order();
  std::vector<numeric_edge> edges(graphs.edges);
  for(int j = 0; j < graphs.edges; ++j) {
    edges[j].type = get(boost::edge_type, refG)[ refO[j] ];
    edges[j].weight = get(boost::edge_weight, refG)[ refO[j] ];
    edges[j].degree = get(boost::edge_degree, refG)[ refO[j] ];
  }

  std::map<int, double> num, den;
  if(!numeric_transfer(graphs, edges, num, den, boost::mrt_current_progress()))
    throw boost::mrt_cancelled();
  for(const auto& c : num)
    digit_.first[c.first].push_back(basic_expression(c.second,
      symbol_list()));
  for(const auto& c : den)
    digit_.second[c.first].push_back(basic_expression(c.second,
      symbol_list()));

  // as operator(): the denominator is zero when the circuit has no unique
  // solution (with no trees at all the raw result would be empty too)
  singular_ = !simplify(digit_);
}


double metacircuit::estimate_trees(const circuit& circuit, double seconds,
  bool* exact)
{
  if(exact)
    *exact = true;
  tree_graphs graphs;
  std::vector<bool> forcedIn;
  if(!tree_problem(circuit, graphs, forcedIn))
    return 0.;

  // a small circuit (up to 100,000 trees: a moment to solve): counted
  // (the time limit only guards against a slow search, not the count)
  // (one thread: its parts all count into `trees`)
  struct counter
  {
    struct enough { };
    long long* trees = nullptr;
    std::chrono::steady_clock::time_point until;
    counter part() const { return *this; }
    void merge(counter&&) { }
    void tree(const std::vector<bool>&, const tree_position&)
    {
      if((++*trees & 1023) == 0 && (*trees > 100000
          || std::chrono::steady_clock::now() > until))
        throw enough();
    }
  };
  long long trees = 0;
  counter count;
  count.trees = &trees;
  count.until = std::chrono::steady_clock::now()
    + std::chrono::microseconds((long long)(1e6 * std::max(3., seconds)));
  try {
    tree_search<counter> search(graphs);
    search.run(forcedIn, count, nullptr);
    return double(trees);
  } catch(const counter::enough&) {
  }

  // a big one: estimated
  if(exact)
    *exact = false;
  tree_search<counter> search(graphs);
  boost::mrt_progress* progress = boost::mrt_current_progress();
  return search.estimate(forcedIn, seconds,
    progress ? progress->pool : nullptr);
}


bool metacircuit::simplify(std::pair<expression, expression>& p)
{
  compress(p.first);
  compress(p.second);
  const bool denominatorOk = (p.second.size() != 0);

  if(p.first.size() == 0) {

    expression en;
    degree_expression den;
    basic_expression ben = std::make_pair(0., symbol_list());
    den.push_back(ben);
    en[0] = den;

    expression ed;
    degree_expression ded;
    basic_expression bed = std::make_pair(1., symbol_list());
    ded.push_back(bed);
    ed[0] = ded;

    p.first = en;
    p.second = ed;

  } else if(p.second.size() == 0) {

    expression en;
    degree_expression den;
    basic_expression ben = std::make_pair(1., symbol_list());
    den.push_back(ben);
    en[0] = den;

    expression ed;
    degree_expression ded;
    basic_expression bed = std::make_pair(0., symbol_list());
    ded.push_back(bed);
    ed[0] = ded;

    p.first = en;
    p.second = ed;

  } else {

    while(p.first.find(0) == p.first.end()
        && p.second.find(0) == p.second.end())
    {
      expression en;
      for(expression::iterator
          it = p.first.begin(); it != p.first.end(); ++it)
        en[it->first - 1] = it->second;

      expression ed;
      for(expression::iterator
          it = p.second.begin(); it != p.second.end(); ++it)
        ed[it->first - 1] = it->second;

      p.first = en;
      p.second = ed;
    }

    if(group_minus(p.first) && group_minus(p.second)) {
      toggle_minus(p.first);
      toggle_minus(p.second);
    }

    double div = find_div(p.first);
    // the numerator's highest coefficient made 1, unless that takes a
    // coefficient beyond what a double holds (a big circuit's span 300
    // decades and more): then the middle of their range is made 1 (a
    // power of ten), which keeps them all
    if(div != 0 && div != 1 && !fits(p, div))
      div = middle(p);
    if(div != 0 && div != 1) {
      normalize(p.first, div);
      normalize(p.second, div);
    }

    const symbol_list candidates = p.first.begin()->second.front().second;
    for(symbol_list::const_iterator
        it = candidates.begin(); it != candidates.end(); ++it)
    {
      if(has_candidate(p.first, *it)
          && has_candidate(p.second, *it))
      {
        remove_candidate(p.first, *it);
        remove_candidate(p.second, *it);
      }
    }

    if(p.first == p.second) {

      expression en;
      degree_expression den;
      basic_expression ben = std::make_pair(1., symbol_list());
      den.push_back(ben);
      en[0] = den;

      expression ed;
      degree_expression ded;
      basic_expression bed = std::make_pair(1., symbol_list());
      ded.push_back(bed);
      ed[0] = ded;

      p.first = en;
      p.second = ed;

    }

  }

  return denominatorOk;
}


void metacircuit::compress(expression& e)
{
  // Drop terms with a zero coefficient, then powers of s left without terms.
  // erase() invalidates the erased iterator, so continue from the one it
  // returns (advancing the erased one crashed the solver, issue #6).
  for(expression::iterator it = e.begin(); it != e.end(); ) {
    it->second.remove_if(detail::basic_checker());
    if(detail::degree_checker()(*it))
      it = e.erase(it);
    else
      ++it;
  }
}


bool metacircuit::group_minus(const expression& e)
{
  bool has_positive = false;;
  for(expression::const_iterator
      it = e.begin(); it != e.end() && !has_positive; ++it)
  {
    for(degree_expression::const_iterator
        dit = it->second.begin(); dit != it->second.end(); ++dit)
    {
      if(dit->first > 0)
        has_positive = true;
    }
  }

  return !has_positive;
}


void metacircuit::toggle_minus(expression& e)
{
  for(expression::iterator it = e.begin(); it != e.end(); ++it)
  {
    for(degree_expression::iterator
        dit = it->second.begin(); dit != it->second.end(); ++dit)
      dit->first *= -1;
  }
}


bool metacircuit::has_candidate(const expression& e, const std::string& c)
{
  bool miss = false;
  for(expression::const_iterator
      it = e.begin(); it != e.end() && !miss; ++it)
  {
    for(degree_expression::const_iterator
        dit = it->second.begin(); dit != it->second.end() && !miss; ++dit)
    {
      if(std::find(dit->second.begin(), dit->second.end(), c)
          == dit->second.end())
        miss = true;
    }
  }

  return !miss;
}


void metacircuit::remove_candidate(expression& e, const std::string& c)
{
  for(expression::iterator it = e.begin(); it != e.end(); ++it)
  {
    for(degree_expression::iterator
        dit = it->second.begin(); dit != it->second.end(); ++dit)
    {
      symbol_list::const_iterator f =
        std::find(dit->second.begin(), dit->second.end(), c);
      if(f != dit->second.end())
        dit->second.erase(f);
    }
  }
}


bool metacircuit::fits(const std::pair<expression, expression>& p,
  double div)
{
  for(const expression* e : { &p.first, &p.second })
    for(expression::const_iterator it = e->begin(); it != e->end(); ++it)
      for(degree_expression::const_iterator
          dit = it->second.begin(); dit != it->second.end(); ++dit)
      {
        if(dit->first == 0.)
          continue;
        const double q = std::fabs(dit->first / div);
        if(!std::isfinite(q) || q < 1e-300 || q > 1e300)
          return false;
      }
  return true;
}


double metacircuit::middle(const std::pair<expression, expression>& p)
{
  double low = HUGE_VAL, high = -HUGE_VAL;
  for(const expression* e : { &p.first, &p.second })
    for(expression::const_iterator it = e->begin(); it != e->end(); ++it)
      for(degree_expression::const_iterator
          dit = it->second.begin(); dit != it->second.end(); ++dit)
        if(dit->first != 0. && std::isfinite(dit->first)) {
          const double l = std::log10(std::fabs(dit->first));
          low = std::min(low, l);
          high = std::max(high, l);
        }
  if(!(high >= low))
    return 1.;
  return std::pow(10., std::round((low + high) / 2));
}


double metacircuit::find_div(const expression& e)
{
  degree_expression top_expr = e.rbegin()->second;
  degree_expression::iterator dit = top_expr.begin();

  bool miss = false;
  double base = std::abs(dit->first);
  for(++dit; dit != top_expr.end() && !miss; ++dit)
    if(std::abs(dit->first) != base)
      miss = true;

  if(!miss)
    return base;

  return 1.;
}


void metacircuit::normalize(expression& e, double div)
{
  for(expression::iterator it = e.begin(); it != e.end(); ++it)
  {
    for(degree_expression::iterator
        dit = it->second.begin(); dit != it->second.end(); ++dit)
      dit->first /= div;
  }
}



}
