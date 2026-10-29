/*
    SapecNG - Next Generation Symbolic Analysis Program for Electric Circuit
    Copyright (C) 2026, QSapecNG contributors

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


#ifndef NUMERICSOLVE_H
#define NUMERICSOLVE_H


#include "model/treesearch.h"

#include <map>
#include <vector>


namespace boost { struct mrt_progress; }


namespace sapecng
{


// one edge of the two graphs, as the numeric solve sees it
struct numeric_edge
{
  int type = 0;        // EdgeType
  double weight = 1.;
  int degree = 0;      // the power of s it carries (0 or 1)
};


// The numeric transfer function (the "digit" result) without listing the
// common spanning trees. The sum over the trees is a determinant
// (Cauchy-Binet): for the incidence matrices Ai, Av of the two graphs
// without their last row,
//
//   det(Ai W Av^T) = sum over the trees T of sign(T) prod_{e in T} w_e,
//
// with the same signs as the tree search. A Y edge weighs its admittance,
// a Z edge 1/z (and the whole is multiplied by every z: a Z edge counts
// when it is out of the tree); the edges a tree must have (F, and Z edges
// of value 0) weigh t and the term with every one of them is the highest
// power of t; GREF and YREF weigh g and r, which split the trees between
// numerator and denominator.
//
// Every double is an exact binary fraction, so the determinant is worked
// out exactly: modulo many primes, at enough points s to give each power
// of s, and put together again (Chinese remainder). Each coefficient is
// the exact sum over the trees, rounded once.
//
// num, den: coefficient by power of s (only those not zero). Returns false
// when cancelled (progress->cancel). A pool in the progress spreads the
// primes over its threads.
bool numeric_transfer(const tree_graphs& graphs,
  const std::vector<numeric_edge>& edges,
  std::map<int, double>& num, std::map<int, double>& den,
  boost::mrt_progress* progress);


}


#endif // NUMERICSOLVE_H
