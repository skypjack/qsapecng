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


#ifndef WORKPLANE_H
#define WORKPLANE_H


#include "model/metacircuit.h"

#include <QWidget>
#include <QPointer>
#include <functional>
#include <QMenu>
#include <QContextMenuEvent>
#include <QPrinter>

#include <QObject>
#include <QVector>
#include <QPair>
#include <QHash>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>

#include <complex>
#include <string>
#include <map>


class QVBoxLayout;
class QHBoxLayout;
class QDoubleSpinBox;
class QTableWidget;
class QLineEdit;
class QLabel;

class QwtPlotGrid;
class QwtPickerMachine;
class QwtPlotPicker;
class QwtPlotMarker;
class QwtPlotItem;
class QwtSymbol;


namespace qsapecng
{


class QwtPlot_ContextMenu: public QwtPlot
{

public:
  QwtPlot_ContextMenu(QWidget* parent = 0): QwtPlot(parent), contextMenu_(0) { }
  QwtPlot_ContextMenu(const QwtText& title, QWidget* parent = 0)
    : QwtPlot(title, parent), contextMenu_(0) { }

  inline void setContextMenu(QMenu* menu) { contextMenu_ = menu; }
  inline QMenu* contextMenu() const { return contextMenu_; }
  // actions shown above the context menu's own, for a click at a point of
  // the canvas (e.g. Add Marker there); the menu deletes them
  std::function<QList<QAction*>(const QPoint& canvasPos)> extraActions;
  // actions of the context menu moved up next to those (not deleted)
  QList<QAction*> leadingActions;

  void contextMenuEvent(QContextMenuEvent* event)
  {
    QList<QAction*> extra;
    if(extraActions)
      extra = extraActions(canvas()->mapFromGlobal(event->globalPos()));
    if(extra.isEmpty()) {
      if(contextMenu_)
        contextMenu_->exec(event->globalPos());
      return;
    }
    QMenu menu;
    menu.addActions(extra);
    menu.addActions(leadingActions);
    if(contextMenu_) {
      menu.addSeparator();
      foreach(QAction* a, contextMenu_->actions())
        if(!leadingActions.contains(a))
          menu.addAction(a);
    }
    menu.exec(event->globalPos());
    qDeleteAll(extra);
  }

private:
  QMenu* contextMenu_;

};



typedef
  std::pair< std::vector<double>, std::vector<double> >
  (*functor)
    (
      const sapecng::metacircuit::expression& numerator,
      const sapecng::metacircuit::expression& denominator,
      std::map< std::string, double > values
    )
  ;

class MarkableCurve: public QObject, public QwtPlotCurve
{

  Q_OBJECT

public:
  MarkableCurve(): QwtPlotCurve() { initialize(); }
  ~MarkableCurve();

  void setVisible(bool on);

  // units for the marker labels ("Hz", "rad/s"; "dB", "rad")
  void setUnits(const QString& x, const QString& y)
    { xUnit_ = x; yUnit_ = y; }

  // placed markers: a click on the plot pins one at the nearest point of
  // the curve, a click on a pinned marker removes it

public slots:
  void selected(const QPointF& pos);
  void appended(const QPointF& pos);
  void moved(const QPointF& pos);

private:
  void initialize();
  int nearestSample(const QPointF& pos) const;
  QString labelFor(int idx) const;
  void placeLabel(QwtPlotMarker* marker, int idx) const;

private:
  QwtPlotMarker* marker_;
  QwtSymbol* symbol_;
  QString xUnit_;
  QString yUnit_;

};



class MarkedCurve: public QwtPlotCurve
{

public:
  MarkedCurve(): QwtPlotCurve() { }
  ~MarkedCurve();

  void setVisible(bool on);
  void replotMarker();

private:
  void resetMarker();

private:
  QVector<QwtPlotMarker*> markers_;

};



// Everything a plot is set up with (Run > New Plot, Plot > Settings).
struct PlotSettings
{
  int kind = 3;                 // WorkPlane::Kind, Bode by default
  bool magnitudeDb = true;
  bool phaseDegrees = true;
  bool angular = false;         // frequency in rad/s
  bool xLog = true;
  bool yLog = false;            // linear magnitude only
  bool autoRange = true;        // a decade beyond the poles and zeros
  double fStart = 1.;           // Hz
  double fStop = 1e5;           // Hz
  int pointsPerDecade = 100;
  // Smith chart: the reference impedance, and the admittance circles
  // (where a shunt part moves the point) over the impedance ones
  double z0 = 50.;              // ohm
  bool smithAdmittance = false;
  bool smithReturnLoss = false;   // markers: return loss instead of VSWR
  // vertical ranges (in the units chosen above); this plot's only, not
  // kept as defaults: automatic unless set
  bool magAuto = true;
  double magMin = 0., magMax = 1.;
  bool phaseAuto = true;
  double phaseMin = 0., phaseMax = 1.;
  // which output of the circuit (Out1, VM1 ...; empty: the first). Not
  // kept as a default for the next plot
  QString output;

  // last used, as defaults for the next plot
  static PlotSettings load();
  void save() const;
};


class WorkPlane: public QWidget
{

  Q_OBJECT

public:
  enum Kind { MagnitudeKind = 0, PhaseKind = 1, PoleZeroKind = 2,
              BodeKind = 3, SmithKind = 4 };

  enum F
  {
    MAGNITUDE,
    MAGNITUDE_RAD,
    PHASE,
    PHASE_RAD,
    GAIN,
    GAIN_RAD,
    LOSS,
    LOSS_RAD,
    ZEROS,
    POLES,
    SMITH,      // the reflection coefficient of the output taken as Z
    NOOP
  };

public:
  WorkPlane(QWidget* parent = 0);
  ~WorkPlane();

  // a source with internal resistance: the magnitude as transducer gain,
  // |H| times k = 2 sqrt(Rs / RL)
  void setGainScale(double k, bool transducer, bool assumedLoad);
  // the output is an impedance (a current source drives the circuit, no
  // voltage source): the Smith chart means something; otherwise it says
  // so in its title
  void setImpedance(bool impedance) { impedance_ = impedance; }
  void setData(const std::map<std::string, double>& values,
      const sapecng::metacircuit::expression& numerator,
      const sapecng::metacircuit::expression& denominator
    );

  inline void setContextMenu(QMenu* menu) { plot_->setContextMenu(menu); }
  // actions of that menu shown first when the plot is right-clicked
  inline void setLeadingContextActions(const QList<QAction*>& actions)
    { plot_->leadingActions = actions; }
  PlotSettings settings() const;
  inline QString output() const { return output_; }
  inline int kind() const { return kind_; }
  inline bool hasPlot() const { return lastId_ != NOOP; }
  inline QMenu* contextMenu() const { return plot_->contextMenu(); }

  const QwtPlot& const_plot() const { return *plot_; }

signals:
  void markersPanelChanged(bool visible);

public slots:
  void setDirty();
  void xAxisLogScale(bool log = true);
  void yAxisLogScale(bool log = false);
  void plot(WorkPlane::F f);
  void plot(int f);
  void redraw();
  void clearMarkers();

  // what is plotted (WorkPlane::Kind), independent of its units
  void showKind(int kind);
  // all settings at once; replots when there is data
  void applySettings(const PlotSettings& settings);
  void setMagnitudeDb(bool db);
  void setPhaseDegrees(bool degrees);
  void setAngularFrequency(bool radPerSecond);
  // WorkPlane > Frequency Range...
  void editFrequencyRange();
  // WorkPlane > Vertical Range...
  void editVerticalRange();
  // WorkPlane > Reset View: undo zooming and panning
  void resetView();

public:
  // smooth curves and lines (Plot > Antialiasing, kept between sessions)
  static bool antialiased();
  static void setAntialiasedDefault(bool on);
  void applyAntialiasing();
  void equalizeAxes();
  // marker searches (Markers panel)
  double magnitudeAt(double hz) const;
  QVector<QPointF> magnitudeGrid() const;
  void markerToPeak(int direction);
  void markerMinus3dB();
  // markers (frequencies, Hz) and their panel
  void addMarker(double hz);
  // the Markers panel of this plot (table, buttons): shown by the plot
  // window in a dock of its own; usable for frequency plots only
  QWidget* markersPanel() const { return markerPanel_; }
  bool hasFrequencyAxis() const;
  void print(QPrinter& printer);

private:
  std::map<std::string, double> actValues() const;
  void createMainLayout();
  void setupCurves();
  void setupCurve(
      std::pair< std::vector<double>, std::vector<double> > data,
      WorkPlane::F f
    );

private:
  sapecng::metacircuit::expression num_;
  sapecng::metacircuit::expression rawNum_;   // as solved (num_ may be scaled)
  double gainScale_ = 1.;
  bool impedance_ = true;   // see setImpedance()
  bool transducer_ = false;    // magnitude = transducer gain
  bool assumedLoad_ = false;   // ... with RL = Rs, the output has no load
  sapecng::metacircuit::expression den_;

  QwtPlotGrid* grid_ = nullptr;
  QwtPlot_ContextMenu* plot_ = nullptr;
  QVector< QwtPlotCurve* > attached_;
  QVector< QPair< QwtPlotCurve*, bool > > curves_;
  QwtPickerMachine *clickPointMachine_;
  QwtPlotPicker* tracker_;
  F lastId_;
  bool xLog_ = true;   // user's x-axis log-scale choice
  bool yLog_ = false;  // user's y-axis log-scale choice (linear magnitude)
  int kind_ = MagnitudeKind;
  QString output_;   // the output of the circuit shown
  bool magnitudeDb_ = true;
  bool phaseDegrees_ = true;
  bool angular_ = false;   // frequency axis in rad/s instead of Hz
  double z0_ = 50.;        // Smith chart reference impedance (ohm)
  bool admittance_ = false;   // Smith chart: admittance circles too
  bool returnLoss_ = false;   // Smith markers: return loss, not VSWR
  QVector<double> smithHz_;   // the frequency of each Smith curve sample
  QwtPlotItem* smithGrid_ = nullptr;
  QwtPlotMarker* smithGhost_ = nullptr;   // where a marker would go
  // the plots with both axes in the same units (no frequency axis)
  static bool isMap(F f) { return f == ZEROS || f == POLES || f == SMITH; }
  // Z(j 2 pi hz) from the transfer function (the output over a 1 A source)
  bool impedanceAt(double hz, std::complex<double>* z) const;
  int nearestSmithSample(const QPointF& canvasPos) const;
  static double roundedHz(double hz, double resolution);
  static QString hzText(double hz);
  double hzPerPixel(double canvasX) const;
  double smithSampleStep(int i) const;

  F currentF() const;
  void relabel(F f);

  void applyAxisScales(bool forceLinear);

protected:
  void changeEvent(QEvent* event) override;

  // component values (from the schematic) and the frequency sweep
  std::map<std::string, double> values_;
  double fStart_ = 1.;          // Hz
  double fStop_ = 1e5;          // Hz
  int pointsPerDecade_ = 100;
  bool autoRange_ = true;       // a decade beyond the poles and zeros
  bool keepAttached_ = false;   // drawing the second curve of a Bode plot

  void autoFrequencyRange();
  int sweepPoints() const;
  void invalidateCurves();
  void plotBode();

  // vertical range per plotted quantity ("magDb", "mag", "phaseDeg",
  // "phaseRad"); automatic unless set in the dialog
  struct YRange { bool automatic = true; double min = 0.; double max = 1.; };
  YRange yRange(const QString& key) const;
  QHash<QString, YRange> yRanges_;   // set by hand, this plot only
  void setYRange(const QString& key, const YRange& range);
  QString magnitudeKey() const;
  QString phaseKey() const;
  void applyYRange(int axis, const QString& key);

  // markers: vertical lines with magnitude and phase, listed in a panel
  QList<double> markers_;                 // Hz
  QList<QwtPlotMarker*> markerLines_;
  QPointer<QWidget> markerPanel_;   // lives in the plot window's dock
  QTableWidget* markerTable_ = nullptr;
  QLineEdit* stepEdit_ = nullptr;
  bool fillingTable_ = false;
  QLabel* markerNote_ = nullptr;      // what a marker search found (or not)
  double refHz_ = 0.;                     // reference marker (0: none)
  void setMarker(int row, double hz);
  void removeMarker(int row);
  void refreshMarkers();
  bool markerValues(double hz, double* magnitude, double* phase) const;
  QString magnitudeText(double v) const;
  QString phaseText(double v) const;
  void moveSelectedMarker(int direction);
  int selectedMarker() const;
  void highlightMarkers();

private slots:
  void plotClicked(const QPointF& pos);
  void smithPreview(const QPointF& pos);
  void markerEdited(int row, int column);

private:
  // mouse wheel zoom around the pointer
  bool eventFilter(QObject* watched, QEvent* event) override;
  void zoomAt(const QPoint& pos, double factor, bool x, bool y);

  QVBoxLayout* centralLayout_;
  QHBoxLayout* mainLayout_;

};


}


#endif // WORKPLANE_H
