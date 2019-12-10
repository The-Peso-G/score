#pragma once
#include <Audio/PortAudioInterface.hpp>
#include <Audio/AudioInterface.hpp>

namespace Audio
{
#if __has_include(<pa_win_wdmks.h>)
class WDMKSFactory final : public QObject, public AudioFactory
{
  SCORE_CONCRETE("d98fca36-4e50-4802-a825-2fa213f95265")
public:
  std::vector<PortAudioCard> devices;

  WDMKSFactory() { rescan(); }

  ~WDMKSFactory() override {}

  void rescan()
  {
    devices.clear();
    PortAudioScope portaudio;

    devices.push_back(
        PortAudioCard{{}, {}, QObject::tr("No device"), -1, 0, 0, {}});
    for (int i = 0; i < Pa_GetHostApiCount(); i++)
    {
      auto hostapi = Pa_GetHostApiInfo(i);
      if (hostapi->type == PaHostApiTypeId::paWDMKS)
      {
        for (int card = 0; card < hostapi->deviceCount; card++)
        {
          auto dev_idx = Pa_HostApiDeviceIndexToDeviceIndex(i, card);
          auto dev = Pa_GetDeviceInfo(dev_idx);
          auto raw_name = QString::fromUtf8(Pa_GetDeviceInfo(dev_idx)->name);

          devices.push_back(PortAudioCard{"WDMKS",
                                          raw_name,
                                          raw_name,
                                          dev_idx,
                                          dev->maxInputChannels,
                                          dev->maxOutputChannels,
                                          hostapi->type});
        }
      }
    }
  }

  QString prettyName() const override { return QObject::tr("WDMKS"); }
  std::unique_ptr<ossia::audio_engine> make_engine(
      const Audio::Settings::Model& set,
      const score::ApplicationContext& ctx) override
  {
    return std::make_unique<ossia::portaudio_engine>(
        "ossia score",
        set.getCardIn().toStdString(),
        set.getCardOut().toStdString(),
        set.getDefaultIn(),
        set.getDefaultOut(),
        set.getRate(),
        set.getBufferSize());
  }

  void setCard(QComboBox* combo, QString val)
  {
    auto dev_it = ossia::find_if(
        devices, [&](const PortAudioCard& d) { return d.raw_name == val; });
    if (dev_it != devices.end())
    {
      combo->setCurrentIndex(dev_it->out_index);
    }
  }

  QWidget* make_settings(
      Audio::Settings::Model& m,
      Audio::Settings::View& v,
      score::SettingsCommandDispatcher& m_disp,
      QWidget* parent) override
  {
    auto w = new QWidget{parent};
    auto lay = new QFormLayout{w};

    auto card_list = new QComboBox{w};

    // Disabled case
    card_list->addItem(devices.front().pretty_name, 0);
    devices.front().out_index = 0;

    // Normal devices
    for (std::size_t i = 1; i < devices.size(); i++)
    {
      auto& card = devices[i];
      card_list->addItem(card.pretty_name, (int)i);
      card.out_index = card_list->count() - 1;
    }

    using Model = Audio::Settings::Model;

    {
      lay->addRow(QObject::tr("Device"), card_list);

      auto update_dev = [=, &m, &m_disp](const PortAudioCard& dev) {
        if (dev.raw_name != m.getCardOut())
        {
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardIn>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelCardOut>(
              m, dev.raw_name);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultIn>(
              m, dev.inputChan);
          m_disp.submitDeferredCommand<Audio::Settings::SetModelDefaultOut>(
              m, dev.outputChan);
        }
      };

      QObject::connect(
          card_list,
          SignalUtils::QComboBox_currentIndexChanged_int(),
          &v,
          [=](int i) {
            auto& device = devices[card_list->itemData(i).toInt()];
            update_dev(device);
          });

      if (m.getCardOut().isEmpty())
      {
        if (!devices.empty())
        {
          update_dev(devices.front());
        }
      }
      else
      {
        setCard(card_list, m.getCardOut());
      }
    }

    addBufferSizeWidget(*w, m, v);
    addSampleRateWidget(*w, m, v);

    con(m, &Model::changed, w, [=, &m] {
      setCard(card_list, m.getCardOut());
    });
    return w;
  }
};
#endif
}
