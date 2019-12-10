#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <score/model/ObjectRemover.hpp>

#include <Nodal/Metadata.hpp>

namespace Nodal
{

class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Nodal::Model)
  W_OBJECT(Model)

public:
  std::unique_ptr<Process::AudioInlet> inlet;
  std::unique_ptr<Process::AudioOutlet> outlet;

  Model(
      const TimeVal& duration, const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    m_inlets.push_back(inlet.get());
    m_outlets.push_back(outlet.get());
  }

  ~Model() override;

  score::EntityMap<Process::ProcessModel> nodes;

  void resetExecution()
  W_SIGNAL(resetExecution)

private:
  QString prettyName() const noexcept override;

  void setDurationAndScale(const TimeVal& newDuration) noexcept override;
  void setDurationAndGrow(const TimeVal& newDuration) noexcept override;
  void setDurationAndShrink(const TimeVal& newDuration) noexcept override;

  void startExecution() override;
  void stopExecution() override;
  void reset() override;
};

using ProcessFactory = Process::ProcessFactory_T<Nodal::Model>;

class NodeRemover : public score::ObjectRemover
{
  SCORE_CONCRETE("5e1c7e92-5beb-4313-92c8-f690089ff340")
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};
}
