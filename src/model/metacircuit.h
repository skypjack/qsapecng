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


#ifndef METACIRCUIT_H
#define METACIRCUIT_H


#include "model/circuit.h"

#include <exception>
#include <list>
#include <memory>
#include <string>
#include "model/symbols.h"
#include <map>


namespace sapecng
{



namespace detail
{


struct basic_checker
{

bool operator()(const std::pair< double, symbol_list >& e) const
  { return (e.first == 0); }

};


struct degree_checker
{

bool operator()(const std::pair< int, std::list<
    std::pair< double, symbol_list > > >& e) const
  { return (e.second.size() == 0); }

};


}



class metacircuit
{

public:
  typedef
  // a number times component names (symbol_list: a compact list of names)
  std::pair< double, symbol_list >
  basic_expression;

  typedef
  std::list< basic_expression >
  degree_expression;

  typedef
  std::map< int, degree_expression >
  expression;

  // the symbolic result would take more memory than allowed (see
  // boost::mrt_progress::term_limit): `terms`, the distinct terms counted
  // when it was given up
  struct too_large: public std::exception
  {
    explicit too_large(long long n): terms(n) { }
    const char* what() const noexcept { return "too many terms"; }
    long long terms;
  };

  static std::string as_string(const expression& e);

  // The symbolic result's terms as the trees give them (raw, before
  // simplify()): the same for every value of the named parts and whichever
  // of them are symbolic. Kept by operator() when keep_terms() asks for it;
  // reuse() makes the whole result from them, in a fraction of the time
  struct term_cache
  {
    std::string shape;   // the circuit but the named parts' values
    std::pair<expression, expression> raw;
  };
  inline void keep_terms(bool keep) { keep_ = keep; }
  inline std::shared_ptr<const term_cache> terms() const { return terms_; }
  // `circ` differs from the circuit of `cache` only in the values of named
  // parts (and which are symbolic): their terms are the same
  static bool fits(const circuit& circ, const term_cache& cache);
  // the whole result (raw, digit, mixed) of `circ` from `cache`, as
  // operator() gives it; false, and nothing done, when it does not fit
  bool reuse(const circuit& circ,
    const std::shared_ptr<const term_cache>& cache);
  // the whole result: raw, digit and mixed (lists the trees; can take
  // hours for a big circuit)
  void operator()(const circuit& circ);
  // the numeric result (digit) alone, without listing the trees: fast
  // whatever the size; raw and mixed stay empty
  void numeric(const circuit& circ);
  // the number of trees operator() would list (and so of its terms before
  // they are added up): counted when there are up to 100,000 (*exact
  // true: the symbolic result takes a moment), otherwise estimated by
  // random paths down the search, for `seconds` at most (within a few per
  // cent); 0 when there are none
  static double estimate_trees(const circuit& circ, double seconds = 0.3,
    bool* exact = nullptr);

  inline std::pair<metacircuit::expression, metacircuit::expression>
    raw() const { return raw_; }
  inline std::pair<metacircuit::expression, metacircuit::expression>
    digit() const { return digit_; }
  inline std::pair<metacircuit::expression, metacircuit::expression>
    mixed() const { return mixed_; }

  // the circuit has no unique solution (its determinant, the denominator,
  // is zero): e.g. a loop of voltage sources, a cut-set of current
  // sources, a floating part or an output that does not depend on the
  // input in a defined way. The expressions are then meaningless
  inline bool singular() const { return singular_; }

private:
  // test hook: the tests (metacircuit_compress_test.cpp, reference_solver.h)
  // call simplify()
  friend struct metacircuit_test_access;

  // returns false when the denominator is zero
  bool simplify(std::pair<expression, expression>& p);

  void compress(expression& e);
  bool group_minus(const expression& e);
  void toggle_minus(expression& e);
  bool has_candidate(const expression& e, const std::string& c);
  void remove_candidate(expression& e, const std::string& c);
  double find_div(const expression& e);
  // every coefficient of both divided by div is a fair double; the power
  // of ten in the middle of their range
  static bool fits(const std::pair<expression, expression>& p, double div);
  static double middle(const std::pair<expression, expression>& p);
  void normalize(expression& e, double div);

  static std::string stringify(double x);
  static std::string as_string(const degree_expression& e);
  static std::string as_string(const basic_expression& e);


private:
  std::pair<metacircuit::expression, metacircuit::expression> raw_;
  std::pair<metacircuit::expression, metacircuit::expression> digit_;
  std::pair<metacircuit::expression, metacircuit::expression> mixed_;
  bool singular_ = false;
  bool keep_ = false;
  std::shared_ptr<const term_cache> terms_;

};



}


#endif // METACIRCUIT_H
