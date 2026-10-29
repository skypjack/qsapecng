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


#ifndef EXACTSUM_H
#define EXACTSUM_H


#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>


namespace sapecng
{


// A sum of doubles kept exactly (Shewchuk's non-overlapping partials, as
// Python's math.fsum): the result is the exact sum rounded once, so it does
// not depend on the order the terms are added in. The solver adds up the
// terms of each coefficient on several threads, in no fixed order.
class exact_sum
{

public:
  exact_sum() { }
  exact_sum(const exact_sum& o) { copy(o); }
  exact_sum& operator=(const exact_sum& o)
  {
    if(this != &o) {
      release();
      copy(o);
    }
    return *this;
  }
  exact_sum(exact_sum&& o) noexcept { take(o); }
  exact_sum& operator=(exact_sum&& o) noexcept
  {
    if(this != &o) {
      release();
      take(o);
    }
    return *this;
  }
  ~exact_sum() { release(); }

  void add(double x)
  {
    plain_ += x;
    if(!std::isfinite(x) || overflow_) {
      overflow_ = true;
      return;
    }
    double* p = data();
    std::uint32_t i = 0;
    for(std::uint32_t k = 0; k < n_; ++k) {
      double y = p[k];
      if(std::fabs(x) < std::fabs(y)) {
        const double t = x;
        x = y;
        y = t;
      }
      const double hi = x + y;
      if(!std::isfinite(hi)) {
        overflow_ = true;
        return;
      }
      const double lo = y - (hi - x);
      if(lo != 0.)
        p[i++] = lo;
      x = hi;
    }
    if(i + 1 > capacity()) {
      grow(i + 1);
      p = data();
    }
    p[i] = x;
    n_ = i + 1;
  }

  void add(const exact_sum& other)
  {
    if(other.overflow_) {
      overflow_ = true;
      plain_ += other.plain_;
      return;
    }
    const double* q = other.data();
    for(std::uint32_t k = 0; k < other.n_; ++k)
      add(q[k]);
  }

  // the exact sum, rounded to the nearest double (ties to even)
  double value() const
  {
    if(overflow_)
      return plain_;   // an infinity or a NaN: no exact sum to give
    const double* p = data();
    std::uint32_t n = n_;
    if(n == 0)
      return 0.;
    double hi = p[--n];
    double lo = 0.;
    while(n > 0) {
      const double x = hi;
      const double y = p[--n];
      hi = x + y;
      const double yr = hi - x;
      lo = y - yr;
      if(lo != 0.)
        break;
    }
    // half-way between two doubles: the partials below decide
    if(n > 0 && ((lo < 0. && p[n - 1] < 0.) || (lo > 0. && p[n - 1] > 0.))) {
      const double y = lo * 2.;
      const double x = hi + y;
      const double yr = x - hi;
      if(y == yr)
        hi = x;
    }
    return hi;
  }

private:
  // the partials: two in place (nearly every sum needs no more), more on
  // the heap. A big symbolic result keeps millions of these
  static const std::uint32_t inplace = 2;

  double* data() { return big_ ? big_ : small_; }
  const double* data() const { return big_ ? big_ : small_; }
  std::uint32_t capacity() const { return big_ ? cap_ : inplace; }

  void grow(std::uint32_t need)
  {
    std::uint32_t cap = std::max<std::uint32_t>(4, capacity());
    while(cap < need)
      cap *= 2;
    double* q = new double[cap];
    std::copy(data(), data() + n_, q);
    delete [] big_;
    big_ = q;
    cap_ = cap;
  }
  void release()
  {
    delete [] big_;
    big_ = nullptr;
    cap_ = 0;
    n_ = 0;
  }
  void copy(const exact_sum& o)
  {
    plain_ = o.plain_;
    overflow_ = o.overflow_;
    if(o.n_ > inplace)
      grow(o.n_);
    std::copy(o.data(), o.data() + o.n_, data());
    n_ = o.n_;
  }
  void take(exact_sum& o)
  {
    plain_ = o.plain_;
    overflow_ = o.overflow_;
    n_ = o.n_;
    if(o.big_) {
      big_ = o.big_;
      cap_ = o.cap_;
      o.big_ = nullptr;
      o.cap_ = 0;
    } else {
      std::copy(o.small_, o.small_ + o.n_, small_);
    }
    o.n_ = 0;
  }

  double small_[inplace];
  double* big_ = nullptr;
  double plain_ = 0.;      // used only after an overflow
  std::uint32_t n_ = 0;
  std::uint32_t cap_ = 0;
  bool overflow_ = false;

};


}


#endif // EXACTSUM_H
