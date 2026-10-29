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


#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H


#include <QDialog>
#include <QList>

#include "gui/configdialog/configpage.h"


class QListWidget;
class QListWidgetItem;
class QStackedWidget;


namespace qsapecng
{


class ConfigDialog: public QDialog
{

  Q_OBJECT

public:
  ConfigDialog(QWidget* parent = 0);

signals:
  // the Appearance page was applied: open schematics and plots need redrawing
  void appearanceApplied();

public slots:
  void changePage(QListWidgetItem* current, QListWidgetItem* previous);

private slots:
  void apply();
  void applyAndClose();
  void checkBeforeClose();

private:
  void createIcons();
  void createPages();

  QListWidget* contents_;
  QStackedWidget* pages_;

  QList<ConfigPage*> pages() const;

  ConfigPage* generalPage_;
  ConfigPage* appearancePage_;
  ConfigPage* fontPage_;

};


}


#endif // CONFIGDIALOG_H
