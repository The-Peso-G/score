// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TemporalIntervalPresenter.hpp"

#include "TemporalIntervalHeader.hpp"
#include "TemporalIntervalView.hpp"

#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Interval/CreateProcessInNewSlot.hpp>
#include <Scenario/Commands/Interval/Rack/SwapSlots.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Interval/DefaultHeaderDelegate.hpp>
#include <Scenario/Document/Interval/IntervalHeader.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/SlotHandle.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/selection/Selectable.hpp>
#include <score/tools/Bind.hpp>

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMenu>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TemporalIntervalPresenter)

namespace Scenario
{
TemporalIntervalPresenter::TemporalIntervalPresenter(
    const IntervalModel& interval,
    const EventModel& start,
    const EventModel& end,
    const Process::ProcessPresenterContext& ctx,
    bool handles,
    QGraphicsItem* parentobject,
    QObject* parent)
    : IntervalPresenter{interval,
                        new TemporalIntervalView{*this, parentobject},
                        new TemporalIntervalHeader{*this},
                        ctx,
                        parent}
    , startEvent{start}
    , endEvent{end}
    , m_handles{handles}
{
  m_header->setPos(15, -IntervalHeader::headerHeight());
  TemporalIntervalView& v = *view();
  auto head = header();
  con(v,
      &TemporalIntervalView::intervalHoverEnter,
      this,
      &TemporalIntervalPresenter::intervalHoverEnter);

  con(v,
      &TemporalIntervalView::intervalHoverLeave,
      this,
      &TemporalIntervalPresenter::intervalHoverLeave);

  con(v,
      &IntervalView::requestOverlayMenu,
      this,
      &TemporalIntervalPresenter::on_requestOverlayMenu);

  // Execution
  con(interval,
      &IntervalModel::executionStateChanged,
      &v,
      &TemporalIntervalView::setExecutionState);

  con(interval,
      &IntervalModel::executionStarted,
      this,
      [=] {
        m_view->setExecuting(true);
        head->setExecuting(true);
        m_view->updatePaths();
        m_view->update();
      },
      Qt::QueuedConnection);
  con(interval,
      &IntervalModel::executionStopped,
      this,
      [=] {
         m_view->setExecuting(false);
         head->setExecuting(false);
      },
      Qt::QueuedConnection);
  con(interval,
      &IntervalModel::executionFinished,
      this,
      [=] {
        m_view->setExecuting(false);
        head->setExecuting(false);
        m_view->setPlayWidth(0.);
        m_view->updatePaths();
        m_view->update();
      },
      Qt::QueuedConnection);

  // Events

  con(startEvent, &EventModel::statusChanged, m_view,
      [this] { m_view->update(); });
  con(endEvent, &EventModel::statusChanged, m_view,
      [this] { m_view->update(); });

  // Metadata
  const auto& metadata = m_model.metadata();
  con(metadata,
      &score::ModelMetadata::NameChanged,
      m_header, &IntervalHeader::on_textChanged);
  con(metadata,
      &score::ModelMetadata::LabelChanged,
      m_header, &IntervalHeader::on_textChanged);

  con(metadata,
      &score::ModelMetadata::ColorChanged,
      m_header, [this] {
    m_view->update();
    m_header->on_textChanged();
    for(auto& slot : m_slots)
    {
      if(slot.headerDelegate)
        slot.headerDelegate->updateText();
    }
  });

  m_header->on_textChanged();
  v.setExecutionState(m_model.executionState());

  // Selection
  con(m_model.selection, &Selectable::changed, this, [&, head](bool b) {
    if(b)
      v.setZValue(ZPos::SelectedInterval);
    else if(m_header->state() == IntervalHeader::State::RackShown)
      v.setZValue(ZPos::IntervalWithRack);
    else
      v.setZValue(ZPos::Interval);

    v.setSelected(b);

    head->setSelected(b);
  });
  //con(m_model, &IntervalModel::focusChanged, this, [&](bool b) {
  //  qDebug() << "focus changed: " << m_model.metadata().getName() << b;
  //});

  // Drop
  con(v,
      &TemporalIntervalView::dropReceived,
      this,
      [=](const QPointF& pos, const QMimeData& mime) {
        m_context.app.interfaces<Scenario::IntervalDropHandlerList>().drop(
            m_context, m_model, mime);
      });

  // Time
  con(interval.duration,
      &IntervalDurations::defaultDurationChanged,
      this,
      [&](const TimeVal& val) {
        on_defaultDurationChanged(val);
        updateChildren();
      });

  // Header set-up
  connect(
      head,
      &TemporalIntervalHeader::intervalHoverEnter,
      this,
      &TemporalIntervalPresenter::intervalHoverEnter);
  connect(
      head,
      &TemporalIntervalHeader::intervalHoverLeave,
      this,
      &TemporalIntervalPresenter::intervalHoverLeave);

  connect(
      head,
      &TemporalIntervalHeader::dropReceived,
      this,
      [=](const QPointF& pos, const QMimeData& mime) {
        m_context.app.interfaces<Scenario::IntervalDropHandlerList>().drop(
            m_context, m_model, mime);
      });

  // Go to full-view on double click
  connect(head, &TemporalIntervalHeader::doubleClicked, this, [this]() {
    using namespace score::IDocument;

    ScenarioDocumentPresenter* base
        = get<ScenarioDocumentPresenter>(*documentFromObject(m_model));

    if (base)
      base->setDisplayedInterval(const_cast<IntervalModel&>(m_model));
  });

  // Slots & racks
  con(m_model,
      &IntervalModel::smallViewVisibleChanged,
      this,
      &TemporalIntervalPresenter::on_rackVisibleChanged);

  con(m_model, &IntervalModel::rackChanged, this, [=](Slot::RackView t) {
    if (t == Slot::SmallView)
    {
      on_rackChanged();
    }
  });
  con(m_model, &IntervalModel::slotAdded, this, [=](const SlotId& s) {
    if (s.smallView())
    {
      createSlot(s.index, m_model.smallView()[s.index]);
    }
  });

  con(m_model,
      &IntervalModel::slotsSwapped,
      this,
      [=](int i, int j, Slot::RackView v) {
        if (v == Slot::SmallView)
          on_rackChanged();
      });

  con(m_model, &IntervalModel::slotRemoved, this, [=](const SlotId& s) {
    if (s.smallView())
      on_slotRemoved(s.index);
  });

  con(m_model, &IntervalModel::slotResized, this, [this](const SlotId& s) {
    if (s.smallView())
      this->updatePositions();
  });

  con(m_model,
      &IntervalModel::layerAdded,
      this,
      [=](SlotId s, Id<Process::ProcessModel> proc) {
        if (s.smallView())
          createLayer(s.index, m_model.processes.at(proc));
      });
  con(m_model,
      &IntervalModel::layerRemoved,
      this,
      [=](SlotId s, Id<Process::ProcessModel> proc) {
        if (s.smallView())
          removeLayer(m_model.processes.at(proc));
      });
  con(m_model,
      &IntervalModel::frontLayerChanged,
      this,
      [=](int pos, OptionalId<Process::ProcessModel> proc) {
        if (proc)
        {
          on_layerModelPutToFront(pos, m_model.processes.at(*proc));
        }
        else
        {
          if (pos < (int)m_slots.size())
          {
            auto& slt = m_slots[pos];
            slt.cleanupHeaderFooter();
          }
        }
      });

  m_model.processes.added
      .connect<&TemporalIntervalPresenter::on_processesChanged>(this);
  m_model.processes.removed
      .connect<&TemporalIntervalPresenter::on_processesChanged>(this);

  on_defaultDurationChanged(m_model.duration.defaultDuration());
  on_rackVisibleChanged(m_model.smallViewVisible());
}

TemporalIntervalPresenter::~TemporalIntervalPresenter()
{
  auto view = Scenario::view(this);
  auto sc = view->scene();
  for (auto& slt : m_slots)
  {
    slt.cleanup(sc);
  }

  if (sc)
  {
    sc->removeItem(view);
  }

  delete view;
}

void TemporalIntervalPresenter::on_requestOverlayMenu(QPointF)
{
  auto& fact = m_context.app.interfaces<Process::ProcessFactoryList>();
  auto dialog = new AddProcessDialog{fact,
                                     Process::ProcessFlags::SupportsTemporal,
                                     QApplication::activeWindow()};

  dialog->on_okPressed = [&](const auto& key, QString dat) {
    using namespace Scenario::Command;

    if (fact.get(key)->flags() & Process::ProcessFlags::PutInNewSlot)
    {
      Macro m{new AddProcessInNewSlot, m_context};

      if (auto p = m.createProcess(this->model(), key, dat))
      {
        m.createSlot(this->model());
        m.addLayerToLastSlot(this->model(), *p);
        m.commit();
      }
    }
    else
    {
      CommandDispatcher<> d{m_context.commandStack};
      d.submit<AddProcessToInterval>(this->model(), key, dat);
    }
  };

  dialog->launchWindow();
  dialog->deleteLater();
}

double TemporalIntervalPresenter::rackHeight() const
{
  qreal height = 1. + m_model.smallView().size() * (SlotHeader::headerHeight() + SlotFooter::footerHeight());
  for (const auto& slot : m_model.smallView())
  {
    height += slot.height;
  }
  return height;
}
double TemporalIntervalPresenter::smallRackHeight() const
{
  qreal height = 1. + m_model.smallView().size() * (SlotHeader::headerHeight() + SlotFooter::footerHeight());

  return height;
}

void TemporalIntervalPresenter::updateHeight()
{
  if (m_model.smallViewVisible())
  {
    m_view->setHeight(rackHeight());
  }
  else if (!m_model.smallViewVisible() && !m_model.processes.empty())
  {
    m_view->setHeight(smallRackHeight());
  }
  else
  {
    m_view->setHeight(8);
  }

  updateChildren();
  heightChanged();
}

static SlotDragOverlay* temporal_slot_drag_overlay{};
void TemporalIntervalPresenter::startSlotDrag(int curslot, QPointF pos) const
{
  // Create an overlay object
  temporal_slot_drag_overlay = new SlotDragOverlay{*this, Slot::SmallView};
  connect(
      temporal_slot_drag_overlay,
      &SlotDragOverlay::dropBefore,
      this,
      [=](int slot) {
        if (slot == curslot)
          return;
        CommandDispatcher<> disp{this->m_context.commandStack};
        if (qApp->keyboardModifiers() & Qt::ALT
            || m_model.smallView()[curslot].processes.size() == 1)
        {
          disp.submit<Command::ChangeSlotPosition>(
              this->m_model, Slot::RackView::SmallView, curslot, slot);
        }
        else
        {
          disp.submit<Command::MoveLayerInNewSlot>(
              this->m_model, curslot, slot);
        }
      },
      Qt::QueuedConnection); // needed because else SlotHeader is removed and
                             // stopSlotDrag can't be called

  connect(
      temporal_slot_drag_overlay,
      &SlotDragOverlay::dropIn,
      this,
      [=](int slot) {
        if (slot == curslot)
          return;

        CommandDispatcher<> disp{this->m_context.commandStack};
        if (qApp->keyboardModifiers() & Qt::ALT
            || m_model.smallView()[curslot].processes.size() == 1)
        {
          disp.submit<Command::MergeSlots>(this->m_model, curslot, slot);
        }
        else
        {
          disp.submit<Command::MergeLayerInSlot>(this->m_model, curslot, slot);
        }
      },
      Qt::QueuedConnection);
  temporal_slot_drag_overlay->setParentItem(view());
  temporal_slot_drag_overlay->onDrag(pos);
}

void TemporalIntervalPresenter::stopSlotDrag() const
{
  delete temporal_slot_drag_overlay;
  temporal_slot_drag_overlay = nullptr;
}

void TemporalIntervalPresenter::on_rackVisibleChanged(bool b)
{
  if (b)
  {
    if (!m_model.processes.empty())
    {
      m_header->setState(IntervalHeader::State::RackShown);
      m_view->setZValue(ZPos::IntervalWithRack);
    }
    else
    {
      m_header->setState(IntervalHeader::State::Hidden);
      m_view->setZValue(ZPos::Interval);
    }
  }
  else if (!m_model.processes.empty())
  {
    m_header->setState(IntervalHeader::State::RackHidden);
    m_view->setZValue(ZPos::Interval);
  }
  else
  {
    m_header->setState(IntervalHeader::State::Hidden);
    m_view->setZValue(ZPos::Interval);
  }

  on_rackChanged();
}

void TemporalIntervalPresenter::createSlot(int pos, const Slot& aSlt)
{
  if (m_model.smallViewVisible())
  {
    SlotPresenter p;
    p.header = new SlotHeader{*this, pos, m_view};
    if(m_handles)
      p.footer = new AmovibleSlotFooter{*this, pos, m_view};
    else
      p.footer = new FixedSlotFooter{*this, pos, m_view};

    // p.view = new SlotView{};
    m_slots.insert(m_slots.begin() + pos, std::move(p));

    // FIXME: due to a crash with slots with invalid processes being
    // serialized. fix the model !!
    auto& slt = const_cast<Slot&>(aSlt);
    for (auto it = slt.processes.begin(); it != slt.processes.end();)
    {
      auto pit = m_model.processes.find(*it);
      if (pit != m_model.processes.end())
      {
        createLayer(pos, *pit);
        ++it;
      }
      else
      {
        it = slt.processes.erase(it);
      }
    }

    updatePositions();
  }
}
void TemporalIntervalPresenter::createSmallSlot(int pos, const Slot& slt)
{
  SlotPresenter p;
  p.header = new SlotHeader{*this, pos, m_view};
  p.footer = new FixedSlotFooter{*this, pos, m_view};

  // FIXME: due to a crash with slots with invalid processes being
  // serialized. fix the model !!

  const auto frontLayer = slt.frontProcess;
  if(frontLayer)
  {
    const Id<Process::ProcessModel>& id = *frontLayer;
    auto proc = m_model.processes.find(id);
    SCORE_ASSERT(proc != m_model.processes.end());
    const auto& procKey = proc->concreteKey();
    auto factory = m_context.processList.findDefaultFactory(procKey);

    {
      p.headerDelegate = factory->makeHeaderDelegate(*proc, m_context, nullptr);
      p.headerDelegate->setParentItem(p.header);
      p.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
      p.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
      p.headerDelegate->setPos(15, 0);
    }

    {
      p.footerDelegate = factory->makeFooterDelegate(*proc, m_context);
      p.footerDelegate->setParentItem(p.footer);
      p.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
      p.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
      p.footerDelegate->setPos(15, 0);
    }

    setHeaderWidth(p, m_model.duration.defaultDuration().toPixels(m_zoomRatio));
  }
  m_slots.insert(m_slots.begin() + pos, std::move(p));

  updatePositions();
}

void TemporalIntervalPresenter::createLayer(
    int slot_i,
    const Process::ProcessModel& proc)
{
  if (m_model.smallViewVisible())
  {
    auto& layers = m_slots.at(slot_i).layers;
    LayerData& ld = layers.emplace_back(&proc);

    const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
    const auto slot_height = m_model.smallView().at(slot_i).height;

    ld.updateLoops(m_context, m_zoomRatio, def_width, def_width, slot_height, m_view, this);

    con(proc, &Process::ProcessModel::loopsChanged,
        this, [this, slot_i] (bool b) {
      SCORE_ASSERT(slot_i < m_slots.size());
      auto& slt = this->m_slots[slot_i];

      SCORE_ASSERT(!slt.layers.empty());
      LayerData& ld = slt.layers.front();
      const auto def_width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
      const auto slot_height = m_model.smallView().at(slot_i).height;
      ld.updateLoops(m_context, m_zoomRatio, def_width, def_width, slot_height, this->m_view, this);
    });

    con(proc, &Process::ProcessModel::startOffsetChanged,
        this, [this, slot_i] {
      SCORE_ASSERT(slot_i < m_slots.size());
      auto& slot = this->m_slots[slot_i];

      SCORE_ASSERT(!slot.layers.empty());
      auto& ld = slot.layers.front();

      ld.updateStartOffset(-ld.model().startOffset().toPixels(m_zoomRatio));
    });
    con(proc, &Process::ProcessModel::loopDurationChanged,
        this, [this, slot_i] {
      SCORE_ASSERT(slot_i < m_slots.size());
      auto& slot = this->m_slots[slot_i];

      SCORE_ASSERT(!slot.layers.empty());
      auto& ld = slot.layers.front();
      auto view_width = ld.model().loopDuration().toPixels(m_zoomRatio);
      ld.updateXPositions(view_width);
      ld.updateContainerWidths(view_width);
    });
/*
    auto con_id = con(
        proc,
        &Process::ProcessModel::durationChanged,
        this,
        [&](const TimeVal&) {
          int i = 0;
          for (const SlotPresenter& slot : m_slots)
          {
            auto it
                = ossia::find_if(slot.layers, [&](const LayerData& ld) {
                    return ld.model().id() == proc.id();
                  });

            if (it != slot.layers.end())
              updateProcessShape(i, *it);
            i++;
          }
        });

    con(proc,
        &IdentifiedObjectAbstract::identified_object_destroying,
        this,
        [=] { QObject::disconnect(con_id); });
*/
    auto frontLayer = m_model.smallView().at(slot_i).frontProcess;
    if (frontLayer && (*frontLayer == proc.id()))
    {
      on_layerModelPutToFront(slot_i, proc);
    }
    else
    {
      on_layerModelPutToBack(slot_i, proc);
    }

    updatePositions();
  }
}

void TemporalIntervalPresenter::updateProcessShape(
    int slot,
    const LayerData& ld)
{
  if (m_model.smallViewVisible())
  {
    const auto h = m_model.smallView().at(slot).height;
    ld.setHeight(h);
    ld.updateContainerHeights(h);

    auto width = m_model.duration.defaultDuration().toPixels(m_zoomRatio);
    ld.setWidth(width, width);
    ld.parentGeometryChanged();
    ld.update();
  }
}

void TemporalIntervalPresenter::removeLayer(const Process::ProcessModel& proc)
{
  if (m_model.smallViewVisible())
  {
    for (SlotPresenter& slot : m_slots)
    {
      ossia::remove_erase_if(slot.layers, [&](LayerData& ld) {
        bool to_delete = ld.model().id() == proc.id();

        if (to_delete)
        {
          LayerData::disconnect(ld.model(), *this);
          ld.cleanup();
        }

        return to_delete;
      });
    }
  }
}

void TemporalIntervalPresenter::on_slotRemoved(int pos)
{
  if (pos < (int)m_slots.size())
  {
    SlotPresenter& slot = m_slots[pos];
    for(auto& layer : slot.layers)
    {
      LayerData::disconnect(layer.model(), *this);
    }
    slot.cleanup(this->view()->scene());
    m_slots.erase(m_slots.begin() + pos);

    updatePositions();
  }
}

void TemporalIntervalPresenter::updateProcessesShape()
{
  if (m_model.smallViewVisible())
  {
    for (int i = 0; i < (int)m_slots.size(); i++)
    {
      for (const LayerData& ld : m_slots[i].layers)
      {
        updateProcessShape(i, ld);
      }
    }
  }
  updateScaling();
}

void TemporalIntervalPresenter::updatePositions()
{
  using namespace std;
  // Vertical shape
  m_view->setHeight(rackHeight() + IntervalHeader::headerHeight());

  // Set the slots position graphically in order.
  qreal currentSlotY = 1.;

  const bool sv = m_model.smallViewVisible();
  for (int i = 0; i < (int)m_slots.size(); i++)
  {
    SlotPresenter& slot = m_slots[i];
    const Slot& model = m_model.smallView()[i];

    if (slot.header)
    {
      slot.header->setPos(QPointF{0.5, currentSlotY});
      slot.header->setSlotIndex(i);
    }
    currentSlotY += SlotHeader::headerHeight();

    if(sv)
    {
      for (LayerData& ld : slot.layers)
      {
        ld.updatePositions(currentSlotY, 0);
        ld.update();
      }
      currentSlotY += model.height;
    }
    else
    {
      currentSlotY -= 1.;
    }

    if (slot.footer)
    {
      slot.footer->setPos(QPointF{0.5, currentSlotY});
      slot.footer->setSlotIndex(i);
      currentSlotY += SlotFooter::footerHeight();
    }
  }

  // Horizontal shape
  on_defaultDurationChanged(m_model.duration.defaultDuration());

  updateProcessesShape();
}
void TemporalIntervalPresenter::on_layerModelPutToFront(
    int slot,
    const Process::ProcessModel& proc)
{
  if (m_model.smallViewVisible())
  {
    // Put the selected one at z+1 and the others at -z; set "disabled"
    // graphics mode. OPTIMIZEME by saving the previous to front and just
    // switching...
    SlotPresenter& slt = m_slots.at(slot);
    slt.cleanupHeaderFooter();
    for (const LayerData& ld : slt.layers)
    {
      if (ld.model().id() == proc.id())
      {
        auto factory = m_context.processList.findDefaultFactory(
            ld.model().concreteKey());
        ld.putToFront();
        ld.setZValue(2);
        {
        slt.headerDelegate = factory->makeHeaderDelegate(ld.model(), m_context, ld.mainPresenter());
        slt.headerDelegate->setParentItem(slt.header);
        slt.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
        slt.headerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
        slt.headerDelegate->setPos(15, 0);
        }

        {
        slt.footerDelegate = factory->makeFooterDelegate(ld.model(), m_context);
        slt.footerDelegate->setParentItem(slt.footer);
        slt.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsToShape);
        slt.footerDelegate->setFlag(QGraphicsItem::GraphicsItemFlag::ItemClipsChildrenToShape);
        slt.footerDelegate->setPos(15, 0);
        }

        setHeaderWidth(
            slt, m_model.duration.defaultDuration().toPixels(m_zoomRatio));
      }
      else
      {
        ld.putBehind();
        ld.setZValue(1);
      }
    }
  }
}

void TemporalIntervalPresenter::on_layerModelPutToBack(
    int slot,
    const Process::ProcessModel& proc)
{
  if (m_model.smallViewVisible())
  {
    for (const LayerData& ld : m_slots.at(slot).layers)
    {
      if (ld.model().id() == proc.id())
      {
        ld.putBehind();
        return;
      }
    }
  }
}

void TemporalIntervalPresenter::on_rackChanged()
{
  // Remove existing
  for (auto& slot : m_slots)
  {
    slot.cleanup(m_view->scene());
  }

  m_slots.clear();

  // Recreate
  if (m_model.smallViewVisible())
  {
    m_slots.reserve(m_model.smallView().size());

    int i = 0;
    for (const auto& slt : m_model.smallView())
    {
      createSlot(i, slt);
      i++;
    }
  }
  else if(!m_model.processes.empty())
  {
    m_slots.reserve(m_model.smallView().size());

    int i = 0;
    for (const auto& slt : m_model.smallView())
    {
      createSmallSlot(i, slt);
      i++;
    }
  }

  // Update view
  updatePositions();
}

void TemporalIntervalPresenter::updateScaling()
{
  on_defaultDurationChanged(model().duration.defaultDuration());
  IntervalPresenter::updateScaling();
  updateHeight();
}

void TemporalIntervalPresenter::on_zoomRatioChanged(ZoomRatio ratio)
{
  IntervalPresenter::on_zoomRatioChanged(ratio);
  auto def_width = m_model.duration.defaultDuration().toPixels(ratio);

  int i = 0;
  for (SlotPresenter& slot : m_slots)
  {
    const auto slot_height = m_model.smallView()[i].height;

    if (slot.headerDelegate)
      slot.headerDelegate->on_zoomRatioChanged(ratio);
    for (LayerData& ld : slot.layers)
    {
      ld.on_zoomRatioChanged(m_context, ratio, def_width, def_width, slot_height, m_view, this);
    }
    i++;
  }

  updateProcessesShape();
}

void TemporalIntervalPresenter::changeRackState()
{
  ((IntervalModel&)m_model)
      .setSmallViewVisible(
          !m_model.smallViewVisible() && !m_model.smallView().empty());
}

void TemporalIntervalPresenter::selectedSlot(int i) const
{
  score::SelectionDispatcher disp{m_context.selectionStack};
  SCORE_ASSERT(size_t(i) < m_slots.size());
  auto& slot = m_slots[i];
  if (slot.layers.empty())
  {
    disp.setAndCommit({&m_model});
  }
  else
  {
    auto proc = m_model.getSmallViewSlot(i).frontProcess;
    if (proc)
    {
      m_context.focusDispatcher.focus(m_slots[i].headerDelegate->m_presenter);
      disp.setAndCommit({&m_model.processes.at(*proc)});
    }
  }
}

TemporalIntervalView* TemporalIntervalPresenter::view() const
{
  return static_cast<TemporalIntervalView*>(this->m_view);
}

TemporalIntervalHeader* TemporalIntervalPresenter::header() const
{
  {
    return static_cast<TemporalIntervalHeader*>(this->m_header);
  }
}

void TemporalIntervalPresenter::requestSlotMenu(
    int slot,
    QPoint pos,
    QPointF sp) const
{
  if (const auto& proc = m_model.getSmallViewSlot(slot).frontProcess)
  {
    const SlotPresenter& slt = m_slots.at(slot);
    for (auto& p : slt.layers)
    {
      if (p.model().id() == proc)
      {
        auto menu = new QMenu;
        auto& reg = score::GUIAppContext()
                        .guiApplicationPlugin<ScenarioApplicationPlugin>()
                        .layerContextMenuRegistrar();
        p.fillContextMenu(*menu, pos, sp, reg);
        menu->exec(pos);
        menu->close();
        menu->deleteLater();
        break;
      }
    }
  }
}

void TemporalIntervalPresenter::setHeaderWidth(
    const SlotPresenter& slot,
    double w)
{
  w -= 1.;
  slot.header->setWidth(w);
  slot.footer->setWidth(w);

  if (slot.headerDelegate)
  {
    slot.headerDelegate->setSize(QSizeF{
        std::max(0., w - SlotHeader::handleWidth() - SlotHeader::menuWidth()),
        SlotHeader::headerHeight()});
    slot.headerDelegate->setX(15);
  }

  if (slot.footerDelegate)
  {
    slot.footerDelegate->setSize(QSizeF{w, SlotFooter::footerHeight()});
    slot.footerDelegate->setX(15);
  }
}

void TemporalIntervalPresenter::requestProcessSelectorMenu(
    int slot,
    QPoint pos,
    QPointF sp) const
{
  if (const auto& proc = m_model.getSmallViewSlot(slot).frontProcess)
  {
    const SlotPresenter& slt = m_slots.at(slot);
    for (auto& p : slt.layers)
    {
      if (p.model().id() == proc)
      {
        auto menu = new QMenu;
        ScenarioContextMenuManager::createProcessSelectorContextMenu(
              context(), *menu, *this, slot);
        menu->exec(pos);
        menu->deleteLater();
        break;
      }
    }
  }
}

void TemporalIntervalPresenter::on_defaultDurationChanged(const TimeVal& val)
{
  const auto w = val.toPixels(m_zoomRatio);
  m_view->setDefaultWidth(w);
  m_view->updateCounterPos();
  m_header->setWidth(w - 20.);
  ((TemporalIntervalHeader*)m_header)->updateButtons();
  ((TemporalIntervalHeader*)m_header)->update();
  updateBraces();

  int i = 0;
  for (SlotPresenter& slot : m_slots)
  {
    setHeaderWidth(slot, w);
    const auto slot_height = m_model.smallView()[i].height;
    i++;

    for (LayerData& ld : slot.layers)
    {
      ld.setWidth(w, w);
      ld.updateLoops(m_context, m_zoomRatio, w, w, slot_height, m_view, this);
    }
  }
}

void TemporalIntervalPresenter::on_processesChanged(
    const Process::ProcessModel&)
{
  if (m_model.smallViewVisible())
  {
    m_header->setState(IntervalHeader::State::RackShown);
  }
  else if (!m_model.processes.empty())
  {
    m_header->setState(IntervalHeader::State::RackHidden);
  }
  else
  {
    m_header->setState(IntervalHeader::State::Hidden);
  }
}
}
