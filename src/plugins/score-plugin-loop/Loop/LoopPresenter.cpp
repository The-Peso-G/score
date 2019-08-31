// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "LoopPresenter.hpp"

#include "Loop/LoopViewUpdater.hpp"

#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopView.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/IntervalActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/StateActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicsItem.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QGraphicsItem>
#include <QMenu>
#include <QTimer>

#include <wobjectimpl.h>

#include <tuple>

#include <type_traits>
W_OBJECT_IMPL(Loop::LayerPresenter)
namespace Process
{
class ProcessModel;
}
class QMenu;
class QObject;
namespace Scenario
{
struct VerticalExtent;
}
namespace Loop
{
LayerPresenter::LayerPresenter(
    const ProcessModel& layer,
    LayerView* view,
    const Process::Context& ctx,
    QObject* parent)
    : Process::LayerPresenter{ctx, parent}
    , BaseScenarioPresenter<
          Loop::ProcessModel,
          Scenario::TemporalIntervalPresenter>{layer}
    , m_layer{layer}
    , m_view{view}
    , m_viewUpdater{*this}
    , m_palette{m_layer, *this, m_context, *m_view}
{
  using namespace Scenario;
  m_intervalPresenter = new TemporalIntervalPresenter{
      layer.interval(), layer.startEvent(), layer.endEvent(), ctx, false, view, this};
  m_startStatePresenter = new StatePresenter{
      layer.BaseScenarioContainer::startState(), ctx, m_view, this};
  m_endStatePresenter = new StatePresenter{
      layer.BaseScenarioContainer::endState(), ctx, m_view, this};
  m_startEventPresenter = new EventPresenter{layer.startEvent(), m_view, this};
  m_endEventPresenter = new EventPresenter{layer.endEvent(), m_view, this};
  m_startNodePresenter
      = new TimeSyncPresenter{layer.startTimeSync(), m_view, this};
  m_endNodePresenter
      = new TimeSyncPresenter{layer.endTimeSync(), m_view, this};

  auto elements = std::make_tuple(
      m_intervalPresenter,
      m_startStatePresenter,
      m_endStatePresenter,
      m_startEventPresenter,
      m_endEventPresenter,
      m_startNodePresenter,
      m_endNodePresenter);

  ossia::for_each_in_tuple(elements, [&](auto elt) {
    using elt_t = std::remove_reference_t<decltype(*elt)>;
    connect(elt, &elt_t::pressed, this, &LayerPresenter::pressed);
    connect(elt, &elt_t::moved, this, &LayerPresenter::moved);
    connect(elt, &elt_t::released, this, &LayerPresenter::released);
  });

  con(m_endEventPresenter->model(),
      &EventModel::extentChanged,
      this,
      [=](const VerticalExtent&) {
        m_viewUpdater.updateEvent(*m_endEventPresenter);
      });
  con(m_endEventPresenter->model(),
      &EventModel::dateChanged,
      this,
      [=](const TimeVal&) {
        m_viewUpdater.updateEvent(*m_endEventPresenter);
      });
  con(m_endNodePresenter->model(),
      &TimeSyncModel::extentChanged,
      this,
      [=](const VerticalExtent&) {
        m_viewUpdater.updateTimeSync(*m_endNodePresenter);
      });
  con(m_endNodePresenter->model(),
      &TimeSyncModel::dateChanged,
      this,
      [=](const TimeVal&) {
        m_viewUpdater.updateTimeSync(*m_endNodePresenter);
      });

  m_model.interval().processes.added.connect<&LayerPresenter::on_addProcess>(
      *this);
  m_model.interval()
      .processes.removed.connect<&LayerPresenter::on_removeProcess>(*this);
  connect(
      m_view,
      &LayerView::askContextMenu,
      this,
      &LayerPresenter::contextMenuRequested);
  connect(m_view, &LayerView::pressed, this, [&]() {
    m_context.context.focusDispatcher.focus(this);
  });

  con(ctx.execTimer,
      &QTimer::timeout,
      this,
      &LayerPresenter::on_intervalExecutionTimer);

  m_startStatePresenter->view()->disableOverlay();
  m_endStatePresenter->view()->disableOverlay();

  m_intervalPresenter->view()->unsetCursor();
}

void LayerPresenter::on_addProcess(const Process::ProcessModel& p)
{
  QTimer::singleShot(32, this, [=] { setHeight(m_view->height()); });
}
void LayerPresenter::on_removeProcess(const Process::ProcessModel& p)
{
  QTimer::singleShot(32, this, [=] { setHeight(m_view->height()); });
}
LayerPresenter::~LayerPresenter()
{
  disconnect(
      &m_context.context.execTimer,
      &QTimer::timeout,
      this,
      &LayerPresenter::on_intervalExecutionTimer);

  delete m_intervalPresenter;
  delete m_startStatePresenter;
  delete m_endStatePresenter;
  delete m_startEventPresenter;
  delete m_endEventPresenter;
  delete m_startNodePresenter;
  delete m_endNodePresenter;
}

void LayerPresenter::on_intervalExecutionTimer()
{
  if (m_intervalPresenter->on_playPercentageChanged(
          m_intervalPresenter->model().duration.playPercentage()))
    m_intervalPresenter->view()->update();
}

void LayerPresenter::setWidth(qreal width, qreal defaultWidth)
{
  m_view->setWidth(width);
}

void LayerPresenter::setHeight(qreal height)
{
  m_view->setHeight(height);
  auto& c = m_intervalPresenter->model();
  auto max_height = height - 22.;
  const auto N = c.smallView().size();

  if(N > 0)
  {
    const double totalSlotHeight = max_height / double(N);
    const double slotContentHeight = totalSlotHeight - Scenario::SlotHeader::headerHeight() - Scenario::SlotFooter::footerHeight();
    const double slotHeight = std::max(20., slotContentHeight);

    auto& itv = const_cast<Scenario::IntervalModel&>(c);
    for (std::size_t i = 0U; i < N; i++)
    {
      itv.setSlotHeight(
          Scenario::SlotId{i, Scenario::Slot::SmallView}, slotHeight);
    }

  }
}

void LayerPresenter::putToFront()
{
  //m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
  m_view->setOpacity(1);
}

void LayerPresenter::putBehind()
{
  //m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
  m_view->setOpacity(0.1);
}

void LayerPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;
  m_intervalPresenter->on_zoomRatioChanged(m_zoomRatio);
}

void LayerPresenter::parentGeometryChanged()
{
  updateAllElements();
  m_view->update();
}

const Loop::ProcessModel& LayerPresenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& LayerPresenter::modelId() const
{
  return m_layer.id();
}

void LayerPresenter::updateAllElements()
{
  m_viewUpdater.updateInterval(*m_intervalPresenter);
  m_viewUpdater.updateEvent(*m_startEventPresenter);
  m_viewUpdater.updateEvent(*m_endEventPresenter);
  m_viewUpdater.updateTimeSync(*m_startNodePresenter);
  m_viewUpdater.updateTimeSync(*m_endNodePresenter);
}

void LayerPresenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const Process::LayerContextMenuManager&)
{
  // TODO ACTIONS
  /*
  auto selected = layerModel().processModel().selectedChildren();

  auto& appPlug =
  m_context.context.app.applicationPlugin<Scenario::ScenarioApplicationPlugin>();
  for(auto elt : appPlug.pluginActions())
  {
      if(auto oma = dynamic_cast<Scenario::ObjectMenuActions*>(elt))
      {
          oma->eventActions()->fillContextMenu(menu, selected, pos, scenepos);
          if(m_model.interval().selection.get())
              oma->intervalActions()->fillContextMenu(menu, selected,
  m_layer.interval(), pos, scenepos);
          menu->addSeparator();
      }
  }
  */
}

}
