/*
    SapecNG - Next Generation Symbolic Analysis Program for Electric Circuit
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


#include "config.h"
#include "gui/settings.h"

#include <QtCore/QSettings>
#include <QtCore/QRegExp>
#include <QtCore/QDir>


namespace qsapecng
{


QRegExp Settings::notation_;

QPoint Settings::mwPos_;
QSize Settings::mwSize_;
QByteArray Settings::mwState_;

QString Settings::workspace_;

QFont Settings::appFont_;

int Settings::logLvl_;
QColor Settings::debugColor_;
QColor Settings::infoColor_;
QColor Settings::warningColor_;
QColor Settings::errorColor_;
QColor Settings::fatalColor_;

QStringList Settings::recentFiles_;



void Settings::load()
{
  QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);

  settings.beginGroup("Application");
    appFont_ = settings.value("font").value<QFont>();
    recentFiles_ = settings.value("recents").value<QStringList>();
    workspace_ =
      settings.value("workspace", QDir::homePath()).value<QString>();
    notation_ = settings.value(
        "notation",
        QRegExp("[-+]?(\\d*\\.)?\\d+([eE][-+]?\\d+)?")
      ).value<QRegExp>();
  settings.endGroup();

  settings.beginGroup("MainWindow");
    mwPos_ = settings.value("pos", QPoint(200, 200)).value<QPoint>();
    mwSize_ = settings.value("size", QSize(400, 400)).value<QSize>();
    mwState_ = settings.value("state").value<QByteArray>();
  settings.endGroup();

  settings.beginGroup("Log");
    logLvl_ = settings.value("logLvl", QVariant(0)).value<int>();
    debugColor_ =
      settings.value("debugColor", QColor(Qt::black)).value<QColor>();
    infoColor_ =
      settings.value("infoColor", QColor(Qt::black)).value<QColor>();
    warningColor_ =
      settings.value("warningColor", QColor(Qt::blue)).value<QColor>();
    errorColor_ =
      settings.value("errorColor", QColor(Qt::red)).value<QColor>();
    fatalColor_ =
      settings.value("fatalColor", QColor(Qt::darkRed)).value<QColor>();
  settings.endGroup();
}


void Settings::save()
{
  QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);

  settings.beginGroup("Application");
    settings.setValue("font", appFont_);
    settings.setValue("recents", recentFiles_);
    settings.setValue("workspace", workspace_);
    settings.setValue("notation", notation_);
  settings.endGroup();

  settings.beginGroup("MainWindow");
    settings.setValue("pos", mwPos_);
    settings.setValue("size", mwSize_);
    settings.setValue("state", mwState_);
  settings.endGroup();

  settings.beginGroup("Log");
    settings.setValue("logLvl", logLvl_);
    settings.setValue("debugColor", debugColor_);
    settings.setValue("infoColor", infoColor_);
    settings.setValue("warningColor", warningColor_);
    settings.setValue("errorColor", errorColor_);
    settings.setValue("fatalColor", fatalColor_);
  settings.endGroup();
}


}
