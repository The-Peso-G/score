
#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <ControlSurface/Presenter.hpp>
#include <ControlSurface/Process.hpp>
#include <ControlSurface/View.hpp>
#include <Process/Dataflow/Port.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <ControlSurface/CommandFactory.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <Control/DefaultEffectItem.hpp>
#include <ControlSurface/Commands.hpp>
#include <Effect/EffectLayout.hpp>
#include <Process/Dataflow/PortItem.hpp>
#include <Process/Style/Pixmaps.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <QTimer>

namespace ControlSurface
{
struct Presenter::Port
{
  score::EmptyRectItem* root;
  Dataflow::PortItem* port;
  QRectF rect;
};

Presenter::Presenter(
    const Model& layer, View* view,
    const Process::ProcessPresenterContext& ctx, QObject* parent)
    : Process::LayerPresenter{ctx, parent}, m_model{layer}, m_view{view}
{
  connect(view, &View::addressesDropped,
          this, [this] (State::MessageList lst) {
    if(lst.isEmpty())
      return;

    auto& docctx = this->context().context;
    MacroCommandDispatcher<AddControlMacro> disp{docctx.commandStack};
    for(auto& message : lst) {
      disp.submit(new AddControl{docctx, m_model, std::move(message)});
    }
    disp.commit();
  });

  connect(
      &layer,
      &Process::ProcessModel::controlAdded,
      this,
      &Presenter::on_controlAdded);

  connect(
      &layer,
      &Process::ProcessModel::controlRemoved,
      this,
      &Presenter::on_controlRemoved);

  auto& portFactory = ctx.app.interfaces<Process::PortFactoryList>();
  for (auto& e : layer.inlets())
  {
    auto inlet = qobject_cast<Process::ControlInlet*>(e);
    if (!inlet)
      continue;

    setupInlet(*inlet, portFactory, ctx);
  }
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setOpacity(1);
}

void Presenter::putBehind()
{
  m_view->setOpacity(0.2);
}

void Presenter::on_zoomRatioChanged(ZoomRatio)
{
}

void Presenter::parentGeometryChanged()
{
}

const Process::ProcessModel& Presenter::model() const
{
  return m_model;
}

const Id<Process::ProcessModel>& Presenter::modelId() const
{
  return m_model.id();
}

void Presenter::setupInlet(
    Process::ControlInlet& port,
    const Process::PortFactoryList& portFactory,
    const Process::ProcessPresenterContext& doc)
{
  // Main item creation
  int i = m_ports.size();

  auto csetup = Process::controlSetup(
   [ ] (auto& factory, auto& inlet, const auto& doc, auto item, auto parent)
   { return factory.makeItem(inlet, doc, item, parent); },
   [ ] (auto& factory, auto& inlet, const auto& doc, auto item, auto parent)
   { return factory.makeControlItem(inlet, doc, item, parent); },
   [&] (int j) { return m_ports[j].rect.size(); },
   [&] { return port.customData(); }
  );
  auto [item, portItem, widg, lab, itemRect]
      = Process::createControl(i, csetup, port, portFactory, doc, m_view, this);

  // Remove button
  {
    const auto& pixmaps = Process::Pixmaps::instance();
    auto rm_item = new score::QGraphicsPixmapButton{pixmaps.close_on, pixmaps.close_off, item};
    connect(
          rm_item,
          &score::QGraphicsPixmapButton::clicked,
          &port,
          [this, &port] {
      QTimer::singleShot(0, &port, [this, &port] {
        CommandDispatcher<> disp{m_context.context.commandStack};
        disp.submit<RemoveControl>(m_model, port);
      });
    });

    rm_item->setPos(8., 16.);
  }

  m_ports.push_back(Port{item, portItem, itemRect});
  // TODO updateRect();

  // TODO con(inlet, &Process::ControlInlet::domainChanged, this, [this, &inlet] {
  // TODO   on_controlRemoved(inlet);
  // TODO   on_controlAdded(inlet.id());
  // TODO });

}

void Presenter::on_controlAdded(const Id<Process::Port>& id)
{
  auto& portFactory = m_context.context.app.interfaces<Process::PortFactoryList>();
  auto inlet = safe_cast<Process::ControlInlet*>(m_model.inlet(id));
  setupInlet(*inlet, portFactory, m_context.context);
}

void Presenter::on_controlRemoved(const Process::Port& port)
{
  for (auto it = m_ports.begin(); it != m_ports.end(); ++it)
  {
    auto ptr = it->port;
    if (&ptr->port() == &port)
    {
      auto parent_item = it->root;
      auto h = parent_item->boundingRect().height();
      delete parent_item;
      m_ports.erase(it);

      for (; it != m_ports.end(); ++it)
      {
        auto item = it->root;
        item->moveBy(0., -h);
      }
      return;
    }
  }
}

}
