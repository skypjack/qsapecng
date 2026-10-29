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


#include "gui/theme.h"
#include "model/metacircuit.h"
#include "functor/functor.hpp"
#include "gui/workplane/workplane.h"
#include "gui/units.h"
#include <algorithm>
#include <QSplitter>
#include <QWheelEvent>
#include <QDoubleValidator>
#include <QGroupBox>
#include "config.h"
#include <cmath>
#include <QSpinBox>
#include <QSettings>
#include <QDialogButtonBox>
#include <QDialog>
#include <QCheckBox>
#include "gui/functor/functor_traits.hpp"
#include "gui/qlogger.h"
#include "gui/settings.h"

#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QFontMetrics>
#include <QScrollBar>
#include <qwt_scale_widget.h>
#include <qwt_scale_draw.h>
#include <QHeaderView>
#include <QFrame>

#include <QEvent>
#include <QApplication>
#include <QTimer>
#include <complex>
#include <qwt_plot_item.h>
#include <QPainterPath>
#include <QPainter>
#include <qwt_plot_grid.h>
#include <qwt_plot_canvas.h>
#include <qwt_plot_picker.h>
#include <qwt_plot_marker.h>
#include <qwt_plot_renderer.h>
#include <qwt_plot_panner.h>
#include <qwt_scale_engine.h>
#include <qwt_scale_div.h>
#include <qwt_painter.h>
#include <qwt_curve_fitter.h>
#include <qwt_weeding_curve_fitter.h>
#include <qwt_text.h>
#include <qwt_picker_machine.h>
#include <qwt_symbol.h>
#include <qwt_scale_map.h>
#include <qwt_text.h>

#include <map>
#include <limits>


namespace qsapecng
{


MarkableCurve::~MarkableCurve()
{
  marker_->detach();
  delete marker_;
  // delete symbol_;
}


void MarkableCurve::setVisible(bool on)
{
  marker_->setVisible(false);

  if(on) marker_->attach(plot());
  else marker_->detach();

  QwtPlotCurve::setVisible(on);
}


int MarkableCurve::nearestSample(const QPointF& pos) const
{
  if(!data() || data()->size() == 0)
    return -1;
  std::size_t idx;
  for(idx = 0; idx < (data()->size() - 1)
      && data()->sample(idx + 1).x() < pos.x(); ++idx);
  // the closer of the two samples around pos
  if(idx + 1 < data()->size()
      && qAbs(data()->sample(idx + 1).x() - pos.x())
         < qAbs(data()->sample(idx).x() - pos.x()))
    ++idx;
  return int(idx);
}


QString MarkableCurve::labelFor(int idx) const
{
  const QPointF p = data()->sample(idx);
  const QString x = units::format(QString::number(p.x(), 'g', 8), xUnit_);
  QString y = QString::number(p.y(), 'g', 4);
  if(!yUnit_.isEmpty())
    y += QLatin1Char(' ') + yUnit_;
  return x + QLatin1Char('\n') + y;
}


void MarkableCurve::placeLabel(QwtPlotMarker* marker, int idx) const
{
  const QPointF p = data()->sample(idx);
  marker->setValue(p);

  QwtText text(labelFor(idx));
  text.setColor(QApplication::palette().color(QPalette::Text));
  QColor bg = QApplication::palette().color(QPalette::Base);
  bg.setAlpha(220);
  text.setBackgroundBrush(bg);
  text.setBorderRadius(3);
  marker->setLabel(text);

  // keep the label inside the plot: towards the middle
  const Qt::Alignment h =
      (p.x() >= minXValue() + (maxXValue() - minXValue()) / 2)
    ? Qt::AlignLeft : Qt::AlignRight;
  const Qt::Alignment v =
      (p.y() >= minYValue() + (maxYValue() - minYValue()) / 2)
    ? Qt::AlignBottom : Qt::AlignTop;
  marker->setLabelAlignment(h | v);
}


void MarkableCurve::selected(const QPointF&)
{
  // the live readout ends; WorkPlane::plotClicked places markers
  marker_->setVisible(false);
  if(plot())
    plot()->replot();
}


void MarkableCurve::appended(const QPointF& pos)
{
  if(isVisible()) {
    marker_->setVisible(true);
    moved(pos);
  }
}


void MarkableCurve::moved(const QPointF& pos)
{
  if(isVisible()) {
    const int idx = nearestSample(pos);
    if(idx < 0)
      return;
    placeLabel(marker_, idx);
    marker_->itemChanged();
  }
}


void MarkableCurve::initialize()
{
  symbol_ = new QwtSymbol;
  symbol_->setStyle(QwtSymbol::Ellipse);
  symbol_->setSize(QSize(5, 5));

  marker_ = new QwtPlotMarker;
  marker_->setLineStyle(QwtPlotMarker::NoLine);
  marker_->setSymbol(symbol_);
  marker_->setVisible(false);
}



MarkedCurve::~MarkedCurve()
{
  resetMarker();
}


void MarkedCurve::setVisible(bool on)
{
  if(!on) {
    resetMarker();
  } else {
    replotMarker();
    foreach(QwtPlotMarker* marker, markers_) {
      marker->setVisible(on);
      marker->attach(plot());
      marker->itemChanged();
    }
  }
}


void MarkedCurve::replotMarker()
{
  resetMarker();

  for(std::size_t i = 0; i < data()->size(); ++i) {
    QwtPlotMarker* marker = new QwtPlotMarker;
    marker->setLineStyle(QwtPlotMarker::NoLine);
    marker->setLabelAlignment(Qt::AlignBottom | Qt::AlignRight);
    marker->setValue(data()->sample(i).x(), data()->sample(i).y());

    // s = -1.2k or -500 + j3.1k (rad/s)
    const double re = data()->sample(i).x(), im = data()->sample(i).y();
    auto si = [](double v) {
        return units::format(QString::number(v, 'g', 4), QString()); };
    QString label = si(re);
    if(im != 0.)
      label += (im > 0 ? QStringLiteral(" + j") : QStringLiteral(" − j"))
        + si(std::fabs(im));
    QwtText text(label);
    text.setColor(QApplication::palette().color(QPalette::Text));
    marker->setLabel(text);

    markers_.push_back(marker);
  }
}


void MarkedCurve::resetMarker()
{
  foreach(QwtPlotMarker* marker, markers_) {
    marker->detach();
    delete marker;
  }

  markers_.clear();
}



namespace
{


// Modern, palette-aware styling for the analysis plot: frameless canvas,
// low-contrast grid, antialiased curves in the accent colour and clearly
// distinguishable zero (o) / pole (x) markers.
void applyPlotStyle(
    const QPalette& pal,
    QwtPlot* plot,
    QwtPlotGrid* grid,
    QVector< QPair< QwtPlotCurve*, bool > >& curves,
    int zerosId,
    int polesId
  )
{
  const QColor base = pal.color(QPalette::Base);
  const QColor text = pal.color(QPalette::Text);
  const QColor accent = pal.color(QPalette::Link);
  const QColor poleColor(0xe5, 0x48, 0x4d);

  if(QwtPlotCanvas* canvas = qobject_cast<QwtPlotCanvas*>(plot->canvas())) {
    canvas->setFrameStyle(QFrame::NoFrame);
    QPalette cp = canvas->palette();
    cp.setColor(QPalette::Window, base);
    canvas->setPalette(cp);
    canvas->setAutoFillBackground(true);
  }

  QColor major = text;
  major.setAlpha(50);
  QColor minor = text;
  minor.setAlpha(20);
  grid->enableXMin(true);
  grid->setMajorPen(major, 0, Qt::SolidLine);
  grid->setMinorPen(minor, 0, Qt::SolidLine);

  for(int i = 0; i < curves.size(); ++i) {
    QwtPlotCurve* curve = curves[i].first;
    curve->setRenderHint(QwtPlotItem::RenderAntialiased, WorkPlane::antialiased());

    if(i == zerosId) {
      curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
        Qt::NoBrush, QPen(accent, 2), QSize(10, 10)));
    } else if(i == polesId) {
      curve->setSymbol(new QwtSymbol(QwtSymbol::XCross,
        Qt::NoBrush, QPen(poleColor, 2), QSize(11, 11)));
    } else {
      QPen pen = curve->pen();
      // phase in its own colour, so it reads apart in a Bode plot
      pen.setColor(i == WorkPlane::PHASE || i == WorkPlane::PHASE_RAD
        ? QColor(0xe0, 0x6c, 0x00) : accent);
      pen.setWidthF(2.);
      pen.setCosmetic(true);
      curve->setPen(pen);
    }
  }
}


// The Smith chart's grid: the unit circle, circles of constant resistance
// and arcs of constant reactance (normalized to Z0), labelled; optionally
// the admittance chart over it (the same circles mirrored: constant
// conductance and susceptance), in a colour of its own. A series part moves
// the point along an impedance circle, a shunt part along an admittance one
class SmithGrid: public QwtPlotItem
{

public:
  SmithGrid() { setZ(5); setItemAttribute(QwtPlotItem::AutoScale, false); }

  void setAdmittance(bool on) { admittance_ = on; }

  int rtti() const override { return QwtPlotItem::Rtti_PlotUserItem + 7; }

  void draw(QPainter* painter, const QwtScaleMap& xMap,
    const QwtScaleMap& yMap, const QRectF&) const override
  {
    const QPalette pal = QApplication::palette();
    QColor line = pal.color(QPalette::Text);
    line.setAlpha(70);
    QColor outline = pal.color(QPalette::Text);
    outline.setAlpha(150);
    QColor text = pal.color(QPalette::Text);
    text.setAlpha(170);
    const QColor admit(0x2e, 0xa0, 0x5a, 140);
    QColor admitText = admit;
    admitText.setAlpha(220);

    auto px = [&](double x, double y) {
        return QPointF(xMap.transform(x), yMap.transform(y));
      };
    auto circle = [&](double cx, double cy, double r) {
        return QRectF(px(cx - r, cy + r), px(cx + r, cy - r)).normalized();
      };

    const double values[] = { 0.2, 0.5, 1., 2., 5. };

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPainterPath unit;
    unit.addEllipse(circle(0., 0., 1.));
    painter->setClipPath(unit, Qt::IntersectClip);
    painter->setBrush(Qt::NoBrush);

    // the admittance chart under the impedance one
    for(int pass = admittance_ ? 0 : 1; pass < 2; ++pass) {
      const double s = pass == 0 ? -1. : 1.;   // mirrored for admittance
      QPen pen(pass == 0 ? admit : line, 1.);
      pen.setCosmetic(true);
      if(pass == 0)
        pen.setStyle(Qt::DashLine);
      painter->setPen(pen);
      for(double v : values) {
        // constant resistance (conductance)
        painter->drawEllipse(circle(s * v / (1. + v), 0., 1. / (1. + v)));
        // constant reactance (susceptance), both signs
        painter->drawEllipse(circle(s, 1. / v, 1. / v));
        painter->drawEllipse(circle(s, -1. / v, 1. / v));
      }
    }
    QPen axis(line, 1.);
    axis.setCosmetic(true);
    painter->setPen(axis);
    painter->drawLine(px(-1., 0.), px(1., 0.));
    painter->restore();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen rim(outline, 1.5);
    rim.setCosmetic(true);
    painter->setPen(rim);
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(circle(0., 0., 1.));

    // labels: resistance on the axis, reactance around the rim
    QFont font = painter->font();
    font.setPointSizeF(font.pointSizeF() * 0.8);
    painter->setFont(font);
    const QFontMetricsF fm(font);
    auto label = [&](const QPointF& at, const QString& s, const QColor& color,
        int align) {
        const QSizeF size = fm.size(0, s);
        QPointF topLeft = at;
        if(align & Qt::AlignHCenter)
          topLeft.rx() -= size.width() / 2;
        else if(align & Qt::AlignRight)
          topLeft.rx() -= size.width() + 2;
        else
          topLeft.rx() += 2;
        if(align & Qt::AlignVCenter)
          topLeft.ry() -= size.height() / 2;
        else if(align & Qt::AlignTop)
          topLeft.ry() += 1;
        else
          topLeft.ry() -= size.height() + 1;
        painter->setPen(color);
        painter->drawText(QRectF(topLeft, size), Qt::AlignCenter, s);
      };
    auto number = [](double v) { return QString::number(v, 'g', 3); };

    label(px(-1., 0.), QStringLiteral("0"), text, Qt::AlignRight | Qt::AlignVCenter);
    label(px(1., 0.), QStringLiteral("∞"), text, Qt::AlignLeft | Qt::AlignVCenter);
    for(double v : values) {
      const double g = (v - 1.) / (v + 1.);
      label(px(g, 0.), number(v), text, Qt::AlignLeft | Qt::AlignBottom);
      // where z = +-jv meets the rim, a little outside it
      const double d = v * v + 1.;
      const double gx = (v * v - 1.) / d, gy = 2. * v / d;
      for(int sign : { 1, -1 }) {
        const QPointF at = px(gx * 1.07, sign * gy * 1.07);
        label(at, (sign > 0 ? QStringLiteral("+j") : QStringLiteral("−j"))
          + number(v), text, Qt::AlignHCenter | Qt::AlignVCenter);
      }
      if(admittance_) {
        label(px(-g, 0.), number(v), admitText, Qt::AlignRight | Qt::AlignTop);
        for(int sign : { 1, -1 }) {
          const QPointF at = px(-gx * 0.9, sign * gy * 0.9);
          label(at, (sign > 0 ? QStringLiteral("−j") : QStringLiteral("+j"))
            + number(v), admitText, Qt::AlignHCenter | Qt::AlignVCenter);
        }
      }
    }
    painter->restore();
  }

private:
  bool admittance_ = false;

};


}


WorkPlane::WorkPlane(QWidget* parent)
  : QWidget(parent)
{
  QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
  autoRange_ = true;
  fStart_ = 1.;
  fStop_ = 1e5;
  pointsPerDecade_ = 100;

  createMainLayout();
  setupCurves();
  setDirty();

  setLayout(mainLayout_);
  setWindowTitle(tr("WorkPlane"));
  setAttribute(Qt::WA_DeleteOnClose);

}


namespace {

// Qwt rounds the points of a curve to whole pixels on screen, so even an
// antialiased curve came out in steps; off, they keep their fractional
// positions (items drawn without antialiasing are rounded by QPainter
// anyway: grid, axes, marker lines look the same)
const bool noRounding = []() {
    QwtPainter::setRoundingAlignment(false);
    return true;
  }();

}


WorkPlane::~WorkPlane()
{
  // the Markers panel may be in the plot window's dock: it goes with the
  // plot (its buttons act on this plot)
  delete markerPanel_;

  // delete clickPointMachine_;

  for(int i = 0; i < curves_.size(); ++i) {
    // delete curves_[i].first->curveFitter();
    // delete curves_[i].first->symbol();
    delete curves_[i].first;
  }

  attached_.clear();
  curves_.clear();

  grid_->detach();
  delete grid_;
  smithGrid_->detach();
  delete smithGrid_;
  if(smithGhost_) {
    smithGhost_->detach();
    delete smithGhost_;
  }
}


void WorkPlane::setData(
    const std::map<std::string, double>& values,
    const sapecng::metacircuit::expression& numerator,
    const sapecng::metacircuit::expression& denominator
  )
{
  setDirty();

  // the plots use the component values of the schematic
  values_ = values;
  rawNum_ = numerator;
  num_ = numerator;
  den_ = denominator;
  gainScale_ = 1.;
  transducer_ = false;

  if(autoRange_)
    autoFrequencyRange();
}


void WorkPlane::setDirty()
{
  attached_.clear();
  for(int i = 0; i < curves_.size(); ++i) {
    curves_[i].first->setVisible(false);
    curves_[i].first->detach();
    curves_[i].second = false;
  }

  plot_->setTitle(tr("none"));
  plot_->setAxisTitle(QwtPlot::yLeft, tr("none"));
  plot_->setAxisTitle(QwtPlot::xBottom, tr("none"));
  plot_->updateAxes();
  plot_->replot();
}


void WorkPlane::changeEvent(QEvent* event)
{
  const QEvent::Type t = event->type();
  if((t == QEvent::PaletteChange || t == QEvent::ApplicationPaletteChange
      || t == QEvent::StyleChange) && plot_ && grid_)
  {
    // defer until the application has finished re-polishing widgets
    QTimer::singleShot(0, this, [this]() {
      applyPlotStyle(QApplication::palette(), plot_, grid_, curves_, ZEROS, POLES);
      // labels drawn in the old theme's colours (pole/zero values, the
      // live readout): recoloured; the frequency markers are redrawn
      const QPalette pal = QApplication::palette();
      foreach(QwtPlotItem* item, plot_->itemList(QwtPlotItem::Rtti_PlotMarker)) {
        QwtPlotMarker* m = static_cast<QwtPlotMarker*>(item);
        QwtText label = m->label();
        if(label.isEmpty())
          continue;
        label.setColor(pal.color(QPalette::Text));
        if(label.backgroundBrush().style() != Qt::NoBrush) {
          QColor bg = pal.color(QPalette::Base);
          bg.setAlpha(220);
          label.setBackgroundBrush(bg);
        }
        m->setLabel(label);
      }
      refreshMarkers();
    });
  }
  QWidget::changeEvent(event);
}


void WorkPlane::applyAxisScales(bool forceLinear)
{
  // A pole/zero map has (mostly negative) real parts on the x axis, so it
  // is always drawn with linear axes; frequency plots use the user's choice.
  if(xLog_ && !forceLinear)
    plot_->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine(10));
  else
    plot_->setAxisScaleEngine(QwtPlot::xBottom, new QwtLinearScaleEngine);

  // dB, phase and pole/zero values can be negative: log only for a
  // linear magnitude
  const bool linearMagnitude = !magnitudeDb_
    && (kind_ == MagnitudeKind || kind_ == BodeKind);
  if(yLog_ && !forceLinear && linearMagnitude)
    plot_->setAxisScaleEngine(QwtPlot::yLeft, new QwtLogScaleEngine(10));
  else
    plot_->setAxisScaleEngine(QwtPlot::yLeft, new QwtLinearScaleEngine);
}


void WorkPlane::xAxisLogScale(bool log)
{
  xLog_ = log;
  applyAxisScales(isMap(lastId_));
  plot_->updateAxes();
  plot_->replot();
}


void WorkPlane::yAxisLogScale(bool log)
{
  yLog_ = log;
  applyAxisScales(isMap(lastId_));
  plot_->updateAxes();
  plot_->replot();
}


void WorkPlane::plot(WorkPlane::F f)
{
  if(f == NOOP) {
    setDirty();
    return;
  }

  setEnabled(false);

  lastId_ = f;
  if(!keepAttached_) {
    if(!attached_.isEmpty()) {
      foreach(QwtPlotCurve* curve, attached_) {
        curve->setVisible(false);
        curve->detach();
      }
      attached_.clear();
    }
    // a single plot: everything on the left axis, autoscaled
    for(int i = 0; i < curves_.size(); ++i)
      curves_[i].first->setYAxis(QwtPlot::yLeft);
    plot_->enableAxis(QwtPlot::yRight, false);
    // drop the Bode plot's coloured axis titles (setAxisTitle(QString)
    // keeps the previous text colour)
    plot_->setAxisTitle(QwtPlot::yLeft, QwtText());
    plot_->setAxisTitle(QwtPlot::yRight, QwtText());
    plot_->setAxisAutoScale(QwtPlot::xBottom);
    plot_->setAxisAutoScale(QwtPlot::yLeft);
    // the Smith chart has its own grid and no axes
    const bool smith = (f == SMITH);
    plot_->enableAxis(QwtPlot::xBottom, !smith);
    plot_->enableAxis(QwtPlot::yLeft, !smith);
    grid_->setVisible(!smith);
    if(smith) {
      static_cast<SmithGrid*>(smithGrid_)->setAdmittance(admittance_);
      smithGrid_->attach(plot_);
    } else
      smithGrid_->detach();
    if(smithGhost_)
      smithGhost_->detach();
    tracker_->setRubberBand(smith || f == ZEROS || f == POLES
      ? QwtPicker::NoRubberBand : QwtPicker::VLineRubberBand);
  }
  applyAxisScales(isMap(f));

  switch(f)
  {
  case MAGNITUDE:
  case MAGNITUDE_RAD:
    {
      std::pair< std::vector<double>, std::vector<double> > res;
      if(!curves_[f].second) {
        QLogger::info(QObject::tr("Calculate magnitude."));
        std::map<std::string, double> values = actValues();
        res = sapecng::magnitude(
            std::make_pair(fStart_, fStop_), sweepPoints(), true
          )(num_, den_, values);
      }

      plot_->setTitle(functor_traits<sapecng::magnitude>::title);
      plot_->setAxisTitle(
        QwtPlot::yLeft, functor_traits<sapecng::magnitude>::yLabel);

      if(f == MAGNITUDE)
        plot_->setAxisTitle(
          QwtPlot::xBottom, functor_traits<sapecng::magnitude>::xLabel);
      else if(f == MAGNITUDE_RAD)
        plot_->setAxisTitle(QwtPlot::xBottom,
          functor_traits<sapecng::magnitude>::xLabel + tr(" (rad/s)"));

      setupCurve(res, MAGNITUDE);
      std::vector<double> rad = res.first;
      for(std::vector<double>::iterator i = rad.begin(); i < rad.end(); ++i)
        *i = 2. * 4. * std::atan(1.) * *i;
      res.first = rad;
      setupCurve(res, MAGNITUDE_RAD);

      curves_[f].first->attach(plot_);
      curves_[f].first->setVisible(true);
      attached_.push_back(curves_[f].first);

      break;
    }
  case PHASE:
  case PHASE_RAD:
    {
      std::pair< std::vector<double>, std::vector<double> > res;
      if(!curves_[f].second) {
        QLogger::info(QObject::tr("Calculate phase."));
        std::map<std::string, double> values = actValues();
        res = sapecng::phase(
            std::make_pair(fStart_, fStop_), sweepPoints(), true
          )(num_, den_, values);
      }

      plot_->setTitle(functor_traits<sapecng::phase>::title);
      plot_->setAxisTitle(
        QwtPlot::yLeft, functor_traits<sapecng::phase>::yLabel);

      if(f == PHASE)
        plot_->setAxisTitle(
          QwtPlot::xBottom, functor_traits<sapecng::phase>::xLabel);
      else if(f == PHASE_RAD)
        plot_->setAxisTitle(QwtPlot::xBottom,
          functor_traits<sapecng::phase>::xLabel + tr(" (rad/s)"));

      if(phaseDegrees_)
        for(double& v : res.second)
          v *= 180. / (4. * std::atan(1.));
      setupCurve(res, PHASE);
      std::vector<double> rad = res.first;
      for(std::vector<double>::iterator i = rad.begin(); i < rad.end(); ++i)
        *i = 2. * 4. * std::atan(1.) * *i;
      res.first = rad;
      setupCurve(res, PHASE_RAD);

      curves_[f].first->attach(plot_);
      curves_[f].first->setVisible(true);
      attached_.push_back(curves_[f].first);

      break;
    }
  case GAIN:
  case GAIN_RAD:
    {
      std::pair< std::vector<double>, std::vector<double> > res;
      if(!curves_[f].second) {
        QLogger::info(QObject::tr("Calculate gain."));
        std::map<std::string, double> values = actValues();
        res = sapecng::gain(
            std::make_pair(fStart_, fStop_), sweepPoints(), true
          )(num_, den_, values);
      }

      plot_->setTitle(functor_traits<sapecng::gain>::title);
      plot_->setAxisTitle(
        QwtPlot::yLeft, functor_traits<sapecng::gain>::yLabel);

      if(f == GAIN)
        plot_->setAxisTitle(
          QwtPlot::xBottom, functor_traits<sapecng::gain>::xLabel);
      else if(f == GAIN_RAD)
        plot_->setAxisTitle(QwtPlot::xBottom,
          functor_traits<sapecng::gain>::xLabel + tr(" (rad/s)"));

      setupCurve(res, GAIN);
      std::vector<double> rad = res.first;
      for(std::vector<double>::iterator i = rad.begin(); i < rad.end(); ++i)
        *i = 2. * 4. * std::atan(1.) * *i;
      res.first = rad;
      setupCurve(res, GAIN_RAD);

      curves_[f].first->attach(plot_);
      curves_[f].first->setVisible(true);
      attached_.push_back(curves_[f].first);

      break;
    }
  case LOSS:
  case LOSS_RAD:
    {
      std::pair< std::vector<double>, std::vector<double> > res;
      if(!curves_[f].second) {
        QLogger::info(QObject::tr("Calculate loss."));
        std::map<std::string, double> values = actValues();
        res = sapecng::loss(
            std::make_pair(fStart_, fStop_), sweepPoints(), true
          )(num_, den_, values);
      }

      plot_->setTitle(functor_traits<sapecng::loss>::title);
      plot_->setAxisTitle(
        QwtPlot::yLeft, functor_traits<sapecng::loss>::yLabel);

      if(f == LOSS)
        plot_->setAxisTitle(
          QwtPlot::xBottom, functor_traits<sapecng::gain>::xLabel);
      else if(f == LOSS_RAD)
        plot_->setAxisTitle(QwtPlot::xBottom,
          functor_traits<sapecng::gain>::xLabel + tr(" (rad/s)"));

      setupCurve(res, LOSS);
      std::vector<double> rad = res.first;
      for(std::vector<double>::iterator i = rad.begin(); i < rad.end(); ++i)
        *i = 2. * 4. * std::atan(1.) * *i;
      res.first = rad;
      setupCurve(res, LOSS_RAD);

      curves_[f].first->attach(plot_);
      curves_[f].first->setVisible(true);
      attached_.push_back(curves_[f].first);

      break;
    }
  case ZEROS:
  case POLES:
    {
      std::pair< std::vector<double>, std::vector<double> > resZ;
      if(!curves_[ZEROS].second) {
        QLogger::info(QObject::tr("Looking for zeros."));
        std::map<std::string, double> values = actValues();
        resZ = sapecng::zeros()(num_, den_, values);

        unsigned int size = resZ.first.size() > resZ.second.size() ?
          resZ.second.size() : resZ.first.size();

        for(unsigned int i = 0; i < size; ++i)
          QLogger::info(QObject::tr("Zero found near to ")
            + QString("(%1, %2)")
              .arg(QString::number(resZ.first.at(i)))
              .arg(QString::number(resZ.second.at(i))));
      }

      std::pair< std::vector<double>, std::vector<double> > resP;
      if(!curves_[POLES].second) {
        QLogger::info(QObject::tr("Looking for poles."));
        std::map<std::string, double> values = actValues();
        resP = sapecng::poles()(num_, den_, values);

        unsigned int size = resP.first.size() > resP.second.size() ?
          resP.second.size() : resP.first.size();

        for(unsigned int i = 0; i < size; ++i)
          QLogger::info(QObject::tr("Pole found near to ")
            + QString("(%1, %2)")
              .arg(QString::number(resP.first.at(i)))
              .arg(QString::number(resP.second.at(i))));
      }

      plot_->setTitle(
            functor_traits<sapecng::zeros>::title
          + "/" + functor_traits<sapecng::poles>::title
        );
      plot_->setAxisTitle(
        QwtPlot::yLeft, functor_traits<sapecng::zeros>::yLabel);
      plot_->setAxisTitle(
        QwtPlot::xBottom, functor_traits<sapecng::zeros>::xLabel);

      setupCurve(resZ, ZEROS);
      setupCurve(resP, POLES);

      curves_[ZEROS].first->attach(plot_);
      curves_[POLES].first->attach(plot_);
      curves_[ZEROS].first->setVisible(true);
      curves_[POLES].first->setVisible(true);
      attached_.push_back(curves_[ZEROS].first);
      attached_.push_back(curves_[POLES].first);

      break;
    }
  case SMITH:
    {
      // the output read as an impedance: Z = H (the output over a 1 A
      // source), Gamma = (Z - Z0) / (Z + Z0)
      if(!curves_[SMITH].second) {
        QLogger::info(QObject::tr("Calculate the reflection coefficient."));
        std::map<std::string, double> values = actValues();
        const std::pair<double, double> range(fStart_, fStop_);
        const auto m = sapecng::magnitude(range, sweepPoints(), true)(
          num_, den_, values);
        const auto ph = sapecng::phase(range, sweepPoints(), true)(
          num_, den_, values);
        std::pair< std::vector<double>, std::vector<double> > res;
        smithHz_.clear();
        const size_t n = qMin(m.second.size(), ph.second.size());
        for(size_t i = 0; i < n; ++i) {
          // the impedance itself, not scaled as a gain
          const std::complex<double> z =
            std::polar(m.second[i] / gainScale_, ph.second[i]);
          const std::complex<double> g = (z - z0_) / (z + z0_);
          if(!std::isfinite(g.real()) || !std::isfinite(g.imag()))
            continue;
          res.first.push_back(g.real());
          res.second.push_back(g.imag());
          smithHz_.append(m.first[i]);
        }
        setupCurve(res, SMITH);
      }
      // an impedance that does not change with frequency (a resistor) is
      // one point: drawn as a dot, a line of no length shows nothing
      {
        QwtPlotCurve* curve = curves_[SMITH].first;
        double span = 0.;
        if(curve->dataSize() > 0) {
          const QPointF first = curve->sample(0);
          for(size_t i = 1; i < curve->dataSize(); ++i)
            span = qMax(span, QLineF(first, curve->sample(i)).length());
        }
        if(curve->dataSize() > 0 && span < 1e-6)
          curve->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
            curve->pen().color(), QPen(curve->pen().color(), 1), QSize(9, 9)));
        else
          curve->setSymbol(nullptr);
      }
      curves_[SMITH].first->attach(plot_);
      curves_[SMITH].first->setVisible(true);
      attached_.push_back(curves_[SMITH].first);
      break;
    }
  default:
    break;
  }

  relabel(f);
  // the frequency axis spans the sweep exactly (the scale engine, left to
  // itself, rounds a narrow sweep out to whole decades)
  if(!isMap(f) && !keepAttached_) {
    const bool rad = f == MAGNITUDE_RAD || f == PHASE_RAD || f == GAIN_RAD
      || f == LOSS_RAD;
    const double k = rad ? 8. * std::atan(1.) : 1.;
    plot_->setAxisScale(QwtPlot::xBottom, fStart_ * k, fStop_ * k);
  }
  if(!keepAttached_) {
    if(f == PHASE || f == PHASE_RAD)
      applyYRange(QwtPlot::yLeft, phaseKey());
    else if(!isMap(f))
      applyYRange(QwtPlot::yLeft, magnitudeKey());
  }

  if(!keepAttached_)
    refreshMarkers();

  plot_->setFocus(Qt::OtherFocusReason);
  plot_->updateAxes();
  plot_->replot();

  setEnabled(true);
}


WorkPlane::F WorkPlane::currentF() const
{
  switch(kind_) {
  case PhaseKind:
    return angular_ ? PHASE_RAD : PHASE;
  case PoleZeroKind:
    return ZEROS;
  case SmithKind:
    return SMITH;
  default:
    if(magnitudeDb_)
      return angular_ ? GAIN_RAD : GAIN;
    return angular_ ? MAGNITUDE_RAD : MAGNITUDE;
  }
}


void WorkPlane::relabel(F f)
{
  // titles and axes with units
  const QString freq = (f == MAGNITUDE_RAD || f == PHASE_RAD
      || f == GAIN_RAD || f == LOSS_RAD)
    ? tr("Angular frequency (rad/s)") : tr("Frequency (Hz)");
  const QString xUnit = (f == MAGNITUDE_RAD || f == PHASE_RAD
      || f == GAIN_RAD || f == LOSS_RAD) ? QStringLiteral("rad/s")
                                         : QStringLiteral("Hz");
  QString title, yTitle, yUnit;

  switch(f) {
  case MAGNITUDE:
  case MAGNITUDE_RAD:
    title = tr("Frequency response");
    yTitle = tr("Magnitude");
    break;
  case GAIN:
  case GAIN_RAD:
    title = tr("Frequency response");
    yTitle = tr("Magnitude (dB)");
    yUnit = QStringLiteral("dB");
    break;
  case LOSS:
  case LOSS_RAD:
    title = tr("Loss");
    yTitle = tr("Loss (dB)");
    yUnit = QStringLiteral("dB");
    break;
  case PHASE:
  case PHASE_RAD:
    title = tr("Phase response");
    yTitle = phaseDegrees_ ? tr("Phase (°)") : tr("Phase (rad)");
    yUnit = phaseDegrees_ ? QStringLiteral("°") : QStringLiteral("rad");
    break;
  case ZEROS:
  case POLES:
    {
      const bool none = curves_[ZEROS].first->dataSize() == 0
        && curves_[POLES].first->dataSize() == 0;
      plot_->setTitle(none
        ? tr("No poles or zeros: the transfer function is a constant")
        : tr("Poles (×) and zeros (o)"));

      // centred on the origin, the same range on both axes
      double r = 0.;
      for(F c : { ZEROS, POLES }) {
        const QwtSeriesData<QPointF>* d = curves_[c].first->data();
        for(size_t i = 0; d && i < d->size(); ++i)
          r = qMax(r, qMax(qAbs(d->sample(i).x()), qAbs(d->sample(i).y())));
      }
      r = r > 0 ? r * 1.25 : 1.;
      plot_->setAxisScale(QwtPlot::xBottom, -r, r);
      plot_->setAxisScale(QwtPlot::yLeft, -r, r);
      // the same scale on both axes (a circle stays a circle), once the
      // canvas has its size
      QTimer::singleShot(0, this, [this]() { equalizeAxes(); });
    }
    plot_->setAxisTitle(QwtPlot::xBottom, tr("Real part (rad/s)"));
    plot_->setAxisTitle(QwtPlot::yLeft, tr("Imaginary part (rad/s)"));
    return;
  case SMITH:
    {
      // keep the title's font and colour: only the text (rich: Z0 with a
      // subscript) changes
      QwtText title = plot_->title();
      QString text = tr("Smith chart, Z<sub>0</sub> = %1")
        .arg(units::format(QString::number(z0_, 'g', 6), QStringLiteral("Ω")));
      // the output read as an impedance means something only when a 1 A
      // current source drives the circuit; a voltage source makes it a
      // voltage ratio (a gain), which the chart would show as ohms
      if(!impedance_)
        text += QStringLiteral("<br><span style='font-size: small; "
            "font-weight: normal; color: #c0392b;'>%1</span>")
          .arg(tr("The output is not an impedance: drive the circuit with "
                  "a 1 A current source, not a voltage source"));
      title.setText(text, QwtText::RichText);
      plot_->setTitle(title);
      plot_->setAxisScale(QwtPlot::xBottom, -1.15, 1.15);
      plot_->setAxisScale(QwtPlot::yLeft, -1.15, 1.15);
      QTimer::singleShot(0, this, [this]() { equalizeAxes(); });
    }
    return;
  default:
    return;
  }

  // the load assumed: say so
  if(transducer_ && assumedLoad_ && (f == MAGNITUDE || f == MAGNITUDE_RAD
      || f == GAIN || f == GAIN_RAD))
    title += tr(" (load = source resistance)");
  plot_->setTitle(title);
  plot_->setAxisTitle(QwtPlot::xBottom, freq);
  plot_->setAxisTitle(QwtPlot::yLeft, yTitle);
  if(MarkableCurve* mc = dynamic_cast<MarkableCurve*>(curves_[f].first))
    mc->setUnits(xUnit, yUnit);
}


void WorkPlane::showKind(int kind)
{
  kind_ = kind;
  if(kind_ == BodeKind)
    plotBode();
  else
    plot(currentF());
}


void WorkPlane::plotBode()
{
  // magnitude on the left axis, phase on the right one
  const F phaseF = angular_ ? PHASE_RAD : PHASE;
  plot(currentF());
  const F magF = lastId_;

  keepAttached_ = true;
  plot(phaseF);
  keepAttached_ = false;
  lastId_ = magF;

  curves_[phaseF].first->setYAxis(QwtPlot::yRight);
  plot_->enableAxis(QwtPlot::yRight, true);
  plot_->setAxisAutoScale(QwtPlot::yRight);
  plot_->setAxisScaleEngine(QwtPlot::yRight, new QwtLinearScaleEngine);
  applyAxisScales(false);
  applyYRange(QwtPlot::yLeft, magnitudeKey());
  applyYRange(QwtPlot::yRight, phaseKey());

  relabel(magF);
  refreshMarkers();
  plot_->setTitle(tr("Bode plot"));

  // axis titles in the colour of their curve
  QwtText left = plot_->axisTitle(QwtPlot::yLeft);
  left.setColor(curves_[magF].first->pen().color());
  plot_->setAxisTitle(QwtPlot::yLeft, left);
  QwtText right(phaseDegrees_ ? tr("Phase (°)") : tr("Phase (rad)"));
  right.setColor(curves_[phaseF].first->pen().color());
  right.setFont(left.font());
  plot_->setAxisTitle(QwtPlot::yRight, right);

  plot_->updateAxes();
  plot_->replot();
}


QString WorkPlane::magnitudeKey() const
{
  return magnitudeDb_ ? QStringLiteral("magDb") : QStringLiteral("mag");
}


QString WorkPlane::phaseKey() const
{
  return phaseDegrees_ ? QStringLiteral("phaseDeg") : QStringLiteral("phaseRad");
}


WorkPlane::YRange WorkPlane::yRange(const QString& key) const
{
  // this plot's own (a range set by hand for one circuit would hide the
  // curves of the next one): automatic until set
  YRange r = yRanges_.value(key);
  if(!(r.max > r.min))
    r.automatic = true;
  return r;
}


void WorkPlane::setYRange(const QString& key, const YRange& r)
{
  yRanges_.insert(key, r);
}


void WorkPlane::applyYRange(int axis, const QString& key)
{
  const YRange r = yRange(key);
  if(r.automatic) {
    // the range of the curves rounded out to major ticks, plus one major
    // step above and below: a curve never runs along the frame
    double lo = 0., hi = 0.;
    bool any = false;
    // the samples themselves, finite ones only: at a resonance the
    // magnitude can be infinite, and one such point made the curve's
    // bounding rectangle useless (the peak was cut off)
    for(QwtPlotItem* item : plot_->itemList(QwtPlotItem::Rtti_PlotCurve)) {
      if(!item->isVisible() || item->yAxis() != axis)
        continue;
      const QwtPlotCurve* curve = static_cast<QwtPlotCurve*>(item);
      for(size_t i = 0; i < curve->dataSize(); ++i) {
        const double y = curve->sample(i).y();
        if(!std::isfinite(y))
          continue;
        lo = any ? qMin(lo, y) : y;
        hi = any ? qMax(hi, y) : y;
        any = true;
      }
    }
    if(any && hi == lo) {
      // a flat curve: a little room around it
      lo -= 1.;
      hi += 1.;
    }
    QwtScaleEngine* engine = plot_->axisScaleEngine(axis);
    const bool log = engine && dynamic_cast<QwtLogScaleEngine*>(engine);
    if(!any || !engine || (log && lo <= 0.)) {
      plot_->setAxisAutoScale(axis);
      return;
    }
    double step = 0.;
    engine->autoScale(plot_->axisMaxMajor(axis), lo, hi, step);
    const QwtScaleDiv div = engine->divideScale(lo, hi,
      plot_->axisMaxMajor(axis), plot_->axisMaxMinor(axis), step);
    const QList<double> majors = div.ticks(QwtScaleDiv::MajorTick);
    if(majors.size() >= 2)
      step = log ? std::log10(majors.at(1) / majors.at(0))
                 : majors.at(1) - majors.at(0);
    if(!(step > 0.)) {
      plot_->setAxisAutoScale(axis);
      return;
    }
    if(log) {
      lo /= std::pow(10., step);
      hi *= std::pow(10., step);
    } else {
      lo -= step;
      hi += step;
    }
    plot_->setAxisScale(axis, lo, hi, log ? 0. : step);
  } else
    plot_->setAxisScale(axis, r.min, r.max);
}


void WorkPlane::resetView()
{
  if(lastId_ != NOOP)
    showKind(kind_);   // curves are cached: this only rescales
}


void WorkPlane::editVerticalRange()
{
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Vertical Range"));

  struct Axis {
    QString key, title;
    QCheckBox* automatic; QLineEdit* min; QLineEdit* max;
  };
  QList<Axis> axes;
  if(kind_ == PhaseKind)
    axes << Axis{ phaseKey(), phaseDegrees_ ? tr("Phase (°)") : tr("Phase (rad)") };
  else if(kind_ == BodeKind)
    axes << Axis{ magnitudeKey(),
                  magnitudeDb_ ? tr("Magnitude (dB)") : tr("Magnitude") }
         << Axis{ phaseKey(), phaseDegrees_ ? tr("Phase (°)") : tr("Phase (rad)") };
  else
    axes << Axis{ magnitudeKey(),
                  magnitudeDb_ ? tr("Magnitude (dB)") : tr("Magnitude") };

  QVBoxLayout* layout = new QVBoxLayout(&dialog);
  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  auto validate = [&]() {
    bool ok = true;
    for(const Axis& a : axes) {
      a.min->setEnabled(!a.automatic->isChecked());
      a.max->setEnabled(!a.automatic->isChecked());
      if(!a.automatic->isChecked()) {
        bool okMin = false, okMax = false;
        const double lo = a.min->text().toDouble(&okMin);
        const double hi = a.max->text().toDouble(&okMax);
        ok = ok && okMin && okMax && hi > lo;
      }
    }
    buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
  };

  for(Axis& a : axes) {
    const YRange r = yRange(a.key);
    // automatic: show the range on screen as a starting point
    const int axisId = (&a == &axes.last() && kind_ == BodeKind)
      ? QwtPlot::yRight : QwtPlot::yLeft;
    const QwtInterval shown = plot_->axisInterval(axisId);
    a.automatic = new QCheckBox(tr("&Automatic"));
    a.automatic->setChecked(r.automatic);
    a.min = new QLineEdit(QString::number(r.automatic ? shown.minValue() : r.min, 'g', 6));
    a.max = new QLineEdit(QString::number(r.automatic ? shown.maxValue() : r.max, 'g', 6));
    a.min->setValidator(new QDoubleValidator(a.min));
    a.max->setValidator(new QDoubleValidator(a.max));

    QFormLayout* form = new QFormLayout;
    form->addRow(a.automatic);
    form->addRow(tr("Minimum:"), a.min);
    form->addRow(tr("Maximum:"), a.max);
    QGroupBox* box = new QGroupBox(a.title);
    box->setLayout(form);
    layout->addWidget(box);

    connect(a.automatic, &QCheckBox::toggled, &dialog, validate);
    connect(a.min, &QLineEdit::textChanged, &dialog, validate);
    connect(a.max, &QLineEdit::textChanged, &dialog, validate);
  }

  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  validate();

  if(dialog.exec() != QDialog::Accepted)
    return;

  for(const Axis& a : axes) {
    YRange r;
    r.automatic = a.automatic->isChecked();
    r.min = a.min->text().toDouble();
    r.max = a.max->text().toDouble();
    setYRange(a.key, r);
  }
  resetView();
}


void WorkPlane::equalizeAxes()
{
  // pole-zero map: equal units per pixel on both axes, the larger of the
  // two, around the present centre
  if(!isMap(lastId_))
    return;
  const QWidget* canvas = plot_->canvas();
  const double w = canvas->width(), h = canvas->height();
  if(w < 10 || h < 10)
    return;
  const QwtInterval xi = plot_->axisInterval(QwtPlot::xBottom);
  const QwtInterval yi = plot_->axisInterval(QwtPlot::yLeft);
  const double u = qMax(xi.width() / w, yi.width() / h);
  if(!(u > 0) || !std::isfinite(u))
    return;
  const double cx = (xi.minValue() + xi.maxValue()) / 2;
  const double cy = (yi.minValue() + yi.maxValue()) / 2;
  plot_->setAxisScale(QwtPlot::xBottom, cx - u * w / 2, cx + u * w / 2);
  plot_->setAxisScale(QwtPlot::yLeft, cy - u * h / 2, cy + u * h / 2);
  plot_->replot();
}


bool WorkPlane::eventFilter(QObject* watched, QEvent* event)
{
  if(watched == plot_->canvas() && event->type() == QEvent::Resize
      && isMap(lastId_))
    QTimer::singleShot(0, this, [this]() { equalizeAxes(); });

  if(watched == plot_->canvas() && event->type() == QEvent::Wheel) {
    QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
    const int delta = wheel->angleDelta().y();
    if(delta != 0 && lastId_ != NOOP) {
      // Shift: frequency axis only, Ctrl: vertical axis only
      const bool onlyX = wheel->modifiers() & Qt::ShiftModifier;
      const bool onlyY = wheel->modifiers() & Qt::ControlModifier;
      zoomAt(wheel->position().toPoint(), std::pow(1.2, delta / 120.),
        !onlyY, !onlyX);
    }
    return true;
  }
  return QWidget::eventFilter(watched, event);
}


void WorkPlane::zoomAt(const QPoint& pos, double factor, bool x, bool y)
{
  // zoom in pixel space around the pointer; invTransform takes care of
  // logarithmic axes
  auto zoomAxis = [&](int axis, double c) {
    const QwtScaleMap map = plot_->canvasMap(axis);
    const double p1 = c + (map.p1() - c) / factor;
    const double p2 = c + (map.p2() - c) / factor;
    double a = map.invTransform(p1), b = map.invTransform(p2);
    if(a > b) std::swap(a, b);
    if(std::isfinite(a) && std::isfinite(b) && b > a)
      plot_->setAxisScale(axis, a, b);
  };

  if(x)
    zoomAxis(QwtPlot::xBottom, pos.x());
  if(y) {
    zoomAxis(QwtPlot::yLeft, pos.y());
    if(plot_->isAxisVisible(QwtPlot::yRight))
      zoomAxis(QwtPlot::yRight, pos.y());
  }
  plot_->replot();
}


PlotSettings PlotSettings::load()
{
  QSettings q(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
  PlotSettings p;
  p.kind = q.value("Plot/kind", p.kind).toInt();
  p.magnitudeDb = q.value("Workplane/magnitudeDb", p.magnitudeDb).toBool();
  p.phaseDegrees = q.value("Workplane/phaseDegrees", p.phaseDegrees).toBool();
  p.angular = q.value("Workplane/angularFrequency", p.angular).toBool();
  p.xLog = q.value("Workplane/xLog", p.xLog).toBool();
  p.yLog = q.value("Workplane/yLog", p.yLog).toBool();
  // the sweep is not kept: automatic (see save())
  p.z0 = q.value("Plot/smithZ0", p.z0).toDouble();
  if(!(p.z0 > 0.) || !std::isfinite(p.z0)) p.z0 = 50.;
  p.smithAdmittance = q.value("Plot/smithAdmittance", p.smithAdmittance).toBool();
  p.smithReturnLoss = q.value("Plot/smithReturnLoss", p.smithReturnLoss).toBool();
  if(p.kind < 0 || p.kind > 4) p.kind = 3;
  return p;
}


void PlotSettings::save() const
{
  QSettings q(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
  q.setValue("Plot/kind", kind);
  q.setValue("Workplane/magnitudeDb", magnitudeDb);
  q.setValue("Workplane/phaseDegrees", phaseDegrees);
  q.setValue("Workplane/angularFrequency", angular);
  q.setValue("Workplane/xLog", xLog);
  q.setValue("Workplane/yLog", yLog);
  // the sweep is the plot's own (a sweep for one circuit is wrong for the
  // next): a new plot starts automatic, or with what its file asks for
  q.setValue("Plot/smithZ0", z0);
  q.setValue("Plot/smithAdmittance", smithAdmittance);
  q.setValue("Plot/smithReturnLoss", smithReturnLoss);
}


PlotSettings WorkPlane::settings() const
{
  PlotSettings p;
  p.kind = kind_;
  p.magnitudeDb = magnitudeDb_;
  p.phaseDegrees = phaseDegrees_;
  p.angular = angular_;
  p.xLog = xLog_;
  p.yLog = yLog_;
  p.autoRange = autoRange_;
  p.fStart = fStart_;
  p.fStop = fStop_;
  p.pointsPerDecade = pointsPerDecade_;
  p.z0 = z0_;
  p.smithAdmittance = admittance_;
  p.smithReturnLoss = returnLoss_;
  // the ranges: as set, or (automatic) what is on screen, a starting point
  auto fill = [&](const QString& key, int axis, bool* automatic, double* lo,
      double* hi) {
      const YRange r = yRange(key);
      *automatic = r.automatic;
      const QwtInterval shown = plot_->axisInterval(axis);
      *lo = r.automatic ? shown.minValue() : r.min;
      *hi = r.automatic ? shown.maxValue() : r.max;
    };
  fill(magnitudeKey(), QwtPlot::yLeft, &p.magAuto, &p.magMin, &p.magMax);
  fill(phaseKey(), kind_ == BodeKind ? QwtPlot::yRight : QwtPlot::yLeft,
       &p.phaseAuto, &p.phaseMin, &p.phaseMax);
  p.output = output_;
  return p;
}


void WorkPlane::applySettings(const PlotSettings& p)
{
  const bool sweepChanged = p.autoRange != autoRange_
    || p.fStart != fStart_ || p.fStop != fStop_
    || p.pointsPerDecade != pointsPerDecade_
    || p.phaseDegrees != phaseDegrees_;

  kind_ = p.kind;
  output_ = p.output;
  magnitudeDb_ = p.magnitudeDb;
  phaseDegrees_ = p.phaseDegrees;
  angular_ = p.angular;
  xLog_ = p.xLog;
  yLog_ = p.yLog;
  autoRange_ = p.autoRange;
  fStart_ = p.fStart;
  fStop_ = p.fStop;
  pointsPerDecade_ = p.pointsPerDecade;
  if(p.z0 > 0. && p.z0 != z0_) {
    z0_ = p.z0;
    curves_[SMITH].second = false;
  }
  admittance_ = p.smithAdmittance;
  returnLoss_ = p.smithReturnLoss;
  {
    YRange m, ph;
    m.automatic = p.magAuto || !(p.magMax > p.magMin);
    m.min = p.magMin;
    m.max = p.magMax;
    ph.automatic = p.phaseAuto || !(p.phaseMax > p.phaseMin);
    ph.min = p.phaseMin;
    ph.max = p.phaseMax;
    setYRange(magnitudeKey(), m);
    setYRange(phaseKey(), ph);
  }

  if(autoRange_ && !num_.empty())
    autoFrequencyRange();
  if(sweepChanged)
    invalidateCurves();
  if(!num_.empty())
    showKind(kind_);
}


bool WorkPlane::markerValues(double hz, double* magnitude, double* phase) const
{
  // evaluated from the transfer function itself (not the plotted samples)
  if(num_.empty() || !(hz > 0))
    return false;
  const std::pair<double, double> at(hz, hz * 1.000001);
  std::pair< std::vector<double>, std::vector<double> > m =
    magnitudeDb_ ? sapecng::gain(at, 1., true)(num_, den_, values_)
                 : sapecng::magnitude(at, 1., true)(num_, den_, values_);
  std::pair< std::vector<double>, std::vector<double> > p =
    sapecng::phase(at, 1., true)(num_, den_, values_);
  if(m.second.empty() || p.second.empty())
    return false;
  *magnitude = m.second.front();
  *phase = phaseDegrees_ ? p.second.front() * 180. / (4. * std::atan(1.))
                         : p.second.front();
  return true;
}


int WorkPlane::sweepPoints() const
{
  // points per decade for the sweep: as set, but at most about 100 000
  // points in all (a narrow range around a crystal's resonance wants a
  // great many per decade; a wide one would take long)
  const double decades = std::log10(fStop_ / fStart_);
  if(!(decades > 0.) || !std::isfinite(decades))
    return pointsPerDecade_;
  return qMax(10, qMin(pointsPerDecade_, int(100000. / decades)));
}


QString WorkPlane::hzText(double hz)
{
  // as many digits as the value has (up to 9): units::format keeps 4,
  // which shows 10.0123 MHz as 10.01 MHz
  if(!(std::fabs(hz) > 0.))
    return QStringLiteral("0 Hz");
  const int exp = qBound(-15, int(std::floor(std::log10(std::fabs(hz)) / 3.)) * 3, 12);
  const QString prefix = units::format(
    QString::number(std::pow(10., exp), 'g', 3), QString()).mid(1);
  return QString::number(hz / std::pow(10., exp), 'g', 9) + QLatin1Char(' ')
    + prefix + QStringLiteral("Hz");
}


double WorkPlane::roundedHz(double hz, double resolution)
{
  // a clicked frequency, rounded to the resolution of the click (a pixel,
  // a sample): 1.23 kHz on a wide sweep, 10.0123 MHz on a narrow one
  if(!(resolution > 0.) || !(hz > 0.))
    return hz;
  const double step = std::pow(10., std::floor(std::log10(resolution)));
  return std::round(hz / step) * step;
}


double WorkPlane::hzPerPixel(double canvasX) const
{
  const double twoPi = 8. * std::atan(1.);
  const double a = plot_->invTransform(QwtPlot::xBottom, canvasX);
  const double b = plot_->invTransform(QwtPlot::xBottom, canvasX + 1.);
  return std::fabs(b - a) / (angular_ ? twoPi : 1.);
}


double WorkPlane::smithSampleStep(int i) const
{
  if(i < 0 || smithHz_.size() < 2)
    return 0.;
  const int j = i + 1 < smithHz_.size() ? i + 1 : i - 1;
  return std::fabs(smithHz_.at(j) - smithHz_.at(i));
}


bool WorkPlane::impedanceAt(double hz, std::complex<double>* z) const
{
  if(num_.empty() || !(hz > 0))
    return false;
  const std::pair<double, double> at(hz, hz * 1.000001);
  const auto m = sapecng::magnitude(at, 1., true)(num_, den_, values_);
  const auto p = sapecng::phase(at, 1., true)(num_, den_, values_);
  if(m.second.empty() || p.second.empty())
    return false;
  *z = std::polar(m.second.front() / gainScale_, p.second.front());
  return std::isfinite(z->real()) && std::isfinite(z->imag());
}


void WorkPlane::setGainScale(double k, bool transducer, bool assumedLoad)
{
  // the numerator times k: magnitude and gain plots, markers and peak
  // searches all see the scaled transfer function (the phase, poles and
  // zeros do not change; the Smith chart divides it out again)
  if(!(k > 0.) || !std::isfinite(k))
    k = 1.;
  gainScale_ = k;
  transducer_ = transducer;
  assumedLoad_ = assumedLoad;
  num_ = rawNum_;
  if(k != 1.)
    for(auto& degree : num_)
      for(auto& term : degree.second)
        term.first *= k;
  invalidateCurves();
}


int WorkPlane::nearestSmithSample(const QPointF& canvasPos) const
{
  const QwtSeriesData<QPointF>* d = curves_[SMITH].first->data();
  if(!d)
    return -1;
  const QwtScaleMap xm = plot_->canvasMap(QwtPlot::xBottom);
  const QwtScaleMap ym = plot_->canvasMap(QwtPlot::yLeft);
  int best = -1;
  double bestD = 1e18;
  for(size_t i = 0; i < d->size() && int(i) < smithHz_.size(); ++i) {
    const QPointF s = d->sample(i);
    const double dx = xm.transform(s.x()) - canvasPos.x();
    const double dy = ym.transform(s.y()) - canvasPos.y();
    if(dx * dx + dy * dy < bestD) {
      bestD = dx * dx + dy * dy;
      best = int(i);
    }
  }
  return best;
}


QString WorkPlane::magnitudeText(double v) const
{
  if(std::fabs(v) < 5e-3 && magnitudeDb_)
    v = 0.;     // no "-0.00 dB"
  return magnitudeDb_ ? QStringLiteral("%1 dB").arg(v, 0, 'f', 2)
                      : units::format(QString::number(v, 'g', 6), "");
}


QString WorkPlane::phaseText(double v) const
{
  if(std::fabs(v) < (phaseDegrees_ ? 0.05 : 5e-4))
    v = 0.;     // no "-0.0°"
  return phaseDegrees_ ? QStringLiteral("%1°").arg(v, 0, 'f', 1)
                       : QStringLiteral("%1 rad").arg(v, 0, 'f', 3);
}


void WorkPlane::refreshMarkers()
{
  if(!markerPanel_ || !markerTable_)
    return;
  foreach(QwtPlotMarker* m, markerLines_) {
    m->detach();
    delete m;
  }
  markerLines_.clear();

  // markers belong to frequency plots (not to the pole-zero map)
  markerPanel_->setEnabled(hasFrequencyAxis());

  fillingTable_ = true;
  markerTable_->setRowCount(markers_.size());
  QStringList names;
  for(int i = 0; i < markers_.size(); ++i)
    names << QStringLiteral("M%1").arg(i + 1);
  markerTable_->setVerticalHeaderLabels(names);
  const double twoPi = 8. * std::atan(1.);
  // labels of neighbouring markers go to different heights
  const QwtScaleMap xMap = plot_->canvasMap(QwtPlot::xBottom);
  const Qt::Alignment rows[] = { Qt::AlignTop, Qt::AlignBottom, Qt::AlignVCenter };
  double lastPx[3] = { -1e9, -1e9, -1e9 };

  // differential markers: values of the reference
  const bool hasRef = refHz_ > 0. && markers_.contains(refHz_);
  if(!hasRef)
    refHz_ = 0.;
  double refMag = 0., refPh = 0.;
  const bool refOk = hasRef && markerValues(refHz_, &refMag, &refPh);
  const int refIndex = hasRef ? markers_.indexOf(refHz_) : -1;
  const bool smith = (lastId_ == SMITH);
  QList<QRectF> smithLabels;   // labels placed so far (canvas pixels)
  // the Smith chart: Z, the reflection coefficient and the VSWR (or the
  // return loss)
  markerTable_->setColumnHidden(3, smith ? false : !hasRef);
  markerTable_->setColumnHidden(4, !hasRef || smith);
  markerTable_->setHorizontalHeaderItem(3, new QTableWidgetItem(
    smith ? (returnLoss_ ? tr("RL") : tr("VSWR"))
          : QStringLiteral("Δ ") + tr("Mag.")));
  markerTable_->setHorizontalHeaderItem(1, new QTableWidgetItem(
    smith ? tr("Z") : tr("Magnitude")));
  markerTable_->setHorizontalHeaderItem(2, new QTableWidgetItem(
    smith ? QStringLiteral("Γ") : tr("Phase")));

  for(int i = 0; i < markers_.size(); ++i) {
    const double hz = markers_.at(i);
    double mag = 0., ph = 0.;
    const bool ok = markerValues(hz, &mag, &ph);

    QTableWidgetItem* f = new QTableWidgetItem(hzText(hz));
    QTableWidgetItem* m = new QTableWidgetItem(ok ? magnitudeText(mag) : "-");
    QTableWidgetItem* p = new QTableWidgetItem(ok ? phaseText(ph) : "-");
    m->setFlags(m->flags() & ~Qt::ItemIsEditable);
    p->setFlags(p->flags() & ~Qt::ItemIsEditable);
    markerTable_->setItem(i, 0, f);
    markerTable_->setItem(i, 1, m);
    markerTable_->setItem(i, 2, p);

    const bool isRef = (i == refIndex);
    const bool showDelta = hasRef && !isRef && ok && refOk;
    QString dMag, dPh, dF;
    if(showDelta) {
      dMag = magnitudeDb_
        ? QStringLiteral("%1%2 dB").arg(mag - refMag >= 0 ? "+" : "")
            .arg(mag - refMag, 0, 'f', 2)
        : (mag - refMag >= 0 ? QStringLiteral("+") : QString())
            + units::format(QString::number(mag - refMag, 'g', 6), "");
      dPh = (ph - refPh >= 0 ? QStringLiteral("+") : QString())
        + phaseText(ph - refPh);
      dF = (hz - refHz_ >= 0 ? QStringLiteral("+") : QStringLiteral("-"))
        + hzText(std::fabs(hz - refHz_));
    }
    QTableWidgetItem* dm = new QTableWidgetItem(isRef ? tr("ref") : dMag);
    QTableWidgetItem* dp = new QTableWidgetItem(isRef ? tr("ref") : dPh);
    dm->setFlags(dm->flags() & ~Qt::ItemIsEditable);
    dp->setFlags(dp->flags() & ~Qt::ItemIsEditable);
    markerTable_->setItem(i, 3, dm);
    markerTable_->setItem(i, 4, dp);

    if(smith) {
      // a dot on the curve: the impedance, Gamma and the VSWR
      std::complex<double> z;
      if(!impedanceAt(hz, &z))
        continue;
      const std::complex<double> g = (z - z0_) / (z + z0_);
      const double mg = std::abs(g);
      const QString zText = units::format(QString::number(z.real(), 'g', 4), "")
        + (z.imag() < 0 ? QStringLiteral(" − j") : QStringLiteral(" + j"))
        + units::format(QString::number(std::fabs(z.imag()), 'g', 4), "")
        + QStringLiteral(" Ω");
      const QString gText = QStringLiteral("%1 ∠ %2°").arg(mg, 0, 'f', 3)
        .arg(std::arg(g) * 180. / (4. * std::atan(1.)), 0, 'f', 1);
      m->setText(zText);
      p->setText(gText);
      // VSWR, or the return loss -20 log |Gamma| in dB
      const QString vswr = mg < 1.
        ? QString::number((1. + mg) / (1. - mg), 'f', 2) : QStringLiteral("∞");
      const QString rl = mg > 0.
        ? QStringLiteral("%1 dB").arg(-20. * std::log10(mg), 0, 'f', 1)
        : QStringLiteral("∞");
      dm->setText(returnLoss_ ? rl : vswr);
      QString text = QStringLiteral("M%1  %2\nZ = %3\nΓ = %4").arg(i + 1)
        .arg(hzText(hz)).arg(zText)
        .arg(gText);
      text += returnLoss_ ? QStringLiteral("\nRL %1").arg(rl)
                          : QStringLiteral("\nVSWR %1").arg(vswr);
      QwtPlotMarker* dot = new QwtPlotMarker;
      dot->setValue(g.real(), g.imag());
      QColor c = QApplication::palette().color(QPalette::Text);
      dot->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
        QApplication::palette().color(QPalette::Base), QPen(c, 2), QSize(9, 9)));
      QwtText label(text);
      label.setColor(c);
      QColor bg = QApplication::palette().color(QPalette::Base);
      bg.setAlpha(220);
      label.setBackgroundBrush(bg);
      label.setBorderRadius(3);
      label.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
      dot->setLabel(label);
      // the label towards the middle of the chart, on whichever side of
      // the dot keeps it clear of the labels placed so far and inside
      // the canvas
      {
        const QPointF at(plot_->transform(QwtPlot::xBottom, g.real()),
                         plot_->transform(QwtPlot::yLeft, g.imag()));
        const QSizeF size = label.textSize(plot_->canvas()->font())
          + QSizeF(8, 8);
        const QRectF canvas = plot_->canvas()->contentsRect();
        const Qt::Alignment vIn = g.imag() >= 0 ? Qt::AlignBottom : Qt::AlignTop;
        const Qt::Alignment hIn = g.real() >= 0 ? Qt::AlignLeft : Qt::AlignRight;
        const Qt::Alignment vOut = vIn == Qt::AlignTop ? Qt::AlignBottom : Qt::AlignTop;
        const Qt::Alignment hOut = hIn == Qt::AlignLeft ? Qt::AlignRight : Qt::AlignLeft;
        auto rectFor = [&](Qt::Alignment a) {
            QRectF r(QPointF(0, 0), size);
            r.moveCenter(at);
            if(a & Qt::AlignLeft) r.moveRight(at.x() - 6);
            if(a & Qt::AlignRight) r.moveLeft(at.x() + 6);
            if(a & Qt::AlignTop) r.moveBottom(at.y() - 6);
            if(a & Qt::AlignBottom) r.moveTop(at.y() + 6);
            return r;
          };
        Qt::Alignment best = vIn | hIn;
        int bestScore = -1;
        for(Qt::Alignment a : { vIn | hIn, vOut | hIn, vIn | hOut, vOut | hOut }) {
          const QRectF r = rectFor(a);
          int score = canvas.contains(r) ? 2 : 0;
          bool clear = true;
          for(const QRectF& o : smithLabels)
            clear = clear && !o.intersects(r);
          score += clear ? 4 : 0;
          if(score > bestScore) {
            bestScore = score;
            best = a;
          }
        }
        smithLabels.append(rectFor(best));
        dot->setLabelAlignment(best);
      }
      dot->setZ(30);
      dot->attach(plot_);
      markerLines_.append(dot);
      continue;
    }
    if(showDelta)
      f->setToolTip(QStringLiteral("Δf = ") + dF);
    if(isRef) {
      QFont bold = f->font();
      bold.setBold(true);
      for(QTableWidgetItem* it : { f, m, p, dm, dp })
        it->setFont(bold);
    }

    if(!hasFrequencyAxis())
      continue;

    // dashed vertical line with the values at the top
    QwtPlotMarker* line = new QwtPlotMarker;
    line->setLineStyle(QwtPlotMarker::VLine);
    QColor c = QApplication::palette().color(QPalette::Text);
    c.setAlpha(150);
    line->setLinePen(c, isRef ? 2 : 1, isRef ? Qt::DashDotLine : Qt::DashLine);
    line->setXValue(angular_ ? hz * twoPi : hz);
    QString text = QStringLiteral("M%1%2  %3").arg(i + 1)
      .arg(isRef ? tr(" (ref)") : QString())
      .arg(hzText(hz));
    if(ok)
      text += QLatin1Char('\n') + magnitudeText(mag)
        + QLatin1Char('\n') + phaseText(ph);
    if(showDelta)
      text += QStringLiteral("\nΔ ") + dF + QStringLiteral("\nΔ ") + dMag
        + QStringLiteral("\nΔ ") + dPh;
    QwtText label(text);
    label.setColor(QApplication::palette().color(QPalette::Text));
    QColor bg = QApplication::palette().color(QPalette::Base);
    bg.setAlpha(220);
    label.setBackgroundBrush(bg);
    label.setBorderRadius(3);
    label.setRenderFlags(Qt::AlignLeft | Qt::AlignTop);
    line->setLabel(label);
    const double px = xMap.transform(angular_ ? hz * twoPi : hz);
    const int labelWidth = label.textSize(plot_->canvas()->font()).width() + 16;
    int row = 0;
    while(row < 2 && px - lastPx[row] < labelWidth)
      ++row;
    lastPx[row] = px;
    line->setLabelAlignment(rows[row] | Qt::AlignRight);
    line->setZ(30);
    line->attach(plot_);
    markerLines_.append(line);
  }
  fillingTable_ = false;

  // the panel is as wide as the columns need
  int width = 2 * markerTable_->frameWidth() + 8
    + markerTable_->verticalHeader()->sizeHint().width();
  for(int c = 0; c < markerTable_->columnCount(); ++c)
    if(!markerTable_->isColumnHidden(c))
    {
      int w = markerTable_->horizontalHeader()->sectionSizeHint(c);
      const QFontMetrics fm(markerTable_->font());
      for(int r = 0; r < markerTable_->rowCount(); ++r)
        if(QTableWidgetItem* it = markerTable_->item(r, c))
          w = qMax(w, fm.horizontalAdvance(it->text()) + 12);
      width += w + 4;
    }
  if(markerTable_->verticalScrollBar()->isVisible())
    width += markerTable_->verticalScrollBar()->width();
  markerPanel_->setMinimumWidth(qBound(260, width + 8, 560));

  highlightMarkers();   // replots
}


void WorkPlane::setMarker(int row, double hz)
{
  // the reference follows its marker
  if(markers_.at(row) == refHz_)
    refHz_ = hz;
  markers_[row] = hz;
  std::sort(markers_.begin(), markers_.end());
}


void WorkPlane::removeMarker(int row)
{
  if(markers_.at(row) == refHz_)
    refHz_ = 0.;
  markers_.removeAt(row);
}


double WorkPlane::magnitudeAt(double hz) const
{
  // |H(j 2 pi f)|, linear
  if(num_.empty() || !(hz > 0))
    return NAN;
  const std::pair<double, double> at(hz, hz * 1.000001);
  std::pair< std::vector<double>, std::vector<double> > m =
    sapecng::magnitude(at, 1., true)(num_, den_, values_);
  return m.second.empty() ? NAN : m.second.front();
}


QVector<QPointF> WorkPlane::magnitudeGrid() const
{
  // log(f), |H| over the plotted range, 400 points per decade
  QVector<QPointF> grid;
  const double a = std::log10(fStart_), b = std::log10(fStop_);
  const int n = qBound(50, int((b - a) * 400), 20000);
  grid.reserve(n + 1);
  for(int i = 0; i <= n; ++i) {
    const double lf = a + (b - a) * i / n;
    grid.append(QPointF(lf, magnitudeAt(std::pow(10., lf))));
  }
  return grid;
}


void WorkPlane::markerToPeak(int direction)
{
  const int row = selectedMarker();
  if(row < 0) {
    markerNote_->setText(tr("Select a marker first."));
    return;
  }
  const double f0 = markers_.at(row);
  const QVector<QPointF> g = magnitudeGrid();
  const double l0 = std::log10(f0);

  // the next peak or dip (a notch, a series resonance) of the magnitude
  auto extremum = [&](int i) {
      return (g[i].y() > g[i - 1].y() && g[i].y() >= g[i + 1].y())
          || (g[i].y() < g[i - 1].y() && g[i].y() <= g[i + 1].y());
    };
  // the peak or dip around grid point i, refined between its neighbours
  // (golden section on log f)
  auto refine = [&](int i) {
      const double sign = g[i].y() > g[i - 1].y() ? 1. : -1.;   // dip: -1
      double lo = g[i - 1].x(), hi = g[i + 1].x();
      const double phi = (std::sqrt(5.) - 1) / 2;
      for(int k = 0; k < 60; ++k) {
        const double c = hi - phi * (hi - lo), d = lo + phi * (hi - lo);
        if(sign * magnitudeAt(std::pow(10., c)) > sign * magnitudeAt(std::pow(10., d)))
          hi = d;
        else
          lo = c;
      }
      return QString::number(std::pow(10., (lo + hi) / 2), 'g', 9).toDouble();
    };

  // the first one beyond the marker; a marker already on a peak finds a
  // grid point next to it that refines to that same peak: skipped, so it
  // moves on to the next one
  double hz = 0.;
  const int first = direction > 0 ? 1 : g.size() - 2;
  for(int i = first; i >= 1 && i + 1 < g.size(); i += direction > 0 ? 1 : -1) {
    if(direction > 0 ? g[i].x() <= l0 + 1e-6 : g[i].x() >= l0 - 1e-6)
      continue;
    if(!extremum(i))
      continue;
    const double at = refine(i);
    if(std::fabs(at - f0) <= 1e-6 * f0)
      continue;   // the peak the marker is on
    if(direction > 0 ? at < f0 : at > f0)
      continue;   // refined back past the marker
    hz = at;
    break;
  }
  if(!(hz > 0.)) {
    markerNote_->setText(direction > 0
      ? tr("No peak or dip to the right in the plotted range.")
      : tr("No peak or dip to the left in the plotted range."));
    return;
  }

  setMarker(row, hz);
  markerNote_->clear();
  refreshMarkers();
  markerTable_->selectRow(markers_.indexOf(hz));
}


void WorkPlane::markerMinus3dB()
{
  const int row = selectedMarker();
  if(row < 0) {
    markerNote_->setText(tr("Select a marker first."));
    return;
  }
  const double f0 = markers_.at(row);
  const double m0 = magnitudeAt(f0);
  if(!(m0 > 0)) {
    markerNote_->setText(tr("No magnitude at the selected marker."));
    return;
  }
  // 20 log10(m / m0) = -3.0103 dB  <=>  m = m0 / sqrt(2)
  const double target = m0 / std::sqrt(2.);
  const QVector<QPointF> g = magnitudeGrid();
  const double l0 = std::log10(f0);

  // nearest crossing of the target on each side
  auto crossing = [&](int dir, double* lf) {
      int start = 0;
      while(start < g.size() && g[start].x() < l0) ++start;
      if(dir < 0) --start;
      double prevL = l0, prevM = m0;
      for(int i = start; i >= 0 && i < g.size(); i += dir) {
        const double m = g[i].y();
        if(std::isfinite(m) && (prevM - target) * (m - target) <= 0 && prevM != m) {
          // bisection between prevL and g[i].x()
          double a = prevL, b = g[i].x();
          for(int k = 0; k < 60; ++k) {
            const double c = (a + b) / 2;
            if((magnitudeAt(std::pow(10., a)) - target)
                * (magnitudeAt(std::pow(10., c)) - target) <= 0)
              b = c;
            else
              a = c;
          }
          *lf = (a + b) / 2;
          return true;
        }
        prevL = g[i].x();
        prevM = m;
      }
      return false;
    };
  double up = 0., down = 0.;
  const bool hasUp = crossing(1, &up), hasDown = crossing(-1, &down);
  if(!hasUp && !hasDown) {
    markerNote_->setText(tr("The magnitude does not fall 3 dB below the "
                            "selected marker in the plotted range."));
    return;
  }
  const double lf = (hasUp && (!hasDown || up - l0 <= l0 - down)) ? up : down;
  const double hz = QString::number(std::pow(10., lf), 'g', 6).toDouble();
  refHz_ = f0;
  markerNote_->clear();
  addMarker(hz);
}


bool WorkPlane::antialiased()
{
  return QSettings().value(QStringLiteral("Plot/antialiased"), true).toBool();
}


void WorkPlane::setAntialiasedDefault(bool on)
{
  QSettings().setValue(QStringLiteral("Plot/antialiased"), on);
}


void WorkPlane::applyAntialiasing()
{
  // the curves only: the grid and the marker lines are straight lines,
  // which antialiasing only blurs (a line between two pixel rows is
  // smeared over both)
  // every curve of the plot, also those not shown now: each kind of plot
  // (magnitude, phase, dB, rad/s ...) has its own, attached when chosen,
  // and those kept the old setting
  const bool on = antialiased();
  for(int i = 0; i < curves_.size(); ++i)
    if(curves_[i].first)
      curves_[i].first->setRenderHint(QwtPlotItem::RenderAntialiased, on);
  plot_->replot();
}


void WorkPlane::addMarker(double hz)
{
  if(!(hz > 0) || !std::isfinite(hz))
    return;
  // the Markers tab comes to the front with the first marker
  if(markers_.isEmpty())
    emit markersPanelChanged(true);
  markers_.append(hz);
  std::sort(markers_.begin(), markers_.end());
  refreshMarkers();
  markerTable_->selectRow(markers_.indexOf(hz));
}


int WorkPlane::selectedMarker() const
{
  // the current one of those selected (the others go along when moved)
  const QModelIndex cur = markerTable_->currentIndex();
  if(cur.isValid() && markerTable_->selectionModel()->isRowSelected(cur.row()))
    return cur.row();
  const QList<QTableWidgetSelectionRange> r = markerTable_->selectedRanges();
  return r.isEmpty() ? -1 : r.first().topRow();
}


void WorkPlane::highlightMarkers()
{
  // the markers selected in the table: drawn bold, in the accent colour
  if(!markerTable_)
    return;
  const QColor accent = QApplication::palette().color(QPalette::Link);
  QColor plain = QApplication::palette().color(QPalette::Text);
  plain.setAlpha(150);
  for(int i = 0; i < markerLines_.size() && i < markers_.size(); ++i) {
    const bool on = markerTable_->selectionModel()->isRowSelected(i);
    QwtPlotMarker* m = markerLines_.at(i);
    const bool isRef = markers_.at(i) == refHz_;
    if(m->lineStyle() == QwtPlotMarker::VLine)
      m->setLinePen(on ? accent : plain, on ? 2.5 : (isRef ? 2 : 1),
        isRef ? Qt::DashDotLine : Qt::DashLine);
    else
      m->setSymbol(new QwtSymbol(QwtSymbol::Ellipse,
        QApplication::palette().color(QPalette::Base),
        QPen(on ? accent : QApplication::palette().color(QPalette::Text),
             on ? 3 : 2), on ? QSize(12, 12) : QSize(9, 9)));
  }
  plot_->replot();
}


void WorkPlane::moveSelectedMarker(int direction)
{
  const int row = selectedMarker();
  double step = 0.;
  if(row < 0 || !units::parse(stepEdit_->text(), "Hz", &step) || !(step > 0))
    return;
  const double hz = markers_.at(row) + direction * step;
  if(!(hz > 0))
    return;
  setMarker(row, hz);
  refreshMarkers();
  markerTable_->selectRow(markers_.indexOf(hz));
}


void WorkPlane::markerEdited(int row, int column)
{
  if(fillingTable_ || column != 0 || row < 0 || row >= markers_.size())
    return;
  double hz = 0.;
  if(units::parse(markerTable_->item(row, 0)->text(), "Hz", &hz) && hz > 0)
    setMarker(row, hz);
  // re-show the list (also when the text was not a frequency)
  QTimer::singleShot(0, this, [this]() { refreshMarkers(); });
}


void WorkPlane::smithPreview(const QPointF& pos)
{
  if(lastId_ != SMITH)
    return;
  const QPointF canvasPos(plot_->transform(QwtPlot::xBottom, pos.x()),
                          plot_->transform(QwtPlot::yLeft, pos.y()));
  const int i = nearestSmithSample(canvasPos);
  const QwtSeriesData<QPointF>* d = curves_[SMITH].first->data();
  if(i < 0 || !d) {
    if(smithGhost_)
      smithGhost_->detach();
    return;
  }
  if(!smithGhost_) {
    smithGhost_ = new QwtPlotMarker;
    smithGhost_->setZ(31);
  }
  QColor c = QApplication::palette().color(QPalette::Text);
  c.setAlpha(160);
  smithGhost_->setSymbol(new QwtSymbol(QwtSymbol::Ellipse, Qt::NoBrush,
    QPen(c, 2, Qt::DotLine), QSize(11, 11)));
  const QPointF g = d->sample(i);
  smithGhost_->setValue(g);
  QwtText label(hzText(roundedHz(smithHz_.at(i), smithSampleStep(i))));
  label.setColor(c);
  QColor bg = QApplication::palette().color(QPalette::Base);
  bg.setAlpha(200);
  label.setBackgroundBrush(bg);
  smithGhost_->setLabel(label);
  smithGhost_->setLabelAlignment((g.y() >= 0 ? Qt::AlignBottom : Qt::AlignTop)
    | (g.x() >= 0 ? Qt::AlignLeft : Qt::AlignRight));
  smithGhost_->attach(plot_);
  plot_->replot();
}


void WorkPlane::plotClicked(const QPointF& pos)
{
  if(smithGhost_)
    smithGhost_->detach();
  if(lastId_ == NOOP || lastId_ == ZEROS || lastId_ == POLES)
    return;
  if(lastId_ == SMITH) {
    // the nearest point of the curve; a click on a marker removes it
    const QPointF canvasPos(plot_->transform(QwtPlot::xBottom, pos.x()),
                            plot_->transform(QwtPlot::yLeft, pos.y()));
    for(int i = 0; i < markers_.size(); ++i) {
      std::complex<double> z;
      if(!impedanceAt(markers_.at(i), &z))
        continue;
      const std::complex<double> g = (z - z0_) / (z + z0_);
      if(QLineF(canvasPos, QPointF(plot_->transform(QwtPlot::xBottom, g.real()),
          plot_->transform(QwtPlot::yLeft, g.imag()))).length() < 7.) {
        removeMarker(i);
        refreshMarkers();
        return;
      }
    }
    const int i = nearestSmithSample(canvasPos);
    if(i >= 0)
      addMarker(roundedHz(smithHz_.at(i), smithSampleStep(i)));
    return;
  }
  const double twoPi = 8. * std::atan(1.);
  const double hz = angular_ ? pos.x() / twoPi : pos.x();

  // a click on a marker line removes it
  const QwtScaleMap map = plot_->canvasMap(QwtPlot::xBottom);
  const double px = map.transform(pos.x());
  for(int i = 0; i < markers_.size(); ++i) {
    const double x = angular_ ? markers_.at(i) * twoPi : markers_.at(i);
    if(qAbs(map.transform(x) - px) < 5.) {
      removeMarker(i);
      refreshMarkers();
      return;
    }
  }
  // the clicked position, to the resolution of a pixel
  addMarker(roundedHz(hz, hzPerPixel(px)));
}


bool WorkPlane::hasFrequencyAxis() const
{
  return lastId_ != NOOP && lastId_ != ZEROS && lastId_ != POLES;
}


void WorkPlane::invalidateCurves()
{
  for(int i = 0; i < curves_.size(); ++i)
    curves_[i].second = false;
}


void WorkPlane::autoFrequencyRange()
{
  // a decade below the lowest and above the highest pole or zero
  const double twoPi = 8. * std::atan(1.);
  double lo = 0., hi = 0.;
  for(int which = 0; which < 2; ++which) {
    const std::pair< std::vector<double>, std::vector<double> > roots =
      which == 0 ? sapecng::zeros()(num_, den_, values_)
                 : sapecng::poles()(num_, den_, values_);
    const size_t n = qMin(roots.first.size(), roots.second.size());
    for(size_t i = 0; i < n; ++i) {
      const double w = std::hypot(roots.first[i], roots.second[i]);
      if(w <= 0. || !std::isfinite(w))
        continue;
      lo = lo > 0. ? qMin(lo, w) : w;
      hi = qMax(hi, w);
    }
  }

  if(hi > 0.) {
    fStart_ = std::pow(10., std::floor(std::log10(lo / twoPi / 10.)));
    fStop_ = std::pow(10., std::ceil(std::log10(hi / twoPi * 10.)));
  } else {
    fStart_ = 1.;     // no poles or zeros: any range shows the same
    fStop_ = 1e6;
  }
  if(fStop_ <= fStart_)
    fStop_ = fStart_ * 1000.;
}


void WorkPlane::editFrequencyRange()
{
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Frequency Range"));

  QCheckBox* automatic = new QCheckBox(
    tr("&Automatic: a decade beyond the poles and zeros"));
  automatic->setChecked(autoRange_);

  auto field = [](double value) {
    QLineEdit* edit = new QLineEdit(
      units::formatForEdit(QString::number(value, 'g', 8)));
    edit->setValidator(new units::ValueValidator(QStringLiteral("Hz"), edit));
    edit->setToolTip(tr("Frequency in Hz; prefixes allowed: 10, 1k, 2.5MEG"));
    return edit;
  };
  QLineEdit* start = field(fStart_);
  QLineEdit* stop = field(fStop_);

  QSpinBox* points = new QSpinBox;
  points->setRange(10, 1000000);
  points->setValue(pointsPerDecade_);
  points->setToolTip(tr("Points per decade, spaced logarithmically"));

  QFormLayout* form = new QFormLayout;
  form->addRow(automatic);
  form->addRow(tr("&Start (Hz):"), start);
  form->addRow(tr("St&op (Hz):"), stop);
  form->addRow(tr("&Points per decade:"), points);

  QDialogButtonBox* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto validate = [&]() {
    double a = 0., b = 0.;
    const bool ok = automatic->isChecked()
      || (units::parse(start->text(), "Hz", &a)
          && units::parse(stop->text(), "Hz", &b) && a > 0 && b > a);
    buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
    start->setEnabled(!automatic->isChecked());
    stop->setEnabled(!automatic->isChecked());
  };
  connect(automatic, &QCheckBox::toggled, &dialog, validate);
  connect(start, &QLineEdit::textChanged, &dialog, validate);
  connect(stop, &QLineEdit::textChanged, &dialog, validate);
  validate();

  QVBoxLayout* layout = new QVBoxLayout(&dialog);
  layout->addLayout(form);
  layout->addWidget(buttons);

  if(dialog.exec() != QDialog::Accepted)
    return;

  autoRange_ = automatic->isChecked();
  pointsPerDecade_ = points->value();
  if(autoRange_) {
    autoFrequencyRange();
  } else {
    units::parse(start->text(), "Hz", &fStart_);
    units::parse(stop->text(), "Hz", &fStop_);
  }


  invalidateCurves();
  if(lastId_ != NOOP)
    showKind(kind_);
}


void WorkPlane::setMagnitudeDb(bool db)
{
  magnitudeDb_ = db;
  if(lastId_ != NOOP && (kind_ == MagnitudeKind || kind_ == BodeKind))
    showKind(kind_);
}


void WorkPlane::setPhaseDegrees(bool degrees)
{
  if(phaseDegrees_ == degrees)
    return;
  phaseDegrees_ = degrees;
  // phase curves hold converted values: compute them again
  curves_[PHASE].second = false;
  curves_[PHASE_RAD].second = false;
  if(lastId_ != NOOP && (kind_ == PhaseKind || kind_ == BodeKind))
    showKind(kind_);
}


void WorkPlane::setAngularFrequency(bool radPerSecond)
{
  angular_ = radPerSecond;
  if(lastId_ != NOOP && kind_ != PoleZeroKind)
    showKind(kind_);
}


void WorkPlane::plot(int f)
{
  plot((F) f);
}


void WorkPlane::redraw()
{
  const bool shown = lastId_ != NOOP;
  setDirty();
  if(shown)
    showKind(kind_);
}


void WorkPlane::print(QPrinter& printer) {
  // print on white, also when the dark theme is active
  const QPalette paper = theme::paperPalette();
  plot_->setPalette(paper);
  applyPlotStyle(paper, plot_, grid_, curves_, ZEROS, POLES);

  QwtPlotRenderer().renderTo(plot_, printer);

  plot_->setPalette(QPalette());   // back to the inherited palette
  applyPlotStyle(QApplication::palette(), plot_, grid_, curves_, ZEROS, POLES);
  plot_->replot();
}


std::map<std::string, double> WorkPlane::actValues() const
{
  return values_;
}


namespace {

// axes: 100, 1k, 10k, 1M instead of 100,000 and 1e+06
class SiScaleDraw: public QwtScaleDraw
{
public:
  QwtText label(double v) const override
  {
    // prefixes from 1000 up; below that the plain numbers read better
    if(std::fabs(v) < 999.5)
      return QwtScaleDraw::label(v);
    // enough digits to tell neighbouring ticks apart (a narrow sweep:
    // 10.005M, 10.01M, 10.015M)
    const QList<double> ticks = scaleDiv().ticks(QwtScaleDiv::MajorTick);
    double step = 0.;
    for(int i = 1; i < ticks.size(); ++i) {
      const double d = std::fabs(ticks.at(i) - ticks.at(i - 1));
      if(d > 0. && (step == 0. || d < step))
        step = d;
    }
    int digits = 4;
    if(step > 0.)
      digits = qBound(4, int(std::ceil(std::log10(std::fabs(v) / step))) + 1, 9);
    // the prefix as units::format gives it for 10^exp ("1k" -> "k")
    const int exp = int(std::floor(std::log10(std::fabs(v)) / 3.)) * 3;
    const QString prefix = units::format(
      QString::number(std::pow(10., exp), 'g', 3), QString()).mid(1);
    return QwtText(QString::number(v / std::pow(10., exp), 'g', digits) + prefix);
  }
};

}


void WorkPlane::createMainLayout()
{
  plot_ = new QwtPlot_ContextMenu(this);
  plot_->setAxisScaleEngine(QwtPlot::yLeft, new QwtLinearScaleEngine);
  plot_->setAxisScaleEngine(QwtPlot::xBottom, new QwtLogScaleEngine(10));
  plot_->setAutoReplot(true);

  tracker_ = new QwtPlotPicker(plot_->canvas());
  clickPointMachine_ = new QwtPickerDragPointMachine;
  tracker_->setStateMachine(clickPointMachine_);
  tracker_->setRubberBand(QwtPicker::VLineRubberBand);
  tracker_->setTrackerMode(QwtPicker::AlwaysOff);

  // middle button pans, the wheel zooms (see eventFilter)
  QwtPlotPanner* panner = new QwtPlotPanner(plot_->canvas());
  panner->setMouseButton(Qt::MiddleButton);
  plot_->canvas()->installEventFilter(this);

  grid_ = new QwtPlotGrid;
  grid_->setPen(QPen(Qt::gray, 0., Qt::DotLine));
  grid_->attach(plot_);
  plot_->replot();

  connect(plot_->axisWidget(QwtPlot::xBottom), &QwtScaleWidget::scaleDivChanged,
    this, [this]() { if(!markers_.isEmpty()) QTimer::singleShot(0, this, [this]() { refreshMarkers(); }); });

  plot_->setAxisScaleDraw(QwtPlot::xBottom, new SiScaleDraw);
  plot_->setAxisScaleDraw(QwtPlot::yLeft, new SiScaleDraw);

  // click on the plot: a marker at that frequency (or remove one there)
  connect(tracker_, SIGNAL(selected(const QPointF&)),
    this, SLOT(plotClicked(const QPointF&)));
  // Smith chart: while the button is held, a ghost marker shows where the
  // marker goes (the other plots show a vertical line); released: placed
  connect(tracker_, &QwtPlotPicker::appended, this, &WorkPlane::smithPreview);
  connect(tracker_, &QwtPlotPicker::moved, this, &WorkPlane::smithPreview);

  // markers panel
  markerTable_ = new QTableWidget(0, 5);
  markerTable_->setHorizontalHeaderLabels(
    { tr("Frequency"), tr("Magnitude"), tr("Phase"),
      QStringLiteral("Δ ") + tr("Mag."), QStringLiteral("Δ ") + tr("Phase") });
  // the visible columns share the width (the last one may be hidden)
  markerTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  // the marker names (M1, M2 ...) in the leftmost column
  markerTable_->verticalHeader()->setVisible(true);
  markerTable_->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
  markerTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
  // Ctrl / Shift add to the selection; a click on the empty part of the
  // table clears it (the plot shows the selected markers in bold)
  markerTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  connect(markerTable_, &QTableWidget::itemSelectionChanged,
    this, &WorkPlane::highlightMarkers);
  markerTable_->setToolTip(tr("Click a curve to add a marker; edit a "
    "frequency here (10k, 2.5MEG)"));
  connect(markerTable_, &QTableWidget::cellChanged,
    this, &WorkPlane::markerEdited);

  QPushButton* add = new QPushButton(tr("Add"));
  add->setToolTip(tr("A marker in the middle of the visible range"));
  connect(add, &QPushButton::clicked, this, [this]() {
      if(lastId_ == SMITH) {
        // the middle of the sweep
        addMarker(roundedHz(std::sqrt(fStart_ * fStop_),
          (fStop_ - fStart_) / 1000.));
        return;
      }
      const QwtScaleMap map = plot_->canvasMap(QwtPlot::xBottom);
      double x = map.invTransform((map.p1() + map.p2()) / 2);
      addMarker(angular_ ? x / (8. * std::atan(1.)) : x);
    });
  QPushButton* remove = new QPushButton(tr("Remove"));
  connect(remove, &QPushButton::clicked, this, [this]() {
      const int row = selectedMarker();
      if(row >= 0) {
        removeMarker(row);
        refreshMarkers();
      }
    });
  QPushButton* left = new QPushButton(QStringLiteral("◀"));
  left->setToolTip(tr("Move the selected marker down by the step"));
  connect(left, &QPushButton::clicked, this, [this]() { moveSelectedMarker(-1); });
  QPushButton* right = new QPushButton(QStringLiteral("▶"));
  right->setToolTip(tr("Move the selected marker up by the step"));
  connect(right, &QPushButton::clicked, this, [this]() { moveSelectedMarker(1); });
  for(QPushButton* b : { left, right })
    b->setMaximumWidth(32);
  stepEdit_ = new QLineEdit(QStringLiteral("100"));
  stepEdit_->setValidator(new units::ValueValidator(QStringLiteral("Hz"), stepEdit_));
  stepEdit_->setToolTip(tr("Step for moving a marker, in Hz (10, 1k, 1MEG)"));
  stepEdit_->setMaximumWidth(70);

  QPushButton* reference = new QPushButton(tr("Reference"));
  reference->setToolTip(tr("Differential markers: the selected marker becomes "
    "the reference and the others show their difference from it in "
    "magnitude and phase (and frequency). Again on the reference: off"));
  connect(reference, &QPushButton::clicked, this, [this]() {
      const int row = selectedMarker();
      if(row < 0)
        return;
      refHz_ = (markers_.at(row) == refHz_) ? 0. : markers_.at(row);
      refreshMarkers();
      markerTable_->selectRow(row);
    });

  QHBoxLayout* buttons = new QHBoxLayout;
  buttons->addWidget(add);
  buttons->addWidget(remove);
  buttons->addWidget(reference);
  QHBoxLayout* move = new QHBoxLayout;
  move->addWidget(left);
  move->addWidget(right);
  move->addWidget(new QLabel(tr("Step:")));
  move->addWidget(stepEdit_);
  move->addWidget(new QLabel(tr("Hz")));
  move->addStretch();

  // searches: peaks of the magnitude, the -3 dB point
  QPushButton* prevPeak = new QPushButton(tr("◀ Peak"));
  prevPeak->setToolTip(tr("Move the selected marker to the previous peak or "
                          "dip of the magnitude"));
  connect(prevPeak, &QPushButton::clicked, this, [this]() { markerToPeak(-1); });
  QPushButton* nextPeak = new QPushButton(tr("Peak ▶"));
  nextPeak->setToolTip(tr("Move the selected marker to the next peak or dip "
                          "of the magnitude"));
  connect(nextPeak, &QPushButton::clicked, this, [this]() { markerToPeak(1); });
  QPushButton* minus3 = new QPushButton(tr("−3 dB"));
  minus3->setToolTip(tr("A marker where the magnitude is 3 dB below the "
    "selected marker (the nearest, on either side); the selected marker "
    "becomes the reference, so the difference columns show the bandwidth"));
  connect(minus3, &QPushButton::clicked, this, &WorkPlane::markerMinus3dB);
  QHBoxLayout* search = new QHBoxLayout;
  search->addWidget(prevPeak);
  search->addWidget(nextPeak);
  search->addWidget(minus3);
  markerNote_ = new QLabel;
  markerNote_->setWordWrap(true);
  QFont small = markerNote_->font();
  small.setPointSizeF(small.pointSizeF() * 0.9);
  markerNote_->setFont(small);

  QVBoxLayout* panelLayout = new QVBoxLayout;
  panelLayout->setContentsMargins(4, 4, 4, 4);
  panelLayout->addWidget(markerTable_, 1);
  panelLayout->addLayout(buttons);
  panelLayout->addLayout(search);
  panelLayout->addLayout(move);
  panelLayout->addWidget(markerNote_);
  // shown by the plot window in its Markers dock (QSapecNGWindow)
  markerPanel_ = new QWidget;
  markerPanel_->setLayout(panelLayout);
  markerPanel_->setMinimumWidth(260);
  markerPanel_->setMaximumWidth(560);
  markerPanel_->hide();

  centralLayout_ = new QVBoxLayout;
  centralLayout_->addWidget(plot_);

  // right-click on the plot: a marker at that frequency
  plot_->extraActions = [this](const QPoint& canvasPos) {
      QList<QAction*> actions;
      if(!hasFrequencyAxis() || !plot_->canvas()->rect().contains(canvasPos))
        return actions;
      if(lastId_ == SMITH) {
        const int i = nearestSmithSample(canvasPos);
        if(i < 0)
          return actions;
        const double hz = roundedHz(smithHz_.at(i), smithSampleStep(i));
        QAction* add = new QAction(tr("Add &Marker Here"), nullptr);
        connect(add, &QAction::triggered, this, [this, hz]() { addMarker(hz); });
        actions << add;
        return actions;
      }
      const double x = plot_->invTransform(QwtPlot::xBottom, canvasPos.x());
      const double twoPi = 8. * std::atan(1.);
      const double hz = roundedHz(angular_ ? x / twoPi : x,
        hzPerPixel(canvasPos.x()));
      if(!(hz > 0))
        return actions;
      QAction* add = new QAction(tr("Add &Marker Here"), nullptr);
      connect(add, &QAction::triggered, this, [this, hz]() { addMarker(hz); });
      actions << add;
      return actions;
    };

  mainLayout_ = new QHBoxLayout;
  mainLayout_->setContentsMargins(0, 0, 0, 0);
  mainLayout_->addLayout(centralLayout_);
}


void WorkPlane::setupCurves()
{
  curves_ = QVector< QPair< QwtPlotCurve*, bool > >(NOOP);
  QwtSymbol* symbol;

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::magnitude>::symbol);
  symbol->setSize(functor_traits<sapecng::magnitude>::ssize);
  curves_[MAGNITUDE].first = new MarkableCurve;
  curves_[MAGNITUDE].first->setPen(functor_traits<sapecng::magnitude>::pen);
  curves_[MAGNITUDE].first->setStyle(functor_traits<sapecng::magnitude>::style);
  curves_[MAGNITUDE].first->setSymbol(symbol);
  curves_[MAGNITUDE].first->setCurveAttribute(
    functor_traits<sapecng::magnitude>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[MAGNITUDE].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[MAGNITUDE].first,
    SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[MAGNITUDE].first,
    SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::magnitude>::symbol);
  symbol->setSize(functor_traits<sapecng::magnitude>::ssize);
  curves_[MAGNITUDE_RAD].first = new MarkableCurve;
  curves_[MAGNITUDE_RAD].first->setPen(functor_traits<sapecng::magnitude>::pen);
  curves_[MAGNITUDE_RAD].first->setStyle(functor_traits<sapecng::magnitude>::style);
  curves_[MAGNITUDE_RAD].first->setSymbol(symbol);
  curves_[MAGNITUDE_RAD].first->setCurveAttribute(
    functor_traits<sapecng::magnitude>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[MAGNITUDE_RAD].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[MAGNITUDE_RAD].first,
    SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[MAGNITUDE_RAD].first,
    SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::phase>::symbol);
  symbol->setSize(functor_traits<sapecng::phase>::ssize);
  curves_[PHASE].first = new MarkableCurve;
  curves_[PHASE].first->setPen(functor_traits<sapecng::phase>::pen);
  curves_[PHASE].first->setStyle(functor_traits<sapecng::phase>::style);
  curves_[PHASE].first->setSymbol(symbol);
  curves_[PHASE].first->setCurveAttribute(
    functor_traits<sapecng::phase>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[PHASE].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[PHASE].first,
    SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[PHASE].first, SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::phase>::symbol);
  symbol->setSize(functor_traits<sapecng::phase>::ssize);
  curves_[PHASE_RAD].first = new MarkableCurve;
  curves_[PHASE_RAD].first->setPen(functor_traits<sapecng::phase>::pen);
  curves_[PHASE_RAD].first->setStyle(functor_traits<sapecng::phase>::style);
  curves_[PHASE_RAD].first->setSymbol(symbol);
  curves_[PHASE_RAD].first->setCurveAttribute(
    functor_traits<sapecng::phase>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[PHASE_RAD].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[PHASE_RAD].first,
    SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[PHASE_RAD].first,
    SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::gain>::symbol);
  symbol->setSize(functor_traits<sapecng::gain>::ssize);
  curves_[GAIN].first = new MarkableCurve;
  curves_[GAIN].first->setPen(functor_traits<sapecng::gain>::pen);
  curves_[GAIN].first->setStyle(functor_traits<sapecng::gain>::style);
  curves_[GAIN].first->setSymbol(symbol);
  curves_[GAIN].first->setCurveAttribute(
    functor_traits<sapecng::gain>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[GAIN].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[GAIN].first,
    SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[GAIN].first, SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::gain>::symbol);
  symbol->setSize(functor_traits<sapecng::gain>::ssize);
  curves_[GAIN_RAD].first = new MarkableCurve;
  curves_[GAIN_RAD].first->setPen(functor_traits<sapecng::gain>::pen);
  curves_[GAIN_RAD].first->setStyle(functor_traits<sapecng::gain>::style);
  curves_[GAIN_RAD].first->setSymbol(symbol);
  curves_[GAIN_RAD].first->setCurveAttribute(
    functor_traits<sapecng::gain>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[GAIN_RAD].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[GAIN_RAD].first,
    SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[GAIN_RAD].first,
    SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::loss>::symbol);
  symbol->setSize(functor_traits<sapecng::loss>::ssize);
  curves_[LOSS].first = new MarkableCurve;
  curves_[LOSS].first->setPen(functor_traits<sapecng::loss>::pen);
  curves_[LOSS].first->setStyle(functor_traits<sapecng::loss>::style);
  curves_[LOSS].first->setSymbol(symbol);
  curves_[LOSS].first->setCurveAttribute(
    functor_traits<sapecng::loss>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[LOSS].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[LOSS].first, SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[LOSS].first, SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::loss>::symbol);
  symbol->setSize(functor_traits<sapecng::loss>::ssize);
  curves_[LOSS_RAD].first = new MarkableCurve;
  curves_[LOSS_RAD].first->setPen(functor_traits<sapecng::loss>::pen);
  curves_[LOSS_RAD].first->setStyle(functor_traits<sapecng::loss>::style);
  curves_[LOSS_RAD].first->setSymbol(symbol);
  curves_[LOSS_RAD].first->setCurveAttribute(
    functor_traits<sapecng::loss>::attribute);

  connect(tracker_, SIGNAL(selected(const QPointF&)),
    (MarkableCurve*) curves_[LOSS_RAD].first, SLOT(selected(const QPointF&)));
  connect(tracker_, SIGNAL(appended(const QPointF&)),
    (MarkableCurve*) curves_[LOSS_RAD].first,
    SLOT(appended(const QPointF&)));
  connect(tracker_, SIGNAL(moved(const QPointF&)),
    (MarkableCurve*) curves_[LOSS_RAD].first,
    SLOT(moved(const QPointF&)));

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::zeros>::symbol);
  symbol->setSize(functor_traits<sapecng::zeros>::ssize);
  curves_[ZEROS].first = new MarkedCurve;
  curves_[ZEROS].first->setPen(functor_traits<sapecng::zeros>::pen);
  curves_[ZEROS].first->setStyle(functor_traits<sapecng::zeros>::style);
  curves_[ZEROS].first->setSymbol(symbol);
  curves_[ZEROS].first->setCurveAttribute(
    functor_traits<sapecng::zeros>::attribute);

  symbol = new QwtSymbol;
  symbol->setStyle(functor_traits<sapecng::poles>::symbol);
  symbol->setSize(functor_traits<sapecng::poles>::ssize);
  curves_[POLES].first = new MarkedCurve;
  curves_[POLES].first->setPen(functor_traits<sapecng::poles>::pen);
  curves_[POLES].first->setStyle(functor_traits<sapecng::poles>::style);
  curves_[POLES].first->setSymbol(symbol);
  curves_[POLES].first->setCurveAttribute(
    functor_traits<sapecng::poles>::attribute);

  curves_[SMITH].first = new QwtPlotCurve;
  curves_[SMITH].first->setStyle(QwtPlotCurve::Lines);
  smithGrid_ = new SmithGrid;

  for(int i = 0; i < curves_.size(); ++i) {
    // plain polyline through the samples: the sweep is dense enough
    // (log-spaced), and point weeding left visible kinks
    curves_[i].first->setCurveAttribute(QwtPlotCurve::Fitted, false);
    curves_[i].first->setVisible(false);
    curves_[i].second = false;
  }

  applyPlotStyle(QApplication::palette(), plot_, grid_, curves_, ZEROS, POLES);

  lastId_ = NOOP;

  // units in the marker labels
  const struct { F f; const char* x; const char* y; } units[] = {
    { MAGNITUDE, "Hz", "" }, { MAGNITUDE_RAD, "rad/s", "" },
    { PHASE, "Hz", "rad" }, { PHASE_RAD, "rad/s", "rad" },
    { GAIN, "Hz", "dB" }, { GAIN_RAD, "rad/s", "dB" },
    { LOSS, "Hz", "dB" }, { LOSS_RAD, "rad/s", "dB" }
  };
  for(const auto& u : units)
    if(MarkableCurve* mc = dynamic_cast<MarkableCurve*>(curves_[u.f].first))
      mc->setUnits(QString::fromLatin1(u.x), QString::fromLatin1(u.y));
}


void WorkPlane::clearMarkers()
{
  markers_.clear();
  refHz_ = 0.;
  refreshMarkers();
}


void WorkPlane::setupCurve(
    std::pair< std::vector<double>, std::vector<double> > data,
    WorkPlane::F f
  )
{
  if(!curves_[f].second) {
    curves_[f].second = true;

    double* x = new double [data.first.size()];
    std::copy(data.first.begin(), data.first.end(), x);
    double* y = new double [data.second.size()];
    std::copy(data.second.begin(), data.second.end(), y);

    curves_[f].first->setSamples(x, y,
        data.first.size() < data.second.size() ?
            data.first.size()
          : data.second.size()
      );

    delete [] y;
    delete [] x;
  }
}


}
