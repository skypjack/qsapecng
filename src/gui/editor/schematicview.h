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


#ifndef SCHEMATICVIEW_H
#define SCHEMATICVIEW_H


#include "gui/theme.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QGraphicsView>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>


namespace qsapecng
{


class SchematicScene;


class SchematicView: public QGraphicsView
{

  Q_OBJECT

public:
  SchematicView(QWidget* parent = 0): QGraphicsView(parent)
  {
    setCacheMode(QGraphicsView::CacheBackground);
    // Repaint the whole viewport on changes: antialiased (subpixel) text
    // can draw slightly outside an item's bounding rectangle, and minimal
    // updates then leave trails when items are dragged.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);
//     setDragMode(QGraphicsView::ScrollHandDrag);
//     setAlignment(Qt::AlignCenter);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    setAcceptDrops(true);
    setMouseTracking(true);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setAttribute(Qt::WA_DeleteOnClose);
    adjustSize();
  }

protected:
  // follow light/dark theme changes: recolour text on the sheet and redraw
  // the cached background (sheet colour and grid)
  void changeEvent(QEvent* event) override
  {
    QGraphicsView::changeEvent(event);
    if(event->type() == QEvent::PaletteChange) {
      theme::refreshSheetText(scene());
      resetCachedContent();
      viewport()->update();
    }
  }

signals:
  void zoomChanged(qreal factor);          // 1 = 100 %
  void cursorMoved(const QPointF& scenePos);

public slots:
  void zoomIn()
    { zoomBy(1.2); }

  void zoomOut()
    { zoomBy(1 / 1.2); }

  // 100 %, centred on the drawing
  void normalSize()
  {
    resetTransform();
    updateCanvas();
    if(scene() && !scene()->itemsBoundingRect().isEmpty())
      centerOn(scene()->itemsBoundingRect().center());
    emit zoomChanged(1.);
  }

  // the scrollable area: the drawing plus a screenful around it, so there
  // is always room to pan, place parts and zoom towards an edge. What is on
  // the screen stays in it: the drawing never shifts under the pointer
  // (a part being placed would follow it, grow the area again and jump)
  void updateCanvas()
  {
    if(!scene())
      return;
    QRectF r = scene()->itemsBoundingRect() | QRectF(0, 0, 1, 1);
    const QRectF screen = mapToScene(viewport()->rect()).boundingRect();
    r.adjust(-screen.width(), -screen.height(),
             screen.width(), screen.height());
    if(placed_)
      r |= screen;
    setSceneRect(r);
    if(!placed_)
      centerOn((scene()->itemsBoundingRect() | QRectF(0, 0, 1, 1)).center());
  }

  void fitToView()
  {
    if(!scene())
      return;
    const QRectF drawing =
      scene()->itemsBoundingRect().adjusted(-20, -20, 20, 20);
    const QRectF vp = viewport()->rect();
    if(drawing.isEmpty() || vp.isEmpty())
      return;

    // own scale (fitInView anchors on the old scroll position), then centre
    const qreal s = qBound(qreal(0.1),
      qMin(vp.width() / drawing.width(), vp.height() / drawing.height()),
      qreal(8.));
    const ViewportAnchor anchor = transformationAnchor();
    setTransformationAnchor(QGraphicsView::NoAnchor);
    resetTransform();
    scale(s, s);
    setTransformationAnchor(anchor);
    updateCanvas();
    centerOn(drawing.center());
    emit zoomChanged(s);

    // scroll bars may appear or vanish once the canvas changes, resizing
    // the viewport: centre again when the layout has settled
    QTimer::singleShot(0, this, [this, drawing]() {
      updateCanvas();
      centerOn(drawing.center());
    });
  }


protected:
  // CAD-style box selection: dragging to the right selects what lies
  // entirely inside the box (blue), dragging to the left also what it
  // touches (green)
  // wheel: zoom around the pointer; Shift+wheel / Ctrl+wheel: scroll
  // sideways / up and down (as in KiCad)
  void wheelEvent(QWheelEvent* event) override
  {
    placed_ = true;
    const QPoint delta = event->angleDelta();
    if(event->modifiers() & Qt::ShiftModifier) {
      horizontalScrollBar()->setValue(
        horizontalScrollBar()->value() - (delta.y() ? delta.y() : delta.x()));
    } else if(event->modifiers() & Qt::ControlModifier) {
      verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    } else if(delta.y() != 0) {
      zoomBy(qPow(1.2, delta.y() / 120.));
    }
    event->accept();
  }

  void resizeEvent(QResizeEvent* event) override
  {
    QGraphicsView::resizeEvent(event);
    updateCanvas();

    // until the user scrolls, keep the drawing centred (the canvas
    // extends a screen beyond it in every direction)
    if(!placed_ && scene())
      centerOn((scene()->itemsBoundingRect() | QRectF(0, 0, 1, 1)).center());
  }

  void mousePressEvent(QMouseEvent* event) override
  {
    // middle button: pan
    placed_ = true;
    if(event->button() == Qt::MiddleButton) {
      panFrom_ = event->position().toPoint();
      panning_ = true;
      viewport()->setCursor(Qt::ClosedHandCursor);
      event->accept();
      return;
    }
    boxStart_ = event->position().toPoint();
    boxDirection_ = 0;
    QGraphicsView::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent* event) override
  {
    // the pointer is on the drawing (maybe holding a part to place): no
    // more centring, which would move the drawing under it
    placed_ = true;
    emit cursorMoved(mapToScene(event->position().toPoint()));
    if(panning_) {
      const QPoint p = event->position().toPoint();
      const QPoint d = p - panFrom_;
      panFrom_ = p;
      horizontalScrollBar()->setValue(horizontalScrollBar()->value() - d.x());
      verticalScrollBar()->setValue(verticalScrollBar()->value() - d.y());
      event->accept();
      return;
    }
    if(event->buttons() & Qt::LeftButton) {
      const int dir = event->position().x() >= boxStart_.x() ? 1 : -1;
      if(dir != boxDirection_) {
        boxDirection_ = dir;
        setRubberBandSelectionMode(dir > 0
          ? Qt::ContainsItemShape : Qt::IntersectsItemShape);
        QPalette pal = viewport()->palette();
        pal.setColor(QPalette::Highlight, dir > 0
          ? QColor(0x25, 0x63, 0xeb) : QColor(0x16, 0xa3, 0x4a));
        viewport()->setPalette(pal);
      }
    }
    QGraphicsView::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent* event) override
  {
    if(panning_ && event->button() == Qt::MiddleButton) {
      panning_ = false;
      viewport()->unsetCursor();
      event->accept();
      return;
    }
    QGraphicsView::mouseReleaseEvent(event);
    if(boxDirection_ != 0) {
      boxDirection_ = 0;
      viewport()->setPalette(QPalette());   // follow the theme again
    }
  }

private:
  void zoomBy(qreal factor)
  {
    // 10 % ... 800 %
    const qreal current = transform().m11() ? qAbs(transform().m11())
                                            : qAbs(transform().m12());
    const qreal target = qBound(qreal(0.1), current * factor, qreal(8.));
    factor = target / current;
    if(qFuzzyCompare(factor, qreal(1)))
      return;
    scale(factor, factor);
    updateCanvas();
    emit zoomChanged(target);
  }

  QPoint boxStart_;
  int boxDirection_ = 0;
  QPoint panFrom_;
  bool panning_ = false;
  bool placed_ = false;

protected:
//   void resizeEvent(QResizeEvent* event)
//   {
//     setSceneRect(0, 0, event->size().width(), event->size().height());
//     QGraphicsView::resizeEvent(event);
//   }

};


}


#endif // SCHEMATICVIEW_H
