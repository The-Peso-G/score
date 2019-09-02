// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DisplayedElementsModel.hpp"

#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/selection/Selection.hpp>

#include <ossia/detail/algorithms.hpp>



namespace Scenario
{
void DisplayedElementsModel::setSelection(const Selection& s)
{
  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    elt->selection.set(s.contains(elt.data())); // OPTIMIZEME
  });
}

void DisplayedElementsModel::setDisplayedElements(
    DisplayedElementsContainer&& elts)
{
  m_elements = std::move(elts);
  m_initialized = true;
}

IntervalModel& DisplayedElementsModel::interval() const
{
  return *m_elements.interval;
}

const TimeSyncModel& DisplayedElementsModel::startTimeSync() const
{
  return *m_elements.startNode;
}

const TimeSyncModel& DisplayedElementsModel::endTimeSync() const
{
  return *m_elements.endNode;
}

const EventModel& DisplayedElementsModel::startEvent() const
{
  return *m_elements.startEvent;
}

const EventModel& DisplayedElementsModel::endEvent() const
{
  return *m_elements.endEvent;
}

const StateModel& DisplayedElementsModel::startState() const
{
  return *m_elements.startState;
}

const StateModel& DisplayedElementsModel::endState() const
{
  return *m_elements.endState;
}
}
