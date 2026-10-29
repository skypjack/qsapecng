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


#include "gui/units.h"

#include <QRegularExpression>

#include <cmath>


namespace qsapecng
{


namespace units
{


namespace
{


struct Prefix { int exp; const char16_t* symbol; };

const Prefix prefixes[] = {
  { -15, u"f" }, { -12, u"p" }, { -9, u"n" }, { -6, u"µ" },
  { -3, u"m" }, { 0, u"" }, { 3, u"k" }, { 6, u"M" }, { 9, u"G" },
  { 12, u"T" }
};


// "4.7k" style: mantissa and prefix, or false when not a number
bool split(const QString& value, QString* mantissa, QString* prefix)
{
  bool ok = false;
  const double v = value.toDouble(&ok);
  if(!ok)
    return false;

  int exp = 0;
  if(v != 0.) {
    exp = int(std::floor(std::log10(std::fabs(v)) / 3.)) * 3;
    exp = qBound(-15, exp, 12);
  }

  for(const Prefix& p : prefixes)
    if(p.exp == exp)
      *prefix = QString::fromUtf16(p.symbol);

  *mantissa = QString::number(v / std::pow(10., exp), 'g', 4);
  return true;
}


// multiplier of a prefix as typed (u for µ, K for k, meg for M)
bool multiplier(const QString& p, double* m)
{
  static const struct { const char16_t* text; double mult; } table[] = {
    { u"f", 1e-15 }, { u"p", 1e-12 }, { u"n", 1e-9 }, { u"u", 1e-6 },
    { u"µ", 1e-6 }, { u"μ", 1e-6 }, { u"m", 1e-3 }, { u"k", 1e3 },
    { u"K", 1e3 }, { u"M", 1e6 }, { u"meg", 1e6 }, { u"MEG", 1e6 },
    { u"Meg", 1e6 }, { u"G", 1e9 }, { u"T", 1e12 }
  };
  for(const auto& e : table)
    if(p == QString::fromUtf16(e.text)) {
      *m = e.mult;
      return true;
    }
  return false;
}


}


QString format(const QString& value, const QString& unit)
{
  QString mantissa, prefix;
  if(!split(value, &mantissa, &prefix))
    return value;
  if(unit.isEmpty())
    return mantissa + prefix;
  return mantissa + QLatin1Char(' ') + prefix + unit;
}


QString formatForEdit(const QString& value)
{
  QString mantissa, prefix;
  if(!split(value, &mantissa, &prefix))
    return value;
  return mantissa + prefix;
}


bool parse(const QString& text, const QString& unit, double* value)
{
  QString s = text.trimmed();
  s.remove(QLatin1Char(' '));

  // ratios (gains, unit "" or "V/V"): datasheet forms 100dB, 200V/mV,
  // 1e5V/V (or A/A)
  if(unit.isEmpty() || unit == QLatin1String("V/V")) {
    if(s.endsWith(QLatin1String("dB"), Qt::CaseInsensitive)) {
      bool ok = false;
      const double db = s.chopped(2).toDouble(&ok);
      if(!ok)
        return false;
      *value = std::pow(10., db / 20.);
      return std::isfinite(*value);
    }
    static const struct { const char16_t* suffix; double mult; } ratios[] = {
      { u"V/mV", 1e3 }, { u"V/uV", 1e6 }, { u"V/µV", 1e6 }, { u"V/μV", 1e6 },
      { u"V/V", 1. }, { u"A/A", 1. }, { u"A/mA", 1e3 }
    };
    for(const auto& r : ratios) {
      const QString suffix = QString::fromUtf16(r.suffix);
      if(s.endsWith(suffix, Qt::CaseInsensitive) && s.size() > suffix.size()) {
        double v = 0.;
        if(!parse(s.chopped(suffix.size()), QStringLiteral("-"), &v))
          return false;
        *value = v * r.mult;
        return std::isfinite(*value);
      }
    }
  }
  if(!unit.isEmpty() && s.endsWith(unit))
    s.chop(unit.size());
  // ohms are often typed as "R", "ohm" or "Ohm"
  if(unit == QStringLiteral("Ω")) {
    for(const char* alt : { "ohm", "Ohm", "ohms", "Ohms" })
      if(s.endsWith(QLatin1String(alt))) {
        s.chop(int(qstrlen(alt)));
        break;
      }
  }
  if(s.isEmpty())
    return false;

  // plain number, scientific notation allowed
  bool ok = false;
  double v = s.toDouble(&ok);
  if(ok) {
    *value = v;
    return std::isfinite(v);
  }

  static const QRegularExpression re(QStringLiteral(
    "^([-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?)"   // number
    "(meg|MEG|Meg|[fpnuµμmkKMGTR])"                       // prefix
    "(\\d*)$"));                                          // 4k7 digits
  const QRegularExpressionMatch m = re.match(s);
  if(!m.hasMatch())
    return false;

  QString number = m.captured(1);
  const QString prefix = m.captured(2);
  const QString tail = m.captured(3);

  double mult = 1.;
  if(prefix == QLatin1String("R")) {
    // "4R7" = 4.7 (resistor notation); "R" alone means ohms
    if(unit != QStringLiteral("Ω"))
      return false;
  } else if(!multiplier(prefix, &mult)) {
    return false;
  }

  if(!tail.isEmpty()) {
    // "4k7": the digits after the prefix are the decimals
    if(number.contains(QLatin1Char('.')) || number.contains(QLatin1Char('e'))
        || number.contains(QLatin1Char('E')))
      return false;
    number += QLatin1Char('.') + tail;
  }

  v = number.toDouble(&ok);
  if(!ok)
    return false;
  *value = v * mult;
  return std::isfinite(*value);
}


QString toPlain(double value)
{
  return QString::number(value, 'g', 12);
}


QValidator::State ValueValidator::validate(QString& input, int&) const
{
  double v;
  if(parse(input, unit_, &v))
    return Acceptable;
  return Intermediate;
}


}


}
