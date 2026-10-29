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


#ifndef THEME_H
#define THEME_H


#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QPixmap>
#include <QSize>
#include <QString>


class QApplication;
class QGraphicsScene;


namespace qsapecng
{
namespace theme
{


// Pick a modern widget style (Fusion unless the user or platform already
// selected a modern one) and apply small global polish. Call once, right
// after constructing QApplication.
void applyAppearance(QApplication& app);

// Colour scheme chosen by the user (Settings > Appearance), persisted in
// QSettings. System follows the platform; Light/Dark force Fusion with a
// matching palette (useful on desktops/Qt versions that do not report a
// dark preference).
enum class Mode { System, Light, Dark };
Mode mode();
void setMode(Mode mode);

// Resistor (and conductance) symbol: ANSI zigzag or IEC 60617 / DIN
// rectangle; persisted in QSettings, IEC / DIN by default.
enum class ResistorStyle { Ansi, Din };
ResistorStyle resistorStyle();
void setResistorStyle(ResistorStyle style);
// Name of the resistor symbol for symbolIcon()/symbolPixmap().
QString resistorSymbolName();

// Component labels: name only (default) or name and value.
bool showComponentValues();
void setShowComponentValues(bool show);

// True when the current application palette is a dark one.
bool isDark();

// Line icon from :/icons/<name>.svg, tinted with the palette text colour
// at paint time (so it follows light/dark and the disabled state) and
// rendered as vector at any size / device pixel ratio.
QIcon icon(const QString& name);

// Circuit symbol from :/images/symbols/<name>.svg as an icon, tinted like
// icon() and drawn with its aspect ratio preserved.
QIcon symbolIcon(const QString& name);

// Circuit symbol rendered to a pixmap of the given logical size (HiDPI
// aware), for places that need a QPixmap (list decorations, drag images).
QPixmap symbolPixmap(const QString& name, const QSize& size);

// Sans-serif font for text drawn on the schematic (labels, values).
QFont schematicFont(int pointSize);

// Colours of the schematic sheet: dark in a dark theme, black on white
// otherwise and always while a PaperSheet guard exists (printing).
bool sheetIsDark();
QColor sheetBackground();
QColor sheetForeground();
QColor sheetGrid();

// the grid of the schematic: dots or lines, and how strong
enum class GridStyle { Dots, Lines };
GridStyle gridStyle();
void setGridStyle(GridStyle style);
// 0 faint, 1 medium (default), 2 strong
int gridStrength();
void setGridStrength(int strength);

// Colour of a solved net (wires are coloured per net after Resolve).
QColor netColor(int net);

// Re-applies sheetForeground() to the text items (labels, values, node
// numbers) of a scene; call after the theme or a PaperSheet guard changes.
void refreshSheetText(QGraphicsScene* scene);

// While alive, the sheet uses paper colours (black on white) whatever the
// theme; used for printing.
class PaperSheet
{
public:
  PaperSheet();
  ~PaperSheet();
  PaperSheet(const PaperSheet&) = delete;
  PaperSheet& operator=(const PaperSheet&) = delete;
};

// Light palette used for printed output (plots).
QPalette paperPalette();

// Makes a user-configured log colour readable on the current background:
// pure black follows the palette text colour, and very dark colours are
// lightened when the theme is dark.
QColor readableOn(const QColor& color);


}
}


#endif // THEME_H
