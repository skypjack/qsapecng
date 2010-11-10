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


#ifndef INODE_H
#define INODE_H


#include <QtCore/QSharedData>


namespace qsapecng
{


class Node: public QSharedData
{

public:
  Node(): QSharedData() { dirty_ = true; }
  Node(const Node& other)
  {
    dirty_ = other.dirty_;
    if(!dirty_)
      value_ = other.value_;
  }

  inline int value() const
    { return value_; }
  inline void setValue(int value)
    { value_ = value; dirty_ = false; }

  inline bool isValid() const
    { return !dirty_; }
  inline void setDirty()
    { dirty_ = true; }

private:
  int value_;
  bool dirty_;

};


}


#endif // INODE_H
