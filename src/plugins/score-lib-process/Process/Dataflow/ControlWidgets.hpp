#pragma once
#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/TimeSignature.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/GraphicsItem.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/tools/Unused.hpp>

#include <ossia/network/value/value_conversion.hpp>
#include <ossia/detail/math.hpp>
#include <QGraphicsItem>
#include <QGraphicsProxyWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <ossia/detail/algorithms.hpp>
#include <score_lib_process_export.h>

namespace WidgetFactory
{
inline QGraphicsItem* wrapWidget(QWidget* widg)
{
  if(widg->maximumWidth() > 130 || widg->maximumWidth() == 0)
    widg->setMaximumWidth(130);
  widg->setContentsMargins(0, 0, 0, 0);
  widg->setPalette(score::transparentPalette());
  widg->setAutoFillBackground(false);
  widg->setStyleSheet(score::transparentStylesheet());

  auto wrap = new QGraphicsProxyWidget{};
  wrap->setWidget(widg);
  wrap->setContentsMargins(0, 0, 0, 0);
  return wrap;
}
template<typename T>
using SetControlValue =
typename std::conditional<
  std::is_base_of<Process::ControlInlet, T>::value,
  Process::SetControlValue,
  Process::SetControlOutletValue
>::type;

template<typename ControlUI>
struct FloatControl
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;
    auto sl = new score::ValueDoubleSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->min = min;
    sl->max = max;
    sl->setValue((ossia::convert<double>(inlet.value()) - min) / (max - min));

    QObject::connect(
        sl,
        &score::DoubleSlider::sliderMoved,
        context,
        [=, &inlet, &ctx](int v) {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, min + (v / score::DoubleSlider::max) * (max - min));
        });
    QObject::connect(
        sl,
        &score::DoubleSlider::sliderReleased,
        context,
        [=, &inlet, &ctx]() {
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet,
              min
                  + (((QSlider*)sl)->value() / score::DoubleSlider::max)
                        * (max - min));
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](ossia::value val) {
          if (!sl->moving)
            sl->setValue((ossia::convert<double>(val) - min) / (max - min));
        });

    return sl;
  }

  template <typename T, typename Control_T>
  static auto make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;
    auto sl = new ControlUI{nullptr};
    sl->min = min;
    sl->max = max;
    sl->setValue((ossia::convert<double>(inlet.value()) - min) / (max - min));

    QObject::connect(
        sl, &ControlUI::sliderMoved, context, [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, min + sl->value() * (max - min));
        });
    QObject::connect(
        sl, &ControlUI::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](ossia::value val) {
          if (!sl->moving)
            sl->setValue((ossia::convert<double>(val) - min) / (max - min));
        });

    return sl;
  }
};

template<typename ControlUI>
struct LogFloatControl
{
  static_assert(std::numeric_limits<float>::is_iec559, "IEEE 754 required");

  static float to01(float min, float range, float val)
  {
    return ossia::log_to_normalized(min, range, val);
  }

  static float from01(float min, float range, float val)
  {
    return ossia::normalized_to_log(min, range, val);
  }

  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if(max == min)
      max += 1;

    auto range = max - min;
    auto sl = new score::ValueLogDoubleSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->min = min;
    sl->max = max;
    sl->setValue(to01(min, range, ossia::convert<double>(inlet.value())));

    QObject::connect(
        sl,
        &score::DoubleSlider::sliderMoved,
        context,
        [=, &inlet, &ctx](int v) {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, from01(min, range, v / score::DoubleSlider::max));
        });
    QObject::connect(
        sl, &score::DoubleSlider::sliderReleased, context, [=, &inlet, &ctx] {
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet,
              from01(
                  min,
                  range,
                  ((QSlider*)sl)->value() / score::DoubleSlider::max));
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](ossia::value val) {
          if (!sl->moving)
            sl->setValue(to01(min, max, ossia::convert<double>(val)));
        });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if(max == min)
      max += 1;

    auto range = max - min;

    auto sl = new ControlUI{nullptr};
    sl->min = min;
    sl->max = max;
    sl->setValue(to01(min, range, ossia::convert<double>(inlet.value())));

    QObject::connect(
        sl,
        &ControlUI::sliderMoved,
        context,
        [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, from01(min, range, sl->value()));
        });
    QObject::connect(
        sl, &ControlUI::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](ossia::value val) {
          if (!sl->moving)
            sl->setValue(to01(min, range, ossia::convert<double>(val)));
        });

    return sl;
  }
};

struct IntSlider
{

  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;
    auto sl = new score::ValueSlider{parent};
    sl->setOrientation(Qt::Horizontal);
    sl->setRange(min, max);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, &QSlider::sliderMoved, context, [sl, &inlet, &ctx](int p) {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, p);
        });
    QObject::connect(
        sl, &QSlider::sliderReleased, context, [sl, &inlet, &ctx] {
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [sl](ossia::value val) {
          if (!sl->moving)
            sl->setValue(ossia::convert<int>(val));
        });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;

    auto sl = new score::QGraphicsIntSlider{nullptr};
    sl->setRange(min, max);
    sl->setValue(ossia::convert<int>(inlet.value()));

    QObject::connect(
        sl,
        &score::QGraphicsIntSlider::sliderMoved,
        context,
        [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsIntSlider::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](ossia::value val) {
          if (!sl->moving)
            sl->setValue(ossia::convert<int>(val));
        });

    return sl;
  }
};

struct IntSpinBox
{

  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;

    auto sl = new QSpinBox{parent};
    sl->setRange(min, max);
    sl->setValue(ossia::convert<int>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl,
        SignalUtils::QSpinBox_valueChanged_int(),
        context,
        [&inlet, &ctx](int val) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, val);
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [sl](ossia::value val) { sl->setValue(ossia::convert<int>(val)); });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    auto min = slider.getMin();
    auto max = slider.getMax();
    if (max - min == 0)
      max = min + 1;

    auto sl = new score::QGraphicsIntSlider{nullptr};
    sl->setRange(min, max);
    sl->setValue(ossia::convert<int>(inlet.value()));

    QObject::connect(
        sl,
        &score::QGraphicsIntSlider::sliderMoved,
        context,
        [=, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, sl->value());
        });
    QObject::connect(
        sl, &score::QGraphicsIntSlider::sliderReleased, context, [&ctx, sl]() {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](ossia::value val) {
          if (!sl->moving)
            sl->setValue(ossia::convert<int>(val));
        });

    return sl;
  }
};

struct Toggle
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QCheckBox{parent};
    sl->setChecked(ossia::convert<bool>(inlet.value()));
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, &QCheckBox::toggled, context, [&inlet, &ctx](bool val) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, val);
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [sl](ossia::value val) { sl->setChecked(ossia::convert<bool>(val)); });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    return wrapWidget(make_widget(slider, inlet, ctx, nullptr, context));
  }
};

struct Button
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QPushButton{parent};
    sl->setText(inlet.customData().isEmpty() ? QObject::tr("Bang") : inlet.customData());
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(sl, &QPushButton::pressed, context, [&inlet] {
      inlet.setValue(true);
    });
    QObject::connect(sl, &QPushButton::released, context, [&inlet] {
      inlet.setValue(false);
    });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    return wrapWidget(make_widget(slider, inlet, ctx, nullptr, context));
  }
};
struct ChooserToggle
{
  template<typename T>
  static constexpr auto getAlternatives(const T& t) -> decltype(auto)
  {
      if constexpr(std::is_member_function_pointer_v<decltype(&T::alternatives)>)
      {
          return t.alternatives();
      }
      else
      {
          return t.alternatives;
      }
  }
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    const auto& alts = getAlternatives(slider);
    SCORE_ASSERT(alts.size() == 2);
    auto sl = new score::ToggleButton{alts, parent};
    sl->setCheckable(true);
    bool b = ossia::convert<bool>(inlet.value());
    if (b && !sl->isChecked())
      sl->toggle();
    else if (!b && sl->isChecked())
      sl->toggle();
    sl->setContentsMargins(0, 0, 0, 0);

    QObject::connect(
        sl, &QCheckBox::toggled, context, [&inlet, &ctx](bool val) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, val);
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [sl](ossia::value val) {
          bool b = ossia::convert<bool>(val);
          if (b && !sl->isChecked())
            sl->toggle();
          else if (!b && sl->isChecked())
            sl->toggle();
        });

    return sl;
  }

  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    return wrapWidget(make_widget(slider, inlet, ctx, nullptr, context));
  }
};

struct LineEdit
{
  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QLineEdit{parent};
    sl->setText(
        QString::fromStdString(ossia::convert<std::string>(inlet.value())));
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setMaximumWidth(70);

    QObject::connect(
        sl, &QLineEdit::editingFinished, context, [sl, &inlet, &ctx]() {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, sl->text().toStdString());
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [sl](ossia::value val) {
          sl->setText(
              QString::fromStdString(ossia::convert<std::string>(val)));
        });

    return sl;
  }
  template <typename T, typename Control_T>
  static QGraphicsItem* make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    return wrapWidget(make_widget(slider, inlet, ctx, nullptr, context));
  }
};

struct Enum
{
  static const auto& toStd(const char* const& s) { return s; }
  static const auto& toStd(const std::string& s) { return s; }
  static auto toStd(const QString& s) { return s.toStdString(); }

  static const auto& convert(const std::string& str, const char*)
  {
    return str;
  }
  static auto convert(const std::string& str, const QString&)
  {
    return QString::fromStdString(str);
  }

  template <typename T, typename Control_T>
  static auto make_widget(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    const auto& values = slider.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;
    auto sl = new QComboBox{parent};
    for (const auto& e : values)
    {
      sl->addItem(e);
    }

    auto set_index = [values, sl](const ossia::value& val) {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if (it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl,
        SignalUtils::QComboBox_currentIndexChanged_int(),
        context,
        [values, &inlet, &ctx](int idx) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, toStd(values[idx]));
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](const ossia::value& val) { set_index(val); });

    return sl;
  }

  template <typename T, typename Control_T>
  static auto make_item(
      const T& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    const auto& values = slider.getValues();
    using val_t = std::remove_reference_t<decltype(values[0])>;

    auto sl = slider.pixmaps.empty() || slider.pixmaps[0] == nullptr
        ? new score::QGraphicsEnum{values, nullptr}
        : (score::QGraphicsEnum*)new score::QGraphicsPixmapEnum{values, slider.pixmaps, nullptr};

    auto set_index = [values, sl](const ossia::value& val) {
      auto v = ossia::convert<std::string>(val);
      auto it = ossia::find(values, convert(v, val_t{}));
      if (it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };

    set_index(inlet.value());

    QObject::connect(
        sl, &score::QGraphicsEnum::currentIndexChanged, context, [sl, &inlet, &ctx] (int idx) {
          ctx.dispatcher.submit<SetControlValue<Control_T>>(inlet, toStd(sl->array[idx]));
          ctx.dispatcher.commit();
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](const ossia::value& val) {
          set_index(val);
        });

    return sl;
  }
};

struct ComboBox
{
  template <typename U, typename Control_T>
  static auto make_widget(
      const U& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    const auto& values = slider.getValues();
    auto sl = new QComboBox{parent};
    for (auto& e : values)
    {
      sl->addItem(e.first);
    }
    sl->setContentsMargins(0, 0, 0, 0);

    auto set_index = [values, sl](const ossia::value& val) {
      auto it = ossia::find_if(
          values, [&](const auto& pair) { return pair.second == val; });
      if (it != values.end())
      {
        sl->setCurrentIndex(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl,
        SignalUtils::QComboBox_currentIndexChanged_int(),
        context,
        [values, &inlet, &ctx](int idx) {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, values[idx].second);
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](const ossia::value& val) { set_index(val); });

    return sl;
  }

  template <typename U, typename Control_T>
  static QGraphicsItem* make_item(
      const U& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    const auto N = slider.count();

    const auto& values = slider.getValues();
    QStringList arr;
    arr.reserve(N);
    for (std::size_t i = 0; i < N; i++)
      arr.push_back(values[i].first);

    auto sl = new score::QGraphicsCombo{arr, nullptr};

    auto set_index = [values, sl](const ossia::value& val) {
      auto it = ossia::find_if(
          values, [&](const auto& pair) { return pair.second == val; });
      if (it != values.end())
      {
        sl->setValue(std::distance(values.begin(), it));
      }
    };
    set_index(inlet.value());

    QObject::connect(
        sl,
        &score::QGraphicsCombo::sliderMoved,
        context,
        [values, sl, &inlet, &ctx] {
          sl->moving = true;
          ctx.dispatcher.submit<SetControlValue<Control_T>>(
              inlet, values[sl->value()].second);
        });
    QObject::connect(
        sl, &score::QGraphicsCombo::sliderReleased, context, [sl, &ctx] {
          ctx.dispatcher.commit();
          sl->moving = false;
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](const ossia::value& val) {
          if (sl->moving)
            return;

          set_index(val);
        });

    return sl;
  }
};

struct TimeSignatureValidator final : public QValidator
{
  using QValidator::QValidator;
  State validate(QString& str, int&) const override
  {
    auto p = Control::get_time_signature(str.toStdString());
    if (!p)
      return State::Invalid;

    return State::Acceptable;
  }
};

struct TimeSignatureChooser
{
  template<typename Control_T>
  static auto make_widget(
      const unused_t& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QWidget* parent,
      QObject* context)
  {
    auto sl = new QLineEdit;

    sl->setValidator(new TimeSignatureValidator{sl});
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setMaximumWidth(70);

    auto set_text = [sl](const ossia::value& val) {
      const auto& vptr = val.target<std::string>();
      if (!vptr)
        return;
      if (!Control::get_time_signature(*vptr))
        return;

      sl->setText(QString::fromStdString(*vptr));
    };
    set_text(inlet.value());

    QObject::connect(
        sl, &QLineEdit::editingFinished, context, [sl, &inlet, &ctx] {
          CommandDispatcher<>{ctx.commandStack}.submit<SetControlValue<Control_T>>(
              inlet, sl->text().toStdString());
        });

    QObject::connect(
        &inlet,
        &Control_T::valueChanged,
        sl,
        [=](const ossia::value& val) { set_text(val); });

    return sl;
  }

  template <typename U, typename Control_T>
  static QGraphicsItem* make_item(
      const U& slider,
      Control_T& inlet,
      const score::DocumentContext& ctx,
      QGraphicsItem* parent,
      QObject* context)
  {
    return wrapWidget(make_widget(slider, inlet, ctx, nullptr, context));
  }
};

struct RGBAEdit
{
  // TODO
};

struct XYZEdit
{
  // TODO
};

using FloatSlider = FloatControl<score::QGraphicsSlider>;
using LogFloatSlider = LogFloatControl<score::QGraphicsLogSlider>;
using FloatKnob = FloatControl<score::QGraphicsKnob>;
using LogFloatKnob = LogFloatControl<score::QGraphicsLogKnob>;
}
