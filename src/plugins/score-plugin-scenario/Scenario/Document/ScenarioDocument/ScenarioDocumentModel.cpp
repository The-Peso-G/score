// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ScenarioDocumentModel.hpp"
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/model/IdentifierDebug.hpp>
#include <QFileInfo>
#include <QDebug>
#include <wobjectimpl.h>


W_OBJECT_IMPL(Scenario::ScenarioDocumentModel)
namespace Process
{
class LayerPresenter;
}
namespace Scenario
{
ScenarioDocumentModel::ScenarioDocumentModel(
    const score::DocumentContext& ctx,
    QObject* parent)
    : score::DocumentDelegateModel{Id<score::DocumentDelegateModel>(
                                       score::id_generator::getFirstId()),
                                   "Scenario::ScenarioDocumentModel",
                                   parent}
    , m_context{ctx}
    , m_baseScenario{new BaseScenario{Id<BaseScenario>{0}, this}}
{
  auto dur
      = ctx.app.settings<Scenario::Settings::Model>().getDefaultDuration();

  m_baseScenario->interval().duration.setRigid(false);
  IntervalDurations::Algorithms::changeAllDurations(
      m_baseScenario->interval(), dur);
  m_baseScenario->interval().duration.setMaxInfinite(true);
  m_baseScenario->endEvent().setDate(
      m_baseScenario->interval().duration.defaultDuration());
  m_baseScenario->endTimeSync().setDate(
      m_baseScenario->interval().duration.defaultDuration());

  auto& doc_metadata
      = score::IDocument::documentContext(*parent).document.metadata();
  m_baseScenario->interval().metadata().setName(doc_metadata.fileName());

  connect(
      &doc_metadata,
      &score::DocumentMetadata::fileNameChanged,
      this,
      [&](const QString& newName) {
        QFileInfo info(newName);

        m_baseScenario->interval().metadata().setName(info.baseName());
      });

  using namespace Scenario::Command;

  AddOnlyProcessToInterval cmd1{
      m_baseScenario->interval(),
      Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(),
      QString{}};
  cmd1.redo(ctx);
  m_baseScenario->interval().processes.begin()->setSlotHeight(1500);

  // Select the first state
  score::SelectionDispatcher d{ctx.selectionStack};
  auto scenar = qobject_cast<Scenario::ProcessModel*>(
      &*m_baseScenario->interval().processes.begin());
  if (scenar)
    d.setAndCommit({&scenar->startEvent()});
}

void ScenarioDocumentModel::finishLoading()
{
  const auto& cbl = m_savedCables;
  for (const auto& json_vref : cbl)
  {
    auto cbl = new Process::Cable{
        JSONObject::Deserializer{json_vref.toObject()}, this};
    auto src = cbl->source().try_find(m_context);
    auto snk = cbl->sink().try_find(m_context);
    if (src && snk)
    {
      src->addCable(*cbl);
      snk->addCable(*cbl);

      cables.add(cbl);
    }
    else
    {
      qWarning() << "Could not find either source or sink for cable "
                 << cbl->id() << src << snk;
      delete cbl;
    }
  }
  m_savedCables = QJsonArray{};
}

ScenarioDocumentModel::~ScenarioDocumentModel() {}

void ScenarioDocumentModel::initializeNewDocument(
    const IntervalModel& Interval_model)
{
}

IntervalModel& ScenarioDocumentModel::baseInterval() const
{
  return m_baseScenario->interval();
}
}
