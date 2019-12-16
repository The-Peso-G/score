#include "score_plugin_dataflow.hpp"
#include <Dataflow/PortItem.hpp>
#include <Dataflow/AudioInletItem.hpp>
#include <Dataflow/AudioOutletItem.hpp>
#include <Dataflow/ControlInletItem.hpp>
#include <Dataflow/ControlOutletItem.hpp>
#include <Dataflow/MidiInletItem.hpp>
#include <Dataflow/MidiOutletItem.hpp>
#include <Dataflow/PortInspectorFactory.hpp>
#include <Dataflow/WidgetInletFactory.hpp>
#include <Dataflow/DropPortInScenario.hpp>
#include <Dataflow/CableInspector.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <score/plugins/FactorySetup.hpp>

score_plugin_dataflow::score_plugin_dataflow()
{
}

score_plugin_dataflow::~score_plugin_dataflow()
{
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_dataflow::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Inspector::InspectorWidgetFactory,
         Dataflow::CableInspectorFactory,
         Dataflow::PortInspectorFactory>,
      FW<Scenario::DropHandler,
         Dataflow::DropPortInScenario>,
      FW<Process::PortFactory,
          Dataflow::ControlInletFactory,
          Dataflow::MidiInletFactory,
          Dataflow::MidiOutletFactory,
          Dataflow::AudioInletFactory,
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
          Dataflow::WidgetInletFactory<Process::TimeSignatureChooser>
      >
      >(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_dataflow)
