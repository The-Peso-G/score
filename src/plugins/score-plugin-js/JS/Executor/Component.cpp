﻿// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Component.hpp"

#include "JSAPIWrapper.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/ConsolePanel.hpp>

#include <Scenario/Execution/score2OSSIA.hpp>
#include <score/tools/Bind.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/js_utilities.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/editor/state/state.hpp>

#include <QEventLoop>
#include <QQmlContext>
#include <QQmlComponent>

#include <Engine/OSSIA2score.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <vector>

namespace JS
{
namespace Executor
{
class js_node final : public ossia::graph_node
{
public:
  js_node(ossia::execution_state& st);

  void setScript(const QString& val);

  void run(const ossia::token_request& t, ossia::exec_state_facade) noexcept override;

  QQmlEngine m_engine;
  QList<std::pair<ControlInlet*, ossia::inlet_ptr>> m_ctrlInlets;
  QList<std::pair<ValueInlet*, ossia::inlet_ptr>> m_valInlets;
  QList<std::pair<ValueOutlet*, ossia::outlet_ptr>> m_valOutlets;
  QList<std::pair<AudioInlet*, ossia::inlet_ptr>> m_audInlets;
  QList<std::pair<AudioOutlet*, ossia::outlet_ptr>> m_audOutlets;
  QList<std::pair<MidiInlet*, ossia::inlet_ptr>> m_midInlets;
  QList<std::pair<MidiOutlet*, ossia::outlet_ptr>> m_midOutlets;
  QObject* m_object{};
  ExecStateWrapper* m_execFuncs{};

private:
  void setupComponent(QQmlComponent& c);
};

struct js_control_updater
{
  ValueInlet& control;
  ossia::value v;
  void operator()() const
  {
    control.setValue(v.apply(ossia::qt::ossia_to_qvariant{}));
  }
};

struct js_process final : public ossia::node_process
{
  using node_process::node_process;
  js_node& js() const { return static_cast<js_node&>(*node); }
  void start() override
  {
    QMetaObject::invokeMethod(js().m_object, "start", Qt::DirectConnection);
  }
  void stop() override
  {
    QMetaObject::invokeMethod(js().m_object, "stop", Qt::DirectConnection);
  }
  void pause() override
  {
    QMetaObject::invokeMethod(js().m_object, "pause", Qt::DirectConnection);
  }
  void resume() override
  {
    QMetaObject::invokeMethod(js().m_object, "resume", Qt::DirectConnection);
  }
  void transport_impl(ossia::time_value date) override
  {
    QMetaObject::invokeMethod(
        js().m_object,
        "transport",
        Qt::DirectConnection,
        Q_ARG(QVariant, double(date.impl)));
  }
  void offset_impl(ossia::time_value date) override
  {
    QMetaObject::invokeMethod(
        js().m_object,
        "offset",
        Qt::DirectConnection,
        Q_ARG(QVariant, double(date.impl)));
  }
};
Component::Component(
    JS::ProcessModel& element,
    const ::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ::Execution::ProcessComponent_T<JS::ProcessModel, ossia::node_process>{
          element,
          ctx,
          id,
          "JSComponent",
          parent}
{
  std::shared_ptr<js_node> node = std::make_shared<js_node>(*ctx.execState);
  this->node = node;
  auto proc = std::make_shared<js_process>(node);
  m_ossia_process = proc;

  node->setScript(element.qmlData());
  if (!node->m_object)
    throw std::runtime_error{"Invalid JS"};

  const auto& inlets = element.inlets();
  int inl = 0;
  for (auto n : node->m_object->children())
  {
    if (auto val_in = qobject_cast<Inlet*>(n))
    {
      if (val_in->is_control())
      {
        auto val_inlet = qobject_cast<ValueInlet*>(val_in);
        SCORE_ASSERT(val_inlet);
        SCORE_ASSERT((int)inlets.size() > inl);
        auto port = inlets[inl];
        auto ctrl = qobject_cast<Process::ControlInlet*>(port);
        SCORE_ASSERT(ctrl);
        connect(
            ctrl,
            &Process::ControlInlet::valueChanged,
            this,
            [=](const ossia::value& val) {
              this->in_exec(js_control_updater{*val_inlet, val});
            });
        js_control_updater{*val_inlet, ctrl->value()}();
      }
      inl++;
    }
  }

  con(element,
      &JS::ProcessModel::qmlDataChanged,
      this,
      [=](const QString& str) {
        in_exec([proc = std::dynamic_pointer_cast<js_node>(node), str] {
          proc->setScript(std::move(str));
        });
      });

  connect(node->m_execFuncs, &ExecStateWrapper::exec,
      this, [=] (const QString& code) {
    auto& console = system().doc.app.panel<JS::PanelDelegate>();
    console.evaluate(code);
  }, Qt::QueuedConnection);
}

Component::~Component() {}

js_node::js_node(ossia::execution_state& st)
{
  m_execFuncs = new ExecStateWrapper{st, &m_engine};
  m_engine.rootContext()->setContextProperty("Device", m_execFuncs);
}

void js_node::setupComponent(QQmlComponent& c)
{
  m_object = c.create();
  if (m_object)
  {
    m_object->setParent(&m_engine);

    for (auto n : m_object->children())
    {
      if (auto ctrl_in = qobject_cast<ControlInlet*>(n))
      {
        inputs().push_back(new ossia::value_inlet);
        m_ctrlInlets.push_back({ctrl_in, inputs().back()});
        m_ctrlInlets.back().second->target<ossia::value_port>()->is_event
            = false;
      }
      else if (auto val_in = qobject_cast<ValueInlet*>(n))
      {
        inputs().push_back(new ossia::value_inlet);

        if (!val_in->is_control())
        {
          inputs().back()->target<ossia::value_port>()->is_event = true;
        }

        m_valInlets.push_back({val_in, inputs().back()});
      }
      else if (auto aud_in = qobject_cast<AudioInlet*>(n))
      {
        inputs().push_back(new ossia::audio_inlet);
        m_audInlets.push_back({aud_in, inputs().back()});
      }
      else if (auto mid_in = qobject_cast<MidiInlet*>(n))
      {
        inputs().push_back(new ossia::midi_inlet);
        m_midInlets.push_back({mid_in, inputs().back()});
      }
      else if (auto val_out = qobject_cast<ValueOutlet*>(n))
      {
        outputs().push_back(new ossia::value_outlet);
        m_valOutlets.push_back({val_out, outputs().back()});
      }
      else if (auto aud_out = qobject_cast<AudioOutlet*>(n))
      {
        outputs().push_back(new ossia::audio_outlet);
        m_audOutlets.push_back({aud_out, outputs().back()});
      }
      else if (auto mid_out = qobject_cast<MidiOutlet*>(n))
      {
        outputs().push_back(new ossia::midi_outlet);
        m_midOutlets.push_back({mid_out, outputs().back()});
      }
    }
  }
}
void js_node::setScript(const QString& val)
{
  if (val.trimmed().startsWith("import"))
  {
    QQmlComponent c{&m_engine};
    c.setData(val.toUtf8(), QUrl());
    const auto& errs = c.errors();
    if (!errs.empty())
    {
      ossia::logger().error(
          "Uncaught exception at line {} : {}",
          errs[0].line(),
          errs[0].toString().toStdString());
    }
    else
    {
      setupComponent(c);
    }
  }
  else
  {
    qDebug() << "URL: " << val << QUrl::fromLocalFile(val);
    QQmlComponent c{&m_engine, QUrl::fromLocalFile(val)};
    const auto& errs = c.errors();
    if (!errs.empty())
    {
      ossia::logger().error(
          "Uncaught exception at line {} : {}",
          errs[0].line(),
          errs[0].toString().toStdString());
    }
    else
    {
      setupComponent(c);
    }
  }
}

void js_node::run(const ossia::token_request& tk, ossia::exec_state_facade estate) noexcept
{
  // if (t.date == ossia::Zero)
  //   return;

  QEventLoop e;
  // Copy audio
  for (int i = 0; i < m_audInlets.size(); i++)
  {
    auto& dat
        = m_audInlets[i].second->target<ossia::audio_port>()->samples;

    const int dat_size = (int)dat.size();
    QVector<QVector<double>> audio(dat_size);
    for (int i = 0; i < dat_size; i++)
    {
      const int dat_i_size = dat[i].size();
      audio[i].resize(dat_i_size);
      for (int j = 0; j < dat_i_size; j++)
        audio[i][j] = dat[i][j];
    }
    m_audInlets[i].first->setAudio(audio);
  }

  // Copy values
  for (int i = 0; i < m_valInlets.size(); i++)
  {
    auto& vp = *m_valInlets[i].second->target<ossia::value_port>();
    auto& dat = vp.get_data();

    m_valInlets[i].first->clear();
    if (dat.empty())
    {
      if (vp.is_event)
      {
        m_valInlets[i].first->setValue(QVariant{});
      }
      else
      {
        // Use control or same method as before
      }
    }
    else
    {
      for (auto& val : dat)
      {
        auto qvar = val.value.apply(ossia::qt::ossia_to_qvariant{});
        m_valInlets[i].first->setValue(qvar);
        m_valInlets[i].first->addValue(QVariant::fromValue(
            ValueMessage{(double)val.timestamp, std::move(qvar)}));
      }
    }
  }

  // Copy controls
  for (int i = 0; i < m_ctrlInlets.size(); i++)
  {
    auto& vp = *m_ctrlInlets[i].second->target<ossia::value_port>();
    auto& dat = vp.get_data();

    m_ctrlInlets[i].first->clear();
    if (!dat.empty())
    {
      auto var = dat.back().value.apply(ossia::qt::ossia_to_qvariant{});
      m_ctrlInlets[i].first->setValue(std::move(var));
    }
  }

  // Copy midi
  for (int i = 0; i < m_midInlets.size(); i++)
  {
    auto& dat
        = m_midInlets[i].second->target<ossia::midi_port>()->messages;
    m_midInlets[i].first->setMidi(dat);
  }

  QMetaObject::invokeMethod(
      m_object,
      "onTick",
      Qt::DirectConnection,
      Q_ARG(QVariant, double(tk.prev_date.impl)),
      Q_ARG(QVariant, double(tk.date.impl)),
      Q_ARG(QVariant, tk.position()),
      Q_ARG(QVariant, double(tk.offset.impl)));

  for (int i = 0; i < m_valOutlets.size(); i++)
  {
    auto& dat = *m_valOutlets[i].second->target<ossia::value_port>();
    const auto& v = m_valOutlets[i].first->value();
    if (!v.isNull() && v.isValid())
      dat.write_value(ossia::qt::qt_to_ossia{}(v), estate.physical_start(tk));
    for (auto& v : m_valOutlets[i].first->values)
    {
      dat.write_value(
          ossia::qt::qt_to_ossia{}(std::move(v.value)), v.timestamp);
    }
    m_valOutlets[i].first->clear();
  }

  for (int i = 0; i < m_midOutlets.size(); i++)
  {
    auto& dat = *m_midOutlets[i].second->target<ossia::midi_port>();
    for (const auto& mess : m_midOutlets[i].first->midi())
    {
      rtmidi::message m;
      m.bytes.resize(mess.size());
      for (int j = 0; j < mess.size(); j++)
      {
        m.bytes[j] = mess[j];
      }
      dat.messages.push_back(std::move(m));
    }
    m_midOutlets[i].first->clear();
  }

  auto tick_start = estate.physical_start(tk);
  for (int out = 0; out < m_audOutlets.size(); out++)
  {
    auto& src = m_audOutlets[out].first->audio();
    auto& snk
        = m_audOutlets[out].second->target<ossia::audio_port>()->samples;
    snk.resize(src.size());
    for (int chan = 0; chan < src.size(); chan++)
    {
      snk[chan].resize(src[chan].size() + tick_start);

      for (int j = 0; j < src[chan].size(); j++)
        snk[chan][j + tick_start] = src[chan][j];
    }
  }

  e.processEvents();
  m_engine.collectGarbage();
}
}
}
