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


#include "model/metacircuit.h"
#include "parser/parser_factory.h"
#include "parser/crc_circuit.h"

#include "gui/qtsolutions/qtpropertyeditor/QtProperty"
#include "gui/editor/schematiceditor.h"
#include "gui/editor/schematicsceneparser.h"
#include "gui/settings.h"
#include "gui/qlogger.h"

#include <QtCore/QFileInfo>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QPlainTextEdit>
#include <QtGui/QUndoStack>
#include <QtGui/QApplication>
#include <QtGui/QVBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QFocusEvent>

#include <fstream>
#include <sstream>
#include <map>


namespace qsapecng
{


SchematicEditor::SchematicEditor(QWidget* parent, Qt::WindowFlags flags)
  : QMdiSubWindow(parent, flags), solved_(false)
{
  scene_ = new SchematicScene(this);
  view_ = new SchematicView(this);

  view_->setScene(scene_);
  connect(scene_, SIGNAL(sceneRectChanged(const QRectF&)),
    view_, SLOT(updateSceneRect(const QRectF&)));
//   scene_->setSceneRect(0, 0, 1024, 768);
  scene_->setSceneRect(QRectF());
  scene_->addRect(0, 0, 1E-99, 1E-99)->setVisible(false);

  setWidget(view_);

  connect(scene_->undoRedoStack(), SIGNAL(indexChanged(int)),
    this, SLOT(setDirty()));
  connect(scene_->undoRedoStack(), SIGNAL(cleanChanged(bool)),
    this, SLOT(cleanChanged(bool)));

  connect(&solver_, SIGNAL(finished()), this, SLOT(finished()));

  connect(this, SIGNAL(windowStateChanged(Qt::WindowStates,Qt::WindowStates)),
    this, SLOT(stateChanged(Qt::WindowStates,Qt::WindowStates)));

  isUntitled_ = true;
  curFile_ = tr("Untitled");

  setGeometry(0, 0, 640, 480);
  setWindowTitle(curFile_ + "[*]");
  setWindowIcon(QIcon(":/images/grid.png"));
  setAttribute(Qt::WA_DeleteOnClose);
}


QString SchematicEditor::userFriendlyCurrentFile() const
{
  return strippedName(curFile_);
}


QString SchematicEditor::currentFile() const
{
  return curFile_;
}


bool SchematicEditor::accept(WorkPlane& workplane)
{
  if(!solved_) {
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, tr("Dirty circuit"),
      QString("'%1'").arg(userFriendlyCurrentFile()
        + tr(" has been modified.\n Solve the circuit?")),
      QMessageBox::Yes | QMessageBox::No
        | QMessageBox::Cancel
    );

    if(ret == QMessageBox::Cancel)
      return false;

    if(ret == QMessageBox::Yes) {
      solve();
      return false;
    }
  }

  std::map<std::string, double> values;
  QList<Item*> items = scene_->activeItems();
  foreach(Item* item, items)
    if(SchematicScene::itemProperties(item)) {
      QtProperty* props = SchematicScene::itemProperties(item);

      QHash<QString, QString> subs;
      if(props) {
        subs.insert("__NAME", props->valueText());
        foreach(QtProperty* prop, props->subProperties())
          subs.insert(prop->propertyName(), prop->valueText());
      }

      if(subs.contains("Value"))
        values[subs.value("__NAME").toStdString()]
          = subs.value("Value").toDouble();
      else if(subs.contains("M"))
        values[subs.value("__NAME").toStdString()]
          = subs.value("M").toDouble();

      if(subs.contains("lp:name") && subs.contains("lp:value"))
        values[subs.value("lp:name").toStdString()]
          = subs.value("lp:value").toDouble();

      if(subs.contains("ls:name") && subs.contains("ls:value"))
        values[subs.value("ls:name").toStdString()]
          = subs.value("ls:value").toDouble();
    }

  workplane.setData(
      values,
      solver_.raw_numerator(),
      solver_.raw_denominator()
    );

  return true;
}


bool SchematicEditor::save()
{
  if(isUntitled_) {
    return saveAs();
  } else {
    return saveFile(curFile_);
  }
}


bool SchematicEditor::saveAs()
{
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save As"), Settings().workspace(),
      tr("%1;;%2;;%3")
        .arg(tr("Info files (*.info)"))
        .arg(tr("XML files (*.xml)"))
        .arg(tr("Netlist files (*.crc)"))
    );
  if(fileName.isEmpty())
    return false;

  return saveFile(fileName);
}


bool SchematicEditor::saveFile(const QString& fileName)
{
  QFile file(fileName);
  if(!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("Write file"),
      tr("Unable to write file ")
        + QString("%1:\n%2.")
          .arg(fileName)
          .arg(file.errorString())
    );
    return false;
  } else {
    file.close();
  }

  QLogger::info(QObject::tr("Saving file..."));
//   QApplication::setOverrideCursor(Qt::WaitCursor);
  QCursor cur = cursor();
  setCursor(Qt::WaitCursor);

  QFileInfo fileInfo(file);
  std::ofstream out_file(QFile::encodeName(fileName));
  sapecng::abstract_builder* out =
    sapecng::builder_factory::builder(
      fileInfo.suffix().toStdString(), out_file);

  SchematicSceneParser* parser = new SchematicSceneParser(*scene_);

  if(out)
    parser->parse(*out);

  delete parser;
  delete out;

  setCurrentFile(fileName);
  scene_->undoRedoStack()->setClean();
  emit fileSaved(fileName);

//   QApplication::restoreOverrideCursor();
  setCursor(cur);

  return true;
}


bool SchematicEditor::loadFile(const QString& fileName)
{
  if(maybeSave()) {
    QFile file(fileName);
    if(!file.open(QFile::ReadOnly | QFile::Text)) {
      QMessageBox::warning(this, tr("Read file"),
        tr("Unable to read file ")
        + QString("%1:\n%2.")
          .arg(fileName)
          .arg(file.errorString())
      );
      return false;
    } else {
      file.close();
    }

    scene_->clearSchematicScene();
    QLogger::info(QObject::tr("Loading file..."));
//     QApplication::setOverrideCursor(Qt::WaitCursor);
    QCursor cur = cursor();
    setCursor(Qt::WaitCursor);

    SchematicSceneBuilder* out = new SchematicSceneBuilder(*scene_);

    QFileInfo fileInfo(file);
    std::ifstream in_file(QFile::encodeName(fileName));
    sapecng::abstract_parser* parser =
      sapecng::parser_factory::parser(
        fileInfo.suffix().toStdString(), in_file);

    if(parser) {
      scene_->undoRedoStack()->beginMacro(QObject::tr("Load file"));
      parser->parse(*out);
      scene_->undoRedoStack()->endMacro();
    }

    delete parser;
    delete out;

    setCurrentFile(fileName);
    scene_->undoRedoStack()->setClean();
    emit fileLoaded(fileName);

//     QApplication::restoreOverrideCursor();
    setCursor(cur);
  }

  return true;
}


void SchematicEditor::solve()
{
  if(solved_) {
    showResult();
    return;
  }

  view_->setInteractive(false);
  scene_->assignNodes();

//   QApplication::setOverrideCursor(Qt::WaitCursor);
  setCursor(Qt::WaitCursor);
  QLogger::info(QObject::tr("Generating circuit..."));

  SchematicSceneParser* parser = new SchematicSceneParser(*scene_);
  solver_.apply(*parser);
  delete parser;

  QLogger::info(QObject::tr("Solving..."));
  solver_.start();
}


void SchematicEditor::cleanChanged(bool clean)
{
  setWindowModified(!clean);
}


void SchematicEditor::reset()
{
  scene_->clearSchematicScene();
}


void SchematicEditor::fileNameChanged(const QString& fileName)
{
  setWindowTitle(fileName + "[*]");
}


void SchematicEditor::finished()
{
//   QApplication::restoreOverrideCursor();
  setCursor(Qt::ArrowCursor);
  view_->setInteractive(true);
  solved_ = true;
  showResult();

  emit solved();
}


void SchematicEditor::stateChanged(
  Qt::WindowStates oldState, Qt::WindowStates newState)
{
  scene_->undoRedoStack()->setActive(
    newState.testFlag(Qt::WindowActive) && !isRunning());
}


void SchematicEditor::closeEvent(QCloseEvent *event)
{
  if(maybeSave()) {
    if(solver_.isRunning())
      solver_.wait();
    QMdiSubWindow::closeEvent(event);
    event->accept();
  } else {
    event->ignore();
  }
}


void SchematicEditor::showResult()
{
  std::string num, den, expr;

  QPlainTextEdit* result = new QPlainTextEdit;
  result->setReadOnly(true);
  result->setUndoRedoEnabled(false);
  result->setLineWrapMode(QPlainTextEdit::NoWrap);
  result->setWordWrapMode(QTextOption::NoWrap);
  result->setTextInteractionFlags(
        Qt::TextSelectableByMouse
      | Qt::TextSelectableByKeyboard
    );

  result->appendPlainText("# " + currentFile() + "\n");

  expr.clear();
  num = sapecng::metacircuit::as_string(solver_.mixed_numerator());
  den = sapecng::metacircuit::as_string(solver_.mixed_denominator());
  expr.append(num);
  expr.append("\n");
  expr.append(std::string(
    (num.size() > den.size() ? num.size() : den.size()) * 3/2, '-'));
  expr.append("\n");
  expr.append(den);
  result->appendPlainText(tr("Result:\n"));
  result->appendPlainText(QString::fromStdString(expr));

  expr.clear();
  num = sapecng::metacircuit::as_string(solver_.raw_numerator());
  den = sapecng::metacircuit::as_string(solver_.raw_denominator());
  expr.append(num);
  expr.append("\n");
  expr.append(std::string(
    (num.size() > den.size() ? num.size() : den.size()) * 3/2, '-'));
  expr.append("\n");
  expr.append(den);
  result->appendPlainText(tr("\n\nRaw:\n"));
  result->appendPlainText(QString::fromStdString(expr));

  expr.clear();
  num = sapecng::metacircuit::as_string(solver_.digit_numerator());
  den = sapecng::metacircuit::as_string(solver_.digit_denominator());
  expr.append(num);
  expr.append("\n");
  expr.append(std::string(
    (num.size() > den.size() ? num.size() : den.size()) * 3/2, '-'));
  expr.append("\n");
  expr.append(den);
  result->appendPlainText(tr("\n\nDigit:\n"));
  result->appendPlainText(QString::fromStdString(expr));

  QDialogButtonBox* button = new QDialogButtonBox(QDialogButtonBox::Ok);

  QLayout* layout = new QVBoxLayout;
  layout->addWidget(result);
  layout->addWidget(button);

  QDialog dialog(this);
  dialog.setLayout(layout);
  connect(button, SIGNAL(accepted()), &dialog, SLOT(accept()));
  dialog.exec();
}


void SchematicEditor::setCurrentFile(const QString& fileName)
{
  curFile_ = QFileInfo(fileName).canonicalFilePath();
  isUntitled_ = false;
  setWindowModified(false);
  setWindowTitle(userFriendlyCurrentFile() + "[*]");
}


QString SchematicEditor::strippedName(const QString& fullFileName) const
{
  return QFileInfo(fullFileName).fileName();
}


bool SchematicEditor::maybeSave()
{
  if(isWindowModified()) {
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, tr("Save file"),
      tr("'%1'").arg(userFriendlyCurrentFile()
        + QString(" has been modified.\n Save the file?")),
      QMessageBox::Save | QMessageBox::Discard
        | QMessageBox::Cancel
      );
    if(ret == QMessageBox::Save)
      return save();
    else if(ret == QMessageBox::Cancel)
      return false;
  }

  return true;
}


}
