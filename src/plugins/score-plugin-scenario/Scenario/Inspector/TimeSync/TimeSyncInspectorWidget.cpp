// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TimeSyncInspectorWidget.hpp"

#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Commands/TimeSync/SetAutoTrigger.hpp>
#include <Scenario/Inspector/TimeSync/TriggerInspectorWidget.hpp>
#include <Scenario/Inspector/MetadataWidget.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/widgets/SpinBoxes.hpp>
#include <score/tools/Bind.hpp>
#include <Process/Dataflow/ControlWidgets.hpp>

#include <QCheckBox>
namespace Scenario
{
class QuantificationWidget : public QComboBox
{
  static int indexForQuantification(double d) noexcept
  {
    if(d == 0.)
      return 0;
    if(d == 0.125)
      return 1;
    if(d == 0.250)
      return 2;
    if(d == 0.500)
      return 3;
    if(d == 1)
      return 4;
    if(d == 2)
      return 5;
    if(d == 4)
      return 6;
    if(d == 8)
      return 7;
    if(d == 16)
      return 8;
    if(d == 32)
      return 9;

    return 0;
  }

  static double quantificationForIndex(int d) noexcept
  {
    switch(d)
    {
      case 0: return 0.;
      case 1: return 0.125;
      case 2: return 0.250;
      case 3: return 0.500;
      case 4: return 1.;
      case 5: return 2.;
      case 6: return 4.;
      case 7: return 8.;
      case 8: return 16.;
      case 9: return 32.;
      default: return 0.;
    }
  }

  W_OBJECT(QuantificationWidget)
public:
  QuantificationWidget(QWidget* parent = nullptr)
    : QComboBox{parent}
  {
    addItems({
               tr("Free"),
               tr("8 bars"),
               tr("4 bars"),
               tr("2 bars"),
               tr("1 bar "),
               tr("1/2   "),
               tr("1/4   "),
               tr("1/8   "),
               tr("1/16  "),
               tr("1/32  ")
             });
  }

  double quantification() const noexcept
  {
    return quantificationForIndex(currentIndex());
  }

  void setQuantification(double d)
  {
    const auto idx = indexForQuantification(d);
    if(idx != currentIndex())
    {
      setCurrentIndex(idx);
      quantificationChanged(d);
    }
  }

  void quantificationChanged(double d) W_SIGNAL(quantificationChanged, d);
};
W_OBJECT_IMPL(QuantificationWidget)


class TimeSignatureWidget : public QLineEdit
{
public:
  TimeSignatureWidget()
  {
    setContentsMargins(0, 0, 0, 0);
  }

  void setSignature(optional<Control::time_signature> t)
  {
    if(t)
    {
      setText(QString{"%1/%2"}.arg(t->upper).arg(t->lower));
    }
    else
    {
      setText(QString{"0/0"});
    }
  }

  optional<Control::time_signature> signature() const
  {
    return Control::get_time_signature(this->text().toStdString());
  }
};

TimeSyncInspectorWidget::TimeSyncInspectorWidget(
    const TimeSyncModel& object,
    const score::DocumentContext& ctx,
    QWidget* parent)
    : InspectorWidgetBase{object,
                          ctx,
                          parent,
                          tr("Sync (%1)").arg(object.metadata().getName())}
    , m_model{object}
{
  setObjectName("TimeSyncInspectorWidget");
  setParent(parent);

  // metadata
  m_metadata = new MetadataWidget{
      m_model.metadata(), ctx.commandStack, &m_model, this};

  m_metadata->setupConnections(m_model);

  addHeader(m_metadata);

  // default date
  m_date
      = new TextLabel{tr("Default date: ") + m_model.date().toString(), this};

  // Trigger
  m_autotrigger = new QCheckBox{tr("Auto-trigger")};
  m_autotrigger->setChecked(object.autotrigger());
  m_autotrigger->setWhatsThis(tr(R"_(Auto-trigger timesyncs are timesyncs which will
                                  directly restart their following floating scenario upon triggering.
                                  Else, triggering the timesync will stop the following subgraph and
                                  it will be necessary to trigger it again to restart it.
                                  This is only relevant for subgraphs not connected
                                  to the root of a score.)_"));
  m_autotrigger->setToolTip(m_autotrigger->whatsThis());
  connect(m_autotrigger, &QCheckBox::toggled,
          this, [&] (bool t) {
    if(t != object.autotrigger())
      CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::SetAutoTrigger>(object, t);
  });
  connect(&object, &TimeSyncModel::autotriggerChanged,
          this, [&] (bool t) {
    if(t != m_autotrigger->isChecked())
      m_autotrigger->setChecked(t);
  });

  // Synchronization
#if defined(SCORE_MUSICAL)
  auto musicalSync = new QuantificationWidget{this};
  musicalSync->setQuantification(m_model.musicalSync());

  QObject::connect(
      musicalSync, qOverload<double>(&QuantificationWidget::quantificationChanged), this, [&ctx, &object] (double v) {
    CommandDispatcher<>{ctx.commandStack}.submit<Scenario::Command::SetTimeSyncMusicalSync>(object, v);
      });

  con(m_model, &TimeSyncModel::musicalSyncChanged,
      musicalSync, &QuantificationWidget::setQuantification);
#endif
  m_trigwidg = new TriggerInspectorWidget{
      ctx,
      ctx.app.interfaces<Command::TriggerCommandFactoryList>(),
      m_model,
      this};
  updateAreaLayout({m_date, m_autotrigger,
                    musicalSync,
                    new TextLabel{tr("Trigger")}, m_trigwidg});

  // display data
  updateDisplayedValues();

  con(m_model,
      &TimeSyncModel::dateChanged,
      this,
      &TimeSyncInspectorWidget::on_dateChanged);
}

void TimeSyncInspectorWidget::updateDisplayedValues()
{
  on_dateChanged(m_model.date());

  m_trigwidg->updateExpression(m_model.expression());
}

void TimeSyncInspectorWidget::on_dateChanged(const TimeVal& t)
{
  m_date->setText(tr("Default date: ") + t.toString());
}
}
