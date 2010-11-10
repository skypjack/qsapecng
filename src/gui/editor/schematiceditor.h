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

#include <QtCore/QMutexLocker>
#include <QtGui/QMdiSubWindow>


class QFocusEvent;


namespace qsapecng
{


class SchematicEditor: public QMdiSubWindow
{

  Q_OBJECT

public:
  SchematicEditor(QWidget* parent = 0, Qt::WindowFlags flags = 0);

  SchematicView& view() const { return *view_; }
  SchematicScene& scene() const { return *scene_; }

  inline bool isUntitled() const { return isUntitled_; }
  QString userFriendlyCurrentFile() const;
  QString currentFile() const;

  bool isRunning() const { return solver_.isRunning(); }
  bool isSolved() const { return solved_; }

  bool accept(WorkPlane& workplane);

public slots:
  bool save();
  bool saveAs();
  bool saveFile(const QString& fileName);
  bool loadFile(const QString& fileName);
  void solve();

  void cleanChanged(bool clean);

signals:
  void fileSaved(const QString& fileName);
  void fileLoaded(const QString& fileName);
  void solved();

private slots:
  void reset();
  void setDirty() { solved_ = false; }
  void fileNameChanged(const QString& fileName);
  void finished();
  void stateChanged(Qt::WindowStates oldState, Qt::WindowStates newState);

protected:
  void closeEvent(QCloseEvent* event);

private:
  void showResult();
  void setCurrentFile(const QString& fileName);
  QString strippedName(const QString& fullFileName) const;
  bool maybeSave();

private:
  MetaCircuit_Thread solver_;
  bool solved_;

  SchematicView* view_;
  SchematicScene* scene_;

  QString curFile_;
  bool isUntitled_;

};


}


#endif // SCHEMATICEDITOR_H
