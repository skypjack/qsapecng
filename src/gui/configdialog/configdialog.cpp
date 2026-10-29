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
#include "gui/configdialog/configdialog.h"
#include <QPixmap>

#include <QMessageBox>
#include <QPushButton>

#include <QListWidget>
#include <QStackedWidget>
#include <QDialogButtonBox>

#include <QHBoxLayout>
#include <QVBoxLayout>


namespace qsapecng
{


ConfigDialog::ConfigDialog(QWidget* parent)
  : QDialog(parent)
{
  pages_ = new QStackedWidget;
  createPages();

  contents_ = new QListWidget;
  contents_->setViewMode(QListView::IconMode);
  contents_->setIconSize(QSize(64, 64));
  contents_->setMovement(QListView::Static);
  contents_->setMaximumWidth(128);
  contents_->setSpacing(12);
  createIcons();

  contents_->setCurrentRow(0);

  QDialogButtonBox* buttonBox = new QDialogButtonBox;
  QPushButton* okButton = buttonBox->addButton(QDialogButtonBox::Ok);
  QPushButton* applyButton = buttonBox->addButton(QDialogButtonBox::Apply);
  QPushButton* closeButton = buttonBox->addButton(QDialogButtonBox::Close);
  okButton->setDefault(true);

  connect(okButton, &QPushButton::clicked, this, &ConfigDialog::applyAndClose);
  connect(closeButton, &QPushButton::clicked, this, &ConfigDialog::checkBeforeClose);
  connect(applyButton, &QPushButton::clicked, this, &ConfigDialog::apply);

  QHBoxLayout* subLayout = new QHBoxLayout;
  subLayout->addWidget(contents_);
  subLayout->addWidget(pages_, Qt::AlignLeft);

  QVBoxLayout* mainLayout = new QVBoxLayout;
  mainLayout->addLayout(subLayout);
  mainLayout->addStretch(1);
  mainLayout->addSpacing(12);
  mainLayout->addWidget(buttonBox);
  setLayout(mainLayout);

  setWindowTitle(tr("Configuration"));
}


void ConfigDialog::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
  if(!current)
    current = previous;
  pages_->setCurrentIndex(contents_->row(current));
}


QList<ConfigPage*> ConfigDialog::pages() const
{
  return { generalPage_, appearancePage_, fontPage_ };
}


void ConfigDialog::apply()
{
  // every page with pending changes, not only the visible one
  for(ConfigPage* page : pages())
    if(page->isPageModified())
      page->apply();
}


void ConfigDialog::applyAndClose()
{
  apply();
  accept();
}


void ConfigDialog::checkBeforeClose()
{
  bool pending = false;
  for(ConfigPage* page : pages())
    pending = pending || page->isPageModified();

  if(pending) {
    QMessageBox::StandardButton button =
      QMessageBox::question(
        this,
        tr("Pending settings"),
        tr("Some settings were changed but not applied. Apply them?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::Yes
      );

    if(button == QMessageBox::Cancel)
      return;
    if(button == QMessageBox::Yes)
      apply();
  }
  accept();
}


void ConfigDialog::createIcons()
{
  // the same pixmap when selected: in this icon list only the caption is
  // highlighted, and the theme's selected icon (highlighted text colour,
  // white) vanished on the white background
  auto steadyIcon = [](const QString& name) {
      const QIcon themed = theme::icon(name);
      QIcon icon;
      for(int size : { 32, 48, 64, 96 }) {
        const QPixmap pm = themed.pixmap(QSize(size, size), QIcon::Normal);
        icon.addPixmap(pm, QIcon::Normal);
        icon.addPixmap(pm, QIcon::Selected);
      }
      return icon;
    };

  QListWidgetItem* generalButton = new QListWidgetItem(contents_);
  generalButton->setIcon(steadyIcon("configuration"));
  generalButton->setText(tr("General"));
  generalButton->setTextAlignment(Qt::AlignHCenter);
  generalButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  QListWidgetItem* appearanceButton = new QListWidgetItem(contents_);
  appearanceButton->setIcon(steadyIcon("palette"));
  appearanceButton->setText(tr("Appearance"));
  appearanceButton->setTextAlignment(Qt::AlignHCenter);
  appearanceButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  QListWidgetItem* fontButton = new QListWidgetItem(contents_);
  fontButton->setIcon(steadyIcon("font"));
  fontButton->setText(tr("Font"));
  fontButton->setTextAlignment(Qt::AlignHCenter);
  fontButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  connect(contents_, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
    this, SLOT(changePage(QListWidgetItem*, QListWidgetItem*)));
}


void ConfigDialog::createPages()
{
  AppearancePage* appearance = new AppearancePage;
  connect(appearance, &AppearancePage::applied,
    this, &ConfigDialog::appearanceApplied);

  pages_->addWidget(generalPage_ = new GeneralPage);
  pages_->addWidget(appearancePage_ = appearance);
  pages_->addWidget(fontPage_ = new FontPage);
}


}
