/*
    QSapecNG - Qt based SapecNG GUI front-end
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


#ifndef FUNCTOR_TRAITS_H
#define FUNCTOR_TRAITS_H


#include "functor/functor.hpp"

#include <qwt_plot_curve.h>
#include <qwt_symbol.h>


namespace qsapecng
{



template < class F >
struct functor_traits { };



template < >
struct functor_traits< sapecng::magnitude >
{
  static const QString title;
  static const QString xLabel;
  static const QString yLabel;
  static const QwtPlotCurve::CurveStyle style;
  static const QwtPlotCurve::CurveAttribute attribute;
  static const QwtSymbol::Style symbol;
  static const int ssize;
  static const QPen pen;
};
const QString
  functor_traits<sapecng::magnitude>::title = QObject::tr("Magnitude");
const QString
  functor_traits<sapecng::magnitude>::xLabel = QObject::tr("Frequency");
const QString
  functor_traits<sapecng::magnitude>::yLabel = QObject::tr("Value");
const QwtPlotCurve::CurveStyle
  functor_traits<sapecng::magnitude>::style = QwtPlotCurve::Lines;
const QwtPlotCurve::CurveAttribute
  functor_traits<sapecng::magnitude>::attribute = QwtPlotCurve::Fitted;
const QwtSymbol::Style
  functor_traits<sapecng::magnitude>::symbol = QwtSymbol::NoSymbol;
const int functor_traits<sapecng::magnitude>::ssize = 0;
const QPen functor_traits<sapecng::magnitude>::pen =
  QPen(Qt::SolidPattern, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);



template < >
struct functor_traits< sapecng::phase >
{
  static const QString title;
  static const QString xLabel;
  static const QString yLabel;
  static const QwtPlotCurve::CurveStyle style;
  static const QwtPlotCurve::CurveAttribute attribute;
  static const QwtSymbol::Style symbol;
  static const int ssize;
  static const QPen pen;
};
const QString functor_traits<sapecng::phase>::title = QObject::tr("Phase");
const QString
  functor_traits<sapecng::phase>::xLabel = QObject::tr("Frequency");
const QString functor_traits<sapecng::phase>::yLabel = QObject::tr("Value");
const QwtPlotCurve::CurveStyle
  functor_traits<sapecng::phase>::style = QwtPlotCurve::Lines;
const QwtPlotCurve::CurveAttribute
  functor_traits<sapecng::phase>::attribute = QwtPlotCurve::Fitted;
const QwtSymbol::Style
  functor_traits<sapecng::phase>::symbol = QwtSymbol::NoSymbol;
const int functor_traits<sapecng::phase>::ssize = 0;
const QPen functor_traits<sapecng::phase>::pen =
  QPen(Qt::SolidPattern, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);



template < >
struct functor_traits< sapecng::gain >
{
  static const QString title;
  static const QString xLabel;
  static const QString yLabel;
  static const QwtPlotCurve::CurveStyle style;
  static const QwtPlotCurve::CurveAttribute attribute;
  static const QwtSymbol::Style symbol;
  static const int ssize;
  static const QPen pen;
};
const QString functor_traits<sapecng::gain>::title = QObject::tr("Gain");
const QString
  functor_traits<sapecng::gain>::xLabel = QObject::tr("Frequency");
const QString functor_traits<sapecng::gain>::yLabel = QObject::tr("dB");
const QwtPlotCurve::CurveStyle
  functor_traits<sapecng::gain>::style = QwtPlotCurve::Lines;
const QwtPlotCurve::CurveAttribute
  functor_traits<sapecng::gain>::attribute = QwtPlotCurve::Fitted;
const QwtSymbol::Style
  functor_traits<sapecng::gain>::symbol = QwtSymbol::NoSymbol;
const int functor_traits<sapecng::gain>::ssize = 0;
const QPen functor_traits<sapecng::gain>::pen =
  QPen(Qt::SolidPattern, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);



template < >
struct functor_traits< sapecng::loss >
{
  static const QString title;
  static const QString xLabel;
  static const QString yLabel;
  static const QwtPlotCurve::CurveStyle style;
  static const QwtPlotCurve::CurveAttribute attribute;
  static const QwtSymbol::Style symbol;
  static const int ssize;
  static const QPen pen;
};
const QString functor_traits<sapecng::loss>::title = QObject::tr("Loss");
const QString
  functor_traits<sapecng::loss>::xLabel = QObject::tr("Frequency");
const QString functor_traits<sapecng::loss>::yLabel = QObject::tr("dB");
const QwtPlotCurve::CurveStyle
  functor_traits<sapecng::loss>::style = QwtPlotCurve::Lines;
const QwtPlotCurve::CurveAttribute
  functor_traits<sapecng::loss>::attribute = QwtPlotCurve::Fitted;
const QwtSymbol::Style
  functor_traits<sapecng::loss>::symbol = QwtSymbol::NoSymbol;
const int functor_traits<sapecng::loss>::ssize = 0;
const QPen functor_traits<sapecng::loss>::pen =
  QPen(Qt::SolidPattern, 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);



template < >
struct functor_traits< sapecng::zeros >
{
  static const QString title;
  static const QString xLabel;
  static const QString yLabel;
  static const QwtPlotCurve::CurveStyle style;
  static const QwtPlotCurve::CurveAttribute attribute;
  static const QwtSymbol::Style symbol;
  static const int ssize;
  static const QPen pen;
};
const QString functor_traits<sapecng::zeros>::title = QObject::tr("Zeros");
const QString
  functor_traits<sapecng::zeros>::xLabel = QObject::tr("Real part");
const QString
  functor_traits<sapecng::zeros>::yLabel = QObject::tr("Img part");
const QwtPlotCurve::CurveStyle
  functor_traits<sapecng::zeros>::style = QwtPlotCurve::NoCurve;
const QwtPlotCurve::CurveAttribute
  functor_traits<sapecng::zeros>::attribute = QwtPlotCurve::Fitted;
const QwtSymbol::Style
  functor_traits<sapecng::zeros>::symbol = QwtSymbol::Ellipse;
const int functor_traits<sapecng::zeros>::ssize = 7;
const QPen functor_traits<sapecng::zeros>::pen = QPen();



template < >
struct functor_traits< sapecng::poles >
{
  static const QString title;
  static const QString xLabel;
  static const QString yLabel;
  static const QwtPlotCurve::CurveStyle style;
  static const QwtPlotCurve::CurveAttribute attribute;
  static const QwtSymbol::Style symbol;
  static const int ssize;
  static const QPen pen;
};
const QString functor_traits<sapecng::poles>::title = QObject::tr("Poles");
const QString
  functor_traits<sapecng::poles>::xLabel = QObject::tr("Real part");
const QString
  functor_traits<sapecng::poles>::yLabel = QObject::tr("Img part");
const QwtPlotCurve::CurveStyle
  functor_traits<sapecng::poles>::style = QwtPlotCurve::NoCurve;
const QwtPlotCurve::CurveAttribute
  functor_traits<sapecng::poles>::attribute = QwtPlotCurve::Fitted;
const QwtSymbol::Style
  functor_traits<sapecng::poles>::symbol = QwtSymbol::XCross;
const int functor_traits<sapecng::poles>::ssize = 7;
const QPen functor_traits<sapecng::poles>::pen = QPen();



}


#endif // FUNCTOR_TRAITS_H
