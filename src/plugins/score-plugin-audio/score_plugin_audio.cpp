// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "score_plugin_audio.hpp"

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>


#include <Audio/AudioPanel.hpp>
#include <Audio/DummyInterface.hpp>
#include <Audio/JackInterface.hpp>
#include <Audio/PortAudioInterface.hpp>
#include <Audio/ALSAPortAudioInterface.hpp>
#include <Audio/ASIOPortAudioInterface.hpp>
#include <Audio/CoreAudioPortAudioInterface.hpp>
#include <Audio/GenericPortAudioInterface.hpp>
#include <Audio/MMEPortAudioInterface.hpp>
#include <Audio/WASAPIPortAudioInterface.hpp>
#include <Audio/WDMKSPortAudioInterface.hpp>
#include <Audio/SDLInterface.hpp>
#include <Audio/Settings/Factory.hpp>
#include <Audio/AudioApplicationPlugin.hpp>

#include <ossia-config.hpp>
#include <wobjectimpl.h>

score_plugin_engine::score_plugin_engine()
{
  qRegisterMetaType<Audio::AudioFactory::ConcreteKey>("AudioKey");
  qRegisterMetaTypeStreamOperators<Audio::AudioFactory::ConcreteKey>(
      "AudioKey");
}

score_plugin_engine::~score_plugin_engine() {}

score::GUIApplicationPlugin* score_plugin_engine::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Engine::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_engine::factoryFamilies()
{
  return make_ptr_vector<Audio::AudioFactoryList>();
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_engine::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Audio;
  return instantiate_factories<
      score::ApplicationContext,
      FW<Audio::AudioFactory,
         Audio::DummyFactory
#if defined(OSSIA_AUDIO_JACK)
         ,
         Audio::JackFactory
#endif
#if defined(OSSIA_AUDIO_PORTAUDIO)
#if __has_include(<pa_asio.h>)
         ,
         Audio::ASIOFactory
#endif
#if __has_include(<pa_win_wdmks.h>)
         ,
         Audio::WDMKSFactory
#endif
#if __has_include(<pa_win_wasapi.h>)
         ,
         Audio::WASAPIFactory
#endif
#if __has_include(<pa_win_wmme.h>)
         ,
         Audio::MMEFactory
#endif
#if __has_include(<pa_linux_alsa.h>)
         ,
         Audio::ALSAFactory
#endif
#if __has_include(<pa_mac_core.h>)
         ,
         Audio::CoreAudioFactory
#endif
#if !__has_include(<pa_asio.h>) && \
    !__has_include(<pa_win_wdmks.h>) && \
    !__has_include(<pa_win_wasapi.h>) && \
    !__has_include(<pa_win_wmme.h>) && \
    !__has_include(<pa_linux_alsa.h>) && \
    !__has_include(<pa_mac_core.h>)
         ,
         Audio::PortAudioFactory
#endif
#endif
#if defined(OSSIA_AUDIO_SDL)
         ,
         Audio::SDLFactory
#endif
         >
      FW<score::SettingsDelegateFactory,
         Audio::Settings::Factory>,
      FW<score::PanelDelegateFactory, Audio::PanelDelegateFactory>
      >(ctx, key);
}

auto score_plugin_engine::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_scenario::static_key(),
          score_plugin_deviceexplorer::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_engine)
