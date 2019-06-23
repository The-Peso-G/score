#pragma once
#include <Scenario/Document/Interval/FullView/AddressBarItem.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>

#include <QRect>

class QGraphicsItem;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace Scenario
{
class AddressBarItem;
class IntervalPresenter;
class FullViewIntervalHeader final : public IntervalHeader
{
public:
  FullViewIntervalHeader(
      const IntervalPresenter& itv,
      const score::DocumentContext& ctx,
      QGraphicsItem*);

  static constexpr double headerHeight() { return 35.; }

  AddressBarItem& bar();

  void setWidth(double width);
  void setState(State s) override {}

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

private:
  AddressBarItem m_bar;
};
}
