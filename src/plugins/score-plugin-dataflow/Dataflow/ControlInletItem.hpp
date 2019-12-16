#pragma once
#include <Dataflow/PortItem.hpp>

namespace Dataflow
{
struct SCORE_PLUGIN_DATAFLOW_EXPORT ControlInletFactory final : public AutomatablePortFactory
{
  using Model_T = Process::ControlInlet;
  UuidKey<Process::Port> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }

  void setupInletInspector(
      Process::Inlet& port,
      const score::DocumentContext& ctx,
      QWidget* parent,
      Inspector::Layout& lay,
      QObject* context) override;
};
}
