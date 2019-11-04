// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "TimeSyncModel.hpp"

#include <Process/TimeValue.hpp>
#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <QJsonObject>


template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamReader::read(const Scenario::TimeSyncModel& timesync)
{
  m_stream << timesync.m_date << timesync.m_events
           << timesync.m_musicalSync
           << timesync.m_active << timesync.m_expression;

  insertDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::TimeSyncModel& timesync)
{
  m_stream >> timesync.m_date >> timesync.m_events
      >> timesync.m_musicalSync
      >> timesync.m_active >> timesync.m_expression;

  checkDelimiter();
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectReader::read(const Scenario::TimeSyncModel& timesync)
{
  obj[strings.Date] = toJsonValue(timesync.date());
  obj[strings.Events] = toJsonArray(timesync.m_events);
  obj["MusicalSync"] = timesync.m_musicalSync;
  obj[strings.AutoTrigger] = toJsonValue(timesync.m_autotrigger);

  QJsonObject trig;
  trig[strings.Active] = timesync.m_active;
  trig[strings.Expression] = toJsonObject(timesync.m_expression);
  obj[strings.Trigger] = trig;
}

template <>
SCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::write(Scenario::TimeSyncModel& timesync)
{
  timesync.m_date = fromJsonValue<TimeVal>(obj[strings.Date]);
  fromJsonValueArray(obj[strings.Events].toArray(), timesync.m_events);
  timesync.m_musicalSync = obj["MusicalSync"].toDouble();

  State::Expression t;
  const auto& trig_obj = obj[strings.Trigger].toObject();
  fromJsonObject(trig_obj[strings.Expression], t);
  timesync.m_expression = std::move(t);
  timesync.m_active = trig_obj[strings.Active].toBool();
  timesync.m_autotrigger = obj[strings.AutoTrigger].toBool();
}
