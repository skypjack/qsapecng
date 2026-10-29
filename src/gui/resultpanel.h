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


#ifndef RESULTPANEL_H
#define RESULTPANEL_H


#include "gui/formula.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QWidget>


class QTabBar;
class QTextBrowser;
class QLabel;
class QUrl;


namespace qsapecng
{


// The transfer function to one output (Out marker, voltmeter, ammeter).
struct OutputResult
{
  QString name;              // Out1, VM1 ...; empty when the circuit has none
  formula::Fraction mixed;   // "Result": symbolic components by name
  formula::Fraction raw;     // every component by name
  formula::Fraction digit;   // numeric
  bool singular = false;     // no unique solution: the fractions mean nothing
};


// Output of one Resolve: one result per output.
struct SolveResult
{
  QString file;
  QList<OutputResult> outputs;

  // the symbolic part (Result, Raw): there, or why it is not
  enum Symbolic
  {
    SymbolicDone,      // there
    SymbolicRunning,   // being worked out in the background
    SymbolicSkipped,   // the user said not now (it would take long)
    SymbolicStopped,   // stopped (Stop, or the circuit was changed)
    SymbolicOff,       // Settings: numeric only
    SymbolicTooLarge   // more terms than the memory holds
  };
  Symbolic symbolic = SymbolicDone;
  double trees = 0.;     // the terms it adds up (estimated)
  double seconds = 0.;   // the time it takes, roughly (0: not known)
  long long terms = 0;   // SymbolicTooLarge: the terms when it stopped

  bool anySingular() const
    {
      for(const OutputResult& o : outputs)
        if(o.singular)
          return true;
      return false;
    }
};


// Shows the result of the active schematic as fractions, with a copy
// link per expression and a notice when the circuit changed since.
class ResultPanel: public QWidget
{

  Q_OBJECT

public:
  explicit ResultPanel(QWidget* parent = 0);

  void showResult(const SolveResult& result, bool outOfDate);

  // a time, the same way wherever one is shown: "3h 50min", "50min", "40s"
  static QString shortTime(double seconds);

  // while the symbolic result is worked out: the time left, as shown
  // under Result ("Calculating... 3h 50min left.")
  void setTimeLeft(double seconds);
  // the "changed since solved" notice only (redrawn when it changes)
  void setOutOfDate(bool outOfDate);
private:
  void updateNotice();
public:
  void showPlaceholder();

  // the output whose result is shown (one tab per output when there are
  // several)
  QString currentOutput() const;

signals:
  // a Plot link was clicked (WorkPlane::Kind)
  void plotRequested(int function);
  // right-click > Clear
  void clearRequested();
  // a "Compute it" link (the symbolic result) was clicked
  void symbolicRequested();

protected:
  void changeEvent(QEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

private slots:
  void linkClicked(const QUrl& url);

private:
  void render();
  void updateTabs();
  const OutputResult* current() const;

  QTabBar* tabs_;
  QString currentName_;   // kept across results
  QTextBrowser* view_;
  QLabel* notice_ = nullptr;   // "out of date", above the result
  bool sideBySide_ = false;
  // long sections shown in full ("output/section"), kept across results
  QSet<QString> expanded_;
  SolveResult result_;
  QString timeLeft_;   // see setTimeLeft()
  bool hasResult_;
  bool outOfDate_;

};


}


#endif // RESULTPANEL_H
