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


#include <algorithm>
#include <cmath>
#include <complex>
#include <list>
#include <map>
#include <vector>


extern int rpoly(double *op, int degree, double *zeror, double *zeroi);


namespace sapecng
{


namespace
{


typedef std::complex<double> cplx;


// [ QSapecNG ] All the roots of sum a[k] x^k (a[degree] != 0, a[0] != 0)
// by the Aberth-Ehrlich method, for when rpoly fails: the roots of a big
// circuit spread over many decades. The starting points lie on circles
// of the sizes the Newton polygon gives (the upper hull of the points
// (k, log |a[k]|)), and every value of the polynomial is summed scaled by
// its largest term (by logarithms), so nothing overflows. False when it
// does not converge.
bool aberth(const std::vector<double>& a, std::vector<cplx>& roots)
{
  const int n = int(a.size()) - 1;
  std::vector<double> la(n + 1, -HUGE_VAL);
  for(int k = 0; k <= n; ++k)
    if(a[k] != 0.)
      la[k] = std::log(std::fabs(a[k]));

  // Newton polygon: the upper convex hull of (k, la[k])
  std::vector<int> hull;
  for(int k = 0; k <= n; ++k) {
    if(!std::isfinite(la[k]))
      continue;
    while(hull.size() >= 2) {
      const int i = hull[hull.size() - 2], j = hull.back();
      // j under the segment i - k: not on the hull
      if((la[j] - la[i]) * (k - i) <= (la[k] - la[i]) * (j - i))
        hull.pop_back();
      else
        break;
    }
    hull.push_back(k);
  }
  // each hull edge from i to j: j - i roots of size e^((la[i] - la[j]) / (j - i))
  roots.clear();
  const double pi = 4. * std::atan(1.);
  for(std::size_t h = 1; h < hull.size(); ++h) {
    const int i = hull[h - 1], j = hull[h];
    const double r = std::exp((la[i] - la[j]) / (j - i));
    for(int m = 0; m < j - i; ++m) {
      const double angle = 2. * pi * (m + 0.25) / (j - i) + 0.4 * h + 0.1;
      roots.push_back(std::polar(r, angle));
    }
  }
  if(int(roots.size()) != n)
    return false;

  // p(z) / p'(z), the sums scaled by the largest term
  auto newton = [&](cplx z) {
      const double lz = std::log(std::abs(z)), az = std::arg(z);
      double top = -HUGE_VAL;
      for(int k = 0; k <= n; ++k)
        if(std::isfinite(la[k]))
          top = std::max(top, la[k] + k * lz);
      cplx p(0.), d(0.);
      for(int k = 0; k <= n; ++k) {
        if(!std::isfinite(la[k]))
          continue;
        const cplx t = std::polar(std::exp(la[k] + k * lz - top), k * az)
          * (a[k] < 0. ? -1. : 1.);
        p += t;
        d += double(k) * t;   // z p'(z), scaled the same way
      }
      return z * p / d;
    };

  std::vector<bool> done(n, false);
  for(int iteration = 0; iteration < 1000; ++iteration) {
    bool all = true;
    for(int i = 0; i < n; ++i) {
      if(done[i])
        continue;
      const cplx w = newton(roots[i]);
      if(!std::isfinite(w.real()) || !std::isfinite(w.imag()))
        return false;
      cplx sum(0.);
      for(int j = 0; j < n; ++j)
        if(j != i)
          sum += 1. / (roots[i] - roots[j]);
      const cplx step = w / (1. - w * sum);
      roots[i] -= step;
      if(std::abs(step) <= 1e-14 * std::abs(roots[i]))
        done[i] = true;
      else
        all = false;
    }
    if(all)
      break;
  }
  for(int i = 0; i < n; ++i)
    if(!std::isfinite(roots[i].real()) || !std::isfinite(roots[i].imag()))
      return false;
  // a real polynomial: real roots exactly real
  for(cplx& z : roots)
    if(std::fabs(z.imag()) <= 1e-12 * std::abs(z))
      z = cplx(z.real(), 0.);
  return true;
}


}


std::list< std::pair< double, double > >
rpoly_adapter(const std::map< int, double >& p)
{
  std::list< std::pair< double, double > > zeros;

  // only the non-zero coefficients count
  std::map< int, double > q;
  for(std::map< int, double >::const_iterator it = p.begin();
      it != p.end(); ++it)
    if(it->second != 0.)
      q[it->first] = it->second;

  // a constant (or zero) polynomial has no roots
  if(q.empty())
    return zeros;

  // s^k factor: k roots at the origin
  const int lowest = q.begin()->first;
  for(int i = 0; i < lowest; ++i)
    zeros.push_back(std::make_pair<double, double>(0, 0));

  const int degree = q.rbegin()->first - lowest;
  if(degree < 1)
    return zeros;

  double* op = new double [degree + 1];
  double* zeror = new double [degree + 1];
  double* zeroi = new double [degree + 1];

  // s = sigma x, sigma the geometric mean of the roots' sizes: the
  // coefficients of a big circuit span more than a double can hold (1 to
  // 1e300 and beyond: rpoly overflowed, and looped for ever); in x they
  // are of like size. Worked out by logarithms, then scaled to the
  // largest; roots of x come back times sigma
  const double lowLog = std::log(std::fabs(q.begin()->second));
  const double highLog = std::log(std::fabs(q.rbegin()->second));
  const double logSigma = (lowLog - highLog) / degree;
  std::vector<double> logs(degree + 1, 0.);
  double top = -HUGE_VAL;
  for(int i = 0; i <= degree; ++i) {
    std::map< int, double >::const_iterator it = q.find(i + lowest);
    if(it == q.end())
      continue;
    logs[i] = std::log(std::fabs(it->second)) + i * logSigma;
    top = std::max(top, logs[i]);
  }

  // highest power first, of the polynomial divided by s^lowest
  for(int i = degree; i >= 0; --i) {
    std::map< int, double >::const_iterator it = q.find(i + lowest);
    double c = 0.;
    if(it != q.end()) {
      c = std::exp(logs[i] - top);   // 0 when negligibly small
      if(it->second < 0.)
        c = -c;
    }
    op[degree - i] = c;
  }

  const int ret = rpoly(op, degree, zeror, zeroi);   // -1: failure

  const double sigma = std::exp(logSigma);
  if(ret == degree) {
    for(int i = 0; i < ret; ++i)
      zeros.push_back(std::make_pair(zeror[i] * sigma, zeroi[i] * sigma));
  } else {
    // rpoly gave up (roots over many decades): Aberth-Ehrlich, on the
    // coefficients as they are (it scales every sum itself)
    std::vector<double> a(degree + 1, 0.);
    for(int i = 0; i <= degree; ++i) {
      std::map< int, double >::const_iterator it = q.find(i + lowest);
      if(it != q.end())
        a[i] = it->second;
    }
    std::vector<cplx> roots;
    if(aberth(a, roots))
      for(const cplx& z : roots)
        zeros.push_back(std::make_pair(z.real(), z.imag()));
  }

  delete [] zeroi;
  delete [] zeror;
  delete [] op;

  return zeros;
}


}
