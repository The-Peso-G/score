// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LayerPresenter.hpp"

#include <Process/Focus/FocusDispatcher.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Process::LayerPresenter)
W_OBJECT_IMPL(FocusDispatcher)
namespace Process
{
LayerPresenter::~LayerPresenter() = default;

LayerPresenter::LayerPresenter(
      const Context& ctx,
      QObject* parent)
  : QObject{parent}, m_context{ctx, *this}
{
}

bool LayerPresenter::focused() const
{
  return m_focus;
}

void LayerPresenter::setFocus(bool focus)
{
  m_focus = focus;
  on_focusChanged();
}

void LayerPresenter::on_focusChanged() {}

void LayerPresenter::setFullView() {}

void LayerPresenter::fillContextMenu(
    QMenu&,
    QPoint pos,
    QPointF scenepos,
    const LayerContextMenuManager&)
{
}

GraphicsShapeItem* LayerPresenter::makeSlotHeaderDelegate()
{
  return nullptr;
}

GraphicsShapeItem::~GraphicsShapeItem() {}

void GraphicsShapeItem::setSize(QSizeF sz)
{
  prepareGeometryChange();
  m_sz = sz;
  update();
}

QRectF GraphicsShapeItem::boundingRect() const
{
  return {0, 0, m_sz.width(), m_sz.height()};
}
}
