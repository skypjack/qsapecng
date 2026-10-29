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


#include "model/numericsolve.h"
#include "model/graph.h"

#include "boost-sapecng/mrt_pool.hpp"
#include "boost-sapecng/mrt_progress.hpp"

#ifndef Q_MOC_RUN
#include <boost/multiprecision/cpp_int.hpp>
#endif

#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>


namespace sapecng
{


namespace
{


typedef std::uint64_t u64;
typedef std::int64_t i64;


// primes just below 2^31: a product of two residues fits in 64 bits
const std::vector<u64>& primes(std::size_t count)
{
  static std::mutex mutex;
  static std::vector<u64> list;
  static std::vector<u64> small;
  std::lock_guard<std::mutex> lock(mutex);
  if(small.empty()) {
    for(u64 n = 2; n < 46341; ++n) {
      bool prime = true;
      for(u64 d : small) {
        if(d * d > n)
          break;
        if(n % d == 0) {
          prime = false;
          break;
        }
      }
      if(prime)
        small.push_back(n);
    }
  }
  u64 n = list.empty() ? (u64(1) << 31) - 1 : list.back() - 2;
  while(list.size() < count) {
    bool prime = true;
    for(u64 d : small) {
      if(d * d > n)
        break;
      if(n % d == 0) {
        prime = false;
        break;
      }
    }
    if(prime)
      list.push_back(n);
    n -= 2;
  }
  return list;
}


inline u64 mul(u64 a, u64 b, u64 p) { return (a * b) % p; }
inline u64 add(u64 a, u64 b, u64 p) { const u64 s = a + b; return s >= p ? s - p : s; }
inline u64 sub(u64 a, u64 b, u64 p) { return a >= b ? a - b : a + p - b; }

u64 power(u64 a, u64 e, u64 p)
{
  u64 r = 1;
  a %= p;
  while(e) {
    if(e & 1)
      r = mul(r, a, p);
    a = mul(a, a, p);
    e >>= 1;
  }
  return r;
}

inline u64 inverse(u64 a, u64 p) { return power(a, p - 2, p); }

inline u64 residue(i64 v, u64 p)
{
  const i64 r = v % i64(p);
  return u64(r < 0 ? r + i64(p) : r);
}


// w = m 2^e, m an odd integer (|m| < 2^53); m = 0 for w = 0
struct binary
{
  i64 m = 0;
  int e = 0;
};

binary split(double w)
{
  binary b;
  if(w == 0.)
    return b;
  int exp = 0;
  const double f = std::frexp(w, &exp);           // w = f 2^exp, |f| in [0.5, 1)
  i64 m = i64(std::ldexp(f, 53));                 // exact
  int e = exp - 53;
  while((m & 1) == 0) {
    m /= 2;
    ++e;
  }
  b.m = m;
  b.e = e;
  return b;
}

// b mod p (2^e by powers of 2 or of its inverse)
u64 residue(const binary& b, u64 p)
{
  u64 r = residue(b.m, p);
  if(b.e >= 0)
    r = mul(r, power(2, u64(b.e), p), p);
  else
    r = mul(r, power(inverse(2, p), u64(-b.e), p), p);
  return r;
}


// determinant of the n x n matrix a (row by row) modulo p; a is spoilt
u64 determinant(std::vector<u64>& a, int n, u64 p)
{
  u64 det = 1;
  for(int c = 0; c < n; ++c) {
    int pivot = -1;
    for(int r = c; r < n; ++r)
      if(a[std::size_t(r) * n + c] != 0) {
        pivot = r;
        break;
      }
    if(pivot < 0)
      return 0;
    if(pivot != c) {
      for(int k = c; k < n; ++k)
        std::swap(a[std::size_t(pivot) * n + k], a[std::size_t(c) * n + k]);
      det = det ? p - det : 0;
    }
    const u64 d = a[std::size_t(c) * n + c];
    det = mul(det, d, p);
    const u64 inv = inverse(d, p);
    for(int r = c + 1; r < n; ++r) {
      u64& lead = a[std::size_t(r) * n + c];
      if(lead == 0)
        continue;
      const u64 f = mul(lead, inv, p);
      const u64* top = &a[std::size_t(c) * n];
      u64* row = &a[std::size_t(r) * n];
      for(int k = c + 1; k < n; ++k)
        if(top[k])
          row[k] = sub(row[k], mul(f, top[k], p), p);
      lead = 0;
    }
  }
  return det;
}


// coefficients (degree 0 .. n-1) of the polynomial through (x[i], y[i])
std::vector<u64> interpolate(const std::vector<u64>& x,
  const std::vector<u64>& y, u64 p)
{
  const std::size_t n = x.size();
  // Newton's divided differences, then expanded
  std::vector<u64> c(y);
  for(std::size_t j = 1; j < n; ++j)
    for(std::size_t i = n - 1; i >= j; --i)
      c[i] = mul(sub(c[i], c[i - 1], p),
        inverse(sub(x[i], x[i - j], p), p), p);
  std::vector<u64> poly(n, 0);
  for(std::size_t k = n; k-- > 0; ) {
    // poly = poly * (s - x[k]) + c[k]
    for(std::size_t d = n - 1; d > 0; --d)
      poly[d] = sub(poly[d - 1], mul(poly[d], x[k], p), p);
    poly[0] = sub(c[k], mul(poly[0], x[k], p), p);
  }
  return poly;
}


// a big integer to the nearest double, times 2^-shift
double to_double(const boost::multiprecision::cpp_int& v, int shift)
{
  using boost::multiprecision::cpp_int;
  if(v == 0)
    return 0.;
  const bool negative = v < 0;
  const cpp_int a = negative ? cpp_int(-v) : v;
  const unsigned top = unsigned(boost::multiprecision::msb(a));
  double d;
  int scale = -shift;
  if(top < 63) {
    d = double(a.convert_to<u64>());
  } else {
    // 63 leading bits and a sticky one for the rest: rounded once
    const unsigned drop = top - 62;
    const cpp_int head = a >> drop;
    u64 bits = head.convert_to<u64>();
    if((head << drop) != a)
      bits |= 1;
    d = double(bits);
    scale += int(drop);
  }
  d = std::ldexp(d, scale);
  return negative ? -d : d;
}


}


bool numeric_transfer(const tree_graphs& g,
  const std::vector<numeric_edge>& edges,
  std::map<int, double>& numOut, std::map<int, double>& denOut,
  boost::mrt_progress* progress)
{
  numOut.clear();
  denOut.clear();
  const int V = g.vertices;
  const int M = g.edges;
  const int n = V - 1;
  std::atomic<bool>* cancel = progress ? progress->cancel : nullptr;

  // what each edge is: its weight in the matrix
  enum Kind { Weight, Inverse, Forced, Numerator, Denominator, Nothing };
  std::vector<Kind> kind(M, Nothing);
  std::vector<binary> value(M);
  int forced = 0, degreeMax = 0;
  long long bits = M + 4;   // a bound on |coefficient| 2^K, in bits
  int K = 0;                // 2^K makes every coefficient an integer
  for(int j = 0; j < M; ++j) {
    const numeric_edge& e = edges[j];
    if(e.type == F) {
      kind[j] = Forced;
      ++forced;
    } else if(e.type == GREF) {
      kind[j] = Numerator;
    } else if(e.type == YREF) {
      kind[j] = Denominator;
    } else if(e.type == Z && e.weight == 0.) {
      kind[j] = Forced;   // out of the tree it gives 0: only trees with it
      ++forced;
    } else if(e.type == Y || e.type == Z) {
      kind[j] = (e.type == Y) ? Weight : Inverse;
      value[j] = split(e.weight);
      degreeMax += e.degree;
      if(value[j].m != 0) {
        K += std::max(0, -value[j].e);
        bits += std::max(0, std::ilogb(e.weight) + 1);
      }
    }
  }
  bits += K;
  if(!std::isfinite(double(bits)) || n < 0)
    return true;

  // primes enough for the bound (and for the sign)
  const std::size_t count = std::size_t(bits / 30 + 2);
  const std::vector<u64>& all = primes(count + 64);

  // the points s and t (all distinct, none 0)
  const int points = degreeMax + 1;

  struct residues
  {
    u64 p = 0;
    std::vector<u64> num, den;   // coefficient by power of s, mod p
  };
  std::vector<residues> found;
  std::mutex foundMutex;

  // one prime: false if it does not suit (a weight is 0 modulo p)
  auto one = [&](u64 p, residues& r) -> bool {
      std::vector<u64> w(M, 0);
      for(int j = 0; j < M; ++j) {
        if(kind[j] != Weight && kind[j] != Inverse)
          continue;
        w[j] = residue(value[j], p);
        if(kind[j] == Inverse && w[j] == 0)
          return false;
      }
      // the leading coefficient in t: sum of P(t_i) / prod (t_i - t_k)
      std::vector<u64> lambda(forced + 1);
      for(int i = 0; i <= forced; ++i) {
        u64 d = 1;
        for(int k = 0; k <= forced; ++k)
          if(k != i)
            d = mul(d, sub(u64(i + 1), u64(k + 1), p), p);
        lambda[i] = inverse(d, p);
      }
      const u64 scale = power(2, u64(K), p);
      std::vector<u64> xs(points), nums(points), dens(points);
      std::vector<u64> a(std::size_t(n) * n);
      for(int k = 0; k < points; ++k) {
        if(cancel && cancel->load(std::memory_order_relaxed))
          return true;
        const u64 x = u64(k + 1);
        xs[k] = x;
        // the weights at s = x; the factor of the Z edges
        std::vector<u64> wx(M, 0);
        u64 factor = 1;
        for(int j = 0; j < M; ++j) {
          if(kind[j] == Weight) {
            wx[j] = edges[j].degree ? mul(w[j], x, p) : w[j];
          } else if(kind[j] == Inverse) {
            const u64 z = edges[j].degree ? mul(w[j], x, p) : w[j];
            factor = mul(factor, z, p);
            wx[j] = inverse(z, p);
          }
        }
        u64 q[3] = { 0, 0, 0 };   // (g, r) = (1, 1), (0, 1), (0, 0)
        const int gs[3] = { 1, 0, 0 }, rs[3] = { 1, 1, 0 };
        for(int c = 0; c < 3; ++c) {
          u64 lead = 0;
          for(int i = 0; i <= forced; ++i) {
            const u64 t = u64(i + 1);
            std::fill(a.begin(), a.end(), 0);
            for(int j = 0; j < M; ++j) {
              u64 wj;
              switch(kind[j]) {
              case Weight: case Inverse: wj = wx[j]; break;
              case Forced: wj = t; break;
              case Numerator: wj = u64(gs[c]); break;
              case Denominator: wj = u64(rs[c]); break;
              default: wj = 0;
              }
              if(wj == 0)
                continue;
              // + w at (target, target), (source, source); - at the others
              const int iu = g.iu[j], iv = g.iv[j], vu = g.vu[j], vv = g.vv[j];
              const int ri[2] = { iv, iu }, rv[2] = { vv, vu };
              for(int s1 = 0; s1 < 2; ++s1) {
                if(ri[s1] >= n)
                  continue;
                for(int s2 = 0; s2 < 2; ++s2) {
                  if(rv[s2] >= n)
                    continue;
                  u64& cell = a[std::size_t(ri[s1]) * n + rv[s2]];
                  cell = (s1 == s2) ? add(cell, wj, p) : sub(cell, wj, p);
                }
              }
            }
            const u64 det = n > 0 ? determinant(a, n, p) : 1;
            lead = add(lead, mul(det, lambda[i], p), p);
          }
          q[c] = mul(mul(lead, factor, p), scale, p);
        }
        nums[k] = sub(q[0], q[1], p);
        dens[k] = sub(q[1], q[2], p);
      }
      r.p = p;
      r.num = interpolate(xs, nums, p);
      r.den = interpolate(xs, dens, p);
      return true;
    };

  // the primes, spread over the pool's threads
  boost::mrt_pool* pool = progress ? progress->pool : nullptr;
  std::atomic<std::size_t> next(0);
  auto work = [&]() {
      for(;;) {
        {
          std::lock_guard<std::mutex> lock(foundMutex);
          if(found.size() >= count)
            return;
        }
        const std::size_t i = next.fetch_add(1);
        if(i >= all.size())
          return;
        if(cancel && cancel->load(std::memory_order_relaxed))
          return;
        residues r;
        if(!one(all[i], r) || r.num.empty())
          continue;
        std::lock_guard<std::mutex> lock(foundMutex);
        if(found.size() < count)
          found.push_back(std::move(r));
      }
    };
  if(pool && pool->size() > 1) {
    boost::mrt_pool::group group;
    for(unsigned i = 1; i < pool->size(); ++i)
      pool->submit(group, work);
    work();
    pool->wait(group);
  } else {
    work();
  }
  if(cancel && cancel->load(std::memory_order_relaxed))
    return false;
  if(found.size() < count)
    return true;   // cannot happen (enough primes): no result

  // the same order whatever the threads did
  std::sort(found.begin(), found.end(),
    [](const residues& a, const residues& b) { return a.p < b.p; });

  // Chinese remainder (Garner), then the integer between -P/2 and P/2
  using boost::multiprecision::cpp_int;
  cpp_int P = 1;
  for(const residues& r : found)
    P *= r.p;
  const cpp_int half = P / 2;
  // inv[i][k]: 1 / p_k modulo p_i (k < i)
  std::vector< std::vector<u64> > inv(found.size());
  for(std::size_t i = 0; i < found.size(); ++i)
    for(std::size_t k = 0; k < i; ++k)
      inv[i].push_back(inverse(found[k].p % found[i].p, found[i].p));
  auto rebuild = [&](int degree, bool numerator) {
      std::vector<u64> mixed(found.size());
      for(std::size_t i = 0; i < found.size(); ++i) {
        const u64 p = found[i].p;
        u64 v = (numerator ? found[i].num : found[i].den)[degree];
        for(std::size_t k = 0; k < i; ++k)
          v = mul(sub(v, mixed[k] % p, p), inv[i][k], p);
        mixed[i] = v;
      }
      cpp_int x = 0;
      for(std::size_t i = found.size(); i-- > 0; ) {
        x *= found[i].p;
        x += mixed[i];
      }
      if(x > half)
        x -= P;
      return x;
    };
  std::vector<cpp_int> nums(points), dens(points);
  for(int d = 0; d < points; ++d) {
    nums[d] = rebuild(d, true);
    dens[d] = rebuild(d, false);
  }

  // A big circuit's coefficients can span more decades than a double
  // reaches at either end (a 9-section ladder: 1e-320 ... 1e0, or once
  // made monic 1 ... 1e320, an infinity). The numerator and the
  // denominator may be multiplied by the same factor: one power of two
  // that puts their range in the middle of the doubles' (the ratio, the
  // transfer function, stays exactly as it is)
  long long top = LLONG_MIN, bottom = LLONG_MAX;
  for(const std::vector<cpp_int>* v : { &nums, &dens })
    for(const cpp_int& c : *v)
      if(c != 0) {
        const long long e = (long long)boost::multiprecision::msb(
          c < 0 ? cpp_int(-c) : c) - K;
        top = std::max(top, e);
        bottom = std::min(bottom, e);
      }
  long long shift = K;
  if(top != LLONG_MIN && (top > 900 || bottom < -900))
    shift += (top + bottom) / 2;   // the middle of the range at 2^0

  for(int d = 0; d < points; ++d) {
    if(nums[d] != 0)
      numOut[d] = to_double(nums[d], int(shift));
    if(dens[d] != 0)
      denOut[d] = to_double(dens[d], int(shift));
  }
  return true;
}


}
