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

#ifndef PLOTDIALOG_H
#define PLOTDIALOG_H


#include "gui/workplane/workplane.h"

#include <QDialog>
#include <QWidget>


class QButtonGroup;
class QGroupBox;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTimer;


namespace qsapecng
{


// What to plot, the frequency sweep and the units of the axes: the form
// of Run > New Plot and of the Plot Settings panel (which applies every
// change at once).
class PlotSettingsWidget: public QWidget
{

  Q_OBJECT

public:
  PlotSettingsWidget(const PlotSettings& settings, QWidget* parent = 0);

  PlotSettings settings() const;
  void setSettings(const PlotSettings& settings);   // no changed()
  bool isValid() const;
  // the outputs of the circuit; the choice is shown when there are two or
  // more (no changed())
  void setOutputs(const QStringList& names);
  // the Smith chart only when the output is an impedance (a current
  // source drives the circuit): otherwise its button is greyed out, and
  // with moveAway a Smith chart chosen becomes a Bode plot (no changed())
  void setSmithAllowed(bool allowed, bool moveAway);

signals:
  // an edit (typing is reported when it ends; spin steps a moment later)
  void changed();

private slots:
  void update();

private:
  QButtonGroup* kind_;   // one square button per kind of plot
  QLabel* outputLabel_;
  QComboBox* output_;
  QCheckBox* autoRange_;
  QLineEdit* start_;
  QLineEdit* stop_;
  QSpinBox* points_;
  QCheckBox* angular_;
  QCheckBox* xLog_;
  QCheckBox* db_;
  QCheckBox* yLog_;
  QCheckBox* degrees_;
  QGroupBox* axesBox_;
  QGroupBox* smithBox_;     // shown for the Smith chart
  QLineEdit* z0_;
  QCheckBox* admittance_;
  QCheckBox* returnLoss_;
  QCheckBox* magAuto_;
  QLineEdit* magMin_;
  QLineEdit* magMax_;
  QCheckBox* phaseAuto_;
  QLineEdit* phaseMin_;
  QLineEdit* phaseMax_;
  QWidget* magRange_;
  QWidget* phaseRange_;
  QTimer* settle_;
  bool filling_ = false;

};


// Run > New Plot
class PlotDialog: public QDialog
{

  Q_OBJECT

public:
  PlotDialog(const PlotSettings& settings, bool newPlot, QWidget* parent = 0);

  PlotSettings settings() const { return form_->settings(); }
  void setOutputs(const QStringList& names, const QString& current);
  void setSmithAllowed(bool allowed) { form_->setSmithAllowed(allowed, true); }

private:
  PlotSettingsWidget* form_;
  QDialogButtonBox* buttons_;

};


}


#endif // PLOTDIALOG_H
