#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>

#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/StyleSheets.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/tools/Bind.hpp>

#include <QGridLayout>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

namespace Scenario
{

SpeedWidget::SpeedWidget(
    bool withButtons,
    bool showText,
    QWidget* parent)
    : QWidget{parent}
{
  setObjectName("SpeedSlider");

  auto lay = new score::MarginLess<QGridLayout>{this};
  lay->setHorizontalSpacing(1);
  lay->setVerticalSpacing(1);

  auto setSpeedFun = [=](double val) {
    if(m_model)
    {
      auto& dur = ((IntervalModel&)(*m_model)).duration;
      auto s = double(val) / 100.0;
      if (dur.speed() != s)
      {
        dur.setSpeed(s);
      }
    }
  };

  if (withButtons)
  {
    // Buttons
    int btn_col = 0;
    for (double factor : {0., 50., 100., 200., 500.})
    {
      auto pb = new QPushButton{"× " + QString::number(factor * 0.01), this};
      pb->setMinimumWidth(35);
      pb->setMaximumWidth(45);
      pb->setFlat(true);
      pb->setContentsMargins(0, 0, 0, 0);
      pb->setStyleSheet(
          "QPushButton { margin: 0px; padding: 0px; border:  1px solid #252930; "
          + score::ValueStylesheet + "}"
          + "QPushButton:hover { border: 1px solid #aaa;} ");

      connect(pb, &QPushButton::clicked, this, [=] { setSpeedFun(factor); });
      lay->addWidget(pb, 1, btn_col++, 1, 1);
    }
  }

  // Slider
  m_slider = new score::SpeedSlider{this};
  m_slider->showText = showText;
  m_slider->setValue(100.);

  if(withButtons)
  {
    lay->addWidget(m_slider, 0, 0, 1, 5);

    for (int i = 0; i < 5; i++)
      lay->setColumnStretch(i, 0);
    lay->setColumnStretch(5, 10);
  }
  else
  {
    lay->addWidget(m_slider, 0, 0, 1, 1);
  }
  connect(m_slider, &QSlider::valueChanged, this, setSpeedFun);
}

SpeedWidget::~SpeedWidget() {}

void SpeedWidget::setInterval(const IntervalModel& m)
{
  if(m_model)
  {
    QObject::disconnect(&m_model->duration, nullptr, this, nullptr);
  }

  m_model = &m;
  m_slider->setValue(m.duration.speed() * 100.);

  con(m.duration, &IntervalDurations::speedChanged, this, [=](double s) {
    double r = s * 100;
    if (!qFuzzyCompare(r, m_slider->value()))
      m_slider->setValue(r);
  });

  ::bind(m, IntervalModel::p_timeSignature{},
      this, [=] (bool t) {
      m_slider->tempo = t;
      m_slider->update();
  });
}

void SpeedWidget::unsetInterval()
{
  if(m_model)
  {
    QObject::disconnect(&m_model->duration, nullptr, this, nullptr);
  }

  m_model = nullptr;
}

QSize SpeedWidget::sizeHint() const
{
  auto sz = QWidget::sizeHint();
  sz.setWidth(200);
  return sz;
}
}


