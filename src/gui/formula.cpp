/*
    QSapecNG - Qt based SapecNG GUI front-end
    Copyright (C) 2009, Michele Caini
    Copyright (C) 2026, Wojciech Kaczmarski

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


#include "gui/formula.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>


namespace qsapecng
{


namespace formula
{


namespace
{


typedef sapecng::metacircuit::basic_expression Term;
typedef sapecng::metacircuit::degree_expression Group;


Expr withoutZeros(const Expr& e)
{
  Expr out;
  for(const auto& d : e) {
    Group g;
    for(const Term& t : d.second)
      if(t.first != 0.)
        g.push_back(t);
    if(!g.empty())
      out[d.first] = g;
  }
  return out;
}


Expr constant(double v)
{
  Expr e;
  e[0].push_back(Term(v, sapecng::symbol_list()));
  return e;
}


// numeric value when the expression is one plain number
bool numericConstant(const Expr& e, double* v)
{
  if(e.size() != 1 || e.begin()->first != 0)
    return false;
  const Group& g = e.begin()->second;
  if(g.size() != 1 || !g.front().second.empty())
    return false;
  *v = g.front().first;
  return true;
}


// 1e-06 -> mantissa/exponent pieces; plain numbers stay as they are
QString number(double v, Style style)
{
  QString n = QString::number(v, 'g', 6);
  const int e = n.indexOf(QLatin1Char('e'));
  if(e < 0)
    return n;

  const QString mant = n.left(e);
  const int exp = n.mid(e + 1).toInt();
  switch(style) {
  case Style::Text:
    return n;
  case Style::Html:
    return (mant == QLatin1String("1") ? QString() : mant + QStringLiteral("·"))
      + QStringLiteral("10<sup>%1</sup>").arg(exp < 0
          ? QStringLiteral("−%1").arg(-exp) : QString::number(exp));
  case Style::Latex:
    return (mant == QLatin1String("1") ? QString()
             : mant + QStringLiteral(" \\cdot "))
      + QStringLiteral("10^{%1}").arg(exp);
  }
  return n;
}


QString symbol(const std::string& name, Style style)
{
  const QString s = QString::fromStdString(name);
  switch(style) {
  case Style::Text:
    return s;
  case Style::Html:
    return s.toHtmlEscaped();
  case Style::Latex:
    {
      static const QRegularExpression indexed(
        QStringLiteral("^([A-Za-z]+)(\\d+)$"));
      const QRegularExpressionMatch m = indexed.match(s);
      if(m.hasMatch())
        return m.captured(1) + QStringLiteral("_{%1}").arg(m.captured(2));
      if(s.size() > 1)
        return QStringLiteral("\\mathrm{%1}")
          .arg(QString(s).replace(QLatin1Char('_'), QStringLiteral("\\_")));
      return s;
    }
  }
  return s;
}


QString times(Style style)
{
  switch(style) {
  case Style::Text:  return QStringLiteral(" * ");
  case Style::Html:  return QStringLiteral(" · ");
  case Style::Latex: return QStringLiteral(" \\cdot ");
  }
  return QStringLiteral(" * ");
}


QString power(int k, Style style)
{
  QString s;
  switch(style) {
  case Style::Text:
    s = QStringLiteral("s");
    if(k > 1) s += QStringLiteral("^%1").arg(k);
    break;
  case Style::Html:
    s = QStringLiteral("<i>s</i>");
    if(k > 1) s += QStringLiteral("<sup>%1</sup>").arg(k);
    break;
  case Style::Latex:
    s = QStringLiteral("s");
    if(k > 1) s += QStringLiteral("^{%1}").arg(k);
    break;
  }
  return s;
}


QString minus(Style style)
{
  return style == Style::Html ? QStringLiteral("−") : QStringLiteral("-");
}


// |term| (sign handled by the caller); 'more' says whether something
// (a power of s) multiplies it, so a lone 1 can be left out
QString magnitude(const Term& t, bool more, Style style)
{
  QStringList parts;
  const double a = std::fabs(t.first);
  if(a != 1. || (t.second.empty() && !more))
    parts << number(a, style);
  for(const std::string& name : t.second)
    parts << symbol(name, style);
  return parts.join(times(style));
}


// a signed list of summands: ("+"/"-", text)
typedef QList< QPair<bool, QString> > Summands;   // bool: negative


QString join(const Summands& summands, Style style)
{
  QString out;
  for(int i = 0; i < summands.size(); ++i) {
    const bool neg = summands.at(i).first;
    if(i == 0)
      out += neg ? minus(style) : QString();
    else
      out += neg ? QStringLiteral(" %1 ").arg(minus(style))
                 : QStringLiteral(" + ");
    out += summands.at(i).second;
  }
  return out;
}


}


Fraction simplified(const Expr& num, const Expr& den,
    const std::set<std::string>& factorable)
{
  Fraction f;
  f.num = withoutZeros(num);
  f.den = withoutZeros(den);

  double d = 0.;
  if(numericConstant(f.den, &d) && d != 0. && d != 1.) {
    for(auto& g : f.num)
      for(Term& t : g.second)
        t.first /= d;
    f.den = constant(1.);
  }

  // a negative denominator constant moves its sign to the numerator
  if(numericConstant(f.den, &d) && d == -1.) {
    for(auto& g : f.num)
      for(Term& t : g.second)
        t.first = -t.first;
    f.den = constant(1.);
  }

  // sources common to every numerator term: H(s) * V1
  if(!f.num.empty()) {
    for(const std::string& name : factorable) {
      bool everywhere = true;
      for(const auto& g : f.num)
        for(const Term& t : g.second)
          if(std::find(t.second.begin(), t.second.end(), name)
              == t.second.end())
            everywhere = false;
      if(!everywhere)
        continue;
      for(auto& g : f.num)
        for(Term& t : g.second)
          t.second.erase(std::find(t.second.begin(), t.second.end(), name));
      f.factor.push_back(name);
    }
  }

  // a denominator with only negative terms: flip both signs
  bool denNegative = !f.den.empty();
  for(const auto& g : f.den)
    for(const Term& t : g.second)
      if(t.first > 0)
        denNegative = false;
  if(denNegative) {
    for(auto& g : f.den)
      for(Term& t : g.second)
        t.first = -t.first;
    for(auto& g : f.num)
      for(Term& t : g.second)
        t.first = -t.first;
  }

  // a minus on every numerator term goes in front
  bool allNegative = !f.num.empty();
  for(const auto& g : f.num)
    for(const Term& t : g.second)
      if(t.first > 0)
        allNegative = false;
  if(allNegative) {
    f.negative = true;
    for(auto& g : f.num)
      for(Term& t : g.second)
        t.first = -t.first;
  }

  return f;
}


bool isOne(const Expr& e)
{
  double v = 0.;
  return numericConstant(withoutZeros(e), &v) && v == 1.;
}


QString render(const Expr& expr, Style style)
{
  const Expr e = withoutZeros(expr);
  if(e.empty())
    return QStringLiteral("0");

  Summands summands;
  // highest power of s first
  for(auto it = e.rbegin(); it != e.rend(); ++it) {
    const int k = it->first;
    const Group& g = it->second;

    if(g.size() == 1) {
      const Term& t = g.front();
      QString body = magnitude(t, k > 0, style);
      if(k > 0)
        body += (body.isEmpty() ? QString() : times(style)) + power(k, style);
      summands.append(qMakePair(t.first < 0, body));
    } else if(k == 0) {
      // a constant sum needs no parentheses: its terms join the others
      for(const Term& t : g)
        summands.append(qMakePair(t.first < 0, magnitude(t, false, style)));
    } else {
      Summands inner;
      for(const Term& t : g)
        inner.append(qMakePair(t.first < 0, magnitude(t, false, style)));
      const QString open = style == Style::Latex
        ? QStringLiteral("\\left(") : QStringLiteral("(");
      const QString close = style == Style::Latex
        ? QStringLiteral("\\right)") : QStringLiteral(")");
      summands.append(qMakePair(false,
        open + join(inner, style) + close + times(style) + power(k, style)));
    }
  }

  return join(summands, style);
}


bool isConstant(const Expr& e)
{
  for(const auto& g : withoutZeros(e))
    if(g.first != 0)
      return false;
  return true;
}


bool isSum(const Expr& e)
{
  int terms = 0;
  for(const auto& g : withoutZeros(e))
    terms += int(g.second.size());
  return terms > 1;
}


QString renderFactor(const Fraction& f, Style style)
{
  QStringList parts;
  for(const std::string& name : f.factor)
    parts << symbol(name, style);
  return parts.join(times(style));
}


QString clipboardText(const Fraction& f, Style style)
{
  const QString factor = renderFactor(f, style);
  const QString sign = f.negative ? QStringLiteral("-") : QString();
  const bool plainNum = isOne(f.num) && !factor.isEmpty();
  QString n = render(f.num, style);

  if(style == Style::Latex) {
    QString body;
    if(isOne(f.den))
      // a sum times the input, or negated, in parentheses
      body = plainNum ? QString()
        : (isSum(f.num) && (!factor.isEmpty() || f.negative))
          ? QStringLiteral("\\left(%1\\right)").arg(n) : n;
    else
      body = QStringLiteral("\\frac{%1}{%2}").arg(n, render(f.den, style));
    if(!factor.isEmpty())
      body += (body.isEmpty() ? QString() : QStringLiteral(" ")) + factor;
    return sign + body;
  }

  // Text: parentheses around sums only
  auto wrap = [](const Expr& e, const QString& t) {
    int terms = 0;
    for(const auto& g : withoutZeros(e))
      terms += int(g.second.size());
    return terms > 1 ? QStringLiteral("(%1)").arg(t) : t;
  };
  QString body;
  if(isOne(f.den)) {
    body = plainNum ? QString() : wrap(f.num, n);
    if(!body.isEmpty())
      body.prepend(sign);
  } else {
    body = wrap(f.num, n) + QStringLiteral(" / ")
      + wrap(f.den, render(f.den, style));
    if(f.negative)
      body = QStringLiteral("-(%1)").arg(body);
  }
  if(!factor.isEmpty()) {
    if(body.isEmpty())
      return sign + factor;
    body += times(style) + factor;
  }
  return body;
}


}


}
