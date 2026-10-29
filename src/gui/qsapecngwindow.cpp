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
#include "gui/resultpanel.h"
#include "gui/plotdialog.h"
#include "gui/editor/component.h"
#include "config.h"

#include "exception/sapecngexception.h"

#include "gui/qsapecngwindow.h"

#include "gui/configdialog/configdialog.h"
#include "gui/settings.h"
#include "gui/qlogger.h"
#include "gui/delegate.h"

#include "gui/editor/schematiceditor.h"
#include "gui/editor/schematicview.h"
#include "gui/editor/schematicscene.h"
#include "gui/extendedmdiarea.h"
#include "gui/sidebarmodel.h"


#include <QProgressBar>
#include <QLocale>

#include <QMdiArea>
#include <QMdiSubWindow>

#include <QTabWidget>

#include <QMessageBox>
#include <QDialogButtonBox>

#include <QFile>
#include <QFileInfo>
#include <QScreen>
#include <QWidgetAction>
#include <QListWidget>
#include <cmath>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QScrollBar>
#include <QMainWindow>
#include <QFileDialog>
#include <QDir>
#include <QSettings>

#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QAction>
#include <QToolBox>
#include <QTextDocument>
#include <QGroupBox>
#include <QPlainTextEdit>

#include <QMenu>
#include <QMenuBar>
#include <QTimer>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>

#include <QSignalMapper>
#include <QActionGroup>
#include <QCloseEvent>
#include <QGraphicsItem>

#include <QUndoView>
#include <QUndoGroup>
#include <QUndoStack>

#include <QFileSystemModel>
#include <QTreeView>

#include <QPrinter>
#include <QPrintDialog>
#include <QPdfWriter>
#include <QSvgGenerator>
#include <QImage>
#include <QPageSize>

#include <QGridLayout>
#include <QVBoxLayout>
#include <QApplication>

#include <limits>


namespace qsapecng
{


namespace
{
// the saved layout of the Plots window: older layouts (the panels side by
// side or one above the other) are not restored
const int PlotWindowLayoutVersion = 1;
}


QSapecNGWindow::QSapecNGWindow()
{
  Settings().load();

  windowMapper_ = new QSignalMapper(this);
  connect(windowMapper_, &QSignalMapper::mappedObject,
    this, [this](QObject* obj) {
      setActiveSubWindow(qobject_cast<QWidget*>(obj));
    });

  fMapper_ = new QSignalMapper(this);
  connect(fMapper_, &QSignalMapper::mappedInt, this, [this](int id) {
      plot(id);
    });

  mdiArea_ = new ExtendedMdiArea;
  mdiArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mdiArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mdiArea_->setViewMode(QMdiArea::TabbedView);
  mdiArea_->setDocumentMode(true);
  mdiArea_->setTabsClosable(true);
  mdiArea_->setTabsMovable(true);

  connect(static_cast<ExtendedMdiArea*>(mdiArea_),
    SIGNAL(dropFileEvent(const QString&)), this, SLOT(open(const QString&)));
  connect(mdiArea_, &QMdiArea::subWindowActivated,
    this, &QSapecNGWindow::syncWithCurrentEditor);

  createLogger();
  createDockWidgets();
  createActions();
  createToolBars();
  createStatusBar();
  createMenus();

  // the schematics fill the window; plots open in their own docks
  setCentralWidget(mdiArea_);

  messageDock_->setObjectName("messageDock");
  undoRedoDock_->setObjectName("undoRedoDock");
  workspaceDock_->setObjectName("workspaceDock");

  fileToolBar_->setObjectName("fileToolBar");
  viewToolBar_->setObjectName("viewToolBar");
  runToolBar_->setObjectName("runToolBar");
  toolToolBar_->setObjectName("toolToolBar");

  readSettings();

  sceneSelectionChanged();
  updateMenus();
  updateDocks();
  loadRecentFileList();
  updateStatus();

  setWindowIcon(QApplication::windowIcon());
  setWindowTitle(QString("%1 - %2")
      .arg(PACKAGE_NAME)
      .arg(PACKAGE_VERSION)
    );
  setUnifiedTitleAndToolBarOnMac(true);
}


void QSapecNGWindow::closeEvent(QCloseEvent* event)
{
  mdiArea_->closeAllSubWindows();
  mdiArea_->activateNextSubWindow();

  if(activeEditor()) {
    event->ignore();
  } else {
    writeSettings();
    Settings().save();
    if(plotWindow_) {
      QSettings().setValue(QStringLiteral("PlotWindow/geometry"),
        plotWindow_->saveGeometry());
      QSettings().setValue(QStringLiteral("PlotWindow/state"),
        plotWindow_->saveState(PlotWindowLayoutVersion));
      plotWindow_->close();
      delete plotWindow_;
      plotWindow_ = nullptr;
    }
    event->accept();
  }
}


void QSapecNGWindow::toggleScreenMode()
{
  setWindowState(windowState() ^ Qt::WindowFullScreen);
}


void QSapecNGWindow::refreshSchematics()
{
  // redraw every open schematic (grid, symbols)
  foreach(QMdiSubWindow* window, mdiArea_->subWindowList())
    if(SchematicEditor* editor = qobject_cast<SchematicEditor*>(window)) {
      editor->view().resetCachedContent();
      editor->scene().update();
      editor->view().viewport()->update();
    }
}


void QSapecNGWindow::configDialog()
{
  ConfigDialog configDialog(this);
  connect(&configDialog, &ConfigDialog::appearanceApplied,
    this, &QSapecNGWindow::appearanceChanged);
  configDialog.exec();

  Settings settings;

  QApplication::setFont(settings.appFont());

  QLogger::setLevel((sapecng::Logger::LogLevel) settings.logLvl());
  policy_->setDebugColor(settings.debugColor());
  policy_->setInfoColor(settings.infoColor());
  policy_->setWarningColor(settings.warningColor());
  policy_->setErrorColor(settings.errorColor());
  policy_->setFatalColor(settings.fatalColor());
}


void QSapecNGWindow::changeWorkspace()
{
  QString dir = QFileDialog::getExistingDirectory(
    this, tr("Choose directory"), workspace_->text(),
    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if(!dir.isEmpty()) {
    workspace_->setText(dir);
    wfsView_->setRootIndex(wfsModel_->setRootPath(dir));
    SettingsManager().setWorkspace(dir);
  }
}


void QSapecNGWindow::zoomIn()
{
  SchematicEditor* editor = activeEditor();
  if(editor) {
    editor->view().zoomIn();
    zoomOutAct_->setEnabled(true);
    zoomNormalAct_->setEnabled(true);
  }
}


void QSapecNGWindow::zoomOut()
{
  SchematicEditor* editor = activeEditor();
  if(editor)
    editor->view().zoomOut();
}


void QSapecNGWindow::zoomNormal()
{
  SchematicEditor* editor = activeEditor();
  if(editor)
    editor->view().normalSize();
}


void QSapecNGWindow::fit()
{
  SchematicEditor* editor = activeEditor();
  if(editor)
    editor->view().fitToView();
}


void QSapecNGWindow::showPlots()
{
  SchematicEditor* editor = activeEditor();
  if(!editor)
    return;

  // the most recent plot of this schematic to the front
  QWidget* last = nullptr;
  for(const PlotEntry& e : plots_)
    if(e.editor == editor && e.plane)
      last = e.plane;
  if(!last) {
    // none yet: a Bode plot (or what the file asks for) at once; New Plot
    // asks for the kind
    if(editor->isSolved()) {
      createPlot(editor, firstPlot(editor));
    } else {
      // solve first; the plot opens when the result is in
      editor->setProperty("plotOnSolve", true);
      resolve();
    }
    return;
  }
  plotTabs_->setCurrentWidget(last);
  showPlotWindow();
}


void QSapecNGWindow::wave()
{
  // Plot > New Plot (Ctrl+F9)
  SchematicEditor* editor = activeEditor();
  if(!editor)
    return;

  // asked from the Plots window: the dialog belongs to it, so the Plots
  // window stays in front when the dialog closes
  QWidget* owner = (plotWindow_ && plotWindow_->isVisible()
      && plotWindow_->isActiveWindow()) ? static_cast<QWidget*>(plotWindow_) : this;
  PlotDialog dialog(plotDefaults(editor, false), true, owner);
  // the output shown in the Result panel, unless the schematic changed
  // its outputs since
  dialog.setOutputs(editor->scene().outputNames(), resultPanel_->currentOutput());
  dialog.setSmithAllowed(editor->impedanceOutput());
  if(dialog.exec() != QDialog::Accepted)
    return;
  const PlotSettings settings = dialog.settings();
  settings.save();

  if(editor->isSolved())
    createPlot(editor, settings);
  else {
    // solve first; the plot opens when the result is in
    pendingPlots_[editor] = settings;
    editor->solve();
  }
}


WorkPlane* QSapecNGWindow::createPlot(SchematicEditor* editor,
    const PlotSettings& settings)
{
  WorkPlane* plane = new WorkPlane;
  plane->setContextMenu(workplaneMenu_);
  plane->setLeadingContextActions({ resetViewAct_ });
  if(!editor->accept(*plane, settings.output)) {
    delete plane;
    return 0;
  }
  plane->applySettings(settings);

  createPlotWindow();
  // the first marker of the plot shown opens the Markers panel
  connect(plane, &WorkPlane::markersPanelChanged, this, [this, plane](bool on) {
      if(plane == currentPlot_ && on && markersDock_)
        markersDock_->raise();
    });
  PlotEntry entry{ plane, editor };
  plots_.append(entry);
  const int index = plotTabs_->addTab(plane, QString());
  updatePlotTitle(plane);

  connect(plane, &QObject::destroyed, this, [this, plane]() {
      for(int i = 0; i < plots_.size(); ++i)
        if(plots_.at(i).plane.isNull() || plots_.at(i).plane == plane)
          plots_.removeAt(i--);
      if(currentPlot_.isNull() || currentPlot_ == plane)
        currentPlot_ = qobject_cast<WorkPlane*>(plotTabs_->currentWidget());
      updateMenus();
    });

  plotTabs_->setCurrentIndex(index);
  showPlotWindow();
  currentPlot_ = plane;
  updateMenus();
  return plane;
}


void QSapecNGWindow::createPlotWindow()
{
  if(plotWindow_)
    return;

  // a top-level window of its own (own taskbar entry, can go to another
  // screen); closed with the main window
  plotWindow_ = new QMainWindow(nullptr);
  plotWindow_->setWindowTitle(tr("Plots - %1").arg(PACKAGE_NAME));
  plotWindow_->setWindowIcon(windowIcon());
  plotWindow_->setAttribute(Qt::WA_QuitOnClose, false);

  plotTabs_ = new QTabWidget;
  plotTabs_->setDocumentMode(true);
  plotTabs_->setTabsClosable(true);
  plotTabs_->setMovable(true);
  plotWindow_->setCentralWidget(plotTabs_);
  connect(plotTabs_, &QTabWidget::tabCloseRequested, this, [this](int index) {
      QWidget* page = plotTabs_->widget(index);
      plotTabs_->removeTab(index);
      delete page;
      // the window stays, also with no plot left (New Plot is there)
    });
  connect(plotTabs_, &QTabWidget::currentChanged, this, [this]() {
      currentPlot_ = qobject_cast<WorkPlane*>(plotTabs_->currentWidget());
      showMarkersOf(currentPlot_);
      // nothing to set with no plot left
      if(plotSettingsForm_)
        plotSettingsForm_->setEnabled(currentPlot_ != nullptr);
      if(currentPlot_ && plotSettingsForm_) {
        plotSettingsForm_->setOutputs(plotOutputs(currentPlot_));
        fitPlotSettingsWidth();
        plotSettingsForm_->setSettings(currentPlot_->settings());
        updateSmithAllowed();
        fitPlotWindowHeight(false);
      }
      updateMenus();
    });

  // the Plot menu and its shortcuts work here as in the main window
  plotWindow_->menuBar()->addMenu(workplaneMenu_);
  QToolBar* bar = plotWindow_->addToolBar(tr("Plot"));
  bar->setObjectName(QStringLiteral("plotToolBar"));
  bar->setMovable(false);
  bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  bar->addAction(waveAct_);
  // Resolve the schematic of the plot shown (F8 here does the same)
  QAction* plotResolve = new QAction(theme::icon("resolve"), tr("&Resolve"),
    plotWindow_);
  plotResolve->setToolTip(tr("Resolve the schematic of this plot again (F8)"));
  connect(plotResolve, &QAction::triggered, this, [this]() {
      // the plots stay in front (see keepPlotsInFront)
      resolvedFrom_ = FromPlots;
      for(const PlotEntry& e : plots_)
        if(e.plane == currentPlot_ && e.editor) {
          e.editor->solve();
          return;
        }
      resolve();
    });
  plotResolve->setShortcut(Qt::Key_F8);
  bar->addAction(plotResolve);
  bar->addAction(stopAct_);
  plotWindow_->addAction(stopAct_);   // Shift+F8 here too
  {
    // and in its menu
    QMenu* run = new QMenu(tr("&Run"), plotWindow_);
    run->addAction(plotResolve);
    run->addAction(stopAct_);
    plotWindow_->menuBar()->addMenu(run);
  }
  bar->addSeparator();
  bar->addAction(resetViewAct_);
  bar->addSeparator();
  bar->addAction(clearMarkersAct_);
  bar->addSeparator();
  bar->addAction(printPlotAct_);

  // settings of the current plot, applied as they are changed
  plotSettingsForm_ = new PlotSettingsWidget(PlotSettings::load());
  QScrollArea* scroll = new QScrollArea;
  scroll->setWidget(plotSettingsForm_);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  // only up and down: the panel is kept as wide as the form
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  plotSettingsScroll_ = scroll;
  QWidget* holder = new QWidget;
  QVBoxLayout* holderLayout = new QVBoxLayout(holder);
  holderLayout->setContentsMargins(8, 8, 8, 8);
  holderLayout->addWidget(scroll);
  plotSettingsDock_ = new QDockWidget(tr("Plot Settings"), plotWindow_);
  plotSettingsDock_->setObjectName(QStringLiteral("plotSettingsDock"));
  plotSettingsDock_->setWidget(holder);
  plotSettingsDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  // always there (no closing, no floating: a floated panel only jumped a
  // little, as its own window); it can go to the left side
  plotSettingsDock_->setFeatures(QDockWidget::DockWidgetMovable);
  plotWindow_->addDockWidget(Qt::RightDockWidgetArea, plotSettingsDock_);

  // the markers of the plot shown (each plot has its own list)
  markersDock_ = new QDockWidget(tr("Markers"), plotWindow_);
  markersDock_->setObjectName(QStringLiteral("markersDock"));
  markersDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  markersDock_->setFeatures(QDockWidget::DockWidgetMovable);
  plotWindow_->addDockWidget(Qt::RightDockWidgetArea, markersDock_);
  // the two panels share the right side as tabs (Plot Settings, Markers)
  plotWindow_->tabifyDockWidget(plotSettingsDock_, markersDock_);
  plotWindow_->setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);
  plotWindow_->setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
  // the tabs name the panels: no title bars above them
  plotSettingsDock_->setTitleBarWidget(new QWidget(plotSettingsDock_));
  markersDock_->setTitleBarWidget(new QWidget(markersDock_));
  plotSettingsDock_->raise();
  fitPlotSettingsWidth();
  // its tab brought to the front
  connect(plotSettingsDock_, &QDockWidget::visibilityChanged,
    this, [this]() { fitPlotWindowHeight(false); });
  // the form changes height with the kind of plot (the Smith chart group)
  connect(plotSettingsForm_, &PlotSettingsWidget::changed,
    this, [this]() { fitPlotWindowHeight(false); });
  connect(plotSettingsForm_, &PlotSettingsWidget::changed, this, [this]() {
      if(!currentPlot_ || !plotSettingsForm_->isValid())
        return;
      const PlotSettings s = plotSettingsForm_->settings();
      s.save();
      // another output: its transfer function
      if(s.output != currentPlot_->output())
        for(const PlotEntry& e : plots_)
          if(e.plane == currentPlot_ && e.editor)
            e.editor->accept(*currentPlot_, s.output);
      currentPlot_->applySettings(s);
      updatePlotTitle(currentPlot_);
    });

  QSettings settings;
  plotWindow_->restoreState(
    settings.value(QStringLiteral("PlotWindow/state")).toByteArray(),
    PlotWindowLayoutVersion);
  if(plotWindow_->restoreGeometry(
      settings.value(QStringLiteral("PlotWindow/geometry")).toByteArray()))
  {
    // the size chosen last time stays (it only grows if the settings
    // form would not fit)
    plotWindowSized_ = true;
  } else {
    plotWindow_->resize(1000, 640);
    plotWindow_->move(geometry().center() - QPoint(500, 320));
  }
}


void QSapecNGWindow::showPlotWindow()
{
  createPlotWindow();
  if(plotWindow_->isMinimized())
    plotWindow_->showNormal();
  else
    plotWindow_->show();
  plotWindow_->raise();
  plotWindow_->activateWindow();
  // a size of its own (from last time) is kept as it is: no fitting while
  // the window comes up (the dock signals that it is shown, too)
  if(!plotWindowSized_)
    fitPlotWindowHeight(true);
  plotWindowSized_ = true;
  plotWindowOpening_ = true;
  QTimer::singleShot(500, this, [this]() { plotWindowOpening_ = false; });
}


void QSapecNGWindow::fitPlotSettingsWidth()
{
  // the panel is never narrower than the form (fields cut off, a
  // horizontal scroll bar); the form gets wider with the Output choice
  if(!plotSettingsScroll_ || !plotSettingsForm_)
    return;
  plotSettingsForm_->adjustSize();
  const int width = plotSettingsForm_->minimumSizeHint().width()
    + plotSettingsScroll_->verticalScrollBar()->sizeHint().width()
    + 2 * plotSettingsScroll_->frameWidth();
  plotSettingsScroll_->setMinimumWidth(width);
}


bool QSapecNGWindow::hasFrequencyResponse(SchematicEditor* editor) const
{
  // some output whose transfer function has s in it
  for(const OutputResult& o : editor->lastResult().outputs)
    if(!o.singular && !(formula::isConstant(o.digit.num)
        && formula::isConstant(o.digit.den)))
      return true;
  return false;
}


bool QSapecNGWindow::fileAsksForPlot(SchematicEditor* editor) const
{
  const QString kind = editor->scene().circuitProperties()
    .value(QStringLiteral("plot.kind")).toLower();
  return kind == QLatin1String("magnitude") || kind == QLatin1String("phase")
    || kind == QLatin1String("polezero") || kind == QLatin1String("bode")
    || kind == QLatin1String("smith");
}


PlotSettings QSapecNGWindow::firstPlot(SchematicEditor* editor) const
{
  PlotSettings s = plotDefaults(editor, true);
  if(!fileAsksForPlot(editor))
    s.kind = WorkPlane::BodeKind;
  for(const OutputResult& o : editor->lastResult().outputs)
    if(!o.singular) {
      s.output = o.name;
      break;
    }
  return s;
}


PlotSettings QSapecNGWindow::plotDefaults(SchematicEditor* editor,
  bool kind) const
{
  PlotSettings s = PlotSettings::load();
  if(!editor)
    return s;
  const QMap<QString, QString> p = editor->scene().circuitProperties();
  bool ok1 = false, ok2 = false;
  const double a = p.value(QStringLiteral("plot.fstart")).toDouble(&ok1);
  const double b = p.value(QStringLiteral("plot.fstop")).toDouble(&ok2);
  if(ok1 && ok2 && a > 0 && b > a) {
    s.autoRange = false;
    s.fStart = a;
    s.fStop = b;
  }
  bool ok = false;
  const int points = p.value(QStringLiteral("plot.points")).toInt(&ok);
  if(ok && points >= 10)
    s.pointsPerDecade = points;
  const double z0 = p.value(QStringLiteral("plot.z0")).toDouble(&ok);
  if(ok && z0 > 0)
    s.z0 = z0;
  if(kind) {
    static const QStringList kinds = { QStringLiteral("magnitude"),
      QStringLiteral("phase"), QStringLiteral("polezero"),
      QStringLiteral("bode"), QStringLiteral("smith") };
    const int k = kinds.indexOf(p.value(QStringLiteral("plot.kind")).toLower());
    if(k >= 0)
      s.kind = k;
  }
  return s;
}


void QSapecNGWindow::fitPlotWindowHeight(bool natural)
{
  // after the layout has settled: tall enough for the Plot Settings tab
  // (no scroll bar), no taller; then wide enough for a plot of sensible
  // proportions; all within the screen
  if(!natural && plotWindowOpening_)
    return;
  // the first sizing waits until the window is laid out and mapped
  QTimer::singleShot(natural ? 250 : 0, this, [this, natural]() {
      if(!plotWindow_ || !plotWindow_->isVisible() || plotWindow_->isMaximized()
          || plotWindow_->isFullScreen() || !plotSettingsScroll_
          || !plotSettingsForm_ || !plotTabs_)
        return;
      const QRect frame = plotWindow_->frameGeometry();
      const QRect inner = plotWindow_->geometry();
      const QScreen* screen = plotWindow_->screen();
      const QRect avail = screen ? screen->availableGeometry() : frame;
      const int chromeW = frame.width() - inner.width();
      const int chromeH = frame.height() - inner.height();

      // never more than 80 % of the screen either way
      const int maxW = avail.width() * 8 / 10, maxH = avail.height() * 8 / 10;

      // the settings form, measured even when the Markers tab is in front
      // (behind the Markers tab its own size is stale: the tab in front
      // has the size both get)
      int viewportH = plotSettingsScroll_->viewport()->height();
      if(!plotSettingsScroll_->isVisible() && markersDock_
          && markersDock_->widget() && markersDock_->widget()->isVisible())
      {
        const QMargins m = plotSettingsDock_->widget()->layout()
          ? plotSettingsDock_->widget()->layout()->contentsMargins()
          : QMargins();
        viewportH = markersDock_->widget()->height() - m.top() - m.bottom()
          - 2 * plotSettingsScroll_->frameWidth();
      }
      int grow = 0;
      // (a height this small is a panel not laid out yet: no measure)
      if(viewportH > 100) {
        const int missing = plotSettingsForm_->sizeHint().height()
          - viewportH;
        grow = qMin(missing, maxH - frame.height());
        if(!natural)
          grow = qMax(0, grow);       // later on it only grows
      }
      // and a plot of a good size: at least 480 pixels high
      if(natural)
        grow = qMax(grow, qMin(480 - plotTabs_->height(),
                               maxH - frame.height()));

      // the plot area 1.25 times as wide as it is high (at least, later)
      const double aspect = 1.25;
      const int plotH = plotTabs_->height() + grow;
      const int wantW = int(std::ceil(aspect * plotH));
      int widen = qMin(wantW - plotTabs_->width(), maxW - frame.width());
      if(!natural)
        widen = 0;   // later on only the height follows the form

      if(grow == 0 && widen == 0)
        return;
      const QSize size(inner.width() + widen, inner.height() + grow);
      plotWindow_->resize(size);
      // still on the screen: moved left / up when it reaches past the edge
      QPoint pos = frame.topLeft();
      pos.setX(qBound(avail.left(), pos.x(),
        avail.right() + 1 - (size.width() + chromeW)));
      pos.setY(qBound(avail.top(), pos.y(),
        avail.bottom() + 1 - (size.height() + chromeH)));
      if(pos != frame.topLeft())
        plotWindow_->move(pos);
    });
}


void QSapecNGWindow::showMarkersOf(WorkPlane* plane)
{
  // the dock shows one plot's panel at a time; the others wait, hidden
  // and without a parent (each plot deletes its own)
  if(!markersDock_)
    return;
  QWidget* panel = plane ? plane->markersPanel() : nullptr;
  QWidget* old = markersDock_->widget();
  if(old == panel)
    return;
  if(old) {
    old->hide();
    old->setParent(nullptr);
  }
  markersDock_->setWidget(panel);
  if(panel)
    panel->show();
}


void QSapecNGWindow::updatePlotTitle(WorkPlane* plane)
{
  for(const PlotEntry& e : plots_) {
    if(e.plane != plane)
      continue;
    static const char* kinds[] = {
      QT_TR_NOOP("Frequency response"), QT_TR_NOOP("Phase response"),
      QT_TR_NOOP("Poles and zeros"), QT_TR_NOOP("Bode plot"),
      QT_TR_NOOP("Smith chart") };
    const int k = qBound(0, plane->kind(), 4);
    QString title = tr(kinds[k]);
    if(e.editor && e.editor->resultOutputs().size() > 1)
      title += QStringLiteral(" - ") + (plane->output().isEmpty()
        ? e.editor->resultOutputs().first() : plane->output());
    if(e.editor)
      title += QStringLiteral(" - ")
        + QFileInfo(e.editor->currentFile()).fileName();
    const int index = plotTabs_->indexOf(plane);
    if(index >= 0) {
      plotTabs_->setTabText(index, title);
      plotTabs_->setTabToolTip(index, e.editor ? e.editor->currentFile() : QString());
    }
  }
}


QStringList QSapecNGWindow::plotOutputs(WorkPlane* plane) const
{
  for(const PlotEntry& e : plots_)
    if(e.plane == plane && e.editor)
      return e.editor->resultOutputs();
  return QStringList();
}


void QSapecNGWindow::refreshPlots(SchematicEditor* editor)
{
  // a new result: every plot of this schematic recomputes
  for(const PlotEntry& e : plots_)
    if(e.editor == editor && e.plane) {
      editor->accept(*e.plane, e.plane->output());
      e.plane->applySettings(e.plane->settings());
      updatePlotTitle(e.plane);
    }
}


// the Plot Settings panel: the Smith chart for the plot shown only when
// its schematic's output is an impedance (an existing Smith chart stays,
// with a note in its title)
void QSapecNGWindow::updateSmithAllowed()
{
  if(!plotSettingsForm_)
    return;
  SchematicEditor* editor = nullptr;
  for(const PlotEntry& e : plots_)
    if(e.plane == currentPlot_ && e.editor)
      editor = e.editor;
  plotSettingsForm_->setSmithAllowed(!editor || editor->impedanceOutput(),
    false);
}


void QSapecNGWindow::editPlotSettings()
{
  createPlotWindow();
  if(currentPlot_) {
    plotSettingsForm_->setOutputs(plotOutputs(currentPlot_));
    fitPlotSettingsWidth();
    plotSettingsForm_->setSettings(currentPlot_->settings());
    updateSmithAllowed();
  }
  plotSettingsDock_->raise();
  showPlotWindow();
}


void QSapecNGWindow::assignNodes()
{
  SchematicEditor* editor = activeEditor();
  if(editor) {
    QLogger::info(tr("Assigning nodes..."));
    editor->scene().assignNodes();
  }
}


// after a Resolve: the Plots window, if open, is not left behind the main
// window. Resolved there: it stays in front with the focus; resolved in
// the main window: it comes back over it, the focus staying there
void QSapecNGWindow::keepPlotsInFront()
{
  const ResolvedFrom from = resolvedFrom_;
  resolvedFrom_ = FromNowhere;
  if(from == FromNowhere || !plotWindow_ || !plotWindow_->isVisible()
      || plotWindow_->isMinimized())
    return;
  plotWindow_->raise();
  if(from == FromPlots)
    plotWindow_->activateWindow();
}


QSapecNGWindow::~QSapecNGWindow()
{
  // The schematics go first, while this window's members are still there:
  // left to the base class destructors, a schematic being destroyed told
  // this window (QObject::destroyed: the plots of it go too), whose lists
  // were freed already - heap corruption at exit. And the MDI area is not
  // to tell about sub-windows activated meanwhile
  if(mdiArea_) {
    disconnect(mdiArea_, nullptr, this, nullptr);
    delete mdiArea_;
    mdiArea_ = nullptr;
  }
}


void QSapecNGWindow::resolve()
{
  SchematicEditor* editor = activeEditor();
  if(editor) {
    if(resolvedFrom_ != FromPlots)
      resolvedFrom_ = FromMain;
    editor->solve();
    mdiArea_->setActiveSubWindow(0);
    mdiArea_->setActiveSubWindow(editor);
    syncWithCurrentEditor();
  }
}


void QSapecNGWindow::stopSolving()
{
  foreach(QMdiSubWindow* window, mdiArea_->subWindowList())
    if(SchematicEditor* editor = qobject_cast<SchematicEditor*>(window))
      editor->stop();
}


// while a solve runs: the progress bar and the status line follow the
// schematic in front (or any that is solving)
void QSapecNGWindow::updateSolveProgress()
{
  SchematicEditor* running = nullptr;
  if(mdiArea_->currentSubWindow())
    running = qobject_cast<SchematicEditor*>(mdiArea_->currentSubWindow());
  if(!running || !running->isRunning()) {
    running = nullptr;
    foreach(QMdiSubWindow* window, mdiArea_->subWindowList()) {
      SchematicEditor* editor = qobject_cast<SchematicEditor*>(window);
      if(editor && editor->isRunning()) {
        running = editor;
        break;
      }
    }
  }
  stopAct_->setEnabled(running != nullptr);
  if(!running) {
    progressTimer_->stop();
    return;
  }

  if(running->isSymbolicRunning()) {
    // the symbolic result, in the background: the trees found of those
    // estimated, and the time left at the speed so far
    const double toFind = running->treesToFind();
    const double found = double(running->treesFound());
    const double seconds = running->solveTime();
    const int value = running->solveProgress();
    // a decimal while under 10 % (a long solve stays there for a while)
    QString format = tr("Symbolic %1 %").arg(value < 100
      ? QLocale().toString(value / 10., 'f', 1) : QString::number(value / 10));
    // the time left: at the speed so far, or (the first seconds) as
    // estimated before it started
    double left = running->hasResult() && running->lastResult().seconds > 0.
      ? std::max(0., running->lastResult().seconds - seconds) : 0.;
    if(toFind > 0. && found > 0.) {
      const double share = found / toFind;
      if(share >= 0.99) {
        format += QStringLiteral(" · ") + tr("almost done");
        left = 0.;
      } else if(seconds > 5.) {
        left = seconds * (1. - share) / share;
        format += QStringLiteral(" · ")
          + tr("%1 left").arg(ResultPanel::shortTime(left));
      }
    }
    if(running == activeEditor())
      resultPanel_->setTimeLeft(left);
    if(progressBar_->isVisible()) {
      progressBar_->setRange(0, 1000);
      progressBar_->setValue(value);
      progressBar_->setFormat(format);
    }
    return;
  }

  // the quick phase: counting the trees, then the numeric result (a
  // second or so; no share of it to show)
  if(progressBar_->isVisible()) {
    progressBar_->setRange(0, 0);
    progressBar_->setFormat(QString());
  }
  const int total = running->outputsToSolve();
  QString text = tr("Solving...");
  if(total > 1)
    text += tr(" - %1 outputs").arg(total);
  text += tr(" - Shift+F8 stops");
  if(running == qobject_cast<SchematicEditor*>(mdiArea_->currentSubWindow()))
    statusLabel_->setText(text);
}


void QSapecNGWindow::plot(int f)
{
  if(currentPlot_)
    currentPlot_->plot(f);
}


void QSapecNGWindow::xAxisLogScale(bool log)
{
  if(currentPlot_)
    currentPlot_->xAxisLogScale(log);
}


void QSapecNGWindow::yAxisLogScale(bool log)
{
  if(currentPlot_)
    currentPlot_->yAxisLogScale(log);
}


void QSapecNGWindow::replot()
{
  if(currentPlot_)
    currentPlot_->redraw();
}


void QSapecNGWindow::newFile()
{
  QMdiSubWindow* subWindow = createSchematicEditor();
  subWindow->showMaximized();
  mdiArea_->setActiveSubWindow(subWindow);
  syncWithCurrentEditor();
}


void QSapecNGWindow::open(const QString& fileName)
{
  if(!fileName.isEmpty()) {
  
    QMdiSubWindow* existing = findEditor(fileName);
    if(existing) {
      mdiArea_->setActiveSubWindow(existing);
      syncWithCurrentEditor();
      return;
    }

    SchematicEditor* editor = createSchematicEditor();
    bool loaded = false;
    try {
      loaded = editor->loadFile(fileName);
    } catch (...) {
      QMessageBox::warning(this, tr("File corrupted"),
          tr("Error loading file.\n")
            .append("Something has gone wrong while reading the file %1.")
            .arg(fileName));
    }

    if(loaded) {
      updateRecentFileList(fileName);
      statusBar()->showMessage(tr("File loaded"), 2000);
      editor->showMaximized();
      mdiArea_->setActiveSubWindow(editor);
      syncWithCurrentEditor();
    } else {
      editor->setWindowModified(false);
      editor->close();
    }
  }
}


void QSapecNGWindow::open()
{
  QString fileName = QFileDialog::getOpenFileName(this,
      tr("Read file"), Settings().workspace(),
      QString("%1;;%2")
        .arg(tr("Info files (*.info)"))
        .arg(tr("XML files (*.xml)"))
    );

  open(fileName);
}


void QSapecNGWindow::openCrc()
{
  // File > Open Netlist: the parts are drawn in a row, wired to one bus
  // per node; saving writes the netlist back
  QString fileName = QFileDialog::getOpenFileName(this,
      tr("Open Netlist"), Settings().workspace(),
      tr("Netlist files (*.crc)")
    );

  if(!fileName.isEmpty()) {
    open(fileName);
    if(findEditor(fileName))
      QLogger::info(tr("Netlist %1: the schematic is drawn automatically; "
        "save it as .info to keep a drawing").arg(QFileInfo(fileName).fileName()));
  }
}


void QSapecNGWindow::openRecent()
{
  QAction* action = qobject_cast<QAction*>(sender());
  if(action) {
  
    QMdiSubWindow* existing = findEditor(action->data().toString());
    if(existing) {
      mdiArea_->setActiveSubWindow(existing);
      syncWithCurrentEditor();
      return;
    }

    SchematicEditor* editor = createSchematicEditor();
    bool loaded = false;
    try {
      loaded = editor->loadFile(action->data().toString());
    } catch (...) {
      QMessageBox::warning(this, tr("File corrupted"),
          tr("Error loading file.\n")
            .append("Something has gone wrong while reading the file %1.")
            .arg(action->data().toString()));
    }

    if(loaded) {
      updateRecentFileList(action->data().toString());
      statusBar()->showMessage(tr("File loaded"), 2000);
      editor->showMaximized();
      mdiArea_->setActiveSubWindow(editor);
      syncWithCurrentEditor();
    } else {
      editor->setWindowModified(false);
      editor->close();
    }
  }
}


void QSapecNGWindow::save()
{
  SchematicEditor* editor = activeEditor();
  if(editor) {
    editor->scene().assignNodes();
    if(editor->save()) {
      updateRecentFileList(editor->currentFile());
      statusBar()->showMessage(tr("File saved"), 2000);
    }
  }
}


void QSapecNGWindow::saveAs()
{
  SchematicEditor* editor = activeEditor();
  if(editor) {
    editor->scene().assignNodes();
    if(editor->saveAs()) {
      updateRecentFileList(editor->currentFile());
      statusBar()->showMessage(tr("File saved"), 2000);
    }
  }
}


namespace
{


// Draws a schematic as on paper: black on white, without the grid and the
// selection, whatever the theme.
void renderSchematic(SchematicScene& scene, QPainter* painter,
    const QRectF& target, const QRectF& source)
{
  const QList<QGraphicsItem*> selected = scene.selectedItems();
  const bool grid = scene.gridVisible();
  scene.clearSelection();
  scene.setGridVisible(false);

  {
    theme::PaperSheet paper;
    theme::refreshSheetText(&scene);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);
    scene.render(painter, target, source);
  }
  theme::refreshSheetText(&scene);

  scene.setGridVisible(grid);
  foreach(QGraphicsItem* item, selected)
    item->setSelected(true);
}


QRectF schematicArea(SchematicScene& scene)
{
  return scene.itemsBoundingRect().adjusted(-10, -10, 10, 10);
}


}


void QSapecNGWindow::exportSpice()
{
  SchematicEditor* editor = activeEditor();
  if(!editor)
    return;

  QFileInfo current(editor->currentFile());
  QString suggested = current.completeBaseName().isEmpty()
    ? tr("circuit") : current.completeBaseName();
  suggested = QFileInfo(Settings().workspace(), suggested + ".cir")
    .absoluteFilePath();

  QString fileName = QFileDialog::getSaveFileName(this,
    tr("Export SPICE netlist"), suggested,
    tr("SPICE netlist (*.cir *.sp *.net *.spi)"));
  if(fileName.isEmpty())
    return;
  if(QFileInfo(fileName).suffix().isEmpty())
    fileName += ".cir";

  // the sweep of the plots when it is set by hand, else 1 Hz to 1 MHz
  const PlotSettings plot = PlotSettings::load();
  QString analysis;
  if(!plot.autoRange && plot.fStart > 0 && plot.fStop > plot.fStart)
    analysis = QStringLiteral(".ac dec %1 %2 %3").arg(plot.pointsPerDecade)
      .arg(plot.fStart, 0, 'g', 6).arg(plot.fStop, 0, 'g', 6);

  if(editor->exportSpice(fileName, analysis))
    statusBar()->showMessage(tr("SPICE netlist written: %1")
      .arg(QDir::toNativeSeparators(fileName)), 5000);
  else
    QMessageBox::warning(this, tr("Export SPICE netlist"),
      tr("Unable to write file %1.").arg(fileName));
}


void QSapecNGWindow::exportSchematic()
{
  SchematicEditor* editor = activeEditor();
  if(!editor)
    return;

  const QString svgFilter = tr("SVG image (*.svg)");
  const QString pngFilter = tr("PNG image (*.png)");
  const QString pdfFilter = tr("PDF document (*.pdf)");

  QFileInfo current(editor->currentFile());
  QString suggested = current.completeBaseName().isEmpty()
    ? tr("schematic") : current.completeBaseName();
  suggested = QFileInfo(Settings().workspace(), suggested + ".svg")
    .absoluteFilePath();

  QString filter = svgFilter;
  QString fileName = QFileDialog::getSaveFileName(this, tr("Export schematic"),
    suggested, svgFilter + ";;" + pngFilter + ";;" + pdfFilter, &filter);
  if(fileName.isEmpty())
    return;

  QString suffix = QFileInfo(fileName).suffix().toLower();
  if(suffix != "svg" && suffix != "png" && suffix != "pdf") {
    suffix = filter == pngFilter ? "png" : filter == pdfFilter ? "pdf" : "svg";
    fileName += "." + suffix;
  }

  SchematicScene& scene = editor->scene();
  const QRectF source = schematicArea(scene);
  bool ok = true;

  if(suffix == "svg") {
    QSvgGenerator svg;
    svg.setFileName(fileName);
    svg.setSize(source.size().toSize());
    svg.setViewBox(QRectF(QPointF(0, 0), source.size()));
    svg.setTitle(QFileInfo(fileName).completeBaseName());
    svg.setDescription(tr("Schematic exported from %1").arg(PACKAGE_NAME));
    QPainter painter;
    ok = painter.begin(&svg);
    if(ok) {
      renderSchematic(scene, &painter,
        QRectF(QPointF(0, 0), source.size()), source);
      painter.end();
    }
  } else if(suffix == "png") {
    const qreal scale = 4.;   // ~ 300 dpi for a typical schematic
    QImage image((source.size() * scale).toSize(), QImage::Format_ARGB32);
    image.setDotsPerMeterX(11811);
    image.setDotsPerMeterY(11811);
    image.fill(Qt::white);
    QPainter painter(&image);
    renderSchematic(scene, &painter, QRectF(image.rect()), source);
    painter.end();
    ok = image.save(fileName, "PNG");
  } else {
    QPdfWriter pdf(fileName);
    pdf.setTitle(QFileInfo(fileName).completeBaseName());
    pdf.setCreator(PACKAGE_NAME);
    pdf.setResolution(300);
    // page sized to the drawing (scene units taken as 1/96 in)
    const QSizeF inches = source.size() / 96.;
    pdf.setPageSize(QPageSize(inches, QPageSize::Inch));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter painter;
    ok = painter.begin(&pdf);
    if(ok) {
      const QRect page = painter.viewport();
      renderSchematic(scene, &painter, QRectF(page), source);
      painter.end();
    }
  }

  if(ok)
    QLogger::info(tr("Schematic exported to %1").arg(fileName));
  else
    QLogger::error(tr("Could not write %1").arg(fileName));
}


void QSapecNGWindow::print()
{
  QPrinter printer;
  SchematicEditor* editor = activeEditor();
  if(editor) {
    if(QPrintDialog(&printer).exec() != QDialog::Accepted)
      return;

    // black on white, also when the dark theme is active
    QPainter painter(&printer);
    renderSchematic(editor->scene(), &painter, QRectF(),
      schematicArea(editor->scene()));
  }
}


void QSapecNGWindow::printPlot()
{
  if(!currentPlot_)
    return;
  QPrinter printer;
  if(QPrintDialog(&printer).exec() == QDialog::Accepted)
    currentPlot_->print(printer);
}


void QSapecNGWindow::props()
{
  // same dialog as double-clicking the component
  SchematicEditor* editor = activeEditor();
  if(!editor)
    return;

  foreach(QGraphicsItem* item, editor->scene().selectedItems())
    if(SchematicScene::itemProperties(item) || SchematicScene::isOutput(item)) {
      editor->scene().editItemProperties(item);
      return;
    }
}


void QSapecNGWindow::bin()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().binSelectedItems();
}


void QSapecNGWindow::cut()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().cutSelectedItems();
}


void QSapecNGWindow::copy()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().copySelectedItems();
}


void QSapecNGWindow::paste()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().pasteItems();
}


void QSapecNGWindow::rotate()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().rotatePlacedOrSelected();
}


void QSapecNGWindow::mirror()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().mirrorSelectedItems();
}


void QSapecNGWindow::license()
{
  QString title = tr("License");
  QString body =
    "QSapecNG, Qt-based GUI for SapecNG<br>"
    "Copyright (C) 2009, Michele Caini<br>"
    "Copyright (C) 2026, Wojciech Kaczmarski<br>"
    "<br>"
    "This program is free software: you can redistribute it and/or modify "
    "it under the terms of the GNU General Public License as published by "
    "the Free Software Foundation, either version 3 of the License, or "
    "(at your option) any later version.<br>"
    "<br>"
    "This program is distributed in the hope that it will be useful, "
    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
    "GNU General Public License for more details.<br>"
    "<br>"
    "You should have received a copy of the GNU General Public License "
    "along with this program.  If not, see "
    "<a href=\"http://www.gnu.org/licenses/\">http://www.gnu.org/licenses/</a>.";

  QMessageBox::about(this, title, body);
}


void QSapecNGWindow::appearanceChanged()
{
  // the settings are stored already (Configure > Appearance); redraw
  foreach(QMdiSubWindow* window, mdiArea_->subWindowList())
    if(SchematicEditor* editor = qobject_cast<SchematicEditor*>(window)) {
      editor->scene().updateResistorSymbols();
      editor->scene().refreshComponentLabels();
    }
  sideBarModel_->refreshSymbols();
  refreshSchematics();

  {
    QSignalBlocker block(antialiasAct_);
    antialiasAct_->setChecked(WorkPlane::antialiased());
  }
  for(const PlotEntry& e : plots_)
    if(e.plane)
      e.plane->applyAntialiasing();
}


void QSapecNGWindow::about()
{
  QMessageBox::about(this, tr("About %1").arg(PACKAGE_NAME),
    tr("<h3>%1 %2</h3>"
       "<p>Qt-based GUI for SapecNG, symbolic analysis of linear analog "
       "circuits.</p>"
       "<p><b>Authors</b><br>"
       "Michele Caini<br>"
       "Wojciech Kaczmarski</p>"
       "<p>%1 is based in part on the work of the "
       "<a href=\"https://qwt.sourceforge.io\">Qwt project</a>.</p>")
      .arg(PACKAGE_NAME, PACKAGE_VERSION)
  );
}


void QSapecNGWindow::changeEvent(QEvent* event)
{
  QMainWindow::changeEvent(event);

  if(event->type() == QEvent::PaletteChange) {
    // QMdiArea keeps the background brush it was created with, and the
    // component list holds pixmaps tinted for the old palette
    mdiArea_->setBackground(palette().brush(QPalette::Dark));
    sideBarModel_->refreshSymbols();
  }
}


void QSapecNGWindow::refreshPartIcons()
{
  for(int row = 0; row < partActions_.size(); ++row) {
    const QVariant d = sideBarModel_->data(sideBarModel_->index(row),
      Qt::DecorationRole);
    partActions_[row]->setIcon(d.canConvert<QIcon>() && !d.value<QIcon>().isNull()
      ? d.value<QIcon>() : QIcon(d.value<QPixmap>()));
  }
}


void QSapecNGWindow::activated(const QModelIndex& index)
{
  SchematicEditor* editor = activeEditor();

  // picking a component with no schematic open starts a new one
  if(!editor) {
    newFile();
    editor = activeEditor();
  }

  if(editor) {
    editor->scene().setActiveItem(
        (SchematicScene::SupportedItemType)
        sideBarModel_->data(index, Qt::UserRole).toInt()
      );
    // keys (Esc cancels, R rotates) go to the schematic from now on
    editor->view().setFocus(Qt::OtherFocusReason);
  }
}


void QSapecNGWindow::sceneSelectionChanged()
{
  bool hasSelectedItems = false;
  SchematicEditor* editor = activeEditor();

  // the Components toolbar shows the part being placed
  for(int row = 0; row < partActions_.size(); ++row)
    partActions_[row]->setChecked(editor
      && editor->scene().hasActiveItemType()
      && sideBarModel_->data(sideBarModel_->index(row), Qt::UserRole).toInt()
           == editor->scene().activeItemType());
  if(editor)
    hasSelectedItems =
      (editor->scene().selectedItems().size() != 0);

  propsAct_->setEnabled(hasSelectedItems);
  binAct_->setEnabled(hasSelectedItems);
  cutAct_->setEnabled(hasSelectedItems);
  copyAct_->setEnabled(hasSelectedItems);
  rotateAct_->setEnabled(hasSelectedItems
    || (editor && editor->scene().isPlacingItem()));
  mirrorAct_->setEnabled(hasSelectedItems
    || (editor && editor->scene().isPlacingItem()));
  flipVAct_->setEnabled(hasSelectedItems
    || (editor && editor->scene().isPlacingItem()));
}


void QSapecNGWindow::updateMenus()
{
  bool hasEditor = (activeEditor() != 0);
  const bool hasPlot = (currentPlot_ != 0);

  saveAct_->setEnabled(hasEditor);
  saveAsAct_->setEnabled(hasEditor);
  printAct_->setEnabled(hasEditor);
  exportAct_->setEnabled(hasEditor);
  exportSpiceAct_->setEnabled(hasEditor);

  pasteAct_->setEnabled(hasEditor);
  if(!hasEditor)
    undoRedoGroup_->setActiveStack(0);

  zoomInAct_->setEnabled(hasEditor);
  zoomOutAct_->setEnabled(hasEditor);
  zoomNormalAct_->setEnabled(hasEditor);
  fitAct_->setEnabled(hasEditor);

  nodeAct_->setEnabled(hasEditor);
  resolveAct_->setEnabled(hasEditor);
  waveAct_->setEnabled(hasEditor);
  plotsAct_->setEnabled(hasEditor);

  cursorAct_->setEnabled(hasEditor);
  userdefAct_->setEnabled(hasEditor);
  disconnectedWireSessionAct_->setEnabled(hasEditor);
  groundSessionAct_->setEnabled(hasEditor);
  portSessionAct_->setEnabled(hasEditor);
  outSessionAct_->setEnabled(hasEditor);
  voltmeterSessionAct_->setEnabled(hasEditor);
  ammeterSessionAct_->setEnabled(hasEditor);
  labelSessionAct_->setEnabled(hasEditor);

  closeAct_->setEnabled(hasEditor);
  closeAllAct_->setEnabled(hasEditor);
  tileAct_->setEnabled(hasEditor);
  cascadeAct_->setEnabled(hasEditor);
  nextAct_->setEnabled(hasEditor);
  previousAct_->setEnabled(hasEditor);

  plotSettingsAct_->setEnabled(hasPlot);
  vrangeAct_->setEnabled(hasPlot);
  resetViewAct_->setEnabled(hasPlot);
  clearMarkersAct_->setEnabled(hasPlot);
  markersPanelAct_->setEnabled(hasPlot);
  if(showPlotsAct_)
    showPlotsAct_->setEnabled(plotWindow_ != nullptr);
  printPlotAct_->setEnabled(hasPlot);
  replotAct_->setEnabled(hasPlot);
}


void QSapecNGWindow::updateWindowMenu()
{
  windowMenu_->clear();
  windowMenu_->addAction(showPlotsAct_);
  windowMenu_->addMenu(showMenu_);
  windowMenu_->addSeparator();
  windowMenu_->addAction(closeAct_);
  windowMenu_->addAction(closeAllAct_);
  // the schematics are tabs: no tiling or cascading; next / previous keep
  // their keys (the window's own actions) and the list below picks one
  windowMenu_->addAction(separatorAct_);

  QList<QMdiSubWindow*> windows = mdiArea_->subWindowList();
  separatorAct_->setVisible(!windows.isEmpty());

  for(int i = 0; i < windows.size(); ++i) {
    SchematicEditor* child = qobject_cast<SchematicEditor*>(windows.at(i));

    QString text;
    if(i < 9) {
      text = QString("&%1 %2").arg(i + 1)
        .arg(child->userFriendlyCurrentFile());
    } else {
      text = QString("%1 %2").arg(i + 1)
        .arg(child->userFriendlyCurrentFile());
    }
    QAction* action = windowMenu_->addAction(text);
    action->setCheckable(true);
    action->setChecked(child == activeEditor());
    connect(action, SIGNAL(triggered()), windowMapper_, SLOT(map()));
    windowMapper_->setMapping(action, windows.at(i));
  }
}


void QSapecNGWindow::updateDocks()
{
  bool hasEditor = (activeEditor() != 0);

  // the component list stays usable: a pick opens a new schematic
  undoRedoDock_->widget()->setEnabled(hasEditor);
}


void QSapecNGWindow::createLogger()
{
  policy_ = new QLogPolicy(this);
  policy_->setDebugColor(Qt::black);
  policy_->setInfoColor(Qt::black);
  policy_->setWarningColor(Qt::blue);
  policy_->setErrorColor(Qt::red);

  QLogger::setLevel(sapecng::Logger::DEBUG);
  QLogger::setPolicy(policy_);
}


void QSapecNGWindow::createActions()
{
  newAct_ = new QAction(theme::icon("new"), tr("&New"), this);
  newAct_->setShortcuts(QKeySequence::New);
  newAct_->setStatusTip(tr("New file"));
  connect(newAct_, SIGNAL(triggered()), this, SLOT(newFile()));

  openAct_ = new QAction(theme::icon("open"), tr("&Open..."), this);
  openAct_->setShortcuts(QKeySequence::Open);
  openAct_->setStatusTip(tr("Open"));
  connect(openAct_, SIGNAL(triggered()), this, SLOT(open()));

  openCrcAct_ = new QAction(theme::icon("open"), tr("Open &Netlist..."), this);
  openCrcAct_->setStatusTip(tr("Open a netlist (.crc) and draw it as a schematic"));
  connect(openCrcAct_, SIGNAL(triggered()), this, SLOT(openCrc()));

  saveAct_ = new QAction(theme::icon("save"), tr("&Save"), this);
  saveAct_->setShortcuts(QKeySequence::Save);
  saveAct_->setStatusTip(tr("Save"));
  connect(saveAct_, SIGNAL(triggered()), this, SLOT(save()));

  saveAsAct_ = new QAction(tr("Save &As..."), this);
  saveAsAct_->setShortcuts(QKeySequence::SaveAs);
  // some desktops have no standard key for it
  if(saveAsAct_->shortcuts().isEmpty())
    saveAsAct_->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_S);
  saveAsAct_->setStatusTip(tr("Save As..."));
  connect(saveAsAct_, SIGNAL(triggered()), this, SLOT(saveAs()));

  exportSpiceAct_ = new QAction(tr("Export SPICE &Netlist..."), this);
  exportSpiceAct_->setStatusTip(tr("Write the circuit as a SPICE netlist "
    "(ngspice, LTspice ...) with an AC analysis of every output"));
  connect(exportSpiceAct_, &QAction::triggered,
    this, &QSapecNGWindow::exportSpice);

  exportAct_ = new QAction(tr("&Export Schematic..."), this);
  exportAct_->setShortcut(tr("Ctrl+E"));
  exportAct_->setStatusTip(tr("Save the schematic as an SVG, PNG or PDF file"));
  connect(exportAct_, &QAction::triggered,
    this, &QSapecNGWindow::exportSchematic);

  printAct_ = new QAction(theme::icon("print"), tr("&Print"), this);
  printAct_->setShortcuts(QKeySequence::Print);
  printAct_->setStatusTip(tr("Print"));
  connect(printAct_, SIGNAL(triggered()), this, SLOT(print()));

  quitAct_ = new QAction(theme::icon("cross"), tr("&Quit"), this);
  quitAct_->setShortcut(tr("Ctrl+Q"));
  quitAct_->setStatusTip(tr("Quit"));
  connect(quitAct_, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));

  binAct_ = new QAction(theme::icon("bin"), tr("&Delete"), this);
  binAct_->setShortcut(QKeySequence::Delete);
  binAct_->setStatusTip(tr("Delete"));
  connect(binAct_, SIGNAL(triggered()), this, SLOT(bin()));

  undoAct_ = new QAction(theme::icon("undo"), tr("&Undo"), this);
  undoAct_->setShortcut(QKeySequence::Undo);
  undoAct_->setStatusTip(tr("Undo"));
  undoAct_->setEnabled(undoRedoGroup_->canUndo());
  connect(undoAct_, SIGNAL(triggered()), undoRedoGroup_, SLOT(undo()));
  connect(
      undoRedoGroup_, SIGNAL(canUndoChanged(bool)),
      undoAct_, SLOT(setEnabled(bool))
    );

  redoAct_ = new QAction(theme::icon("redo"), tr("&Redo"), this);
  redoAct_->setShortcut(QKeySequence::Redo);
  redoAct_->setStatusTip(tr("Redo"));
  redoAct_->setEnabled(undoRedoGroup_->canRedo());
  connect(redoAct_, SIGNAL(triggered()), undoRedoGroup_, SLOT(redo()));
  connect(
      undoRedoGroup_, SIGNAL(canRedoChanged(bool)),
      redoAct_, SLOT(setEnabled(bool))
    );

  propsAct_ = new QAction(tr("Properties..."), this);
  propsAct_->setStatusTip(tr("Properties..."));
  connect(propsAct_, SIGNAL(triggered()), this, SLOT(props()));

  cutAct_ = new QAction(theme::icon("cut"), tr("Cu&t"), this);
  cutAct_->setShortcut(QKeySequence::Cut);
  cutAct_->setStatusTip(tr("Cut"));
  connect(cutAct_, SIGNAL(triggered()), this, SLOT(cut()));

  copyAct_ = new QAction(theme::icon("copy"), tr("&Copy"), this);
  copyAct_->setShortcut(QKeySequence::Copy);
  copyAct_->setStatusTip(tr("Copy"));
  connect(copyAct_, SIGNAL(triggered()), this, SLOT(copy()));

  pasteAct_ = new QAction(theme::icon("paste"), tr("&Paste"), this);
  pasteAct_->setShortcut(QKeySequence::Paste);
  pasteAct_->setStatusTip(tr("Paste"));
  connect(pasteAct_, SIGNAL(triggered()), this, SLOT(paste()));

  rotateAct_ = new QAction(theme::icon("rotate"), tr("R&otate"), this);
  rotateAct_->setShortcut(QKeySequence(Qt::Key_R));
  rotateAct_->setStatusTip(tr("Rotate the selected items, or the item being placed (R)"));
  connect(rotateAct_, SIGNAL(triggered()), this, SLOT(rotate()));

  mirrorAct_ = new QAction(theme::icon("mirror"), tr("Flip &Horizontally"), this);
  mirrorAct_->setShortcut(QKeySequence(Qt::Key_H));
  mirrorAct_->setStatusTip(
    tr("Flip the selected items, or the item being placed, left to right (H)"));
  connect(mirrorAct_, &QAction::triggered, this, [this]() {
      if(SchematicEditor* editor = activeEditor())
        editor->scene().mirrorPlacedOrSelected(false);
    });

  flipVAct_ = new QAction(theme::icon("flipv"), tr("Flip &Vertically"), this);
  flipVAct_->setShortcut(QKeySequence(Qt::Key_V));
  flipVAct_->setStatusTip(
    tr("Flip the selected items, or the item being placed, upside down (V)"));
  connect(flipVAct_, &QAction::triggered, this, [this]() {
      if(SchematicEditor* editor = activeEditor())
        editor->scene().mirrorPlacedOrSelected(true);
    });

  zoomInAct_ = new QAction(theme::icon("zoomin"), tr("Zoom &In"), this);
  zoomInAct_->setShortcuts(QKeySequence::ZoomIn);
  zoomInAct_->setStatusTip(tr("Zoom In"));
  connect(zoomInAct_, SIGNAL(triggered()), this, SLOT(zoomIn()));

  zoomOutAct_ = new QAction(theme::icon("zoomout"), tr("Zoom &Out"), this);
  zoomOutAct_->setShortcuts(QKeySequence::ZoomOut);
  zoomOutAct_->setStatusTip(tr("Zoom Out"));
  connect(zoomOutAct_, SIGNAL(triggered()), this, SLOT(zoomOut()));

  zoomNormalAct_ = new QAction(theme::icon("zoomnormal"), tr("Actual Si&ze (100 %)"), this);
  zoomNormalAct_->setStatusTip(tr("Actual size: 100 %"));
  connect(zoomNormalAct_, SIGNAL(triggered()), this, SLOT(zoomNormal()));

  fitAct_ = new QAction(theme::icon("fit"), tr("&Fit"), this);
  fitAct_->setText(tr("Zoom to &Fit"));
  fitAct_->setStatusTip(tr("The whole schematic in the window"));
  connect(fitAct_, SIGNAL(triggered()), this, SLOT(fit()));

  nodeAct_ = new QAction(theme::icon("node"), tr("Assign &nodes"), this);
  nodeAct_->setShortcut(tr("F7"));
  nodeAct_->setStatusTip(tr("Assign nodes"));
  connect(nodeAct_, SIGNAL(triggered()), this, SLOT(assignNodes()));

  resolveAct_ = new QAction(theme::icon("resolve"), tr("&Resolve..."), this);
  resolveAct_->setShortcut(tr("F8"));
  resolveAct_->setStatusTip(tr("Resolve"));
  connect(resolveAct_, SIGNAL(triggered()), this, SLOT(resolve()));

  stopAct_ = new QAction(theme::icon("stop"), tr("S&top"), this);
  stopAct_->setShortcut(tr("Shift+F8"));
  stopAct_->setToolTip(tr("Stop solving (Shift+F8)"));
  stopAct_->setStatusTip(tr("Stop the running Resolve"));
  stopAct_->setEnabled(false);
  connect(stopAct_, &QAction::triggered, this, &QSapecNGWindow::stopSolving);

  waveAct_ = new QAction(theme::icon("wave"), tr("&New Plot..."), this);
  waveAct_->setShortcut(tr("Ctrl+F9"));
  waveAct_->setStatusTip(tr("Plot the result: frequency or phase response, "
                            "Bode plot, poles and zeros (Ctrl+F9)"));
  connect(waveAct_, SIGNAL(triggered()), this, SLOT(wave()));

  // the main window's way to the plots: the ones of this schematic, or a
  // Bode plot when it has none
  plotsAct_ = new QAction(theme::icon("wave"), tr("&Plots"), this);
  plotsAct_->setShortcut(tr("F9"));
  plotsAct_->setStatusTip(tr("Show the plots of this schematic; with none "
                             "yet, a Bode plot (F9)"));
  connect(plotsAct_, &QAction::triggered, this, &QSapecNGWindow::showPlots);

  toggleScreenModeAct_ = new QAction(tr("F&ull Screen Mode"), this);
  toggleScreenModeAct_->setStatusTip(tr("Switch to Full Screen Mode"));
  toggleScreenModeAct_->setCheckable(true);
  connect(toggleScreenModeAct_, SIGNAL(triggered()),
    this, SLOT(toggleScreenMode()));

  configureAppAct_ = new QAction(theme::icon("configuration"),
    tr("&Configure QSapecNG..."), this);
  configureAppAct_->setStatusTip(tr("Configure QSapecNG..."));
  connect(configureAppAct_, SIGNAL(triggered()),
    this, SLOT(configDialog()));

  closeAct_ = new QAction(tr("&Close"), this);
  closeAct_->setShortcut(tr("Ctrl+F4"));
  closeAct_->setStatusTip(tr("Close active window"));
  connect(closeAct_, SIGNAL(triggered()),
    mdiArea_, SLOT(closeActiveSubWindow()));

  closeAllAct_ = new QAction(tr("Close &All"), this);
  closeAllAct_->setStatusTip(tr("Close all windows"));
  connect(closeAllAct_, SIGNAL(triggered()),
    mdiArea_, SLOT(closeAllSubWindows()));

  tileAct_ = new QAction(tr("&Tile"), this);
  tileAct_->setStatusTip(tr("Tile"));
  connect(tileAct_, SIGNAL(triggered()), mdiArea_, SLOT(tileSubWindows()));

  cascadeAct_ = new QAction(tr("Casca&de"), this);
  cascadeAct_->setStatusTip(tr("Cascade"));
  connect(cascadeAct_, SIGNAL(triggered()), mdiArea_, SLOT(cascadeSubWindows()));

  nextAct_ = new QAction(tr("&Next"), this);
  nextAct_->setShortcuts(QKeySequence::NextChild);
  nextAct_->setStatusTip(tr("Next window"));
  connect(nextAct_, SIGNAL(triggered()),
    mdiArea_, SLOT(activateNextSubWindow()));

  addAction(nextAct_);   // keys only (not in a menu)
  previousAct_ = new QAction(tr("&Previous"), this);
  previousAct_->setShortcuts(QKeySequence::PreviousChild);
  previousAct_->setStatusTip(tr("Previous window"));
  addAction(previousAct_);
  connect(previousAct_, SIGNAL(triggered()),
    mdiArea_, SLOT(activatePreviousSubWindow()));

  separatorAct_ = new QAction(this);
  separatorAct_->setSeparator(true);

  licenseAct_ = new QAction(theme::icon("license"), tr("&License"), this);
  licenseAct_->setStatusTip(tr("Show License"));
  connect(licenseAct_, SIGNAL(triggered()), this, SLOT(license()));

  aboutAct_ = new QAction(theme::icon("info"), tr("&About"), this);
  aboutAct_->setStatusTip(tr("About QSapecNG"));
  connect(aboutAct_, SIGNAL(triggered()), this, SLOT(about()));

  aboutQtAct_ = new QAction(QIcon(":/images/qt-logo.png"), tr("&Qt"), this);
  aboutQtAct_->setStatusTip(tr("About Qt"));
  connect(aboutQtAct_, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

  userdefAct_ =
    new QAction(theme::symbolIcon("userdef"), tr("&User def"), this);
  userdefAct_->setStatusTip(tr("User def"));
  connect(userdefAct_, SIGNAL(triggered()), this, SLOT(userDefRequested()));

  disconnectedWireSessionAct_ =
    new QAction(theme::symbolIcon("wire"), tr("&Wire"), this);
  disconnectedWireSessionAct_->setShortcut(Qt::Key_W);
  disconnectedWireSessionAct_->setStatusTip(tr("Draw wires: click to start, "
    "click to add a corner, end on a pin or a wire (or double-click); "
    "right-click or / flips the bend, Esc stops"));
  connect(disconnectedWireSessionAct_, SIGNAL(triggered()),
    this, SLOT(disconnectedWireSessionRequested()));

  groundSessionAct_ =
    new QAction(theme::symbolIcon("ground"), tr("&Ground"), this);
  groundSessionAct_->setStatusTip(tr("Ground Session Request"));
  groundSessionAct_->setShortcut(QKeySequence(Qt::Key_G));
  connect(groundSessionAct_, SIGNAL(triggered()),
    this, SLOT(groundSessionRequested()));

  portSessionAct_ =
    new QAction(theme::symbolIcon("port"), tr("&Port"), this);
  portSessionAct_->setStatusTip(tr("Port Session Request"));
  portSessionAct_->setShortcut(QKeySequence(Qt::Key_P));
  connect(portSessionAct_, SIGNAL(triggered()),
    this, SLOT(portSessionRequested()));

  outSessionAct_ =
    new QAction(theme::symbolIcon("out"), tr("&Output"), this);
  outSessionAct_->setToolTip(tr("Output (O): mark the net whose voltage is "
                                "the result"));
  outSessionAct_->setStatusTip(tr("Mark an output: the voltage of that net "
                                  "is the result"));
  outSessionAct_->setShortcut(QKeySequence(Qt::Key_O));
  connect(outSessionAct_, SIGNAL(triggered()),
    this, SLOT(outSessionRequested()));

  voltmeterSessionAct_ =
    new QAction(theme::symbolIcon("voltmeter"), tr("&Voltmeter"), this);
  voltmeterSessionAct_->setStatusTip(tr("Voltmeter Session Request"));
  connect(voltmeterSessionAct_, SIGNAL(triggered()),
    this, SLOT(voltmeterSessionRequested()));

  ammeterSessionAct_ =
    new QAction(theme::symbolIcon("ammeter"), tr("&Ammeter"), this);
  ammeterSessionAct_->setStatusTip(tr("Ammeter Session Request"));
  connect(ammeterSessionAct_, SIGNAL(triggered()),
    this, SLOT(ammeterSessionRequested()));

  labelSessionAct_ =
    new QAction(theme::icon("label"), tr("&Label"), this);
  labelSessionAct_->setStatusTip(tr("Insert Label"));
  labelSessionAct_->setShortcut(QKeySequence(Qt::Key_T));
  connect(labelSessionAct_, SIGNAL(triggered()),
    this, SLOT(labelSessionRequested()));

  cursorAct_ = new QAction(theme::icon("cursor"), tr("&Cursor"), this);
  cursorAct_->setStatusTip(tr("Clear pending request"));
  connect(cursorAct_, SIGNAL(triggered()), this, SLOT(clearSessionRequested()));

  toggleGridAct_ =
    new QAction(theme::icon("grid"), tr("&Toggle grid"), this);
  toggleGridAct_->setStatusTip(tr("Toggle grid"));
  toggleGridAct_->setCheckable(true);
  connect(toggleGridAct_, SIGNAL(triggered(bool)),
    this, SIGNAL(toggleGrid(bool)));
  toggleGridAct_->setChecked(true);

  // plots (each in its own dock); the unit and range choices are in the
  // plot dialog
  plotSettingsAct_ = new QAction(tr("Plot &Settings"), this);
  plotSettingsAct_->setStatusTip(tr("The Plot Settings tab beside the plots: "
    "what the current plot shows, its frequency range and units (applied "
    "at once)"));
  connect(plotSettingsAct_, &QAction::triggered,
    this, &QSapecNGWindow::editPlotSettings);

  vrangeAct_ = new QAction(tr("&Vertical Range..."), this);
  vrangeAct_->setStatusTip(tr("Fixed or automatic range of the vertical axis"));
  connect(vrangeAct_, &QAction::triggered, this,
    [this]() { if(currentPlot_) currentPlot_->editVerticalRange(); });

  resetViewAct_ = new QAction(tr("Zoom to &Fit"), this);
  resetViewAct_->setShortcut(Qt::Key_Home);
  resetViewAct_->setStatusTip(tr("Show the whole sweep and fit the vertical "
    "axis to the curves (undoes zooming and panning)"));
  connect(resetViewAct_, &QAction::triggered, this,
    [this]() { if(currentPlot_) currentPlot_->resetView(); });

  clearMarkersAct_ = new QAction(tr("&Clear Markers"), this);
  clearMarkersAct_->setStatusTip(
    tr("Remove the markers placed on the plot (click a curve to place one, "
       "click a marker to remove it)"));
  connect(clearMarkersAct_, &QAction::triggered, this,
    [this]() { if(currentPlot_) currentPlot_->clearMarkers(); });

  markersPanelAct_ = new QAction(tr("&Markers"), this);
  markersPanelAct_->setShortcut(Qt::CTRL | Qt::Key_M);
  markersPanelAct_->setStatusTip(tr("The Markers tab beside the plots: the "
    "list of markers"));
  connect(markersPanelAct_, &QAction::triggered, this, [this]() {
      createPlotWindow();
      markersDock_->raise();   // its tab in front
      showPlotWindow();
    });

  antialiasAct_ = new QAction(tr("&Antialiasing"), this);
  antialiasAct_->setCheckable(true);
  antialiasAct_->setChecked(WorkPlane::antialiased());
  antialiasAct_->setStatusTip(tr("Smooth curves and lines in the plots"));
  connect(antialiasAct_, &QAction::toggled, this, [this](bool on) {
      WorkPlane::setAntialiasedDefault(on);
      for(const PlotEntry& e : plots_)
        if(e.plane)
          e.plane->applyAntialiasing();
    });

  printPlotAct_ = new QAction(theme::icon("print"), tr("&Print Plot..."), this);
  printPlotAct_->setStatusTip(tr("Print the current plot"));
  connect(printPlotAct_, &QAction::triggered, this, &QSapecNGWindow::printPlot);

  replotAct_ = new QAction(tr("&Replot"), this);
  replotAct_->setStatusTip(tr("Replot function"));
  connect(replotAct_, SIGNAL(triggered()), this, SLOT(replot()));

  for(int i = 0; i < maxRecentFiles; ++i) {
    openRecentActs_[i] = new QAction(this);
    openRecentActs_[i]->setVisible(false);
    connect(openRecentActs_[i], SIGNAL(triggered()), this, SLOT(openRecent()));
  }
}


void QSapecNGWindow::createDockWidgets()
{
  setDockOptions(QMainWindow::AllowTabbedDocks);
  setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
  setTabPosition(Qt::RightDockWidgetArea, QTabWidget::North);
  setTabPosition(Qt::BottomDockWidgetArea, QTabWidget::South);

  // the parts (names, symbols, types) for the Components toolbar
  sideBarModel_ = new SideBarModel(this);

  messageDock_ = new QDockWidget(tr("Messages"), this);
  messageDock_->setAllowedAreas(Qt::BottomDockWidgetArea);
  messageDock_->setTitleBarWidget(0);
  createMessageDockLayout();

  undoRedoDock_ = new QDockWidget(tr("Undo/Redo"), this);
  undoRedoDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  undoRedoDock_->setTitleBarWidget(0);
  createUndoRedoDockLayout();

  workspaceDock_ = new QDockWidget(tr("Workspace"), this);
  workspaceDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  workspaceDock_->setTitleBarWidget(0);
  createWorkspaceDockLayout();

  resultDock_ = new QDockWidget(tr("Result"), this);
  resultDock_->setAllowedAreas(Qt::AllDockWidgetAreas);
  resultPanel_ = new ResultPanel;
  connect(resultPanel_, &ResultPanel::clearRequested, this, [this]() {
      if(SchematicEditor* editor = activeEditor())
        editor->clearResult();
      resultPanel_->showPlaceholder();
      updateStatus();
    });
  connect(resultPanel_, &ResultPanel::symbolicRequested, this, [this]() {
      if(SchematicEditor* editor = activeEditor())
        editor->computeSymbolic();
    });
  resultDock_->setWidget(resultPanel_);

  // object names identify the docks in the saved layout
  messageDock_->setObjectName(QStringLiteral("messagesDock"));
  undoRedoDock_->setObjectName(QStringLiteral("undoRedoDock"));
  workspaceDock_->setObjectName(QStringLiteral("workspaceDock"));
  resultDock_->setObjectName(QStringLiteral("resultDock"));

  addDockWidget(Qt::LeftDockWidgetArea, workspaceDock_);
  addDockWidget(Qt::BottomDockWidgetArea, messageDock_);
  addDockWidget(Qt::BottomDockWidgetArea, resultDock_);
  addDockWidget(Qt::RightDockWidgetArea, undoRedoDock_);
  tabifyDockWidget(messageDock_, resultDock_);
}


void QSapecNGWindow::createMenus()
{
  fileMenu_ = menuBar()->addMenu(tr("&File"));
  fileMenu_->addAction(newAct_);
  fileMenu_->addAction(openAct_);
  recentMenu_ = fileMenu_->addMenu(theme::icon("open"), tr("Open &Recent"));
  for(int i = 0; i < maxRecentFiles; ++i)
    recentMenu_->addAction(openRecentActs_[i]);
  createExamplesMenu(fileMenu_);
  fileMenu_->addAction(openCrcAct_);
  fileMenu_->addAction(saveAct_);
  fileMenu_->addAction(saveAsAct_);
  fileMenu_->addSeparator();
  fileMenu_->addAction(exportAct_);
  fileMenu_->addAction(exportSpiceAct_);
  fileMenu_->addAction(printAct_);
  fileMenu_->addSeparator();
  fileMenu_->addAction(quitAct_);

  editMenu_ = menuBar()->addMenu(tr("&Edit"));
  editMenu_->addAction(undoAct_);
  editMenu_->addAction(redoAct_);
  editMenu_->addSeparator();
  editMenu_->addAction(propsAct_);
  editMenu_->addSeparator();
  editMenu_->addAction(cutAct_);
  editMenu_->addAction(copyAct_);
  editMenu_->addAction(pasteAct_);
  editMenu_->addSeparator();
  editMenu_->addAction(rotateAct_);
  editMenu_->addAction(mirrorAct_);
  editMenu_->addAction(flipVAct_);
  editMenu_->addSeparator();
  editMenu_->addAction(binAct_);

  viewMenu_ = menuBar()->addMenu(tr("&View"));
  viewMenu_->addAction(zoomInAct_);
  viewMenu_->addAction(zoomOutAct_);
  viewMenu_->addAction(zoomNormalAct_);
  viewMenu_->addSeparator();
  viewMenu_->addAction(fitAct_);

  runMenu_ = menuBar()->addMenu(tr("&Run"));
  runMenu_->addAction(nodeAct_);
  runMenu_->addAction(resolveAct_);
  runMenu_->addAction(stopAct_);
  runMenu_->addAction(plotsAct_);
  runMenu_->addAction(waveAct_);

  // the Plot menu belongs to the Plots window; here New Plot sits in Run
  workplaneMenu_ = new QMenu(tr("&Plot"), this);
  for(QAction* a : { waveAct_, plotSettingsAct_, resetViewAct_,
        clearMarkersAct_, markersPanelAct_, replotAct_, printPlotAct_ })
    addAction(a);       // their keys still work here
  workplaneMenu_->addAction(waveAct_);
  workplaneMenu_->addAction(plotSettingsAct_);
  workplaneMenu_->addSeparator();
  workplaneMenu_->addAction(resetViewAct_);
  workplaneMenu_->addAction(clearMarkersAct_);
  workplaneMenu_->addAction(markersPanelAct_);
  workplaneMenu_->addAction(antialiasAct_);
  workplaneMenu_->addSeparator();
  workplaneMenu_->addAction(replotAct_);
  workplaneMenu_->addAction(printPlotAct_);

  toolMenu_ = menuBar()->addMenu(tr("&Tools"));
  toolMenu_->addAction(cursorAct_);
  toolMenu_->addAction(labelSessionAct_);
  toolMenu_->addSeparator();
  toolMenu_->addAction(userdefAct_);
  toolMenu_->addAction(disconnectedWireSessionAct_);
  toolMenu_->addAction(groundSessionAct_);
  toolMenu_->addAction(portSessionAct_);
  toolMenu_->addAction(outSessionAct_);
  toolMenu_->addSeparator();
  toolMenu_->addAction(toggleGridAct_);

  settingMenu_ = menuBar()->addMenu(tr("&Settings"));
  toolbarMenu_ = settingMenu_->addMenu(tr("Toolbars"));
  toolbarMenu_->addAction(fileToolBar_->toggleViewAction());
  toolbarMenu_->addAction(viewToolBar_->toggleViewAction());
  toolbarMenu_->addAction(runToolBar_->toggleViewAction());
  toolbarMenu_->addAction(toolToolBar_->toggleViewAction());
  toolbarMenu_->addAction(partsToolBar_->toggleViewAction());
  settingMenu_->addAction(toggleScreenModeAct_);

  settingMenu_->addSeparator();
  settingMenu_->addAction(configureAppAct_);

  windowMenu_ = menuBar()->addMenu(tr("&Window"));
  showPlotsAct_ = new QAction(tr("&Plots Window"), this);
  addAction(showPlotsAct_);
  showPlotsAct_->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_P);
  showPlotsAct_->setStatusTip(tr("Bring the window with the plots to the front"));
  connect(showPlotsAct_, &QAction::triggered, this,
    [this]() { if(plotWindow_) showPlotWindow(); });
  showMenu_ = windowMenu_->addMenu(tr("&Show"));
  updateWindowMenu();
  connect(windowMenu_, SIGNAL(aboutToShow()), this, SLOT(updateWindowMenu()));
  showMenu_->addAction(messageDock_->toggleViewAction());
  showMenu_->addAction(resultDock_->toggleViewAction());
  showMenu_->addAction(undoRedoDock_->toggleViewAction());
  showMenu_->addAction(workspaceDock_->toggleViewAction());

  menuBar()->addSeparator();

  helpMenu_ = menuBar()->addMenu(tr("&Help"));
  helpMenu_->addAction(licenseAct_);
  helpMenu_->addSeparator();
  helpMenu_->addAction(aboutAct_);
  helpMenu_->addAction(aboutQtAct_);

  // right-click menu of the schematic: the editing actions themselves
  sceneMenu_ = new QMenu(this);
  sceneMenu_->addAction(propsAct_);
  sceneMenu_->addSeparator();
  sceneMenu_->addAction(rotateAct_);
  sceneMenu_->addAction(mirrorAct_);
  sceneMenu_->addAction(flipVAct_);
  sceneMenu_->addSeparator();
  sceneMenu_->addAction(cutAct_);
  sceneMenu_->addAction(copyAct_);
  sceneMenu_->addAction(pasteAct_);
  sceneMenu_->addAction(binAct_);
  sceneMenu_->addSeparator();
  QAction* reverseMeters = sceneMenu_->addAction(tr("Reverse &Polarity"));
  // only with a voltmeter or an ammeter selected
  connect(sceneMenu_, &QMenu::aboutToShow, this, [this, reverseMeters]() {
      bool meter = false;
      if(SchematicEditor* editor = activeEditor()) {
        foreach(QGraphicsItem* item, editor->scene().selectedItems()) {
          const SchematicScene::SupportedItemType type =
            SchematicScene::itemType(item);
          meter = meter || type == SchematicScene::VoltmeterItemType
            || type == SchematicScene::AmmeterItemType;
        }
      }
      reverseMeters->setVisible(meter);

      // Properties only for parts that have parameters or a name to edit
      // (outputs), not for ground or wires; the Edit menu keeps it
      bool params = false;
      if(SchematicEditor* editor = activeEditor()) {
        foreach(QGraphicsItem* item, editor->scene().selectedItems())
          params = params || SchematicScene::itemProperties(item)
            || SchematicScene::isOutput(item);
      }
      propsAct_->setVisible(params || !propsAct_->isEnabled());
    });
  connect(sceneMenu_, &QMenu::aboutToHide, this, [this]() {
      propsAct_->setVisible(true);
    });
  reverseMeters->setStatusTip(tr("Voltmeter: swap + and -; ammeter: the "
    "arrow the other way (the result changes sign)"));
  connect(reverseMeters, &QAction::triggered, this, [this]() {
      if(SchematicEditor* editor = activeEditor())
        if(!editor->scene().reverseSelectedMeters())
          statusBar()->showMessage(tr("Select a voltmeter or an ammeter first"), 3000);
    });
  QAction* resetLabels = sceneMenu_->addAction(tr("Reset &Label Position"));
  // only with a part (or its label) selected
  connect(sceneMenu_, &QMenu::aboutToShow, this, [this, resetLabels]() {
      bool part = false;
      if(SchematicEditor* editor = activeEditor()) {
        foreach(QGraphicsItem* item, editor->scene().selectedItems())
          part = part || dynamic_cast<Component*>(item)
            || (item->parentItem() && dynamic_cast<Component*>(item->parentItem()));
      }
      resetLabels->setVisible(part);
    });
  resetLabels->setStatusTip(tr("Put the labels of the selected parts back "
                               "beside them"));
  connect(resetLabels, &QAction::triggered, this, [this]() {
      if(SchematicEditor* editor = activeEditor()) {
        foreach(QGraphicsItem* item, editor->scene().selectedItems()) {
          Component* c = dynamic_cast<Component*>(item);
          if(!c && item->parentItem())     // the label itself selected
            c = dynamic_cast<Component*>(item->parentItem());
          if(c)
            c->resetLabelPosition();
        }
      }
    });
  sceneMenu_->addSeparator();

  // Place: every component of the list, then wires and markers
  QMenu* placeMenu = sceneMenu_->addMenu(tr("&Place"));
  for(int row = 0; row < sideBarModel_->rowCount(); ++row) {
    const QModelIndex index = sideBarModel_->index(row);
    QAction* act = placeMenu->addAction(
      qvariant_cast<QPixmap>(sideBarModel_->data(index, Qt::DecorationRole)),
      sideBarModel_->data(index, Qt::DisplayRole).toString());
    connect(act, &QAction::triggered, this, [this, index]() {
        activated(index);
      });
  }
  placeMenu->addSeparator();
  placeMenu->addAction(disconnectedWireSessionAct_);
  placeMenu->addAction(groundSessionAct_);
  placeMenu->addAction(outSessionAct_);
  placeMenu->addAction(portSessionAct_);
  placeMenu->addAction(labelSessionAct_);

  sceneMenu_->addSeparator();
  sceneMenu_->addAction(zoomInAct_);
  sceneMenu_->addAction(zoomOutAct_);
  sceneMenu_->addAction(fitAct_);
  sceneMenu_->addSeparator();
  sceneMenu_->addAction(resolveAct_);
}


void QSapecNGWindow::createToolBars()
{
  fileToolBar_ = addToolBar(tr("File"));
  fileToolBar_->setObjectName(QStringLiteral("fileToolBar"));
  fileToolBar_->addAction(newAct_);
  fileToolBar_->addAction(openAct_);
  fileToolBar_->addAction(saveAct_);
  fileToolBar_->addSeparator();
  fileToolBar_->addAction(printAct_);

  viewToolBar_ = addToolBar(tr("View"));
  viewToolBar_->setObjectName(QStringLiteral("viewToolBar"));
  viewToolBar_->addAction(zoomInAct_);
  viewToolBar_->addAction(zoomOutAct_);
  viewToolBar_->addAction(zoomNormalAct_);
  viewToolBar_->addSeparator();
  viewToolBar_->addAction(fitAct_);

  runToolBar_ = addToolBar(tr("Run"));
  runToolBar_->setObjectName(QStringLiteral("runToolBar"));
  runToolBar_->addAction(nodeAct_);
  runToolBar_->addAction(resolveAct_);
  runToolBar_->addAction(stopAct_);
  runToolBar_->addAction(plotsAct_);

  // the parts, Inkscape style: a column of tool buttons on the left edge;
  // a click picks the part to place, a second click puts it back
  partsToolBar_ = new QToolBar(tr("Components"), this);
  partsToolBar_->setObjectName(QStringLiteral("partsToolBar"));
  partsToolBar_->setOrientation(Qt::Vertical);
  partsToolBar_->setIconSize(QSize(32, 32));
  // in a row with the other toolbars: their size, or its edge is drawn a
  // pixel off theirs (a double line under it)
  connect(partsToolBar_, &QToolBar::orientationChanged,
    this, [this](Qt::Orientation o) {
      partsToolBar_->setIconSize(o == Qt::Vertical ? QSize(32, 32)
                                                   : iconSize());
    });
  partsToolBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);
  addToolBar(Qt::LeftToolBarArea, partsToolBar_);
  QActionGroup* parts = new QActionGroup(this);
  parts->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
  for(int row = 0; row < sideBarModel_->rowCount(); ++row) {
    const QModelIndex index = sideBarModel_->index(row);
    // the name alone: the symbol says the rest
    const QString name = sideBarModel_->data(index, Qt::DisplayRole).toString();
    QAction* act = new QAction(name, this);
    act->setToolTip(name);
    act->setCheckable(true);
    parts->addAction(act);
    partsToolBar_->addAction(act);
    partActions_.append(act);
    connect(act, &QAction::triggered, this, [this, row](bool checked) {
        if(checked) {
          activated(sideBarModel_->index(row));
        } else if(SchematicEditor* editor = activeEditor()) {
          editor->scene().setActiveItem(SchematicScene::NilItemType);
          sceneSelectionChanged();
        }
      });
  }
  refreshPartIcons();
  // the symbols are redrawn for the theme and the resistor style
  connect(sideBarModel_, &QAbstractItemModel::dataChanged,
    this, [this]() { refreshPartIcons(); });

  toolToolBar_ = addToolBar(tr("Tools"));
  toolToolBar_->setObjectName(QStringLiteral("toolToolBar"));
  toolToolBar_->addAction(cursorAct_);
  toolToolBar_->addAction(labelSessionAct_);
  toolToolBar_->addSeparator();
  toolToolBar_->addAction(userdefAct_);
  toolToolBar_->addAction(disconnectedWireSessionAct_);
  toolToolBar_->addAction(groundSessionAct_);
  toolToolBar_->addAction(portSessionAct_);
  toolToolBar_->addAction(outSessionAct_);
  toolToolBar_->addSeparator();
  toolToolBar_->addAction(toggleGridAct_);
}


void QSapecNGWindow::createStatusBar()
{
  // left: what is going on and what the active tool expects; right: size
  // of the drawing, zoom and pointer position. Transient messages
  // (showMessage) cover the left part for a moment.
  statusLabel_ = new QLabel;
  statusLabel_->setTextFormat(Qt::PlainText);
  statusBar()->addWidget(statusLabel_, 1);

  auto field = [](const QString& tip) {
    QLabel* label = new QLabel;
    label->setToolTip(tip);
    label->setContentsMargins(8, 0, 8, 0);
    return label;
  };
  countLabel_ = field(tr("Parts and wires in the schematic"));
  zoomLabel_ = field(tr("Zoom (mouse wheel)"));
  positionLabel_ = field(tr("Pointer position on the sheet (grid units)"));
  positionLabel_->setMinimumWidth(
    positionLabel_->fontMetrics().horizontalAdvance("x -0000  y -0000") + 16);
  statusBar()->addPermanentWidget(countLabel_);
  statusBar()->addPermanentWidget(zoomLabel_);
  statusBar()->addPermanentWidget(positionLabel_);
  
  progressBar_ = new QProgressBar;
  statusBar()->insertPermanentWidget(0, progressBar_, 0);
  progressBar_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  progressBar_->setMaximumWidth(240);
  progressBar_->setMinimumWidth(240);
  progressBar_->reset();
  progressBar_->hide();   // only while a Resolve is running
  progressBar_->setFormat(QStringLiteral("%p %"));
  progressTimer_ = new QTimer(this);
  progressTimer_->setInterval(250);
  connect(progressTimer_, &QTimer::timeout,
    this, &QSapecNGWindow::updateSolveProgress);
}


void QSapecNGWindow::createMessageDockLayout()
{
  logViewer_ = new LogViewer;
  logViewer_->setAcceptRichText(false);
  logViewer_->setLineWrapMode(QTextEdit::NoWrap);
  logViewer_->setWordWrapMode(QTextOption::NoWrap);
  QFont logFont = logViewer_->font();
  logFont.setPointSize(QApplication::font().pointSize() - 1);
  logViewer_->setFont(logFont);
  logViewer_->setReadOnly(true);
  logViewer_->setUndoRedoEnabled(false);
  logViewer_->setTextInteractionFlags(
        Qt::TextSelectableByMouse
      | Qt::TextSelectableByKeyboard
    );

  connect(policy_, &QLogPolicy::entry, logViewer_, &LogViewer::addEntry);

  messageDock_->setWidget(logViewer_);
}


void QSapecNGWindow::createUndoRedoDockLayout()
{
  undoRedoGroup_ = new QUndoGroup(this);
  undoRedoView_ = new QUndoView(undoRedoGroup_, this);
  undoRedoView_->setCleanIcon(theme::icon("save"));

  undoRedoDock_->setWidget(undoRedoView_);
}


void QSapecNGWindow::createWorkspaceDockLayout()
{
  QPushButton* wpChange = new QPushButton("...");
  connect(wpChange, SIGNAL(clicked(bool)), this, SLOT(changeWorkspace()));

  workspace_ = new QLineEdit(Settings().workspace());
  workspace_->setReadOnly(true);

  wfsModel_ = new QFileSystemModel;
  wfsView_ = new QTreeView;

  QStringList filters;
  filters << "*.info";
  filters << "*.xml";

  wfsModel_->setNameFilters(filters);
  wfsModel_->setRootPath(workspace_->text());
  wfsModel_->setResolveSymlinks(false);

  wfsView_->setModel(wfsModel_);
  wfsView_->setDragEnabled(true);
  wfsView_->setDragDropMode(QAbstractItemView::DragOnly);
  wfsView_->setRootIndex(wfsModel_->index(wfsModel_->rootPath()));
  wfsView_->setSelectionMode(QAbstractItemView::SingleSelection);
  wfsView_->sortByColumn(0, Qt::AscendingOrder);
  wfsView_->setAlternatingRowColors(true);
  wfsView_->setSortingEnabled(true);

  QGridLayout* wpLayout = new QGridLayout;;
  wpLayout->addWidget(workspace_, 0, 0);
  wpLayout->addWidget(wpChange, 0, 1);
  wpLayout->addWidget(wfsView_, 1, 0, 1, 2);

  QWidget* widget = new QWidget;
  widget->setLayout(wpLayout);

  workspaceDock_->setWidget(widget);
}


void QSapecNGWindow::readSettings()
{
  Settings settings;

  QApplication::setFont(settings.appFont());

  move(settings.mwPos());
  resize(settings.mwSize());

  // Default layout: the Components toolbar (left) and Messages (bottom);
  // Workspace and Undo/Redo are in Window > Show. A saved
  // layout replaces this, unless it was saved with an older layout version.
  workspaceDock_->hide();
  undoRedoDock_->hide();
  messageDock_->raise();
  restoreState(settings.mwState(), LayoutVersion);

  QLogger::setLevel((sapecng::Logger::LogLevel) settings.logLvl());
  policy_->setDebugColor(settings.debugColor());
  policy_->setInfoColor(settings.infoColor());
  policy_->setWarningColor(settings.warningColor());
  policy_->setErrorColor(settings.errorColor());
  policy_->setFatalColor(settings.fatalColor());
}


void QSapecNGWindow::writeSettings()
{
  SettingsManager manager;

  manager.setMWPos(pos());
  manager.setMWSize(size());
  manager.setMWState(saveState(LayoutVersion));
}


SchematicEditor* QSapecNGWindow::activeEditor()
{
  if(mdiArea_->currentSubWindow()) {
    SchematicEditor* editor =
      qobject_cast<SchematicEditor*>(mdiArea_->currentSubWindow());
    if(editor && !editor->isBusy())
      return editor;
  }

  return 0;
}


QMdiSubWindow* QSapecNGWindow::findEditor(const QString& fileName)
{
  QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

  foreach (QMdiSubWindow* window, mdiArea_->subWindowList()) {
    SchematicEditor* child = qobject_cast<SchematicEditor*>(window);
    if (child->currentFile() == canonicalFilePath)
      return window;
  }

  return 0;
}


SchematicEditor* QSapecNGWindow::createSchematicEditor()
{
  QApplication::setOverrideCursor(Qt::WaitCursor);

  SchematicEditor* editor = new SchematicEditor;
  setupSchematicEditor(editor);

  QApplication::restoreOverrideCursor();

  return editor;
}


void QSapecNGWindow::setupSchematicEditor(SchematicEditor* editor)
{
  // status bar: follow the tool, selection, solve state, zoom and pointer
  connect(&editor->scene(), &QGraphicsScene::changed,
    this, [this, editor]() { if(editor == activeEditor()) updateStatus(); });
  connect(&editor->scene(), &QGraphicsScene::selectionChanged,
    this, [this, editor]() { if(editor == activeEditor()) updateStatus(); });
  connect(&editor->view(), &SchematicView::zoomChanged,
    this, [this, editor]() { if(editor == activeEditor()) updateStatus(); });
  connect(&editor->view(), &SchematicView::cursorMoved,
    this, [this, editor](const QPointF& p) {
      if(editor != activeEditor())
        return;
      const int step = SchematicScene::GridStep;
      positionLabel_->setText(QStringLiteral("x %1  y %2")
        .arg(qRound(p.x() / step)).arg(qRound(p.y() / step)));
    });

  editor->scene().setGridVisible(toggleGridAct_->isChecked());
  editor->scene().setContextMenu(sceneMenu_);

  undoRedoGroup_->addStack(editor->scene().undoRedoStack());
  editor->scene().undoRedoStack()->setActive(true);

  connect(editor, SIGNAL(stackEditor(SchematicEditor*)),
    this, SLOT(stackEditor(SchematicEditor*)));
  connect(editor, &SchematicEditor::solvingStarted, this, [this]() {
      progressBar_->setRange(0, 1000);
      progressBar_->setValue(0);
      progressBar_->show();
      stopAct_->setEnabled(true);
      progressTimer_->start();
      updateSolveProgress();
    });
  connect(editor, &SchematicEditor::solvingStopped, this, [this, editor]() {
      updateSolveProgress();
      keepPlotsInFront();
      if(editor == activeEditor()) {
        editor->scene().undoRedoStack()->setActive(true);
        progressBar_->setRange(0, 1);
        progressBar_->reset();
        progressBar_->hide();
        updateMenus();
        updateResultPanel();
        updateStatus();
      }
    });
  connect(editor, &SchematicEditor::solved, this, [this, editor]() {
      updateSolveProgress();
    // while solving there was no active editor and so no active undo
    // stack (updateMenus): give it back, or Undo stays off until another
    // schematic is picked
    if(editor == activeEditor()) {
      editor->scene().undoRedoStack()->setActive(true);
      updateMenus();
      // the solver is done: no more progress bar (it was hidden only when
      // another window got active, e.g. the Plots window); it stays while
      // the symbolic result is worked out in the background
      if(!editor->isRunning()) {
        progressBar_->setRange(0, 1);
        progressBar_->reset();
        progressBar_->hide();
      }
    }

    // plots of this schematic follow its result; one asked for before
    // solving opens now
    refreshPlots(editor);
    if(editor == activeEditor()) {
      updateResultPanel();
      updateStatus();
      resultDock_->show();
      resultDock_->raise();
    }

    keepPlotsInFront();
    updateSmithAllowed();   // the circuit may have changed its sources

    // a new plot comes to the front: one asked for (New Plot, or Plots
    // before the circuit was solved), or the one its file asks for (the
    // examples), once. Otherwise no plot opens by itself: the status bar
    // says where it is
    bool hasPlot = false;
    for(const PlotEntry& e : plots_)
      hasPlot = hasPlot || e.editor == editor;
    const bool asked = editor->property("plotOnSolve").toBool();
    editor->setProperty("plotOnSolve", QVariant());
    if(pendingPlots_.contains(editor)) {
      createPlot(editor, pendingPlots_.take(editor));
    } else if(hasPlot) {
      // they follow the result
    } else if(asked) {
      createPlot(editor, firstPlot(editor));
    } else if(!hasFrequencyResponse(editor)) {
      // a circuit of resistors and sources: the same at every frequency
      statusBar()->showMessage(tr("Solved - the result does not depend on "
        "frequency"), 5000);
    } else if(fileAsksForPlot(editor)
        && !editor->property("filePlotShown").toBool()) {
      editor->setProperty("filePlotShown", true);
      createPlot(editor, firstPlot(editor));
    } else {
      statusBar()->showMessage(tr("Solved - Plots (F9) shows the frequency "
        "response"), 5000);
    }
  });
  connect(editor, &SchematicEditor::dirtyChanged, this, [this, editor]() {
    if(editor == activeEditor()) {
      // only the "changed since solved" notice: the result itself is the
      // same (laying out a long one takes a while)
      if(editor->hasResult())
        resultPanel_->setOutOfDate(!editor->isSolved());
      else
        updateResultPanel();
      updateStatus();
    }
  });
  connect(&(editor->scene()), &SchematicScene::placementChanged,
      this, &QSapecNGWindow::sceneSelectionChanged);
  connect(&(editor->scene()), &SchematicScene::notice, this,
      [this](const QString& text) {
        QApplication::beep();
        statusBar()->showMessage(text, 4000);
      });
  connect(&(editor->scene()), SIGNAL(selectionChanged()),
      this, SLOT(sceneSelectionChanged()));
  connect(this, SIGNAL(toggleGrid(bool)),
      &(editor->scene()), SLOT(setGridVisible(bool)));

  connect(editor, &QObject::destroyed, this, [this, editor]() {
      pendingPlots_.remove(editor);
      for(const PlotEntry& e : plots_)
        if(e.editor.isNull() && e.plane)
          e.plane->deleteLater();
    });
  editor->setAttribute(Qt::WA_DeleteOnClose);
  editor->setWindowIcon(theme::icon("schematic"));
  mdiArea_->addSubWindow(editor, Qt::SubWindow);
}


void QSapecNGWindow::createExamplesMenu(QMenu* parent)
{
  // installed next to the program (Windows: <dir>/examples, Linux and the
  // AppImage: share/qsapecng/examples), or the source tree for a build
  const QString app = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
    app + "/../examples",
    app + "/examples",
    app + "/../share/qsapecng/examples",
    QStringLiteral(QSAPECNG_SOURCE_EXAMPLES)
  };

  QFileInfoList files;
  foreach(const QString& dir, candidates) {
    files = QDir(dir).entryInfoList(QStringList("*.info"), QDir::Files,
      QDir::Name);
    if(!files.isEmpty())
      break;
  }
  if(files.isEmpty())
    return;

  // a long list: a list with a scroll bar inside the menu, about a dozen
  // rows high
  QMenu* menu = parent->addMenu(tr("Open E&xample"));
  QListWidget* list = new QListWidget;
  list->setFrameShape(QFrame::NoFrame);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list->setMouseTracking(true);
  // rows spaced like menu entries, on the menu's background
  // colours from the palette, looked up again each time the menu opens
  // (a theme switch while it is hidden left the old ones)
  const QString listStyle = QStringLiteral(
    "QListWidget { background: transparent; color: palette(text); }"
    "QListWidget::item { padding: 4px 22px; }"
    "QListWidget::item:selected { background: palette(highlight);"
    " color: palette(highlighted-text); }");
  list->setStyleSheet(listStyle);
  int width = 0;
  foreach(const QFileInfo& file, files) {
    QListWidgetItem* item = new QListWidgetItem(file.completeBaseName(), list);
    item->setData(Qt::UserRole, file.absoluteFilePath());
    item->setToolTip(file.absoluteFilePath());
    width = qMax(width, list->fontMetrics().horizontalAdvance(item->text()));
  }
  list->ensurePolished();
  const int row = qMax(list->sizeHintForRow(0), list->fontMetrics().height() + 8);
  for(int i = 0; i < list->count(); ++i)
    list->item(i)->setSizeHint(QSize(width + 44, row));
  list->setFixedHeight(row * qMin(12, int(files.size())) + 2);
  list->setMinimumWidth(width + 44 + list->verticalScrollBar()->sizeHint().width());
  // the row under the pointer is highlighted, as in a menu
  connect(list, &QListWidget::itemEntered, list, [list](QListWidgetItem* item) {
      list->setCurrentItem(item);
    });
  auto openItem = [this](QListWidgetItem* item) {
      if(!item)
        return;
      const QString path = item->data(Qt::UserRole).toString();
      // close the menus first
      while(QWidget* popup = QApplication::activePopupWidget())
        popup->close();
      open(path);
    };
  connect(list, &QListWidget::itemClicked, this, openItem);
  connect(list, &QListWidget::itemActivated, this, openItem);
  QWidgetAction* listAction = new QWidgetAction(menu);
  listAction->setDefaultWidget(list);
  menu->addAction(listAction);
  connect(menu, &QMenu::aboutToShow, list, [list, listStyle]() {
      list->setPalette(QApplication::palette());
      list->setStyleSheet(QString());
      list->setStyleSheet(listStyle);
      list->clearSelection();
      list->setCurrentRow(-1);
      list->scrollToTop();
    });
}


void QSapecNGWindow::updateRecentFileList(const QString& fileName)
{
  QStringList files = Settings().recentFiles();

  files.removeAll(fileName);
  files.prepend(fileName);
  while (files.size() > maxRecentFiles)
    files.removeLast();

  SettingsManager().setRecentFiles(files);
  loadRecentFileList();
}


void QSapecNGWindow::loadRecentFileList()
{
  QStringList files = Settings().recentFiles();

  int numRecentFiles = qMin(files.size(), (int) maxRecentFiles);
  for (int i = 0; i < numRecentFiles; ++i) {
    QString text = QString("&%1 %2").arg(i + 1).arg(files[i]);
    openRecentActs_[i]->setText(text);
    openRecentActs_[i]->setData(files[i]);
    openRecentActs_[i]->setVisible(true);
  }
  for (int j = numRecentFiles; j < maxRecentFiles; ++j)
    openRecentActs_[j]->setVisible(false);
}


void QSapecNGWindow::userDefRequested()
{
  SchematicEditor* editor = activeEditor();

  try {
    if(editor)
      editor->scene().setUserDefRequest();
  } catch (...) {
    QMessageBox::warning(this, tr("File corrupted"),
      tr("Incorrect definition of user-defined component."));
  }
}


void QSapecNGWindow::disconnectedWireSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().setWireSessionRequest(false);
}


void QSapecNGWindow::groundSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().setActiveItem(SchematicScene::GroundItemType);
}


void QSapecNGWindow::portSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().setActiveItem(SchematicScene::PortItemType);
}


void QSapecNGWindow::outSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().setActiveItem(SchematicScene::OutItemType);
}


void QSapecNGWindow::voltmeterSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().setActiveItem(SchematicScene::VoltmeterItemType);
}


void QSapecNGWindow::ammeterSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().setActiveItem(SchematicScene::AmmeterItemType);
}


void QSapecNGWindow::labelSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().createLabel();
}


void QSapecNGWindow::clearSessionRequested()
{
  SchematicEditor* editor = activeEditor();

  if(editor)
    editor->scene().resetStatus();
}


void QSapecNGWindow::setActiveSubWindow(QWidget* window)
{
  if (!window)
    return;

  mdiArea_->setActiveSubWindow(qobject_cast<QMdiSubWindow*>(window));
}


void QSapecNGWindow::syncWithCurrentEditor()
{
  // QMdiArea does not always emit subWindowActivated() in tabbed mode
  // (e.g. for files opened before the main window is shown), so the
  // window state is refreshed from currentSubWindow() explicitly.
  subWindowActivated(mdiArea_->currentSubWindow());
  sceneSelectionChanged();
  updateMenus();
  updateDocks();
  updateResultPanel();
  updateStatus();
}


void QSapecNGWindow::updateStatus()
{
  SchematicEditor* editor = activeEditor();
  const bool onWorkplane = false;

  QString status;
  if(!editor) {
    status = tr("No schematic open - File > New (Ctrl+N), File > Open, or "
                "pick a component to start one");
  } else if(editor->isBusy()) {
    status = tr("Solving...");
  } else {
    SchematicScene& scene = editor->scene();
    const QString hint = scene.toolHint();
    const int selected = scene.selectedItems().size();
    if(!hint.isEmpty())
      status = hint;
    else if(selected > 0)
      status = (selected == 1 ? tr("1 item selected")
                              : tr("%1 items selected").arg(selected))
        + tr(" - drag to move, R rotates, Del deletes, double-click edits");
    else if(editor->isSolved())
      status = editor->lastResult().anySingular()
        ? tr("Solved - no unique solution (see the Result panel)")
        : tr("Solved - the transfer function is in the Result panel");
    else if(editor->wasStopped())
      status = tr("Stopped - press F8 to resolve again");
    else if(editor->hasResult())
      status = tr("Changed since the last Resolve - press F8 to update the "
                  "result");
    else
      status = tr("Ready - draw the circuit (W draws wires), mark the "
                  "output with Out, then Resolve (F8)");
  }
  statusLabel_->setText(status);

  const bool showSheet = editor && !onWorkplane;
  countLabel_->setVisible(showSheet);
  zoomLabel_->setVisible(showSheet);
  positionLabel_->setVisible(showSheet);
  if(showSheet) {
    const int parts = editor->scene().partCount();
    const int wires = editor->scene().wireCount();
    countLabel_->setText(
        (parts == 1 ? tr("1 part") : tr("%1 parts").arg(parts))
      + QStringLiteral(" · ")
      + (wires == 1 ? tr("1 wire") : tr("%1 wires").arg(wires)));
    const QTransform t = editor->view().transform();
    zoomLabel_->setText(QStringLiteral("%1 %")
      .arg(qRound(100. * (qAbs(t.m11()) + qAbs(t.m12())))));
  }
}


void QSapecNGWindow::updateResultPanel()
{
  SchematicEditor* editor = activeEditor();
  if(editor && editor->hasResult())
    resultPanel_->showResult(editor->lastResult(), !editor->isSolved());
  else
    resultPanel_->showPlaceholder();
}


void QSapecNGWindow::subWindowActivated(QMdiSubWindow* window)
{
  progressBar_->setRange(0, 1);
  progressBar_->reset();
  progressBar_->hide();

  if(window) {
    SchematicEditor* editor = qobject_cast<SchematicEditor*>(window);
    if(editor && editor->isRunning()) {
      progressBar_->setRange(0, 1000);
      progressBar_->show();
      updateSolveProgress();
      if(editor->isBusy())
        undoRedoGroup_->setActiveStack(0);
    }
  }
}


void QSapecNGWindow::stackEditor(SchematicEditor* editor)
{
  setupSchematicEditor(editor);
  editor->showMaximized();
  mdiArea_->setActiveSubWindow(editor);
  syncWithCurrentEditor();

  statusBar()->showMessage(tr("Component loaded"), 2000);
}


QSapecNGWindow& operator<<(QSapecNGWindow& window, const QString& str)
{
  if(QFile::exists(str))
    window.open(str);

  return window;
}


}
