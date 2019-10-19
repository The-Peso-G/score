#pragma once
#include <score/graphics/GraphicWidgets.hpp>

namespace score
{

template <typename T>
QGraphicsSliderBase<T>::QGraphicsSliderBase(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

template <typename T>
QGraphicsSliderBase<T>::~QGraphicsSliderBase()
{
  if (spinbox || spinboxProxy)
    delete spinboxProxy;
}

template <typename T>
QRectF QGraphicsSliderBase<T>::boundingRect() const
{
  return m_rect;
}

template <typename T>
bool QGraphicsSliderBase<T>::isInHandle(QPointF p)
{
  return m_rect.contains(p);
}

template <typename T>
double QGraphicsSliderBase<T>::getHandleX() const
{
  return sliderRect().width() * static_cast<const T&>(*this).m_value;
}

template <typename T>
QRectF QGraphicsSliderBase<T>::sliderRect() const
{
  return QRectF{2, 2, m_rect.width() - 4, 8};
  // return m_rect.adjusted(1, 1, -1, -1);
}

template <typename T>
QRectF QGraphicsSliderBase<T>::handleRect() const
{
  auto r = sliderRect().adjusted(1, 1, 0, -1);
  r.setWidth(std::max(0., static_cast<const T&>(*this).getHandleX() - 2.));
  return r;
  /*
  return {2, 2, std::max(0., getHandleX() - 2.), 6};
  return {2, 12, std::max(0., getHandleX() - 2.), m_rect.height() - 14};
  return sliderRect().adjusted(1, 10, -1, -1);//{2., 12., getHandleX(),
  m_rect.height() - 13.};
  */
}

template <typename T>
void QGraphicsSliderBase<T>::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

}
