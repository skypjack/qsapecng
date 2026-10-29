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


#ifndef SCHEMATICSCENE_H
#define SCHEMATICSCENE_H


#include <QGraphicsScene>

#include <QList>
#include <QRect>
#include <QHash>
#include <QMap>
#include <QPointF>
#include <QVariant>
#include <QMetaType>
#include <QPointer>

#include "gui/qtsolutions/qtpropertyeditor/qtpropertybrowser.h"

#include <memory>


class QMenu;
class QEvent;
class QPainter;
class QKeyEvent;
class QUndoStack;
class QUndoCommand;
class QGraphicsItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneDragDropEvent;
class QGraphicsSceneContextMenuEvent;

class QtBoolPropertyManager;
class QtGroupPropertyManager;
class QtStringPropertyManager;


class QtProperty;


class QGraphicsLineItem;

namespace qsapecng
{


class Component;
class Item;
class Wire;
class Label;


class SchematicScene: public QGraphicsScene
{

  Q_OBJECT

public:
  enum { Ground = 0 };
  enum { GridStep = 10 };
  enum { OutputNameKey = 3 };   // item data: the name of an output
  enum { OutputLoadKey = 5 };   // item data: an Out marker's load (ohm, 0: none)

  enum SupportedItemType {
    NilItemType = 0,
    
    UserDefItemType,

    CapacitorItemType,
    CCCSItemType,
    CCVSItemType,
    ConductanceItemType,
    CurrentSourceItemType,
    InductorItemType,
    OpAmplItemType,
    ResistorItemType,
    VCCSItemType,
    VCVSItemType,
    VoltageSourceItemType,
    TransformerItemType,
    MutualInductanceItemType,

    VoltmeterItemType,
    AmmeterItemType,

    GroundItemType,
    PortItemType,
    OutItemType,
    WireItemType,

    LabelItemType,

    // new types go last: the numbers are saved in files
    SingleVoltageSourceItemType,
    NullorItemType,
    MultiTransformerItemType,
  };

  static QPainterPath nilPath();

  static QPainterPath groundPath();
  static QPainterPath portPath();
  static QRectF outPivotRect();
  static QPainterPath outPath(const QString& text = QString());
  static QPainterPath singleVoltageSourcePath();
  static QPainterPath nullorPath();
  static QPainterPath multiTransformerPath();
  static QPainterPath voltmeterPath();
  static QPainterPath ammeterPath();

  static QPainterPath capacitorPath();
  static QPainterPath cccsPath();
  static QPainterPath ccvsPath();
  static QPainterPath conductancePath();
  static QPainterPath currentSourcePath();
  static QPainterPath inductorPath();
  static QPainterPath opAmplPath();
  static QPainterPath resistorPath();
  static QPainterPath vccsPath();
  static QPainterPath vcvsPath();
  static QPainterPath voltageSourcePath();
  static QPainterPath transformerPath();
  static QPainterPath mutualInductancePath();

  static QtProperty* itemProperties(QGraphicsItem* item);
  static SchematicScene::SupportedItemType itemType(QGraphicsItem* item);
  static Item* itemByType(SupportedItemType type);
  static QString itemNameByType(SchematicScene::SupportedItemType type);
  static QChar itemIdByType(SchematicScene::SupportedItemType type);
  // unit of the value of a component type ("F", "Ω"...; empty: none)
  static QString valueUnit(SchematicScene::SupportedItemType type);

  // parasitics of a "real" resistor, inductor or capacitor: the property
  // (also the dialog label), the key in files and for the solver, unit,
  // default value and a short explanation
  struct Parasitic
  {
    QString property;
    std::string key;
    QString unit;
    QString defaultValue;
    QString description;
  };
  static QList<Parasitic> parasitics(SchematicScene::SupportedItemType type);

  static QPainterPath userDefPath(uint ports);
  static QList<QPointF> userDefNodes(uint ports);

public:
  SchematicScene(QObject* parent = 0);
  ~SchematicScene();

  QPointF placementPos(const QPointF& pointer) const;
  QPointF closestGridPoint(const QPointF& pos) const;
  inline bool gridVisible() const { return gridVisible_; }
  inline bool isGridPoint(QPointF point) const
    { return (point == closestGridPoint(point)); }

  // properties of the whole schematic, kept in its file (e.g. the sweep a
  // plot of it starts with: plot.fstart, plot.fstop, plot.points,
  // plot.kind, plot.z0)
  inline QMap<QString, QString> circuitProperties() const
    { return circuitProps_; }
  inline void setCircuitProperty(const QString& name, const QString& value)
    { circuitProps_.insert(name, value); }
  void setActiveItem(SupportedItemType item);
  inline SupportedItemType activeItem() const { return activeItem_; }
  QList<Item*> activeItems() const;

  QByteArray registerUserDef(const SchematicScene& scene);
  std::string queryUserDef(QByteArray md5);
  void setUserDefRequest();

  void addItems(QList<QGraphicsItem*> items);
  void addItems(QGraphicsItem* item);
  void addSupportedItem(QGraphicsItem* gItem, bool init = true);
  void removeSupportedItem(QGraphicsItem* gItem);
  void clearSupportedItem(QGraphicsItem* gItem);

  void setWireSessionRequest(bool connectedWire);
  void joinWires(Wire* w1, Wire* w2);
  void modifyWire(Wire* wire);

  void createLabel();
  // asks for a label's text (several lines); undoable. false: cancelled
  bool editLabelText(Label* label, bool fresh = false);
  // the selected voltmeters / ammeters the other way round; undoable
  bool reverseSelectedMeters();

  void moveSelectedItems(QPointF pos);
  // a mouse drag of the selection: wires attached to the moved pins
  // stretch along (preview), committed as one undo step on release
  void dragSelectedBy(const QPointF& offset);
  void cutSelectedItems();
  void copySelectedItems();
  void pasteItems();
  void rotateSelectedItems();
  // R key / Rotate action: rotates the item being placed if there is one,
  // otherwise the selected items
  void rotatePlacedOrSelected();
  // flip left-right, or (vertical) upside down
  void mirrorPlacedOrSelected(bool vertical);
  // redraw resistors/conductances after the resistor symbol style changed
  void updateResistorSymbols();
  // rebuild every label (e.g. after the value display setting changed)
  void refreshComponentLabels();
  // names of the independent voltage and current sources
  QStringList sourceNames() const;
  // what the active tool expects next (empty when no tool is active)
  QString toolHint() const;
  int partCount() const;
  int wireCount() const;
  // open the parameters dialog of an item (double-click); undoable
  void editItemProperties(QGraphicsItem* item);
  inline SupportedItemType activeItemType() const { return activeItem_; }
  // wanted, unless another part already has that name: then the next free
  // one (pasted parts must not repeat names)
  QString uniqueName(SupportedItemType type, const QString& wanted, Item* self);

  // outputs (Out markers, voltmeters, ammeters): each is solved on its own
  // and has a name - Out1, VM1, AM1 ... - shown on the sheet and in the
  // Result panel
  QStringList outputNames() const;
  static QString outputName(const Component* output);
  static double outputLoad(const Component* output);
  // the Out tag shows the name, and its load when it has one
  static void refreshOutputTag(Component* output);
  // an Out marker, a voltmeter or an ammeter
  static bool isOutput(const QGraphicsItem* item);
  // asks for a new name (double-click, Properties); undoable
  void renameOutput(Component* output);
  void setOutputName(Component* output, const QString& name);
  QString uniqueOutputName(SupportedItemType type, const QString& wanted,
    const Component* self) const;
  inline bool hasActiveItemType() const { return activeItem_ != NilItemType; }
  inline bool isPlacingItem() const
    { return hasActiveItem() || hasUserDefReq(); }
  void mirrorSelectedItems();
  void binSelectedItems();
  void clearSchematicScene();

  inline void setContextMenu(QMenu* menu) { contextMenu_ = menu; }
  inline QMenu* contextMenu() const { return contextMenu_; }

  inline QUndoStack* undoRedoStack() const { return undoRedoStack_; }

  inline QtProperty* properties() const { return properties_; }

  QVector<int> ports();
  inline int size() { return portList_.size(); }
  void assignNodes(int seed = SchematicScene::Ground + 1);

public slots:
  // recompute wire taps (T-junction connections) for all wires
  void updateWireTaps();
  void resetNodes();
  void setGridVisible(bool visible = true);
  void resetStatus();

signals:
  void showUserDef(SchematicScene& scene);
  void propertyChanged();
  // placing a new item started or ended (see isPlacingItem())
  void placementChanged();
  // a short notice for the status bar (e.g. why a part was not placed)
  void notice(const QString& text);

protected:
  void drawBackground(QPainter* painter, const QRectF& rect);
  void mousePressEvent(QGraphicsSceneMouseEvent* event);
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event);
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event);
  void contextMenuEvent(QGraphicsSceneContextMenuEvent* event);
  void keyPressEvent(QKeyEvent* event);
  bool event(QEvent* event);

  void dragEnterEvent(QGraphicsSceneDragDropEvent* event);
  void dragMoveEvent(QGraphicsSceneDragDropEvent* event);
  void dragLeaveEvent(QGraphicsSceneDragDropEvent* event);
  void dropEvent(QGraphicsSceneDragDropEvent* event);

private:
  void init();
  void setupProperties();

  inline bool hasActiveItem() const { return (activeItem_ != NilItemType); }
  inline bool hasUserDefReq() const { return userDefSessionRequested_; }
  inline bool hasPendingWire() const { return wireSessionRequested_; }
  void setPrePostWireLine(QPointF toPoint);
  void addWires();
  // wire drawing: a pin or a wire at this point ends the chain of segments
  bool isConnectionPoint(const QPointF& point) const;
  // ends the chain being drawn; the wire tool stays active
  void finishWireChain();
  void flipWireBend();
  // the segments of a bent wire: all wires joined end to end through plain
  // bends (two branches), up to junctions and pins
  QList<Wire*> wireRun(Wire* wire) const;
  void selectWireRunAt(QGraphicsSceneMouseEvent* event);
  void endWireChainMacro();

private:
  bool gridVisible_;
  QMenu* contextMenu_;
  QPointF lastMousePressPos_;
  QPointF lastMousePos_;
  bool validNode_;
  
  SupportedItemType activeItem_;
  Item* item_;

  bool wireSessionRequested_;
  bool horizontalFirst_;
  bool chainMacroOpen_ = false;   // one undo step per drawn wire
  // pasted items follow the pointer until a click places them (one undo
  // step with the paste); Esc takes the paste back
  bool floatingPaste_ = false;
  // a click on a pin or a wire end (press and release without moving)
  // starts a wire there
  bool pinPressed_ = false;
  // a wire started by clicking a pin: the tool ends with it
  bool oneOffWire_ = false;
  // the aiming cursor shown over a pin or a wire end (a click there
  // starts a wire)
  bool hoverAim_ = false;
  QPointF pinPoint_, pinPressScreen_;
  bool connectionAt(const QPointF& point, QPointF* exact) const;
  // true when a part of `items`, moved by `offset`, would lie on another
  // part (touching at the pins is fine); a notice says so
  bool overlapsParts(const QList<QGraphicsItem*>& items,
    const QPointF& offset = QPointF());
  QList<QGraphicsItem*> floating_;
  QPointF floatAnchor_, floatTotal_;
  void finishFloatingPaste(bool place);
  bool bendDecided_ = false;   // set by the first move or by flipping
  bool hasValidFromPoint_;
  bool connectedWire_;
  QPointF fromPoint_;
  QGraphicsLineItem* preWireItem_;
  QGraphicsLineItem* postWireItem_;
  Wire* wireInProgress_;
  QPen wirePen_;

  QList<Item*> outputList_;   // Out markers, voltmeters, ammeters
  QList<Item*> portList_;
  QList<Item*> groundList_;
  QList<Item*> standardList_;
  QList<Item*> wireList_;

  // moving parts keeps their wires attached (see dragSelectedBy)
  struct RubberWire
  {
    Wire* wire;        // null: a new wire from a pin that stays
    QPointF fixed;     // the end that stays
    QPointF moving;    // the end on a moved pin (before the move)
    Qt::Orientation orientation;
    QGraphicsLineItem* preview[2];
  };
  struct DragSession
  {
    bool active = false;
    QList<QGraphicsItem*> items;
    QList<QPointF> origin;
    QList<RubberWire> rubber;
    QPointF total;
  } drag_;
  void numberNetLabels();
  void ensurePreview();
  // after a move: merges wires lying on top of each other on one line and
  // removes the given wires (and merged ones) left with an end that
  // connects nothing; part of the open undo macro
  void tidyWires(QList<Wire*> fresh);
  // splits wires at T-junctions made by the given (new) wires
  void splitAtJunctions(QList<Wire*> fresh);
  void beginDrag();
  void updateDragPreview();
  void endDrag(bool commit);
  static QList<QLineF> rubberSegments(const QPointF& fixed,
      const QPointF& moving, Qt::Orientation orientation);
  QList<Item*> labelList_;
  QList<Item*> userDefList_;
  
  int userDefSize_;
  QByteArray userDefMD5_;
  QHash<QByteArray, std::string> userDefMap_;
  bool userDefSessionRequested_;

  QUndoStack* undoRedoStack_;

  QtBoolPropertyManager* boolManager_;
  QtGroupPropertyManager* groupManager_;
  QtStringPropertyManager* stringManager_;

  QtProperty* properties_;
  bool wireTapsPending_ = false;
  QString nextFreeName(SupportedItemType type);
  QHash<SupportedItemType, unsigned int> itemCntMap_;
  QMap<QString, QString> circuitProps_;
  QHash<SupportedItemType, QtProperty*> typeRootMap_;

};


}


Q_DECLARE_METATYPE(QPointer<qsapecng::SchematicScene>);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(QtProperty*);
#endif


#endif // SCHEMATICSCENE_H
