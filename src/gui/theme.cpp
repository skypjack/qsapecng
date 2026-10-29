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
#include "config.h"

#include <QApplication>
#include <QFile>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGuiApplication>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QGuiApplication>
#include <QSvgRenderer>
#include <QtGlobal>

#include <memory>


namespace qsapecng
{
namespace theme
{


namespace
{


QByteArray loadSvg(const QString& path)
{
  static QHash<QString, QByteArray> cache;
  auto it = cache.constFind(path);
  if(it != cache.constEnd())
    return *it;

  QFile file(path);
  QByteArray data;
  if(file.open(QIODevice::ReadOnly))
    data = file.readAll();
  cache.insert(path, data);
  return data;
}


QColor foreground(QIcon::Mode mode)
{
  const QPalette pal = QApplication::palette();
  switch(mode) {
  case QIcon::Disabled:
    return pal.color(QPalette::Disabled, QPalette::WindowText);
  case QIcon::Selected:
    return pal.color(QPalette::Active, QPalette::HighlightedText);
  default:
    return pal.color(QPalette::Active, QPalette::WindowText);
  }
}


// Renders an SVG whose drawing colour is either "currentColor" (line icons)
// or plain black (the old Inkscape circuit symbols), replacing it with the
// palette colour for the requested mode at paint time.
class TintedSvgIconEngine: public QIconEngine
{

public:
  TintedSvgIconEngine(const QString& path, bool keepAspect, qreal strokeWidth)
    : path_(path), keepAspect_(keepAspect), strokeWidth_(strokeWidth) { }

  QIconEngine* clone() const override
    { return new TintedSvgIconEngine(*this); }

  QString key() const override
    { return QStringLiteral("qsapecng-tinted-svg"); }

  bool isNull() override
    { return loadSvg(path_).isEmpty(); }

  void paint(QPainter* painter, const QRect& rect,
      QIcon::Mode mode, QIcon::State) override
  {
    QByteArray svg = loadSvg(path_);
    if(svg.isEmpty())
      return;

    const QByteArray fg = foreground(mode).name().toLatin1();
    svg.replace("currentColor", fg);
    svg.replace("#000000", fg);
    if(strokeWidth_ > 0)
      svg.replace("stroke-width=\"2\"",
        "stroke-width=\"" + QByteArray::number(strokeWidth_) + "\"");
    // light fills in the old symbols become translucent so they read on
    // dark backgrounds as well
    svg.replace("fill:#e6e6e6", "fill:" + fg + ";fill-opacity:0.15");

    QSvgRenderer renderer(svg);
    QRectF target = rect;
    if(keepAspect_) {
      QSizeF s = renderer.defaultSize();
      s.scale(rect.size(), Qt::KeepAspectRatio);
      target = QRectF(QPointF(0, 0), s);
      target.moveCenter(QRectF(rect).center());
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    renderer.render(painter, target);
    painter->restore();
  }

  QPixmap pixmap(const QSize& size, QIcon::Mode mode,
      QIcon::State state) override
    { return scaledPixmap(size, mode, state, 1.0); }

  QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode,
      QIcon::State state, qreal scale) override
  {
    QPixmap pm(size * scale);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(scale);
    QPainter p(&pm);
    paint(&p, QRect(QPoint(0, 0), size), mode, state);
    return pm;
  }

private:
  QString path_;
  bool keepAspect_;
  qreal strokeWidth_;

};


QPalette darkPalette()
{
  const QColor window(0x2b, 0x2d, 0x31);
  const QColor base(0x1f, 0x21, 0x24);
  const QColor alt(0x26, 0x28, 0x2c);
  const QColor text(0xe6, 0xe8, 0xeb);
  const QColor dim(0x8a, 0x8f, 0x98);
  const QColor button(0x34, 0x37, 0x3c);
  const QColor accent(0x4c, 0x9a, 0xff);
  // selected rows and text: a deeper blue than the accent (links, focus)
  const QColor selection(0x2a, 0x55, 0x8f);

  QPalette pal;
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, base);
  pal.setColor(QPalette::AlternateBase, alt);
  pal.setColor(QPalette::ToolTipBase, button);
  pal.setColor(QPalette::ToolTipText, text);
  pal.setColor(QPalette::PlaceholderText, dim);
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, button);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::BrightText, Qt::white);
  pal.setColor(QPalette::Light, button.lighter(130));
  pal.setColor(QPalette::Midlight, button.lighter(115));
  pal.setColor(QPalette::Mid, button.darker(120));
  pal.setColor(QPalette::Dark, window.darker(140));
  pal.setColor(QPalette::Shadow, Qt::black);
  pal.setColor(QPalette::Highlight, selection);
  pal.setColor(QPalette::HighlightedText, Qt::white);
  pal.setColor(QPalette::Link, accent);
  pal.setColor(QPalette::LinkVisited, accent.lighter(120));

  for(QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text,
        QPalette::ButtonText, QPalette::Light })
    pal.setColor(QPalette::Disabled, role, QColor(0x6b, 0x70, 0x78));
  pal.setColor(QPalette::Disabled, QPalette::Highlight, button);
  return pal;
}


QPalette lightPalette()
{
  // every colour given: Fusion's own palette (Qt 6.5 and newer) follows the
  // system's colour scheme, so on a dark desktop it was dark as well
  const QColor window(0xef, 0xef, 0xef);
  const QColor base(Qt::white);
  const QColor text(Qt::black);
  const QColor button(0xef, 0xef, 0xef);
  const QColor accent(0x25, 0x63, 0xeb);

  QPalette pal;
  pal.setColor(QPalette::Window, window);
  pal.setColor(QPalette::WindowText, text);
  pal.setColor(QPalette::Base, base);
  pal.setColor(QPalette::AlternateBase, QColor(0xf7, 0xf7, 0xf7));
  pal.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xdc));
  pal.setColor(QPalette::ToolTipText, text);
  pal.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
  pal.setColor(QPalette::Text, text);
  pal.setColor(QPalette::Button, button);
  pal.setColor(QPalette::ButtonText, text);
  pal.setColor(QPalette::BrightText, Qt::white);
  pal.setColor(QPalette::Light, Qt::white);
  pal.setColor(QPalette::Midlight, QColor(0xca, 0xca, 0xca));
  pal.setColor(QPalette::Mid, QColor(0xb8, 0xb8, 0xb8));
  pal.setColor(QPalette::Dark, QColor(0x9f, 0x9f, 0x9f));
  pal.setColor(QPalette::Shadow, QColor(0x76, 0x76, 0x76));
  pal.setColor(QPalette::Highlight, accent);
  pal.setColor(QPalette::HighlightedText, Qt::white);
  pal.setColor(QPalette::Link, accent);
  pal.setColor(QPalette::LinkVisited, accent.darker(120));

  for(QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text,
        QPalette::ButtonText })
    pal.setColor(QPalette::Disabled, role, QColor(0xbe, 0xbe, 0xbe));
  pal.setColor(QPalette::Disabled, QPalette::Base, window);
  pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x91, 0x91, 0x91));
  return pal;
}


QString platformStyleName;


}


QPalette paperPalette()
{
  return lightPalette();
}


namespace
{
int resistorStyleCache = -1;   // -1: not read from the settings yet
}


ResistorStyle resistorStyle()
{
  if(resistorStyleCache < 0) {
    QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
    resistorStyleCache = settings.value(
        QStringLiteral("Application/resistorSymbol")).toString()
      == QLatin1String("ansi") ? 0 : 1;   // IEC / DIN unless ANSI chosen
  }
  return resistorStyleCache == 1 ? ResistorStyle::Din : ResistorStyle::Ansi;
}


void setResistorStyle(ResistorStyle style)
{
  resistorStyleCache = (style == ResistorStyle::Din) ? 1 : 0;
  QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
  settings.setValue(QStringLiteral("Application/resistorSymbol"),
    style == ResistorStyle::Din ? QStringLiteral("din") : QStringLiteral("ansi"));
}


QString resistorSymbolName()
{
  return resistorStyle() == ResistorStyle::Din
    ? QStringLiteral("resistor-din") : QStringLiteral("resistor");
}


namespace
{
int showValuesCache = -1;   // -1: not read from the settings yet
}


bool showComponentValues()
{
  if(showValuesCache < 0) {
    QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
    showValuesCache = settings.value(
        QStringLiteral("Application/showValues"), false).toBool() ? 1 : 0;
  }
  return showValuesCache == 1;
}


void setShowComponentValues(bool show)
{
  showValuesCache = show ? 1 : 0;
  QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
  settings.setValue(QStringLiteral("Application/showValues"), show);
}


Mode mode()
{
  QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
  const QString v = settings.value(QStringLiteral("Application/theme")).toString();
  if(v == QLatin1String("light"))
    return Mode::Light;
  if(v == QLatin1String("dark"))
    return Mode::Dark;
  return Mode::System;
}


void setMode(Mode m)
{
  {
    QSettings settings(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
    settings.setValue(QStringLiteral("Application/theme"),
      m == Mode::Light ? QStringLiteral("light")
      : m == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("system"));
  }

  QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
  if(!app)
    return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  // what the platform draws (window frames, native dialogs) follows too
  QGuiApplication::styleHints()->setColorScheme(
    m == Mode::Light ? Qt::ColorScheme::Light
    : m == Mode::Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Unknown);
#endif

  if(m == Mode::System) {
    if(QStyle* style = QStyleFactory::create(platformStyleName))
      app->setStyle(style);
    QApplication::setPalette(app->style()->standardPalette());
  } else {
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(m == Mode::Dark ? darkPalette() : lightPalette());
  }
}


void applyAppearance(QApplication& app)
{
  // Respect an explicit choice (-style argument, QT_STYLE_OVERRIDE, qt6ct
  // ...). Only replace the legacy Windows styles, which look dated and do
  // not support dark mode; windows11 (Qt >= 6.7), macOS and Fusion stay.
  const QString current = app.style()->name().toLower();
  const bool overridden = !qEnvironmentVariableIsEmpty("QT_STYLE_OVERRIDE")
    || app.arguments().contains(QStringLiteral("-style"));
  if(!overridden && (current == QLatin1String("windowsvista")
      || current == QLatin1String("windows")))
  {
    if(QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion")))
      app.setStyle(fusion);
  }
  platformStyleName = app.style()->name();

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  // no contextual alternates: some fonts (Inter) draw the x of "4x200"
  // as a multiplication sign
  QFont font = app.font();
  font.setFeature(QFont::Tag("calt"), 0);
  app.setFont(font);
#endif

  if(mode() != Mode::System)
    setMode(mode());
}


bool isDark()
{
  const QPalette pal = QApplication::palette();
  return pal.color(QPalette::Window).lightness()
    < pal.color(QPalette::WindowText).lightness();
}


QIcon icon(const QString& name)
{
  return QIcon(new TintedSvgIconEngine(
    QStringLiteral(":/icons/%1.svg").arg(name), false, 1.75));
}


QIcon symbolIcon(const QString& name)
{
  return QIcon(new TintedSvgIconEngine(
    QStringLiteral(":/images/symbols/%1.svg").arg(name), true, 0));
}


QPixmap symbolPixmap(const QString& name, const QSize& size)
{
  qreal dpr = 1.0;
  if(QScreen* screen = QGuiApplication::primaryScreen())
    dpr = screen->devicePixelRatio();
  return symbolIcon(name).pixmap(size, dpr);
}


QFont schematicFont(int pointSize)
{
  QFont font = QApplication::font();
  font.setStyleHint(QFont::SansSerif);
  font.setPointSize(pointSize);
  font.setWeight(QFont::Normal);
  return font;
}


namespace
{
int paperSheetDepth = 0;   // > 0 while a PaperSheet guard is alive
}


bool sheetIsDark()
{
  return paperSheetDepth == 0 && isDark();
}


PaperSheet::PaperSheet() { ++paperSheetDepth; }
PaperSheet::~PaperSheet() { --paperSheetDepth; }


QColor sheetBackground()
{
  return sheetIsDark() ? QColor(0x1e, 0x20, 0x24) : QColor(0xff, 0xff, 0xff);
}


QColor sheetForeground()
{
  return sheetIsDark() ? QColor(0xe2, 0xe5, 0xe9) : QColor(Qt::black);
}


QColor netColor(int net)
{
  // the reference (ground) net: dark grey on paper, light grey on the
  // dark sheet; the others take 14 distinct hues, darker on paper
  if(net == 0)
    return sheetIsDark() ? QColor(0x9a, 0x9f, 0xa8) : QColor(0x50, 0x50, 0x50);
  static const QColor paper[] = {
    QColor(0x00, 0x7f, 0x8c), QColor(0xb0, 0x1e, 0x2d), QColor(0x8e, 0x2b, 0xa8),
    QColor(0x1f, 0x7a, 0x2e), QColor(0xa6, 0x6a, 0x00), QColor(0x1f, 0x4f, 0xc4),
    QColor(0xc2, 0x18, 0x7a), QColor(0x5a, 0x6b, 0x00), QColor(0x00, 0x60, 0xa0),
    QColor(0xc4, 0x4d, 0x00), QColor(0x5e, 0x35, 0xb1), QColor(0x00, 0x86, 0x5c),
    QColor(0x8a, 0x4b, 0x2b), QColor(0x6d, 0x1b, 0x5a)
  };
  static const QColor dark[] = {
    QColor(0x4f, 0xc8, 0xcc), QColor(0xff, 0x6b, 0x6b), QColor(0xd6, 0x7b, 0xe0),
    QColor(0x6c, 0xc6, 0x70), QColor(0xe6, 0xc4, 0x4f), QColor(0x6a, 0xa8, 0xff),
    QColor(0xff, 0x7e, 0xc7), QColor(0xb8, 0xd0, 0x4a), QColor(0x4f, 0xb4, 0xf0),
    QColor(0xff, 0x9e, 0x4a), QColor(0xa9, 0x8a, 0xf5), QColor(0x3f, 0xd6, 0xa0),
    QColor(0xd9, 0xa0, 0x7c), QColor(0xe0, 0x8a, 0xc4)
  };
  const int n = int(sizeof(paper) / sizeof(paper[0]));
  const int i = (((net - 1) % n) + n) % n;
  return sheetIsDark() ? dark[i] : paper[i];
}


void refreshSheetText(QGraphicsScene* scene)
{
  if(!scene)
    return;
  const QColor fg = sheetForeground();
  foreach(QGraphicsItem* item, scene->items()) {
    if(QGraphicsTextItem* text = qgraphicsitem_cast<QGraphicsTextItem*>(item))
      if(text->defaultTextColor() != fg)
        text->setDefaultTextColor(fg);
  }
  scene->update();
}


QColor readableOn(const QColor& color)
{
  if(!color.isValid() || color == QColor(Qt::black))
    return QApplication::palette().color(QPalette::Text);
  if(isDark() && color.lightnessF() < 0.55) {
    QColor c = color.toHsl();
    c.setHslF(c.hslHueF(), c.hslSaturationF(), 0.68);
    return c;
  }
  return color;
}


QColor sheetGrid()
{
  // between the sheet and its ink, by the chosen strength. Dots get more
  // ink than lines: a one-pixel dot is far less visible than a line
  static const qreal lineMix[] = { 0.18, 0.32, 0.55 };
  static const qreal dotMix[] = { 0.18, 0.50, 0.72 };
  const int s = qBound(0, gridStrength(), 2);
  const qreal k = gridStyle() == GridStyle::Dots ? dotMix[s] : lineMix[s];
  const QColor a = sheetBackground(), b = sheetForeground();
  return QColor::fromRgbF(
    a.redF() + (b.redF() - a.redF()) * k,
    a.greenF() + (b.greenF() - a.greenF()) * k,
    a.blueF() + (b.blueF() - a.blueF()) * k);
}


GridStyle gridStyle()
{
  return QSettings().value(QStringLiteral("Schematic/gridStyle")).toString()
      == QLatin1String("lines") ? GridStyle::Lines : GridStyle::Dots;
}


void setGridStyle(GridStyle style)
{
  QSettings().setValue(QStringLiteral("Schematic/gridStyle"),
    style == GridStyle::Lines ? QStringLiteral("lines") : QStringLiteral("dots"));
}


int gridStrength()
{
  return qBound(0, QSettings().value(
    QStringLiteral("Schematic/gridStrength"), 1).toInt(), 2);
}


void setGridStrength(int strength)
{
  QSettings().setValue(QStringLiteral("Schematic/gridStrength"), qBound(0, strength, 2));
}


}
}
