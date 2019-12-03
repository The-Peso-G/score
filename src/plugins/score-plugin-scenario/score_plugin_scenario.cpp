// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Dataflow/Inspector/DataflowWidget.hpp>
#include <Dataflow/UI/PortItem.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Application/Drops/AutomationDropHandler.hpp>
#include <Scenario/Application/Drops/MessageDropHandler.hpp>
#include <Scenario/Application/Drops/ScenarioDropHandler.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventList.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/BaseScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/ScenarioTriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/DisplayedElements/BaseScenarioDisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProvider.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPaletteFactory.hpp>
#include <Scenario/Document/DisplayedElements/ScenarioDisplayedElementsProvider.hpp>
#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Tempo/TempoFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioRemover.hpp>
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerList.hpp>
#include <Scenario/Inspector/Interpolation/InterpolationInspectorWidget.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorFactory.hpp>
#include <Scenario/Inspector/ObjectTree/ObjectItemModel.hpp>
#include <Scenario/Inspector/Scenario/ScenarioInspectorFactory.hpp>
#include <Scenario/Inspector/ScenarioInspectorWidgetFactoryWrapper.hpp>
#include <Scenario/Library/SlotLibraryHandler.hpp>
#include <Scenario/Process/ScenarioExecution.hpp>
#include <Scenario/Process/ScenarioFactory.hpp>
#include <Scenario/Settings/ScenarioSettingsFactory.hpp>
#include <State/Message.hpp>
#include <State/Unit.hpp>
#include <State/Value.hpp>
#include <State/ValueSerialization.hpp>

#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/tools/std/HashMap.hpp>

#include <QList>
#include <QMetaType>
#include <QPainterPath>

#include <Interpolation/InterpolationFactory.hpp>
#include <score_plugin_scenario.hpp>
#include <score_plugin_scenario_commands_files.hpp>
#include <wobjectimpl.h>

#include <utility>

W_OBJECT_IMPL(Interpolation::Presenter)
score_plugin_scenario::score_plugin_scenario()
{
#if defined(SCORE_STATIC_PLUGINS)
  Q_INIT_RESOURCE(ScenarioResources);
#endif
  using namespace Scenario;
  // QMetaType::registerComparators<ossia::value>();
  QMetaType::registerComparators<State::Message>();
  QMetaType::registerComparators<State::MessageList>();

  qRegisterMetaType<State::Expression>();
  qRegisterMetaTypeStreamOperators<State::Message>();
  qRegisterMetaTypeStreamOperators<State::MessageList>();
  qRegisterMetaTypeStreamOperators<State::Address>();
  qRegisterMetaTypeStreamOperators<ossia::value>();
  qRegisterMetaTypeStreamOperators<State::Expression>();

  qRegisterMetaTypeStreamOperators<TimeVal>();
  qRegisterMetaType<ExecutionStatus>();
  qRegisterMetaType<Scenario::IntervalExecutionState>();
  qRegisterMetaType<QPointer<Process::LayerPresenter>>();

  qRegisterMetaType<Path<Scenario::IntervalModel>>();
  qRegisterMetaType<Id<Process::ProcessModel>>();

  qRegisterMetaType<LockMode>();
  qRegisterMetaType<Scenario::OffsetBehavior>();
  qRegisterMetaTypeStreamOperators<Scenario::OffsetBehavior>();

  qRegisterMetaType<State::Unit>();
  qRegisterMetaTypeStreamOperators<State::Unit>();
  qRegisterMetaTypeStreamOperators<State::vec2f>();
  qRegisterMetaTypeStreamOperators<State::vec3f>();
  qRegisterMetaTypeStreamOperators<State::vec4f>();

  qRegisterMetaType<QPainterPath>();
  qRegisterMetaType<QList<QPainterPath>>();

  qRegisterMetaType<std::shared_ptr<Execution::ProcessComponent>>();
  qRegisterMetaType<std::shared_ptr<Execution::EventComponent>>();
  qRegisterMetaType<ossia::time_event::status>();
  qRegisterMetaType<ossia::time_value>();
}

score_plugin_scenario::~score_plugin_scenario() = default;

score::GUIApplicationPlugin* score_plugin_scenario::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  using namespace Scenario;
  return new ScenarioApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_scenario::factoryFamilies()
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return make_ptr_vector<
      score::InterfaceListBase,
      MoveEventList,
      CSPCoherencyCheckerList,
      DisplayedElementsToolPaletteFactoryList,
      TriggerCommandFactoryList,
      DisplayedElementsProviderList,
      DropHandlerList,
      IntervalDropHandlerList,
      IntervalResizerList>();
}

template <>
struct FactoryBuilder<
    score::GUIApplicationContext,
    Scenario::ScenarioTemporalLayerFactory>
{
  static auto make(const score::GUIApplicationContext& ctx)
  {
    using namespace Scenario;
    auto& appPlugin = ctx.guiApplicationPlugin<ScenarioApplicationPlugin>();
    return std::make_unique<ScenarioTemporalLayerFactory>(
        appPlugin.editionSettings());
  }
};

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_scenario::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory,
      ScenarioFactory
      , Scenario::TempoFactory
      , Interpolation::InterpolationFactory
      >,
      FW<Process::LayerFactory
      , Interpolation::InterpolationLayerFactory
      , Scenario::TempoLayerFactory
      >,
      FW<MoveEventFactoryInterface, MoveEventClassicFactory>,
      FW<DisplayedElementsToolPaletteFactory,
         BaseScenarioDisplayedElementsToolPaletteFactory,
         ScenarioDisplayedElementsToolPaletteFactory>,
      FW<TriggerCommandFactory,
         ScenarioTriggerCommandFactory,
         BaseScenarioTriggerCommandFactory>,
      FW<DisplayedElementsProvider,
         ScenarioDisplayedElementsProvider,
         BaseScenarioDisplayedElementsProvider>,
      FW<score::DocumentDelegateFactory, Scenario::ScenarioDocumentFactory>,
      FW<score::SettingsDelegateFactory, Scenario::Settings::Factory>,
      FW<score::PanelDelegateFactory, Scenario::ObjectPanelDelegateFactory>,
      //      FW<score::PanelDelegateFactory, Scenario::PanelDelegateFactory>,
      FW<Process::ProcessDropHandler, Scenario::ProcessDataDropHandler>,
      FW<Scenario::DropHandler,
         Scenario::MessageDropHandler,
         Scenario::DropScenario,
         Scenario::DropScore,
         Scenario::DropProcessInScenario,
         Scenario::DropPortInScenario,
         Scenario::DropLayerInScenario>,
      FW<Scenario::IntervalDropHandler,
         Scenario::DropProcessInInterval,
         Scenario::DropLayerInInterval,
         Scenario::AutomationDropHandler>,
      FW<Inspector::InspectorWidgetFactory,
         ScenarioInspectorWidgetFactoryWrapper,
         Interpolation::StateInspectorFactory,
         ScenarioInspectorFactory,
         Interpolation::InspectorFactory,
         Dataflow::CableInspectorFactory,
         Dataflow::PortInspectorFactory>,
      FW<score::ValidityChecker, ScenarioValidityChecker>,
      FW<Process::PortFactory,
         Dataflow::InletFactory,
         Dataflow::ControlInletFactory,
         Dataflow::OutletFactory,
         Dataflow::AudioOutletFactory,
         Dataflow::ControlOutletFactory,
         Dataflow::WidgetInletFactory<Process::FloatSlider>,
         Dataflow::WidgetInletFactory<Process::LogFloatSlider>,
         Dataflow::WidgetInletFactory<Process::IntSlider>,
         Dataflow::WidgetInletFactory<Process::IntSpinBox>,
         Dataflow::WidgetInletFactory<Process::Toggle>,
         Dataflow::WidgetInletFactory<Process::Button>,
         Dataflow::WidgetInletFactory<Process::ChooserToggle>,
         Dataflow::WidgetInletFactory<Process::LineEdit>,
         Dataflow::WidgetInletFactory<Process::ComboBox>,
         Dataflow::WidgetInletFactory<Process::Enum>,
         Dataflow::WidgetInletFactory<Process::TimeSignatureChooser>>,
      FW<Execution::ProcessComponentFactory,
         Execution::ScenarioComponentFactory>,
      FW<Library::LibraryInterface,
         Scenario::SlotLibraryHandler,
         Scenario::ScenarioLibraryHandler>,
      FW<Scenario::IntervalResizer,
         Scenario::ScenarioIntervalResizer,
         Scenario::BaseScenarioIntervalResizer>,
      FW<score::ObjectRemover, Scenario::ScenarioRemover>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_scenario::make_commands()
{
  using namespace Scenario;
  using namespace Dataflow;
  using namespace Scenario::Command;
  using namespace Interpolation;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_scenario_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_scenario::guiFactories(
    const score::GUIApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Scenario::Command;
  return instantiate_factories<
      score::GUIApplicationContext,
      FW<Process::LayerFactory, ScenarioTemporalLayerFactory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_scenario)
