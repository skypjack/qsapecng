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


#ifndef UNITS_H
#define UNITS_H


#include <QString>
#include <QValidator>


namespace qsapecng
{


// Engineering notation for component values.
namespace units
{


// 4700 -> "4.7 k" + unit (one space before the prefix only when there is a
// unit: "4.7 kΩ", "4.7k"). Text that isn't a number is returned unchanged.
QString format(const QString& value, const QString& unit);

// Short form for editing: 4700 -> "4.7k", 1e-06 -> "1µ".
QString formatForEdit(const QString& value);

// Parses "4.7k", "4k7", "100n", "2.2u", "2.2µF", "1e-6", "10 kΩ", "1meg"...
// A trailing unit symbol (when given) is accepted and ignored.
bool parse(const QString& text, const QString& unit, double* value);

// Plain number for the solver and the files, e.g. "4700", "1e-06".
QString toPlain(double value);


// Accepts what parse() accepts (Intermediate while typing).
class ValueValidator: public QValidator
{
public:
  ValueValidator(const QString& unit, QObject* parent)
    : QValidator(parent), unit_(unit) { }
  State validate(QString& input, int& pos) const override;

private:
  QString unit_;
};


}


}


#endif // UNITS_H
