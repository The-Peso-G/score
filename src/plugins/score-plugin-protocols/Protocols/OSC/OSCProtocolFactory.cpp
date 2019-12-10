// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "OSCProtocolFactory.hpp"

#include "OSCDevice.hpp"

#include <Device/Protocol/DeviceSettings.hpp>
#include <Protocols/OSC/OSCProtocolSettingsWidget.hpp>
#include <Protocols/OSC/OSCSpecificSettings.hpp>

#include <ossia/network/base/device.hpp>

#include <QObject>

namespace Device
{
class DeviceInterface;
class ProtocolSettingsWidget;
}
struct VisitorVariant;

namespace Protocols
{
QString OSCProtocolFactory::prettyName() const
{
  return QObject::tr("OSC");
}

Device::DeviceInterface* OSCProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new OSCDevice{settings};
}

const Device::DeviceSettings& OSCProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "OSC";
    OSCSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* OSCProtocolFactory::makeSettingsWidget()
{
  return new OSCProtocolSettingsWidget;
}

QVariant OSCProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<OSCSpecificSettings>(visitor);
}

void OSCProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<OSCSpecificSettings>(data, visitor);
}

bool OSCProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const
{
  auto a_p = a.deviceSpecificSettings.value<OSCSpecificSettings>();
  auto b_p = b.deviceSpecificSettings.value<OSCSpecificSettings>();
  return a.name != b.name && a_p.inputPort != b_p.inputPort
         && a_p.outputPort != b_p.outputPort;
}
}
