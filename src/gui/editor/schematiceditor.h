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


#ifndef SCHEMATICEDITOR_H
#define SCHEMATICEDITOR_H

#include "gui/editor/schematicview.h"
#include "gui/editor/schematicscene.h"
#include "gui/editor/metacircuit_thread.hpp"
#include "gui/workplane/workplane.h"
#include "gui/resultpanel.h"

#include <QMutexLocker>
#include <QMdiSubWindow>


class QFocusEvent;


namespace qsapecng
{


class SchematicEditor: public QMdiSubWindow
{
  bool dirtyNotified_ = false;   // dirtyChanged sent since the last solve

  Q_OBJECT

public:
  SchematicEditor(QWidget* parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  SchematicEditor(
      SchematicScene& scene,
      QWidget* parent = 0,
      Qt::WindowFlags flags = Qt::WindowFlags()
    );
  // a solve still running (the symbolic result in the background) is
  // stopped first: its thread must not outlive it
  ~SchematicEditor() override;

  inline SchematicView& view() const { return *view_; }
  inline SchematicScene& scene() const { return *scene_; }

  inline bool isUntitled() const { return isUntitled_; }
  QString userFriendlyCurrentFile() const;
  QString currentFile() const;

  // solving: the quick phase (the schematic is locked) or the symbolic
  // one in the background (the schematic can be edited; that stops it)
  inline bool isRunning() const { return solver_.isRunning(); }
  inline bool isBusy() const
    { return solver_.isRunning() && solver_.phase() == MetaCircuit_Thread::Quick; }
  inline bool isSymbolicRunning() const
    { return solver_.isRunning() && solver_.phase() == MetaCircuit_Thread::Symbolic; }
  // while the symbolic phase runs: the trees found, of how many, how long
  inline long long treesFound() const { return solver_.treesFound(); }
  inline double treesToFind() const { return solver_.estimatedTrees(); }
  inline double solveTime() const { return solver_.elapsed(); }

  // the setting for the symbolic result (Settings > General > Solver)
  enum SymbolicMode { SymbolicAlways = 0, SymbolicAsk = 1, SymbolicNever = 2 };
  static SymbolicMode symbolicMode();
  // the time the symbolic result would take, roughly (from the speed of
  // the last one on this computer); 0 when not known
  double symbolicSeconds() const;
  // the most distinct terms a symbolic result may have here (the memory)
  static long long termLimit();
  // a running solve: 0 - 1000, and outputs finished of all
  inline int solveProgress() const { return solver_.progress(); }
  inline int outputsSolved() const { return solver_.outputsDone(); }
  inline int outputsToSolve() const { return solver_.outputTotal(); }
  // the last solve was stopped (Stop) before it was done
  inline bool wasStopped() const { return stopped_; }
  inline bool isSolved() const { return solved_; }
  // last result of Resolve; it may be out of date (see isSolved())
  inline bool hasResult() const { return hasResult_; }
  inline const SolveResult& lastResult() const { return lastResult_; }
  // forget the result shown (Result panel > Clear); the next Resolve
  // brings a new one
  void clearResult() { hasResult_ = false; lastResult_ = SolveResult(); }

  // the output can be read as an impedance (the Smith chart): a current
  // source drives the circuit, no voltage source does
  bool impedanceOutput() const;

  // the result for one output (by name; the first when empty or gone)
  // into a plot
  bool accept(WorkPlane& workplane, const QString& output = QString());
  // names of the outputs of the last result
  QStringList resultOutputs() const;

public slots:
  bool save();
  bool saveAs();
  bool saveFile(const QString& fileName);
  bool loadFile(const QString& fileName);
  // the circuit as a SPICE netlist; analysis: the .ac line (empty: a
  // default sweep)
  bool exportSpice(const QString& fileName, const QString& analysis = QString());
  void solve();
  void stop();
  // the symbolic result now (after the numeric one), without asking
  void computeSymbolic();

signals:
  void stackEditor(SchematicEditor* editor);
  void aboutToCloseEditor();
  
  void dirtyChanged(bool dirty);
  
  void fileSaved(const QString& fileName);
  void fileLoaded(const QString& fileName);
  void solved();
  void solvingStarted();
  void solvingStopped();

private slots:
  void reset();
  void fileNameChanged(const QString& fileName);
  void finished();
  void stateChanged(Qt::WindowStates oldState, Qt::WindowStates newState);
  void showUserDef(SchematicScene& scene);
  void cleanChanged(bool clean = false);
  void externalCleanChanged();

protected:
  void closeEvent(QCloseEvent* event);

private:
  void setDirty();
  void showResult();
  // after the quick phase: the symbolic phase, as the setting says
  void continueSymbolic(bool ask);
  void startSymbolic();
  void setCurrentFile(const QString& fileName);
  QString strippedName(const QString& fullFileName) const;
  bool maybeSave();
  void init();

private:
  MetaCircuit_Thread solver_;
  bool solved_;
  // the numeric result of each output (the plots use it; the solver is
  // busy with the symbolic one meanwhile)
  struct NumericOutput
  {
    std::string name;
    sapecng::metacircuit::expression num, den;
    bool singular = true;
  };
  std::vector<NumericOutput> numeric_;
  bool stopped_ = false;
  bool expectFinish_ = false;   // a run was started and is not handled yet
  bool hasResult_ = false;
  SolveResult lastResult_;

  SchematicView* view_;
  SchematicScene* scene_;

  QString curFile_;
  bool isUntitled_;
  bool externalCleanChanged_;

};


}


#endif // SCHEMATICEDITOR_H
