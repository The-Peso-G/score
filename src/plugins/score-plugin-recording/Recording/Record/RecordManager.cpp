// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationProcessMetadata.hpp>
#include <Automation/Commands/InitAutomation.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/ListeningManager.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Recording/Commands/Record.hpp>
#include <Recording/Record/RecordAutomations/RecordAutomationCreationVisitor.hpp>
#include <Recording/Record/RecordAutomations/RecordAutomationFirstParameterCallbackVisitor.hpp>
#include <Recording/Record/RecordAutomations/RecordAutomationParameterCallbackVisitor.hpp>
#include <Recording/Record/RecordData.hpp>
#include <Recording/Record/RecordManager.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QApplication>
#include <qnamespace.h>

#include <utility>

#include <type_traits>
namespace Curve
{
class SegmentModel;
}

#include <wobjectimpl.h>
W_OBJECT_IMPL(Recording::AutomationRecorder)
namespace Recording
{
AutomationRecorder::AutomationRecorder(RecordContext& ctx)
    : context{ctx}
    , m_settings{context.context.app.settings<Curve::Settings::Model>()}
{
}

bool AutomationRecorder::setup(
    const Box& box,
    const RecordListening& recordListening)
{
  std::vector<std::vector<State::Address>> addresses;
  //// Creation of the curves ////
  for (const auto& vec : recordListening)
  {
    addresses.push_back({Device::address(*vec.front()).address});
    addresses.back().reserve(vec.size());

    for (Device::Node* node : vec)
    {
      Device::AddressSettings& addr = node->get<Device::AddressSettings>();
      addr.value.apply(
          RecordAutomationCreationVisitor{*node, box, addr, addresses, *this});
    }
  }

  const auto& devicelist = context.explorer.deviceModel().list();

  //// Setup listening on the curves ////
  const auto curve_mode = m_settings.getCurveMode();
  m_recordingMode = curve_mode;
  int i = 0;
  for (const auto& vec : recordListening)
  {
    auto& dev = devicelist.device(*vec.front());
    if (!dev.connected())
      continue;

    dev.addToListening(addresses[i]);
    // Add a custom callback.
    if (curve_mode == Curve::Settings::Mode::Parameter)
    {
      dev.valueUpdated.connect<&AutomationRecorder::parameterCallback>(*this);
    }
    else
    {
      dev.valueUpdated.connect<&AutomationRecorder::messageCallback>(*this);
    }

    m_recordCallbackConnections.push_back(&dev);

    i++;
  }

  return true;
}

void AutomationRecorder::stop()
{
  // Stop all the recording machinery
  auto msecs = context.time();
  const auto curve_mode = m_recordingMode;
  for (const auto& dev : m_recordCallbackConnections)
  {
    if (dev)
    {
      if (curve_mode == Curve::Settings::Mode::Parameter)
      {
        dev->valueUpdated.disconnect<&AutomationRecorder::parameterCallback>(
            *this);
      }
      else
      {
        dev->valueUpdated.disconnect<&AutomationRecorder::messageCallback>(
            *this);
      }
    }
  }
  m_recordCallbackConnections.clear();

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // Record and then stop
  if (!context.started())
  {
    context.dispatcher.rollback();
    return;
  }

  auto simplify = m_settings.getSimplify();
  auto simplifyRatio = m_settings.getSimplificationRatio();
  // Add a last point corresponding to the current state

  // Create commands for the state of each automation to send on
  // the network, and push them silently.

  auto make_address = [](State::Address a,
                         uint8_t i,
                         ossia::unit_t u) -> State::AddressAccessor {
    return State::AddressAccessor{std::move(a), {i}, u};
  };

  int N = 0;

  // Potentially simplify curve and transform it in segments
  for (const auto& recorded : numeric_records)
  {
    if (finish(
            State::AddressAccessor{recorded.first, {}, recorded.second.unit},
            recorded.second,
            msecs,
            simplify,
            simplifyRatio))
      N++;
  }

  for (const auto& recorded : vec2_records)
  {
    for (int i = 0; i < 2; i++)
      if (finish(
              make_address(recorded.first, i, recorded.second[i].unit),
              recorded.second[i],
              msecs,
              simplify,
              simplifyRatio))
        N++;
  }

  for (const auto& recorded : vec3_records)
  {
    for (int i = 0; i < 3; i++)
      if (finish(
              make_address(recorded.first, i, recorded.second[i].unit),
              recorded.second[i],
              msecs,
              simplify,
              simplifyRatio))
        N++;
  }

  for (const auto& recorded : vec4_records)
  {
    for (int i = 0; i < 4; i++)
      if (finish(
              make_address(recorded.first, i, recorded.second[i].unit),
              recorded.second[i],
              msecs,
              simplify,
              simplifyRatio))
        N++;
  }

  if (N == 0)
  {
    context.dispatcher.rollback();
  }
}

void AutomationRecorder::messageCallback(
    const State::Address& addr,
    const ossia::value& val)
{
  using namespace std::chrono;
  if (context.started())
  {
    val.apply(RecordAutomationSubsequentCallbackVisitor<MessagePolicy>{
        *this, addr, context.time()});
  }
  else
  {
    firstMessageReceived();
    context.start();
    val.apply(RecordAutomationFirstCallbackVisitor{*this, addr});
  }
}

void AutomationRecorder::parameterCallback(
    const State::Address& addr,
    const ossia::value& val)
{
  using namespace std::chrono;
  if (context.started())
  {
    val.apply(RecordAutomationSubsequentCallbackVisitor<ParameterPolicy>{
        *this, addr, context.time()});
  }
  else
  {
    firstMessageReceived();
    context.start();
    val.apply(RecordAutomationFirstCallbackVisitor{*this, addr});
  }
}

bool AutomationRecorder::finish(
    State::AddressAccessor addr,
    const RecordData& recorded,
    const TimeVal& msecs,
    bool simplify,
    int simplifyRatio)
{
  Curve::PointArraySegment& segt = recorded.segment;
  if (segt.points().empty()
      || (segt.points().size() == 1 && segt.points().begin()->first == 0.))
  {
    recorded.addLayCmd->undo(context.context);
    delete recorded.addLayCmd;
    recorded.addProcCmd->undo(context.context);
    delete recorded.addProcCmd;
    return false;
  }

  auto& automation
      = *safe_cast<Automation::ProcessModel*>(recorded.curveModel.parent());

  // Here we add a last point equal to the latest recorded point
  {
    // Add last point
    segt.addPoint(msecs.msec(), segt.points().rbegin()->second);

    automation.setDuration(msecs);
  }

  // Conversion of the piecewise to segments, and
  // serialization.
  if (simplify)
    recorded.segment.simplify(simplifyRatio);

  // TODO if there is no remaining segment or an invalid segment, don't add it.

  // Add a point with the last state.
  auto initCurveCmd
      = new Automation::InitAutomation{automation,
                                       std::move(addr),
                                       recorded.segment.min(),
                                       recorded.segment.max(),
                                       recorded.segment.toPowerSegments()};

  // This one shall not be redone
  context.dispatcher.submit(recorded.addProcCmd);
  context.dispatcher.submit(recorded.addLayCmd);
  context.dispatcher.submit(initCurveCmd);

  return true;
}

/*
Priority AutomationRecorderFactory::matches(
    const Device::Node& n, const score::DocumentContext& ctx)
{
  return 2;
}

std::unique_ptr<RecordProvider> AutomationRecorderFactory::make(
    const Device::NodeList&, const score::DocumentContext& ctx)
{
  return {};
}
*/
}
