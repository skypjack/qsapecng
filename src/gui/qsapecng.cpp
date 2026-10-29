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


#include "gui/qsapecngwindow.h"
#include "gui/theme.h"
#include "config.h"

#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QElapsedTimer>
#include <QTimer>

#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QScreen>
#include <QSvgRenderer>

#include <memory>


namespace
{


// The splash screen, drawn here (sharp at any scale): the icon, the name,
// what it does and the version, on a light card
QPixmap splashPixmap()
{
  const QSize size(520, 240);
  const QScreen* screen = QGuiApplication::primaryScreen();
  const qreal dpr = screen ? screen->devicePixelRatio() : 1.;
  QPixmap pm(size * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::white);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);

  const QColor accent(0x25, 0x63, 0xeb);
  p.fillRect(QRect(0, 0, size.width(), 6), accent);
  p.setPen(QColor(0xcb, 0xd5, 0xe1));
  p.drawRect(QRectF(0.5, 0.5, size.width() - 1, size.height() - 1));

  QSvgRenderer icon(QStringLiteral(":/images/qsapecng.svg"));
  icon.render(&p, QRectF(36, 46, 120, 120));

  QFont title = QApplication::font();
  title.setPixelSize(46);
  title.setWeight(QFont::Bold);
  p.setFont(title);
  p.setPen(QColor(0x1e, 0x29, 0x3b));
  p.drawText(QRectF(184, 58, 320, 56), Qt::AlignLeft | Qt::AlignVCenter,
    QStringLiteral("QSapecNG"));

  QFont sub = QApplication::font();
  sub.setPixelSize(16);
  p.setFont(sub);
  p.setPen(QColor(0x47, 0x55, 0x69));
  p.drawText(QRectF(186, 114, 320, 48), Qt::AlignLeft | Qt::AlignTop
    | Qt::TextWordWrap, QObject::tr("Symbolic analysis of linear circuits"));

  QFont small = QApplication::font();
  small.setPixelSize(13);
  p.setFont(small);
  p.setPen(QColor(0x64, 0x74, 0x8b));
  // on the line of the splash screen's messages (bottom left)
  p.drawText(QRectF(0, 0, size.width() - 12, size.height() - 5),
    Qt::AlignRight | Qt::AlignBottom,
    QObject::tr("Version %1").arg(QStringLiteral(PACKAGE_VERSION)));
  return pm;
}


}


int main(int argc, char** argv)
{
  QApplication app(argc, argv);
  // explicit names: dialogs without a title use the application name, which
  // would otherwise be the executable name (e.g. "AppRun.wrapped" in an
  // AppImage); the desktop file name lets Wayland compositors find the icon
  QApplication::setApplicationName(QStringLiteral(PACKAGE_NAME));
  QApplication::setApplicationVersion(QStringLiteral(PACKAGE_VERSION));
  QGuiApplication::setDesktopFileName(QStringLiteral("qsapecng"));
  qsapecng::theme::applyAppearance(app);

  // the icon in every size it is drawn for (the small ones are drawn
  // apart, for the pixel grid); dialogs get it too
  QIcon appIcon;
  for(int s : { 16, 22, 32, 48, 64, 128, 256 })
    appIcon.addFile(QStringLiteral(":/images/qsapecng-%1x%1.png").arg(s),
      QSize(s, s));
  QApplication::setWindowIcon(appIcon);
  
  // splash screen (Settings > Show splash screen), kept up for a moment
  // in front of the main window; a click closes it early
  const bool showSplash = QSettings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION)
    .value(QStringLiteral("Application/showSplash"), true).toBool();
  const int splashMs = 2500;

  QElapsedTimer started;
  started.start();
  std::unique_ptr<QSplashScreen> splash;
  if(showSplash) {
    splash.reset(new QSplashScreen(splashPixmap(), Qt::WindowStaysOnTopHint));
    splash->show();
    splash->showMessage(QStringLiteral("  ") + QObject::tr("Loading..."),
      Qt::AlignBottom | Qt::AlignLeft, QColor(0x64, 0x74, 0x8b));
    app.processEvents();
  }

  qsapecng::QSapecNGWindow window;

  // Show the main window before opening files given on the command line:
  // QMdiArea only activates sub-windows properly once it is visible.
  window.show();

  const QStringList args = app.arguments();
  if(args.size() > 1) {
    if(splash) {
      splash->showMessage(QStringLiteral("  ") + QObject::tr("Loading file(s)..."),
        Qt::AlignBottom | Qt::AlignLeft, QColor(0x64, 0x74, 0x8b));
      app.processEvents();
    }

    for(int i = 1; i < args.size(); ++i)
      window << args.at(i);
  }

  if(splash) {
    splash->raise();
    const int left = qMax(0, splashMs - int(started.elapsed()));
    QTimer::singleShot(left, splash.get(), &QSplashScreen::close);
  }

  return app.exec();
}
