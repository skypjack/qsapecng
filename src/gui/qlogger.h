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


#ifndef QLOGGER_H
#define QLOGGER_H


#include "gui/theme.h"
#include "logger/logpolicy.h"
#include "logger/logger.h"

#include <QObject>
#include <QRegularExpression>
#include <QColor>
#include <QEvent>
#include <QList>
#include <QPair>
#include <QTextEdit>
#include <QContextMenuEvent>
#include <QMenu>


typedef sapecng::LogPolicy SapecNGLogger;
typedef sapecng::LogPolicy SapecNGLogPolicy;


namespace qsapecng
{


class QLogPolicy: public QObject, public SapecNGLogPolicy
{

  Q_OBJECT

public:
  QLogPolicy(QObject* parent = 0)
    : QObject(parent), SapecNGLogPolicy() { }
  ~QLogPolicy() { }

  void operator()(const std::string& msg)
  {
    QString qmsg = QString::fromStdString(msg);
    // the configured colour is passed on unchanged; LogViewer adapts it
    // to the current theme (and again whenever the theme changes)
    QColor color;
    if(qmsg.startsWith("[DEBUG]"))
      color = debugColor_;
    else if(qmsg.startsWith("[INFO]"))
      color = infoColor_;
    else if(qmsg.startsWith("[WARNING]"))
      color = warningColor_;
    else if(qmsg.startsWith("[ERROR]"))
      color = errorColor_;
    else if(qmsg.startsWith("[FATAL]"))
      color = fatalColor_;
    emit entry(qmsg, color);
  }

  inline QColor debugColor() const
    { return debugColor_; }
  inline void setDebugColor(QColor color)
    { debugColor_ = color; }

  inline QColor infoColor() const
    { return infoColor_; }
  inline void setInfoColor(QColor color)
    { infoColor_ = color; }

  inline QColor warningColor() const
    { return warningColor_; }
  inline void setWarningColor(QColor color)
    { warningColor_ = color; }

  inline QColor errorColor() const
    { return errorColor_; }
  inline void setErrorColor(QColor color)
    { errorColor_ = color; }

  inline QColor fatalColor() const
    { return fatalColor_; }
  inline void setFatalColor(QColor color)
    { fatalColor_ = color; }

signals:
  void entry(const QString& msg, const QColor& color);

private:
  QColor debugColor_;
  QColor infoColor_;
  QColor warningColor_;
  QColor errorColor_;
  QColor fatalColor_;

};


class QLogger
{

public:
  static void setLevel(sapecng::Logger::LogLevel lvl)
    { sapecng::Logger::setLevel(lvl); }

  static void setPolicy(QLogPolicy* policy)
    { sapecng::Logger::setPolicy(policy); }

  static void debug(const QString& msg)
    {
      sapecng::Logger().get(sapecng::Logger::DEBUG)
        << (QObject::tr("[DEBUG] ") + msg).toStdString();
    }

  static void info(const QString& msg)
    {
      sapecng::Logger().get(sapecng::Logger::INFO)
        << (QObject::tr("[INFO] ") + msg).toStdString();
    }

  static void warning(const QString& msg)
    {
      sapecng::Logger().get(sapecng::Logger::WARNING)
        << (QObject::tr("[WARNING] ") + msg).toStdString();
    }

  static void error(const QString& msg)
    {
      sapecng::Logger().get(sapecng::Logger::ERROR)
        << (QObject::tr("[ERROR] ") + msg).toStdString();
    }

  static void fatal(const QString& msg)
    {
      sapecng::Logger().get(sapecng::Logger::FATAL)
        << (QObject::tr("[FATAL] ") + msg).toStdString();
    }

};


// Read-only message log that keeps every entry with its configured colour
// and re-renders them when the palette changes, so the text stays readable
// after switching between light and dark themes.
class LogViewer: public QTextEdit
{

  Q_OBJECT

public:
  LogViewer(QWidget* parent = 0): QTextEdit(parent) { }

public slots:
  void addEntry(const QString& msg, const QColor& color)
  {
    entries_.append(qMakePair(msg, color));
    write(msg, color);
  }

  void clearEntries()
  {
    entries_.clear();
    clear();
  }

protected:
  // the usual menu (copy, select all) and Clear
  void contextMenuEvent(QContextMenuEvent* event) override
  {
    QMenu* menu = createStandardContextMenu();
    menu->addSeparator();
    QAction* clearAct = menu->addAction(tr("C&lear"));
    clearAct->setEnabled(!entries_.isEmpty());
    connect(clearAct, &QAction::triggered, this, &LogViewer::clearEntries);
    menu->exec(event->globalPos());
    delete menu;
  }

  void changeEvent(QEvent* event) override
  {
    QTextEdit::changeEvent(event);
    if(event->type() == QEvent::PaletteChange) {
      clear();
      for(const auto& e : entries_)
        write(e.first, e.second);
    }
  }

private:
  void write(const QString& msg, const QColor& color)
  {
    setTextColor(theme::readableOn(color));
    append(msg);
  }

  QList< QPair<QString, QColor> > entries_;

};


}


#endif // QLOGGER_H
