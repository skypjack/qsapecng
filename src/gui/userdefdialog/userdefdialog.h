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


#ifndef USERDEFDIALOG_H
#define USERDEFDIALOG_H


#include <QtGui/QDialog>


class QVBoxLayout;

#include <QtGui/QLineEdit>
#include <QtGui/QSpinBox>


namespace qsapecng
{


class UserDefDialog: public QDialog
{

  Q_OBJECT

public:
  UserDefDialog(QWidget* parent = 0);

  inline QString fileName() const { return fileName_->text(); }
  inline int size() const { return size_->value(); }

private slots:
  void chooseFile();

private:
  void createLayout();

private:
  QVBoxLayout* layout_;
  QLineEdit* fileName_;
  QSpinBox* size_;

};


}


#endif // USERDEFDIALOG_H
