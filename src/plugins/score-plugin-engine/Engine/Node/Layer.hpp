#pragma once
#include <Dataflow/UI/PortItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Effect/EffectLayout.hpp>

#include <score/graphics/RectItem.hpp>
#include <score/graphics/TextItem.hpp>


#include <Control/Widgets.hpp>
#include <Effect/EffectLayer.hpp>
#include <Engine/Node/Process.hpp>
#include <type_traits>

namespace Control
{
template <typename T, typename = void>
struct HasCustomUI : std::false_type
{
};
template <typename T>
struct HasCustomUI<T, std::void_t<decltype(&T::item)>> : std::true_type
{
};
template <typename T, typename = void>
struct HasCustomLayer : std::false_type
{
};
template <typename T>
struct HasCustomLayer<T, std::void_t<typename T::Layer>> : std::true_type
{
};

template<typename Info>
struct CustomUISetup
{
  const Process::Inlets& inlets;
  const Process::ProcessModel& process;
  QGraphicsItem& parent;
  QObject& context;
  const Process::Context& doc;

  template <std::size_t N>
  auto& getControl() noexcept
  {
    constexpr int i = ossia::safe_nodes::info_functions<Info>::control_start + N;
    constexpr const auto& ctrl = std::get<N>(Info::Metadata::controls);
    using port_type = typename std::remove_reference_t<decltype(ctrl)>::port_type;
    return static_cast<port_type&>(*inlets[i]);
  }

  template <std::size_t... C>
  void make(const std::index_sequence<C...>&) noexcept
  {
    Info::item(getControl<C>()..., process, parent, context, doc);
  }

  CustomUISetup(
      const Process::Inlets& inlets,
      const Process::ProcessModel& process,
      QGraphicsItem& parent,
      QObject& context,
      const Process::Context& doc)
    : inlets{inlets}, process{process}, parent{parent}, context{context}, doc{doc}
  {
    make(std::make_index_sequence<ossia::safe_nodes::info_functions<Info>::control_count>{});
  }
};

struct AutoUISetup
{
  const Process::Inlets& inlets;
  QGraphicsItem& parent;
  QObject& context;
  const Process::Context& doc;
  const Process::PortFactoryList& portFactory = doc.app.interfaces<Process::PortFactoryList>();

  std::size_t i = 0;
  std::size_t ctl_i = 0;

  std::vector<QRectF> controlRects;
  template <typename Info>
  AutoUISetup(
      Info&&,
      const Process::Inlets& inlets,
      QGraphicsItem& parent,
      QObject& context,
      const Process::Context& doc)
    : inlets{inlets}, parent{parent}, context{context}, doc{doc}
  {
    i = ossia::safe_nodes::info_functions<Info>::control_start;
    ossia::for_each_in_tuple(Info::Metadata::controls, *this);
  }

  // Create a single control
  template<typename T>
  void operator()(const T& ctrl)
  {
    auto inlet = static_cast<Process::ControlInlet*>(inlets[i]);
    auto csetup = Process::controlSetup(
     [] (auto& factory, auto& inlet, const auto& doc, auto item, auto parent)
     { return factory.makeItem(inlet, doc, item, parent); },
     [&] (auto& factory, auto& inlet, const auto& doc, auto item, auto parent)
     { return ctrl.make_item(ctrl, inlet, doc, item, parent); },
     [&] (int j) { return controlRects[j].size(); },
    [&] { return ctrl.name; }
    );
    auto res = Process::createControl(ctl_i, csetup, *inlet, portFactory, doc, &parent, &context);
    controlRects.push_back(res.itemRect);
    i++;
    ctl_i++;
  }
};

template <typename Info>
class ControlLayerFactory final : public Process::LayerFactory
{
public:
  virtual ~ControlLayerFactory() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, ControlProcess<Info>>::get();
  }

  Process::LayerView* makeLayerView(
      const Process::ProcessModel& proc,
      const Process::Context& context,
      QGraphicsItem* parent) const final override
  {
    if constexpr (HasCustomLayer<Info>::value)
        return new typename Info::Layer{proc, context, parent};
    else
        return new Process::EffectLayerView{parent};
  }

  Process::LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm,
      Process::LayerView* v,
      const Process::Context& context,
      QObject* parent) const final override
  {
    if constexpr (HasCustomLayer<Info>::value)
    {
      auto view = static_cast<typename Info::Layer*>(v);
      return new Process::EffectLayerPresenter{lm, view, context, parent};
    }
    else
    {
      auto view = safe_cast<Process::EffectLayerView*>(v);
      auto pres = new Process::EffectLayerPresenter{lm, view, context, parent};

      if constexpr (HasCustomUI<Info>::value)
      {
        Control::CustomUISetup<Info>{lm.inlets(), lm, *view, *view, context};
      }
      else if constexpr (ossia::safe_nodes::info_functions<Info>::control_count > 0)
      {
        Control::AutoUISetup{Info{}, lm.inlets(), *view, *view, context};
      }

      return pres;
    }
  }

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc,
      const Process::Context& ctx,
      QGraphicsItem* parent) const final override
  {
    auto rootItem = new score::EmptyRectItem{parent};

    if constexpr (HasCustomUI<Info>::value)
    {
      Control::CustomUISetup<Info>{proc.inlets(), proc, *rootItem, *rootItem, ctx};
    }
    else if constexpr (ossia::safe_nodes::info_functions<Info>::control_count > 0)
    {
      Control::AutoUISetup{Info{}, proc.inlets(), *rootItem, *rootItem, ctx};
    }

    rootItem->setRect(rootItem->childrenBoundingRect());
    return rootItem;
  }
};
}
