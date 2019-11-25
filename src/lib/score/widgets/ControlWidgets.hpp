#pragma once
#include <score/widgets/DoubleSlider.hpp>

#include <QPushButton>

#include <score_lib_base_export.h>

#include <array>
namespace score
{
struct SCORE_LIB_BASE_EXPORT ToggleButton : public QPushButton
{
public:
  ToggleButton(std::array<QString, 2> alts, QWidget* parent);

  ToggleButton(std::array<const char*, 2> alt, QWidget* parent);

  ToggleButton(QStringList alt, QWidget* parent);

  std::array<QString, 2> alternatives;

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ValueSlider : public score::Slider
{
public:
  using Slider::Slider;
  bool moving = false;

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT SpeedSlider : public score::Slider
{
public:
  SpeedSlider(QWidget* parent = nullptr);
  bool moving = false;
  bool showText = true;
  bool tempo = false;

protected:
  void paintEvent(QPaintEvent* event) override;

  void mousePressEvent(QMouseEvent*) override;
};

struct SCORE_LIB_BASE_EXPORT VolumeSlider : public score::DoubleSlider
{
public:
  using DoubleSlider::DoubleSlider;
  bool moving = false;

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ValueDoubleSlider : public score::DoubleSlider
{
public:
  using score::DoubleSlider::DoubleSlider;
  bool moving = false;
  double min{};
  double max{};

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ValueLogDoubleSlider : public score::DoubleSlider
{
public:
  using score::DoubleSlider::DoubleSlider;
  bool moving = false;
  double min{};
  double max{};

protected:
  void paintEvent(QPaintEvent* event) override;
};

struct SCORE_LIB_BASE_EXPORT ComboSlider : public score::Slider
{
  QStringList array;

public:
  template <std::size_t N>
  ComboSlider(const std::array<const char*, N>& arr, QWidget* parent)
      : score::Slider{parent}
  {
    array.reserve(N);
    for (auto str : arr)
      array.push_back(str);
  }

  ComboSlider(const QStringList& arr, QWidget* parent);

  bool moving = false;

protected:
  void paintEvent(QPaintEvent* event) override;
};

SCORE_LIB_BASE_EXPORT const QPalette& transparentPalette();
static inline auto transparentStylesheet()
{
  return QStringLiteral("QWidget { background-color:transparent }");
}
}
