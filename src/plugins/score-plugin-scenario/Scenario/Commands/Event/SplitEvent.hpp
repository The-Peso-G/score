#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>
#include <QVector>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;

namespace Command
{
class SplitEvent final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      SplitEvent,
      "Split an event")

public:
  SplitEvent(
      const Scenario::ProcessModel& scenario,
      Id<EventModel> event,
      QVector<Id<StateModel>> movingstates);
  SplitEvent(
      const Scenario::ProcessModel& scenario,
      Id<EventModel> event,
      Id<EventModel> new_event,
      QVector<Id<StateModel>> movingstates);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Id<EventModel>& newEvent() const { return m_newEvent; }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_scenarioPath;

  Id<EventModel> m_originalEvent;
  Id<EventModel> m_newEvent;
  QString m_createdName;
  QVector<Id<StateModel>> m_movingStates;
};

class SCORE_PLUGIN_SCENARIO_EXPORT SplitWholeEvent final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      SplitWholeEvent,
      "Split an event")
public:
  SplitWholeEvent(const EventModel& path);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<EventModel> m_path;

  Id<EventModel> m_originalEvent;
  std::vector<Id<EventModel>> m_newEvents;
};

class SplitStateMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      SplitStateMacro,
      "Split state from node")
};
}
}
