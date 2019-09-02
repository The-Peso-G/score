// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MappingExecution.hpp"

#include <Curve/CurveConversion.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionFunctions.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/nodes/mapping.hpp>
#include <ossia/editor/mapper/detail/mapper_visitor.hpp>
#include <QDebug>
namespace Mapping
{
namespace RecreateOnPlay
{
Component::Component(
    ::Mapping::ProcessModel& element,
    const ::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Execution::ProcessComponent_T<
          Mapping::ProcessModel,
          ossia::node_process>{element, ctx, id, "MappingElement", parent}
{
  node = std::make_shared<ossia::nodes::mapping>();
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(element,
      &Mapping::ProcessModel::sourceAddressChanged,
      this,
      [this](const auto&) { this->recompute(); });
  con(element,
      &Mapping::ProcessModel::sourceMinChanged,
      this,
      [this](const auto&) { this->recompute(); });
  con(element,
      &Mapping::ProcessModel::sourceMaxChanged,
      this,
      [this](const auto&) { this->recompute(); });

  con(element,
      &Mapping::ProcessModel::targetAddressChanged,
      this,
      [this](const auto&) { this->recompute(); });
  con(element,
      &Mapping::ProcessModel::targetMinChanged,
      this,
      [this](const auto&) { this->recompute(); });
  con(element,
      &Mapping::ProcessModel::targetMaxChanged,
      this,
      [this](const auto&) { this->recompute(); });

  con(element, &Mapping::ProcessModel::curveChanged, this, [this]() {
    this->recompute();
  });

  recompute();
}

Component::~Component() {}

void Component::recompute()
{
  const auto& devices = *system().execState;
  auto ossia_source_addr
      = Execution::makeDestination(devices, process().sourceAddress());
  auto ossia_target_addr
      = Execution::makeDestination(devices, process().targetAddress());

  std::shared_ptr<ossia::curve_abstract> curve;
  if (ossia_source_addr && ossia_target_addr)
  {
    auto sourceAddressType = ossia_source_addr->address().get_value_type();
    auto targetAddressType = ossia_target_addr->address().get_value_type();

    curve = rebuildCurve(
        sourceAddressType, targetAddressType); // If the type changes we need
                                               // to rebuild the curve.
  }
  else if (ossia_source_addr)
  {
    curve = rebuildCurve(
        ossia_source_addr->address().get_value_type(),
        ossia_source_addr->address().get_value_type());
  }
  else if (ossia_target_addr)
  {
    curve = rebuildCurve(
        ossia::val_type::FLOAT, ossia_target_addr->address().get_value_type());
  }
  else
  {
    curve = rebuildCurve(ossia::val_type::FLOAT, ossia::val_type::FLOAT);
  }

  if (curve)
  {
    std::function<void()> v
        = [proc = std::dynamic_pointer_cast<ossia::nodes::mapping>(
               OSSIAProcess().node),
           curve] { proc->set_behavior(std::move(curve)); };
    in_exec([fun = std::move(v)] { fun(); });
    return;
  }
}

template <typename X_T, typename Y_T>
std::shared_ptr<ossia::curve_abstract> Component::on_curveChanged_impl2()
{
  if (process().curve().segments().size() == 0)
    return {};

  const double xmin = process().sourceMin();
  const double xmax = process().sourceMax();

  const double ymin = process().targetMin();
  const double ymax = process().targetMax();

  auto scale_x = [=](double val) -> X_T { return val * (xmax - xmin) + xmin; };
  auto scale_y = [=](double val) -> Y_T { return val * (ymax - ymin) + ymin; };

  auto segt_data = process().curve().sortedSegments();
  if (segt_data.size() != 0)
  {
    return Engine::score_to_ossia::curve<X_T, Y_T>(
        scale_x, scale_y, segt_data, {});
  }
  else
  {
    return {};
  }
}

template <typename X_T>
std::shared_ptr<ossia::curve_abstract>
Component::on_curveChanged_impl(ossia::val_type target)
{
  switch (target)
  {
    case ossia::val_type::INT:
      return on_curveChanged_impl2<X_T, int>();
      break;
    case ossia::val_type::FLOAT:
    case ossia::val_type::LIST:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return on_curveChanged_impl2<X_T, float>();
      break;
    default:
      qDebug() << "Unsupported target address type: " << (int)target;
      SCORE_TODO;
  }

  return {};
}

std::shared_ptr<ossia::curve_abstract>
Component::rebuildCurve(ossia::val_type source, ossia::val_type target)
{
  switch (source)
  {
    case ossia::val_type::INT:
      return on_curveChanged_impl<int>(target);
    case ossia::val_type::FLOAT:
    case ossia::val_type::LIST:
    case ossia::val_type::VEC2F:
    case ossia::val_type::VEC3F:
    case ossia::val_type::VEC4F:
      return on_curveChanged_impl<float>(target);
    default:
      qDebug() << "Unsupported source address type: " << (int)source;
      SCORE_TODO;
  }

  return {};
}
}
}
