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


#include "parser/parser_factory.h"
#include "gui/editor/schematicscene.h"
#include "gui/editor/schematicsceneparser.h"
#include "gui/userdefdialog/userdefdialog.h"
#include "gui/settings.h"

#include <QtGui/QFormLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>

#include <QtGui/QDialogButtonBox>
#include <QtGui/QFileDialog>

#include <QtGui/QPushButton>


namespace qsapecng
{


UserDefDialog::UserDefDialog(QWidget* parent)
  : QDialog(parent)
{
  createLayout();
  setLayout(layout_);
  setWindowTitle(tr("User def component"));
}


void UserDefDialog::chooseFile()
{
  QString fileName = QFileDialog::getOpenFileName(this,
      tr("Read file"), Settings().workspace(),
      QString("%1;;%2")
        .arg(tr("Info files (*.info)"))
        .arg(tr("XML files (*.xml)"))
    );

  if(!fileName.isEmpty()) {
    fileName_->setText(fileName);

    SchematicScene* scene = new SchematicScene;
    SchematicSceneBuilder* out = new SchematicSceneBuilder(*scene);

    QFileInfo fileInfo(fileName);
    std::ifstream in_file(QFile::encodeName(fileName));
    sapecng::abstract_parser* parser =
      sapecng::parser_factory::parser(
	fileInfo.suffix().toStdString(), in_file);

    if(parser)
      parser->parse(*out);

    size_->setMaximum(SchematicScene::size(*scene));
    size_->setValue(size_->maximum());

    delete parser;
    delete out;
  }
}


void UserDefDialog::createLayout()
{
  size_ = new QSpinBox;
  size_->setMinimum(0);
  size_->setMaximum(0);

  fileName_ = new QLineEdit;
  fileName_->setReadOnly(true);
  QPushButton* button = new QPushButton("...");
  connect(button, SIGNAL(clicked(bool)), this, SLOT(chooseFile()));

  QHBoxLayout* fileLayout = new QHBoxLayout;
  fileLayout->addWidget(fileName_);
  fileLayout->addWidget(button);

  QFormLayout* formLayout_ = new QFormLayout;
  formLayout_->addRow(tr("Size:"), size_);
  formLayout_->addRow(tr("Circuit:"), fileLayout);

  QDialogButtonBox* buttonBox =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
  connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

  layout_ = new QVBoxLayout;
  layout_->addLayout(formLayout_);
  layout_->addWidget(buttonBox);
}


}
