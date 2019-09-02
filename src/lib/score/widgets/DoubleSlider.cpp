// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DoubleSlider.hpp"

#include <score/model/Skin.hpp>
#include <score/tools/Clamp.hpp>

#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DoubleSlider)
namespace score
{
AbsoluteSliderStyle::~AbsoluteSliderStyle() = default;
Slider::~Slider() = default;
DoubleSlider::~DoubleSlider() = default;

AbsoluteSliderStyle* AbsoluteSliderStyle::instance() noexcept
{
  static AbsoluteSliderStyle style;
  return &style;
}

int AbsoluteSliderStyle::styleHint(
    QStyle::StyleHint hint,
    const QStyleOption* option,
    const QWidget* widget,
    QStyleHintReturn* returnData) const
{
  switch (hint)
  {
    case QStyle::SH_Slider_AbsoluteSetButtons:
      return Qt::AllButtons;
    default:
      return QProxyStyle::styleHint(hint, option, widget, returnData);
  }
}

DoubleSlider::DoubleSlider(QWidget* parent) : Slider{Qt::Horizontal, parent}
{
  setMinimum(0);
  setMaximum(max + 1.);

  connect(this, &QSlider::valueChanged, this, [&](int val) {
    valueChanged(double(val) / max);
  });
}

void DoubleSlider::setValue(double val)
{
  val = clamp(val, 0, 1);
  blockSignals(true);
  QSlider::setValue(val * max);
  blockSignals(false);
}

double DoubleSlider::value() const
{
  return QSlider::value() / max;
}

Slider::Slider(Qt::Orientation ort, QWidget* widg) : QSlider{ort, widg}
{
  setStyle(AbsoluteSliderStyle::instance());
  switch (ort)
  {
    case Qt::Vertical:
      setMinimumSize(20, 30);
      break;
    case Qt::Horizontal:
      setMinimumSize(30, 20);
      break;
  }
}

Slider::Slider(QWidget* widg) : Slider{Qt::Horizontal, widg} {}

void Slider::paintEvent(QPaintEvent*)
{
  QPainter p{this};
  paint(p);
}
void Slider::paint(QPainter& p)
{
  auto& skin = score::Skin::instance();
  double min = minimum();
  double max = maximum();
  double val = value();

  double ratio = 1. - (max - val) / (max - min);

  static constexpr auto round = 1.5;
  p.setPen(Qt::transparent);
  p.setBrush(skin.SliderBrush);
  p.drawRoundedRect(rect(), round, round);

  p.setBrush(skin.SliderExtBrush);
  p.drawRoundedRect(
      QRect{1, 1, int(ratio * (width() - 2)), height() - 2}, round, round);
}

void Slider::paintWithText(const QString& s)
{
  auto& skin = score::Skin::instance();

  QPainter p{this};
  paint(p);

  p.setPen(skin.SliderTextPen);
  p.setFont(skin.SliderFont);
  p.drawText(
      QRectF{4., 2., (width() - 16.), height() - 4.},
      s,
      QTextOption(Qt::AlignLeft));
}
}
