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


#include "gui/configdialog/configpage.h"
#include "gui/settings.h"
#include "config.h"
#include "gui/theme.h"
#include "gui/workplane/workplane.h"

#include "logger/logger.h"

#include <QApplication>

#include <QCheckBox>
#include <QSpinBox>
#include <algorithm>
#include <QThread>
#include <QComboBox>
#include <QFormLayout>
#include <QSettings>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>

#include <QSignalMapper>

#include <QFontDialog>
#include <QColorDialog>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>


namespace qsapecng
{


GeneralPage::GeneralPage(QWidget* parent)
  : ConfigPage(parent)
{
  Settings settings;

  /*
   * Startup Box
   */
  QGroupBox* startupBox = new QGroupBox(tr("Startup"), this);
  splash_ = new QCheckBox(tr("Show the &splash screen"));
  splash_->setChecked(QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)
    .value(QStringLiteral("Application/showSplash"), true).toBool());
  connect(splash_, &QCheckBox::toggled, this, [this]() { setPageModified(true); });
  QHBoxLayout* startupLayout = new QHBoxLayout;
  startupLayout->addWidget(splash_);
  startupBox->setLayout(startupLayout);

  /*
   * Solver Box
   */
  QGroupBox* solverBox = new QGroupBox(tr("Solver"), this);
  threads_ = new QSpinBox;
  threads_->setRange(1, std::max(1, QThread::idealThreadCount()));
  threads_->setValue(QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)
    .value(QStringLiteral("Application/solverThreads"),
      QThread::idealThreadCount()).toInt());
  threads_->setToolTip(tr("The solver works on this many processor cores "
    "at most, for all the outputs (Out markers and meters) together: "
    "several outputs are solved at the same time, and a single output is "
    "split among the cores"));
  connect(threads_, &QSpinBox::valueChanged, this,
    [this]() { setPageModified(true); });
  QLabel* threadsLabel = new QLabel(tr("Use a &maximum of"));
  QLabel* threadsTail = new QLabel(tr("cores (of %1)")
    .arg(QThread::idealThreadCount()));
  threadsLabel->setBuddy(threads_);
  QHBoxLayout* threadsLayout = new QHBoxLayout;
  threadsLayout->addWidget(threadsLabel);
  threadsLayout->addWidget(threads_);
  threadsLayout->addWidget(threadsTail);
  threadsLayout->addStretch();

  // Resolve gives the numeric result (Digit) and the plots first; the
  // symbolic one (Result, Raw) lists every term and can take hours
  symbolic_ = new QComboBox;
  symbolic_->addItem(tr("Always"), 0);
  symbolic_->addItem(tr("Ask when it takes long"), 1);
  symbolic_->addItem(tr("Never (numeric only)"), 2);
  const int mode = QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)
    .value(QStringLiteral("Application/symbolicResult"), 1).toInt();
  symbolic_->setCurrentIndex(std::max(0, symbolic_->findData(mode)));
  symbolic_->setToolTip(tr("Resolve gives the numeric result (Digit) and "
    "the plots at once, whatever the circuit. The symbolic result (Result, "
    "Raw) can take hours for a big circuit: it is worked out in the "
    "background, and Ask shows the time it would take first when that is "
    "more than about 10 seconds. A small circuit gets it at once, unless "
    "Never is chosen"));
  connect(symbolic_, &QComboBox::currentIndexChanged, this,
    [this]() { setPageModified(true); });
  QLabel* symbolicLabel = new QLabel(tr("&Symbolic result:"));
  symbolicLabel->setBuddy(symbolic_);
  QHBoxLayout* symbolicLayout = new QHBoxLayout;
  symbolicLayout->addWidget(symbolicLabel);
  symbolicLayout->addWidget(symbolic_);
  symbolicLayout->addStretch();

  QVBoxLayout* solverLayout = new QVBoxLayout;
  solverLayout->addLayout(threadsLayout);
  solverLayout->addLayout(symbolicLayout);
  solverBox->setLayout(solverLayout);

  /*
   * Log Settings Box
   */
  QGroupBox* logSettingsBox = new QGroupBox(
      tr("Log Settings"), this
    );
  logLevel_ = new QComboBox;
  logLevel_->addItem(tr("Debug"), QVariant(sapecng::Logger::DEBUG));
  logLevel_->addItem(tr("Info"), QVariant(sapecng::Logger::INFO));
  logLevel_->addItem(tr("Warning"), QVariant(sapecng::Logger::WARNING));
  logLevel_->addItem(tr("Error"), QVariant(sapecng::Logger::ERROR));
  logLevel_->addItem(tr("Fatal"), QVariant(sapecng::Logger::FATAL));
  switch((sapecng::Logger::LogLevel) settings.logLvl())
  {
  case sapecng::Logger::DEBUG:
    logLevel_->setCurrentIndex(0);
    break;
  case sapecng::Logger::INFO:
    logLevel_->setCurrentIndex(1);
    break;
  case sapecng::Logger::WARNING:
    logLevel_->setCurrentIndex(2);
    break;
  case sapecng::Logger::ERROR:
    logLevel_->setCurrentIndex(3);
    break;
  case sapecng::Logger::FATAL:
    logLevel_->setCurrentIndex(4);
    break;
  }
  connect(logLevel_, SIGNAL(currentIndexChanged(int)),
    this, SLOT(levelChanged()));

  QLabel* logLevelLabel = new QLabel(tr("Log Level:"));
  logLevelLabel->setBuddy(logLevel_);

  QLabel* logColorLabel = new QLabel(tr("Log Colors"));
  logColorLabel->setAlignment(Qt::AlignCenter);

  colorMapper_ = new QSignalMapper(this);

  debugColor_ = new QPushButton;
  debugColor_->setAutoFillBackground(true);
  debugColor_->setText(tr("Debug"));
  debugColor_->setFlat(true);
  debugColor_->setPalette(QPalette(settings.debugColor()));
  colorMapper_->setMapping(debugColor_, sapecng::Logger::DEBUG);
  connect(debugColor_, SIGNAL(clicked()),
    colorMapper_, SLOT(map()));

  infoColor_ = new QPushButton;
  infoColor_->setAutoFillBackground(true);
  infoColor_->setText(tr("Info"));
  infoColor_->setFlat(true);
  infoColor_->setPalette(QPalette(settings.infoColor()));
  colorMapper_->setMapping(infoColor_, sapecng::Logger::INFO);
  connect(infoColor_, SIGNAL(clicked()),
    colorMapper_, SLOT(map()));

  warningColor_ = new QPushButton;
  warningColor_->setAutoFillBackground(true);
  warningColor_->setText(tr("Warning"));
  warningColor_->setFlat(true);
  warningColor_->setPalette(QPalette(settings.warningColor()));
  colorMapper_->setMapping(warningColor_, sapecng::Logger::WARNING);
  connect(warningColor_, SIGNAL(clicked()),
    colorMapper_, SLOT(map()));

  errorColor_ = new QPushButton;
  errorColor_->setAutoFillBackground(true);
  errorColor_->setText(tr("Error"));
  errorColor_->setFlat(true);
  errorColor_->setPalette(QPalette(settings.errorColor()));
  colorMapper_->setMapping(errorColor_, sapecng::Logger::ERROR);
  connect(errorColor_, SIGNAL(clicked()),
    colorMapper_, SLOT(map()));

  fatalColor_ = new QPushButton;
  fatalColor_->setAutoFillBackground(true);
  fatalColor_->setText(tr("Fatal"));
  fatalColor_->setFlat(true);
  fatalColor_->setPalette(QPalette(settings.fatalColor()));
  colorMapper_->setMapping(fatalColor_, sapecng::Logger::FATAL);
  connect(fatalColor_, SIGNAL(clicked()),
    colorMapper_, SLOT(map()));

  connect(colorMapper_, &QSignalMapper::mappedInt,
    this, [this](int id) { changeColor(id); });

  QHBoxLayout* logLevelLayout = new QHBoxLayout;
  logLevelLayout->addWidget(logLevelLabel);
  logLevelLayout->addWidget(logLevel_);

  QHBoxLayout* logColorLayout = new QHBoxLayout;
  logColorLayout->addWidget(debugColor_);
  logColorLayout->addWidget(infoColor_);
  logColorLayout->addWidget(warningColor_);
  logColorLayout->addWidget(errorColor_);
  logColorLayout->addWidget(fatalColor_);

  QGridLayout* logSettingsLayout = new QGridLayout;
  logSettingsLayout->addLayout(logLevelLayout, 1, 1);
  logSettingsLayout->addWidget(logColorLabel, 0, 2);
  logSettingsLayout->addLayout(logColorLayout, 1, 2);
  logSettingsLayout->setHorizontalSpacing(32);
  logSettingsLayout->setColumnStretch(0, 1);
  logSettingsLayout->setColumnStretch(3, 1);

  logSettingsBox->setLayout(logSettingsLayout);

  /*
   * Main layout
   */
  QVBoxLayout* mainLayout = new QVBoxLayout;
  mainLayout->addWidget(startupBox);
  mainLayout->addWidget(solverBox);
  mainLayout->addWidget(logSettingsBox);
  mainLayout->addStretch();
  setLayout(mainLayout);
}


void GeneralPage::apply()
{
  QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)
    .setValue(QStringLiteral("Application/showSplash"), splash_->isChecked());
  QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)
    .setValue(QStringLiteral("Application/solverThreads"), threads_->value());
  QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)
    .setValue(QStringLiteral("Application/symbolicResult"),
      symbolic_->currentData().toInt());

  SettingsManager manager;

  manager.setLogLvl(logLevel_->itemData(logLevel_->currentIndex()).toInt());
  manager.setDebugColor(debugColor_->palette().color(QPalette::Window));
  manager.setInfoColor(infoColor_->palette().color(QPalette::Window));
  manager.setWarningColor(warningColor_->palette().color(QPalette::Window));
  manager.setErrorColor(errorColor_->palette().color(QPalette::Window));
  manager.setFatalColor(fatalColor_->palette().color(QPalette::Window));

  setPageModified(false);
}


void GeneralPage::changeColor(int id)
{
  QColor color;
  sapecng::Logger::LogLevel level =
    (sapecng::Logger::LogLevel) id;

  switch(level)
  {
  case sapecng::Logger::DEBUG:
    color = QColorDialog::getColor(
        debugColor_->palette().color(QPalette::Window),
        this, tr("Choose a color")
      );
    if(color.isValid()) {
      debugColor_->setPalette(QPalette(color));
      setPageModified(true);
    }
    break;
  case sapecng::Logger::INFO:
    color = QColorDialog::getColor(
        infoColor_->palette().color(QPalette::Window),
        this, tr("Choose a color")
      );
    if(color.isValid()) {
      infoColor_->setPalette(QPalette(color));
      setPageModified(true);
    }
    break;
  case sapecng::Logger::WARNING:
    color = QColorDialog::getColor(
        warningColor_->palette().color(QPalette::Window),
        this, tr("Choose a color")
      );
    if(color.isValid()) {
      warningColor_->setPalette(QPalette(color));
      setPageModified(true);
    }
    break;
  case sapecng::Logger::ERROR:
    color = QColorDialog::getColor(
        errorColor_->palette().color(QPalette::Window),
        this, tr("Choose a color")
      );
    if(color.isValid()) {
      errorColor_->setPalette(QPalette(color));
      setPageModified(true);
    }
    break;
  case sapecng::Logger::FATAL:
    color = QColorDialog::getColor(
        fatalColor_->palette().color(QPalette::Window),
        this, tr("Choose a color")
      );
    if(color.isValid()) {
      fatalColor_->setPalette(QPalette(color));
      setPageModified(true);
    }
    break;
  }
}


void GeneralPage::levelChanged()
{
  setPageModified(true);
}


AppearancePage::AppearancePage(QWidget* parent)
  : ConfigPage(parent)
{
  auto combo = [this](const QStringList& items, int current) {
      QComboBox* box = new QComboBox;
      box->addItems(items);
      box->setCurrentIndex(current);
      connect(box, &QComboBox::currentIndexChanged, this,
        [this]() { setPageModified(true); });
      return box;
    };

  mode_ = combo({ tr("Follow the system"), tr("Light"), tr("Dark") },
    static_cast<int>(theme::mode()));

  QGroupBox* windowBox = new QGroupBox(tr("Colours"), this);
  QFormLayout* windowLayout = new QFormLayout;
  windowLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  windowLayout->addRow(tr("&Theme:"), mode_);
  windowBox->setLayout(windowLayout);

  resistor_ = combo({ tr("ANSI"), tr("IEC / DIN") },
    theme::resistorStyle() == theme::ResistorStyle::Ansi ? 0 : 1);
  labels_ = combo({ tr("Name only"), tr("Name and value") },
    theme::showComponentValues() ? 1 : 0);
  gridStyle_ = combo({ tr("Dots"), tr("Lines") },
    theme::gridStyle() == theme::GridStyle::Dots ? 0 : 1);
  gridStrength_ = combo({ tr("Faint"), tr("Medium"), tr("Strong") },
    theme::gridStrength());

  QGroupBox* schematicBox = new QGroupBox(tr("Schematic"), this);
  QFormLayout* schematicLayout = new QFormLayout;
  schematicLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
  schematicLayout->addRow(tr("&Resistor symbol:"), resistor_);
  schematicLayout->addRow(tr("Component &labels:"), labels_);
  schematicLayout->addRow(tr("&Grid:"), gridStyle_);
  schematicLayout->addRow(tr("Grid &strength:"), gridStrength_);
  schematicBox->setLayout(schematicLayout);

  antialias_ = new QCheckBox(tr("Smooth (&antialiased) curves and lines"));
  antialias_->setChecked(WorkPlane::antialiased());
  connect(antialias_, &QCheckBox::toggled, this, [this]() { setPageModified(true); });

  QGroupBox* plotBox = new QGroupBox(tr("Plots"), this);
  QVBoxLayout* plotLayout = new QVBoxLayout;
  plotLayout->addWidget(antialias_);
  plotBox->setLayout(plotLayout);

  QVBoxLayout* mainLayout = new QVBoxLayout;
  mainLayout->addWidget(windowBox);
  mainLayout->addWidget(schematicBox);
  mainLayout->addWidget(plotBox);
  mainLayout->addStretch();
  setLayout(mainLayout);
}


void AppearancePage::apply()
{
  const theme::Mode mode = static_cast<theme::Mode>(mode_->currentIndex());
  if(mode != theme::mode())
    theme::setMode(mode);
  theme::setResistorStyle(resistor_->currentIndex() == 0
    ? theme::ResistorStyle::Ansi : theme::ResistorStyle::Din);
  theme::setShowComponentValues(labels_->currentIndex() == 1);
  theme::setGridStyle(gridStyle_->currentIndex() == 0
    ? theme::GridStyle::Dots : theme::GridStyle::Lines);
  theme::setGridStrength(gridStrength_->currentIndex());
  WorkPlane::setAntialiasedDefault(antialias_->isChecked());

  setPageModified(false);
  emit applied();
}


namespace
{

// the font dialog as a page: Esc (or any accept / reject) would hide it and
// leave the page empty; the Configure dialog's own buttons decide instead
class EmbeddedFontDialog: public QFontDialog
{
public:
  EmbeddedFontDialog() { setWindowFlags(Qt::Widget); }
  void done(int) override { }
  void accept() override { }
  void reject() override { }
};

}


FontPage::FontPage(QWidget* parent)
  : ConfigPage(parent)
{
  fontPage_ = new EmbeddedFontDialog;
  fontPage_->setCurrentFont(QApplication::font());
  fontPage_->setOption(QFontDialog::NoButtons, true);

  connect(fontPage_, SIGNAL(currentFontChanged(const QFont&)),
    this, SLOT(currentFontChanged()));

  QHBoxLayout* mainLayout = new QHBoxLayout;
  mainLayout->addWidget(fontPage_);
  setLayout(mainLayout);
}


void FontPage::apply()
{
  SettingsManager().setAppFont(fontPage_->currentFont());
  QApplication::setFont(fontPage_->currentFont());
  setPageModified(false);
}


void FontPage::currentFontChanged()
{
  setPageModified(true);
}


}
