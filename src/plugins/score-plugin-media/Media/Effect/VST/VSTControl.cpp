#if defined(HAS_VST2)

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Dataflow/Commands/EditConnection.hpp>
#include <Media/Commands/VSTCommands.hpp>
#include <Media/Effect/VST/VSTControl.hpp>
#include <Media/Effect/VST/VSTWidgets.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <score/widgets/TextLabel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <QMenu>

#include <QToolButton>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::VST::VSTControlInlet)
namespace Media::VST
{

void VSTControlPortItem::setupMenu(
    QMenu& menu,
    const score::DocumentContext& ctx)
{
  auto rm_act = menu.addAction(QObject::tr("Remove port"));
  connect(rm_act, &QAction::triggered, this, [this, &ctx] {
    QTimer::singleShot(0, [&ctx, parent = port().parent(), id = port().id()] {
      CommandDispatcher<> disp{ctx.commandStack};
      disp.submit<RemoveVSTControl>(*static_cast<VSTEffectModel*>(parent), id);
    });
  });
}

bool VSTControlPortItem::on_createAutomation(
    Scenario::IntervalModel& cst,
    std::function<void(score::Command*)> macro,
    const score::DocumentContext& ctx)
{
  auto make_cmd = new Scenario::Command::AddOnlyProcessToInterval{
      cst, Metadata<ConcreteKey_k, Automation::ProcessModel>::get(), {}};
  macro(make_cmd);

  auto lay_cmd
      = new Scenario::Command::AddLayerInNewSlot{cst, make_cmd->processId()};
  macro(lay_cmd);

  auto& autom = safe_cast<Automation::ProcessModel&>(
      cst.processes.at(make_cmd->processId()));
  macro(new Automation::SetMin{autom, 0.});
  macro(new Automation::SetMax{autom, 1.});

  auto& plug = ctx.model<Scenario::ScenarioDocumentModel>();
  Process::CableData cd;
  cd.type = Process::CableType::ImmediateGlutton;
  cd.source = *autom.outlet;
  cd.sink = port();

  macro(new Dataflow::CreateCable{
      plug, getStrongId(plug.cables), std::move(cd)});
  return true;
}

VSTControlPortFactory::~VSTControlPortFactory() {}

UuidKey<Process::Port> VSTControlPortFactory::concreteKey() const noexcept
{
  return Metadata<ConcreteKey_k, VSTControlInlet>::get();
}

Process::Port*
VSTControlPortFactory::load(const VisitorVariant& vis, QObject* parent)
{
  return score::deserialize_dyn(vis, [&](auto&& deserializer) {
    return new VSTControlInlet{deserializer, parent};
  });
}

Dataflow::PortItem* VSTControlPortFactory::makeItem(
    Process::Inlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  auto port_item = new VSTControlPortItem{port, ctx, parent};
  // Dataflow::setupSimpleInlet(port_item, port, ctx, parent, context);
  return port_item;
}

Dataflow::PortItem* VSTControlPortFactory::makeItem(
    Process::Outlet& port,
    const Process::Context& ctx,
    QGraphicsItem* parent,
    QObject* context)
{
  return nullptr;
}

static void setupVSTControl(
      const VSTControlInlet& inlet,
      QWidget* inlet_widget,
      const score::DocumentContext& ctx,
      Inspector::Layout& vlay,
      QWidget* parent)
{
  // TODO refactor with PortWidgetSetup::setupControl
  auto widg = new QWidget;
  auto advBtn = new QToolButton{widg};
  advBtn->setText("●");

  auto lab = new TextLabel{inlet.customData(), widg};
  auto hl = new score::MarginLess<QHBoxLayout>{widg};
  hl->addWidget(advBtn);
  hl->addWidget(lab);

  auto sw = new QWidget{parent};
  sw->setContentsMargins(0, 0, 0, 0);
  auto hl2 = new score::MarginLess<QHBoxLayout>{sw};
  hl2->addSpacing(30);
  auto lay = new Inspector::Layout{};
  Process::PortWidgetSetup::setupInLayout(inlet, ctx, *lay, sw);
  hl2->addLayout(lay);

  QObject::connect(advBtn, &QToolButton::clicked, sw, [=] {
    sw->setVisible(!sw->isVisible());
  });
  sw->setVisible(false);

  vlay.addRow(widg, inlet_widget);
  vlay.addRow(sw);
}

void VSTControlPortFactory::setupInletInspector(
      Process::Inlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context)
{
  auto& inl = safe_cast<VSTControlInlet&>(port);
  auto proc = safe_cast<VSTEffectModel*>(port.parent());
  auto widg = VSTFloatSlider::make_widget(proc->fx->fx, inl, ctx, parent, context);

  setupVSTControl(inl, widg, ctx, lay, parent);
}
}
#endif
