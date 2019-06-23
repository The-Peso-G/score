// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalHeader.hpp"

#include "IntervalView.hpp"

#include <QCursor>

class QGraphicsSceneMouseEvent;
namespace Scenario
{
const QCursor& openCursor()
{
  static const QCursor c{Qt::OpenHandCursor};
  return c;
}
const QCursor& closedCursor()
{
  static const QCursor c{Qt::ClosedHandCursor};
  return c;
}

void IntervalHeader::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (this->cursor().shape() != closedCursor().shape())
    this->setCursor(closedCursor());
  m_view->mousePressEvent(event);
  event->accept();
}

void IntervalHeader::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_view->mouseMoveEvent(event);
  event->accept();
}

void IntervalHeader::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (this->cursor().shape() != openCursor().shape())
    this->setCursor(openCursor());
  m_view->mouseReleaseEvent(event);
  event->accept();
}
}
