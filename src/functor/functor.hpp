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


#ifndef FUNCTOR_H
#define FUNCTOR_H


#include "model/metacircuit.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <initializer_list>
#include <map>


namespace sapecng
{



struct functor
{

public:
  std::pair< std::vector<double>, std::vector<double> >
  operator()
    (
      const metacircuit::expression& numerator,
      const metacircuit::expression& denominator,
      std::map< std::string, double > values
    )
  {
    std::map< int, double > real_num = synthesis(numerator, values);
    std::map< int, double > real_den = synthesis(denominator, values);
    return op(real_num, real_den);
  }

protected:
  virtual std::pair< std::vector<double>, std::vector<double> >
    op(std::map< int, double > num, std::map< int, double > den) = 0;

private:
  std::map< int, double >
  synthesis(
      const metacircuit::expression& expr,
      std::map< std::string, double > values
    )
  {
    std::map< int, double > syn;

    for(metacircuit::expression::const_iterator
      eit = expr.begin(); eit != expr.end(); ++eit)
    {
      double value = 0.;
      for(metacircuit::degree_expression::const_iterator dit =
        eit->second.begin(); dit != eit->second.end(); ++dit)
      {
        double sub_value = dit->first;
        for(symbol_list::const_iterator
            it = dit->second.begin(); it != dit->second.end(); ++it)
          sub_value *= values[*it]; // 0-default drives from standard C++

        value += sub_value;
      }
      syn[eit->first] += value;
    }

    return syn;
  }

};



struct frequency_range_functor: public functor
{

public:
  // logScale: 'step' is the number of points per decade, spaced
  // logarithmically from range.first to range.second (both > 0);
  // otherwise a linear step in Hz
  frequency_range_functor(std::pair< double, double > range, double step,
      bool logScale = false)
    : range_(range), step_(step), log_(logScale) { }

protected:
  virtual double apply( std::complex<double> v ) = 0;

private:
  std::pair< std::vector<double>, std::vector<double> >
    op(std::map< int, double > num, std::map< int, double > den)
  {
    std::pair< std::vector<double>, std::vector<double> > plot;
    if(log_) {
      if(step_ > 0 && range_.first > 0 && range_.first < range_.second) {
        const double decades = std::log10(range_.second / range_.first);
        const int n = int(std::ceil(decades * step_));
        for(int i = 0; i <= n; ++i) {
          double f = range_.first * std::pow(10., i / step_);
          if(f > range_.second)
            f = range_.second;
          plot.first.push_back(f);
          plot.second.push_back(apply(ratio(num, den, f)));
        }
      }
      return plot;
    }
    if(step_ > 0 && range_.first <= range_.second)
    {
      for(double cnt = range_.first; cnt < range_.second; cnt += step_)
      {
        plot.first.push_back(cnt);
        plot.second.push_back(apply(ratio(num, den, cnt)));
      }
    }

    return plot;
  }

  // num / den at s = j 2 pi f. A big circuit gives high powers of s with
  // huge coefficients (s^28, 1e118): a term alone can pass 1e308 (an
  // infinity, then NaN for the ratio), so every term of both is scaled by
  // the largest one first (by logarithms), which leaves the ratio as it is
  std::complex<double>
  ratio(
      const std::map< int, double >& num,
      const std::map< int, double >& den,
      double f
    )
  {
    const double w = 2. * 4.0 * std::atan(1.0) * f;
    if(!(w > 0.) || !std::isfinite(w))
      return compute(num, f) / compute(den, f);
    const double lw = std::log(w);
    double top = -HUGE_VAL;
    for(const std::map< int, double >* e : { &num, &den })
      for(std::map< int, double >::const_iterator it = e->begin();
          it != e->end(); ++it)
        if(it->second != 0.)
          top = std::max(top, std::log(std::fabs(it->second)) + it->first * lw);
    if(!std::isfinite(top))
      return compute(num, f) / compute(den, f);
    return scaled(num, lw, top) / scaled(den, lw, top);
  }

  // sum of c (j w)^n / e^top, w = e^lw
  static std::complex<double>
  scaled(const std::map< int, double >& expr, double lw, double top)
  {
    std::complex<double> v(0., 0.);
    for(std::map< int, double >::const_iterator it = expr.begin();
        it != expr.end(); ++it)
    {
      if(it->second == 0.)
        continue;
      const int n = it->first;
      double x = std::exp(std::log(std::fabs(it->second)) + n * lw - top);
      if(it->second < 0.)
        x = -x;
      // j^n: 1, j, -1, -j
      switch(((n % 4) + 4) % 4) {
      case 0: v += std::complex<double>(x, 0.); break;
      case 1: v += std::complex<double>(0., x); break;
      case 2: v += std::complex<double>(-x, 0.); break;
      default: v += std::complex<double>(0., -x); break;
      }
    }
    return v;
  }

  std::complex<double>
  compute(
      const std::map< int, double >& expr,
      double step
    )
  {
    std::complex<double> cvalue(0., 0.);
    for(std::map< int, double >::const_iterator it = expr.begin();
      it != expr.end(); ++it)
    {
      bool real = (it->first % 2 == 0);
      double value =
          std::pow(-1., ((it->first - (real ? 0 : 1)) / 2))
        * std::pow(2. * 4.0 * std::atan(1.0) * step, it->first)
        * it->second;

      if(real)
        cvalue += std::complex<double>(value, 0.);
      else
        cvalue += std::complex<double>(0., value);
    }

    return cvalue;
  }

private:
  std::pair< double, double > range_;
  double step_;
  bool log_;

};



struct magnitude: public frequency_range_functor
{

public:
  magnitude(std::pair< double, double > range, double step,
      bool logScale = false)
    : frequency_range_functor(range, step, logScale) { }

private:
  double apply( std::complex<double> v ) { return std::abs(v); }

};



struct phase: public frequency_range_functor
{

public:
  phase(std::pair< double, double > range, double step,
      bool logScale = false)
    : frequency_range_functor(range, step, logScale) { }

private:
  double apply( std::complex<double> v ) { return std::arg(v); }

};



struct gain: public frequency_range_functor
{

public:
  gain(std::pair< double, double > range, double step,
      bool logScale = false)
    : frequency_range_functor(range, step, logScale) { }

private:
  double apply( std::complex<double> v )
    { return (20. * std::log10( std::abs(v) )); }

};



struct loss: public frequency_range_functor
{

public:
  loss(std::pair< double, double > range, double step,
      bool logScale = false)
    : frequency_range_functor(range, step, logScale) { }

private:
  double apply( std::complex<double> v )
    { return (20. * std::log10((1. / std::abs(v) ))); }

};



extern
std::list< std::pair< double, double > >
rpoly_adapter(const std::map< int, double >& p);

struct zeros: public functor
{

private:
  std::pair< std::vector<double>, std::vector<double> >
    op(std::map< int, double > num, std::map< int, double > den)
  {
    std::pair< std::vector<double>, std::vector<double> > ret;

    std::list< std::pair< double, double > > pts = rpoly_adapter(num);
    for(std::list< std::pair< double, double > >::const_iterator
        i = pts.begin(); i != pts.end(); ++i)
    {
      ret.first.push_back(i->first);
      ret.second.push_back(i->second);
    }

    return ret;
  }

};

struct poles: public functor
{

private:
  std::pair< std::vector<double>, std::vector<double> >
    op(std::map< int, double > num, std::map< int, double > den)
  {
    std::pair< std::vector<double>, std::vector<double> > ret;

    std::list< std::pair< double, double > > pts = rpoly_adapter(den);
    for(std::list< std::pair< double, double > >::const_iterator
        i = pts.begin(); i != pts.end(); ++i)
    {
      ret.first.push_back(i->first);
      ret.second.push_back(i->second);
    }

    return ret;
  }

};



}


#endif // FUNCTOR_H
