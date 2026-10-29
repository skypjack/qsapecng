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


#ifndef SIDEBARMODEL_H
#define SIDEBARMODEL_H


#include <QAbstractListModel>

#include <QList>
#include <QMap>


namespace qsapecng
{


class SideBarModel: public QAbstractListModel
{

  Q_OBJECT

public:
  SideBarModel(QObject* parent = 0);

  int rowCount(const QModelIndex& parent = QModelIndex()) const;

  QVariant data(
    const QModelIndex& index,
    int role = Qt::DisplayRole
  ) const;

  QVariant headerData(
    int section,
    Qt::Orientation orientation,
    int role = Qt::DisplayRole
  ) const;

  Qt::ItemFlags flags(const QModelIndex& index) const;
  Qt::DropActions supportedDragActions() const override
    { return Qt::CopyAction; }

  Qt::DropActions supportedDropActions() const;

  QStringList mimeTypes() const;
  QMimeData* mimeData(const QModelIndexList& indexes) const;

public:
  // update the resistor/conductance icons after the symbol style changed
  void refreshSymbols();

private:
  void setupSideBar();

  QList< QMap<int, QVariant> > componentList_;

};


}


#endif // SIDEBARMODEL_H
