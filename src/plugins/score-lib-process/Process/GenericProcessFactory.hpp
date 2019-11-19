#pragma once
#include <Process/ProcessFactory.hpp>

#include <score/serialization/VisitorCommon.hpp>

namespace Process
{
struct default_t
{
};

template <typename Model_T>
class ProcessFactory_T final : public Process::ProcessModelFactory
{
public:
  virtual ~ProcessFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }
  QString prettyName() const override
  {
    return Metadata<PrettyName_k, Model_T>::get();
  }
  QString category() const override
  {
    return Metadata<Category_k, Model_T>::get();
  }
  Descriptor descriptor(QString) const override
  {
    return Metadata<Process::Descriptor_k, Model_T>::get();
  }
  ProcessFlags flags() const override
  {
    return Metadata<ProcessFlags_k, Model_T>::get();
  }

  Model_T* make(
      const TimeVal& duration,
      const QString& data,
      const Id<Process::ProcessModel>& id,
      QObject* parent) final override;

  Model_T* load(const VisitorVariant& vis, QObject* parent) final override;
};

template <typename Model_T>
Model_T* ProcessFactory_T<Model_T>::make(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
{
  if constexpr (std::is_constructible_v<
                    Model_T,
                    TimeVal,
                    QString,
                    Id<Process::ProcessModel>,
                    QObject*>)
    return new Model_T{duration, data, id, parent};
  else
    return new Model_T{duration, id, parent};
}

template <typename Model_T>
Model_T*
ProcessFactory_T<Model_T>::load(const VisitorVariant& vis, QObject* parent)
{
  return score::deserialize_dyn(vis, [&](auto&& deserializer) {
    return new Model_T{deserializer, parent};
  });
}

template <
    typename Model_T,
    typename LayerPresenter_T,
    typename LayerView_T,
    typename HeaderDelegate_T = default_t>
class LayerFactory_T final : public Process::LayerFactory
{
public:
  virtual ~LayerFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  LayerView_T* makeLayerView(
      const Process::ProcessModel& viewmodel,
      const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    if constexpr (std::is_constructible_v<
                      LayerView_T,
                      const Model_T&,
                      QGraphicsItem*>)
      return new LayerView_T{safe_cast<const Model_T&>(viewmodel), parent};
    else
      return new LayerView_T{parent};
  }

  LayerPresenter_T* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::Context& context,
      QObject* parent) const final override
  {
    return new LayerPresenter_T{safe_cast<const Model_T&>(lm),
                                safe_cast<LayerView_T*>(v),
                                context,
                                parent};
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }

  HeaderDelegate*
  makeHeaderDelegate(
      const ProcessModel& model,
      const Process::Context& ctx,
      const LayerPresenter* pres) const override
  {
    if constexpr (std::is_same_v<HeaderDelegate_T, default_t>)
      return LayerFactory::makeHeaderDelegate(model, ctx, pres);
    else
      return new HeaderDelegate_T{model, ctx, safe_cast<const LayerPresenter_T*>(&pres)};
  }
};

template <typename Model_T>
class LayerFactory_T<Model_T, default_t, default_t, default_t>
    : // final :
      public Process::LayerFactory
{
public:
  virtual ~LayerFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};

template <typename Model_T>
using GenericDefaultLayerFactory
    = LayerFactory_T<Model_T, default_t, default_t, default_t>;
}
