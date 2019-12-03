#include <Process/Dataflow/PortFactory.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Magnetism/MagnetismAdjuster.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

#include <score_lib_process.hpp>
#include <score_lib_process_commands_files.hpp>
namespace Process
{
DataflowManager::DataflowManager()
{
}

DataflowManager::~DataflowManager()
{

}
}
score_lib_process::score_lib_process()
{
  qRegisterMetaType<Process::pan_weight>();
  qRegisterMetaTypeStreamOperators<Process::pan_weight>();
}
score_lib_process::~score_lib_process() = default;

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_lib_process::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase,
      Process::ProcessFactoryList,
      Process::PortFactoryList,
      Process::LayerFactoryList,
      Process::ProcessFactoryList,
      Process::ProcessDropHandlerList,
      Process::MagnetismAdjuster
      >();
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_lib_process::make_commands()
{
  using namespace Process;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_lib_process_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_lib_process)
