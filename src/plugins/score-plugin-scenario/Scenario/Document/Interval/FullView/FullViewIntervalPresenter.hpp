#pragma once
#include <Scenario/Document/Interval/FullView/FullViewIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Scenario/Document/Interval/SlotPresenter.hpp>

#include <score/selection/SelectionDispatcher.hpp>

#include <verdigris>
namespace Process
{
class DefaultHeaderDelegate;
}
namespace Scenario
{
class SlotView;
class SlotHandle;
class SCORE_PLUGIN_SCENARIO_EXPORT FullViewIntervalPresenter final
    : public IntervalPresenter
{
  W_OBJECT(FullViewIntervalPresenter)

public:
  using view_type = FullViewIntervalView;

  FullViewIntervalPresenter(
      const IntervalModel& viewModel,
      const Process::Context& ctx,
      QGraphicsItem* parentobject,
      QObject* parent);

  ~FullViewIntervalPresenter() override;

  void updateHeight();
  void on_zoomRatioChanged(ZoomRatio val) override;

  const std::vector<SlotPresenter>& getSlots() const { return m_slots; }

public:
  void intervalSelected(IntervalModel& arg_1)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, intervalSelected, arg_1)

private:
  void startSlotDrag(int slot, QPointF) const override;
  void stopSlotDrag() const override;

  void requestSlotMenu(int slot, QPoint pos, QPointF sp) const override;
  void updateScaling() override;
  void selectedSlot(int) const override;

  void on_defaultDurationChanged(const TimeVal&);
  void on_guiDurationChanged(const TimeVal&);
  void createSlot(int pos, const FullSlot& slt);
  void updateProcessShape(int slot);
  void updateProcessShape(const LayerData& layer, const SlotPresenter& pres);
  void on_slotRemoved(int);

  void updateProcessesShape();
  void updatePositions();

  double rackHeight() const;
  void on_rackChanged();
};
}
