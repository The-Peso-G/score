#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <ossia/network/midi/midi.hpp>

#include <verdigris>

class QComboBox;
class QCheckBox;
class QRadioButton;
class QWidget;
class QLineEdit;

namespace Protocols
{
class MIDIProtocolSettingsWidget final : public Device::ProtocolSettingsWidget
{
  W_OBJECT(MIDIProtocolSettingsWidget)

public:
  MIDIProtocolSettingsWidget(QWidget* parent = nullptr);

private:
  Device::DeviceSettings getSettings() const override;

  void setSettings(const Device::DeviceSettings& settings) override;

  void updateDevices(ossia::net::midi::midi_info::Type);
  void updateInputDevices();
  void updateOutputDevices();

  QLineEdit* m_name{};
  QCheckBox* m_inButton{};
  QCheckBox* m_outButton{};
  QComboBox* m_deviceCBox{};
  QCheckBox* m_createWhole{};
};
}
