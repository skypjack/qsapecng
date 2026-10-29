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


#ifndef QSAPECNGWINDOW_H
#define QSAPECNGWINDOW_H


#include "gui/theme.h"

#include <QMainWindow>
#include <QModelIndex>
#include <QPointer>
#include <QHash>
#include <QList>

#include "gui/workplane/workplane.h"


class QWidget;

class QProgressBar;
class QTimer;
class QLabel;

class QMdiArea;
class QMdiSubWindow;

class QTabWidget;
class QMainWindow;

class QTextEdit;
class QLineEdit;
class QPushButton;
class QAction;
class QMenu;

class QToolBar;
class QDockWidget;
class QScrollArea;

class QSignalMapper;


class QUndoView;
class QUndoGroup;

class QFileSystemModel;
class QTreeView;


namespace qsapecng
{


class LogViewer;
class ResultPanel;


class QLogPolicy;

class SchematicEditor;
class SideBarModel;
class PlotSettingsWidget;

class Item;


class QSapecNGWindow: public QMainWindow
{

  Q_OBJECT

public:
  QSapecNGWindow();
  // the sub-windows are closed while this is torn down: their signals
  // must not reach it any more (see the destructor)
  ~QSapecNGWindow() override;
  
  friend QSapecNGWindow& operator<<(
      QSapecNGWindow& window,
      const QString& str
    );

protected:
  void closeEvent(QCloseEvent* event);
  void changeEvent(QEvent* event) override;

private slots:
  void toggleScreenMode();

  void configDialog();
  void refreshSchematics();
  void changeWorkspace();

  void zoomIn();
  void zoomOut();
  void zoomNormal();
  void fit();

  void wave();
  void showPlots();
  void assignNodes();
  void resolve();
  void stopSolving();
  void updateSolveProgress();
  void plot(int f);
  void xAxisLogScale(bool log);
  void yAxisLogScale(bool log);
  void replot();

  void newFile();
  void open(const QString& fileName);
  void open();
  void openCrc();
  void openRecent();
  void save();
  void saveAs();
  void print();
  void printPlot();
  void editPlotSettings();
  void exportSchematic();
  void exportSpice();

  void props();

  void bin();
  void cut();
  void copy();
  void paste();
  void rotate();
  void mirror();

  void license();
  void about();

  void userDefRequested();
  void disconnectedWireSessionRequested();
  void groundSessionRequested();
  void portSessionRequested();
  void outSessionRequested();
  void voltmeterSessionRequested();
  void ammeterSessionRequested();
  void labelSessionRequested();
  void clearSessionRequested();

  void activated(const QModelIndex& index);
  void sceneSelectionChanged();
  void updateMenus();
  void updateWindowMenu();
  void updateDocks();

  void setActiveSubWindow(QWidget* window);
  void syncWithCurrentEditor();
  void updateResultPanel();
  void updateStatus();
  void appearanceChanged();
  void subWindowActivated(QMdiSubWindow* window);
  void stackEditor(SchematicEditor* editor);
  
signals:
  void toggleGrid(bool visible);

private:
  void createExamplesMenu(QMenu* parent);
  void createLogger();
  void createActions();
  void createDockWidgets();
  void createMenus();
  void createToolBars();
  void createStatusBar();

  void createMessageDockLayout();
  void createUndoRedoDockLayout();
  void createWorkspaceDockLayout();

  void readSettings();
  void writeSettings();

  SchematicEditor* activeEditor();
  QMdiSubWindow* findEditor(const QString& fileName);
  SchematicEditor* createSchematicEditor();
  void setupSchematicEditor(SchematicEditor* editor);

  void updateRecentFileList(const QString& fileName);
  void loadRecentFileList();

private:
  QProgressBar* progressBar_;
  // where Resolve was pressed last (the Plots window is kept in front
  // once the result is in, not when a background solve ends later)
  enum ResolvedFrom { FromNowhere, FromMain, FromPlots };
  ResolvedFrom resolvedFrom_ = FromNowhere;
  void keepPlotsInFront();
  void updateSmithAllowed();
  QTimer* progressTimer_ = nullptr;
  QLabel* statusLabel_;
  QLabel* countLabel_;
  QLabel* zoomLabel_;
  QLabel* positionLabel_;

  SideBarModel* sideBarModel_;


  // version of the saved dock/toolbar layout: bump it when the default
  // layout changes, so layouts saved by older versions are replaced once
  enum { LayoutVersion = 3 };

  QLogPolicy* policy_;
  LogViewer* logViewer_;

  QUndoView* undoRedoView_;
  QUndoGroup* undoRedoGroup_;

  QLineEdit* workspace_;
  QFileSystemModel* wfsModel_;
  QTreeView* wfsView_;

  QMdiArea* mdiArea_;
  QSignalMapper* windowMapper_;

  // plots: one dock each, bound to the schematic they show
  struct PlotEntry
  {
    QPointer<WorkPlane> plane;
    QPointer<SchematicEditor> editor;
  };
  QList<PlotEntry> plots_;
  QPointer<WorkPlane> currentPlot_;
  QHash<SchematicEditor*, PlotSettings> pendingPlots_;   // after solving
  // plots live in their own window, one tab each
  QMainWindow* plotWindow_ = nullptr;
  QTabWidget* plotTabs_ = nullptr;
  QDockWidget* plotSettingsDock_ = nullptr;
  QDockWidget* markersDock_ = nullptr;
  QScrollArea* plotSettingsScroll_ = nullptr;
  PlotSettingsWidget* plotSettingsForm_ = nullptr;
  QAction* showPlotsAct_ = nullptr;
  void createPlotWindow();
  void showPlotWindow();
  WorkPlane* createPlot(SchematicEditor* editor, const PlotSettings& settings);
  void updatePlotTitle(WorkPlane* plane);
  void refreshPlots(SchematicEditor* editor);
  // names of the outputs a plot can show (its schematic's last result)
  QStringList plotOutputs(WorkPlane* plane) const;
  // the Markers dock shows this plot's markers
  void showMarkersOf(WorkPlane* plane);
  // keeps the Plot Settings panel as wide as its form
  void fitPlotSettingsWidth();
  // the last used plot settings, with what the schematic's file asks for
  // (a sweep, and with `kind` the kind of plot)
  PlotSettings plotDefaults(SchematicEditor* editor, bool kind) const;
  // the result of the schematic changes with frequency (s appears in it)
  bool hasFrequencyResponse(SchematicEditor* editor) const;
  // the schematic's file names a kind of plot (plot.kind): its plot opens
  // with the first result (the examples)
  bool fileAsksForPlot(SchematicEditor* editor) const;
  // a schematic's first plot: the kind its file asks for, or a Bode plot,
  // of the first output with a transfer function
  PlotSettings firstPlot(SchematicEditor* editor) const;
  // the Plots window made taller (within the screen) when the Plot
  // Settings panel would need a vertical scroll bar
  // natural: the size it opens with, once a session (it may shrink):
  // just tall enough for the settings, the plot 1.25 times as wide as high
  void fitPlotWindowHeight(bool natural = false);
  bool plotWindowSized_ = false;
  bool plotWindowOpening_ = false;   // just shown: its size stays


  QMenu* fileMenu_;
  QMenu* recentMenu_;
  QMenu* editMenu_;
  QMenu* viewMenu_;
  QMenu* runMenu_;
  QMenu* workplaneMenu_;
  QMenu* toolMenu_;
  QMenu* settingMenu_;
  QMenu* toolbarMenu_;
  QMenu* windowMenu_;
  QMenu* showMenu_;
  QMenu* helpMenu_;
  QMenu* sceneMenu_;

  QToolBar* fileToolBar_;
  QToolBar* viewToolBar_;
  QToolBar* runToolBar_;
  QToolBar* toolToolBar_;
  QToolBar* partsToolBar_ = nullptr;
  QList<QAction*> partActions_;   // one per row of the parts model
  void refreshPartIcons();

  QDockWidget* messageDock_;
  QDockWidget* undoRedoDock_;
  QDockWidget* workspaceDock_;
  QDockWidget* resultDock_;
  ResultPanel* resultPanel_;

  QAction* newAct_;
  QAction* openAct_;
  QAction* openCrcAct_;
  QAction* saveAct_;
  QAction* saveAsAct_;
  QAction* printAct_;
  QAction* exportAct_;
  QAction* exportSpiceAct_;
  QAction* quitAct_;
  QAction* binAct_;
  QAction* undoAct_;
  QAction* redoAct_;
  QAction* propsAct_;
  QAction* cutAct_;
  QAction* copyAct_;
  QAction* pasteAct_;
  QAction* rotateAct_;
  QAction* mirrorAct_;
  QAction* flipVAct_;
  QAction* zoomInAct_;
  QAction* zoomOutAct_;
  QAction* zoomNormalAct_;
  QAction* fitAct_;
  QAction* nodeAct_;
  QAction* resolveAct_;
  QAction* stopAct_;
  QAction* waveAct_;
  QAction* plotsAct_;
  QAction* toggleScreenModeAct_;
  QAction* configureAppAct_;
  QAction* closeAct_;
  QAction* closeAllAct_;
  QAction* tileAct_;
  QAction* cascadeAct_;
  QAction* nextAct_;
  QAction* previousAct_;
  QAction* separatorAct_;
  QAction* licenseAct_;
  QAction* aboutAct_;
  QAction* aboutQtAct_;
  QAction* userdefAct_;
  QAction* disconnectedWireSessionAct_;
  QAction* groundSessionAct_;
  QAction* portSessionAct_;
  QAction* outSessionAct_;
  QAction* voltmeterSessionAct_;
  QAction* ammeterSessionAct_;
  QAction* labelSessionAct_;
  QAction* cursorAct_;
  QAction* toggleGridAct_;

  QSignalMapper* fMapper_;
  QAction* vrangeAct_;
  QAction* plotSettingsAct_;
  QAction* clearMarkersAct_;
  QAction* markersPanelAct_;
  QAction* antialiasAct_;
  QAction* printPlotAct_;
  QAction* resetViewAct_;
  /* plotter noop acts */
  QAction* replotAct_;

  enum { maxRecentFiles = 10 };
  QAction* openRecentActs_[maxRecentFiles];

};


}


#endif // QSAPECNGWINDOW_H
