//// This is an open source non-commercial project. Dear PVS-Studio, please
/// check / it. PVS-Studio Static Code Analyzer for C, C++ and C#:
/// http://www.viva64.com
#include "ApplicationPlugin.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Settings/ExplorerModel.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/TimeValue.hpp>
#include <Process/Style/Pixmaps.hpp>

#include <Protocols/Audio/AudioDevice.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>
#include <Scenario/Inspector/Interval/SpeedSlider.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/widgets/ControlWidgets.hpp>
#include <score/widgets/DoubleSlider.hpp>
#include <score/widgets/SetIcons.hpp>
#include <score/tools/Bind.hpp>
#include <score/actions/ToolbarManager.hpp>
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QAction>
#include <QLabel>
#include <QMessageBox>
#include <QTabWidget>
#include <QToolBar>
#include <QMainWindow>

#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/ContextMenu/PlayContextMenu.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Settings/ExecutorModel.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <wobjectimpl.h>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <vector>

namespace Engine
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}, m_playActions{*this, ctx}
{
  // qInstallMessageHandler(nullptr);
  // Two parts :
  // One that maintains the devices for each document
  // (and disconnects / reconnects them when the current document changes)
  // Also during execution, one shouldn't be able to switch document.

  // Another part that, at execution time, creates structures corresponding
  // to the Scenario plug-in with the OSSIA API.

  if (ctx.applicationSettings.gui)
  {
    auto& play_action = ctx.actions.action<Actions::Play>();
    connect(
        play_action.action(),
        &QAction::triggered,
        this,
        [&](bool b) { on_play(b); },
        Qt::QueuedConnection);

    auto& play_glob_action = ctx.actions.action<Actions::PlayGlobal>();
    connect(
        play_glob_action.action(),
        &QAction::triggered,
        this,
        [&](bool b) {
          if (auto doc = currentDocument())
          {
            auto& mod = doc->model().modelDelegate();
            auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
            if (scenar)
            {
              on_play(scenar->baseInterval(), b, {}, TimeVal::zero());
            }
          }
        },
        Qt::QueuedConnection);

    auto& stop_action = ctx.actions.action<Actions::Stop>();
    connect(
        stop_action.action(),
        &QAction::triggered,
        this,
        &ApplicationPlugin::on_stop,
        Qt::QueuedConnection);

    auto& init_action = ctx.actions.action<Actions::Reinitialize>();
    connect(
        init_action.action(),
        &QAction::triggered,
        this,
        &ApplicationPlugin::on_init,
        Qt::QueuedConnection);

    auto& ctrl
        = ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>();
    con(ctrl.execution(),
        &Scenario::ScenarioExecution::playAtDate,
        this,
        [=, act = play_action.action()](const TimeVal& t) {
          if (m_clock)
          {
            on_transport(t);
          }
          else
          {
            on_play(true, t);
            act->trigger();
          }
        });

    m_playActions.setupContextMenu(ctrl.layerContextMenuRegistrar());
  }
}

ApplicationPlugin::~ApplicationPlugin()
{
  // The scenarios playing should already have been stopped by virtue of
  // aboutToClose.
}

bool ApplicationPlugin::handleStartup()
{
  if (!context.documents.documents().empty())
  {
    if (context.applicationSettings.autoplay)
    {
      // TODO what happens if we load multiple documents ?
        QTimer::singleShot(context.applicationSettings.waitAfterLoad * 1000, this, [=] {
            on_play(true);
        });
      return true;
    }
  }

  return false;
}

void ApplicationPlugin::initialize()
{
  // Update the clock widget
  // See TransportActions::makeGUIElements
  auto& toolbars = this->context.toolbars.get();
  auto transport_toolbar = toolbars.find(StringKey<score::Toolbar>("Transport"));
  if(transport_toolbar != toolbars.end())
  {
    const auto& tb = transport_toolbar->second.toolbar();
    if(tb)
    {
      auto cld = tb->children();
      if(!cld.empty())
      {
        auto label = tb->findChild<QLabel*>("TimeLabel");
        if(label)
          setupTimingWidget(label);

        m_speedSlider = dynamic_cast<Scenario::SpeedWidget*>(cld.back());
      }
    }
  }
}

QWidget* ApplicationPlugin::setupTimingWidget(QLabel* time_label) const
{
  auto timer = new QTimer{time_label};
  connect(timer, &QTimer::timeout, this, [=] {
    if (m_clock)
    {
      auto& itv = m_clock->scenario.baseInterval().scoreInterval().duration;
      auto time = (itv.defaultDuration() * itv.playPercentage()).toQTime();
      time_label->setText(time.toString("HH:mm:ss.zzz"));
    }
    else
    {
      time_label->setText("00:00:00.000");
    }
  });
  timer->start(1000 / 20);
  return time_label;

}
score::GUIElements ApplicationPlugin::makeGUIElements()
{
  GUIElements e;

  auto& toolbars = e.toolbars;

  toolbars.reserve(4);

  // The toolbar with the interval controls
  {
    auto ui_toolbar = new QToolBar(tr("Interval"));
    toolbars.emplace_back(
        ui_toolbar,
        StringKey<score::Toolbar>("UISetup"),
        Qt::BottomToolBarArea,
        10);

    auto actgrp = new QActionGroup{ui_toolbar};
    {
      auto timeline_act = new QAction{tr("Timeline interval"), actgrp};
      timeline_act->setCheckable(true);
      timeline_act->setStatusTip(tr("Change between nodal and timeline mode"));
      setIcons(timeline_act
               , QStringLiteral(":/icons/timeline_on.png")
               , QStringLiteral(":/icons/timeline_off.png")
               , QStringLiteral(":/icons/timeline_off.png")
               );

      ui_toolbar->addAction(timeline_act);
      actgrp->addAction(timeline_act);
    }

    {
      auto nodal_act = new QAction{tr("Nodal interval"), actgrp};
      nodal_act->setCheckable(true);
      nodal_act->setStatusTip(tr("Change between nodal and timeline mode"));
      setIcons(nodal_act
               , QStringLiteral(":/icons/nodal_on.png")
               , QStringLiteral(":/icons/nodal_off.png")
               , QStringLiteral(":/icons/nodal_disabled.png")
               );

      ui_toolbar->addAction(nodal_act);
      actgrp->addAction(nodal_act);
    }

    {
      auto musical_act = new QAction{tr("Enable musical mode"), this};
      musical_act->setCheckable(true);
      musical_act->setStatusTip(tr("Enable musical mode"));
      setIcons(musical_act
               , QStringLiteral(":/icons/music_on.png")
               , QStringLiteral(":/icons/music_off.png")
               , QStringLiteral(":/icons/music_off.png")
               );
      connect(musical_act, &QAction::toggled, this, [this] (bool ok) {
        auto& settings = this->context.settings<Scenario::Settings::Model>();
        settings.setMeasureBars(ok);
        settings.setMagneticMeasures(ok);

        if(auto doc = this->currentDocument())
        {
          auto& mod = doc->model().modelDelegate();
          auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
          if (scenar)
          {
            auto& itv = scenar->baseInterval();
            itv.setHasTimeSignature(ok);
          }
        }
      });
      ui_toolbar->addAction(musical_act);
    }
  }


  return e;
}

void ApplicationPlugin::on_initDocument(score::Document& doc)
{
#if !defined(__EMSCRIPTEN__)
  score::addDocumentPlugin<LocalTree::DocumentPlugin>(doc);
#endif
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  LocalTree::DocumentPlugin* lt
      = doc.context().findPlugin<LocalTree::DocumentPlugin>();
  if (lt)
  {
    lt->init();
    initLocalTreeNodes(*lt);
  }
  score::addDocumentPlugin<Execution::DocumentPlugin>(doc);
}

void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc,
    score::Document* newdoc)
{
  if (olddoc)
  {
    // Disable the local tree for this document by removing
    // the node temporarily
    /*
    auto& doc_plugin = olddoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(false);
    */

    if (m_speedSlider)
      m_speedSlider->unsetInterval();
    // TODO check whether the widget gets deleted
  }

  if (newdoc)
  {
    // Enable the local tree for this document.

    /*
    auto& doc_plugin = newdoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(true);
    */

    // Setup speed toolbar
    auto& root
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(*newdoc);

    if (context.applicationSettings.gui)
    {
      if (m_speedSlider)
        m_speedSlider->setInterval(root.baseInterval());
    }

    // Setup audio & devices
    auto& doc_plugin
        = newdoc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* set
        = newdoc->context().findPlugin<Explorer::ProjectSettings::Model>();
    if (set)
    {
      if (set->getReconnectOnStart())
      {
        auto& list = doc_plugin.list();
        list.apply([&](Device::DeviceInterface& dev) {
          if (&dev != list.audioDevice() && &dev != list.localDevice())
            dev.reconnect();
        });

        if (set->getRefreshOnStart())
        {
          list.apply([&](Device::DeviceInterface& dev) {
            if (&dev != list.audioDevice() && &dev != list.localDevice())
              if (dev.connected())
              {
                auto old_name = dev.name();
                auto new_node = dev.refresh();

                auto& explorer = doc_plugin.explorer();
                const auto& cld = explorer.rootNode().children();
                for (auto it = cld.begin(); it != cld.end(); ++it)
                {
                  auto ds = it->get<Device::DeviceSettings>();
                  if (ds.name == old_name)
                  {
                    explorer.removeNode(it);
                    break;
                  }
                }

                explorer.addDevice(std::move(new_node));
              }
          });
        }
      }
    }
  }
  restart_engine();
}

void ApplicationPlugin::prepareNewDocument()
{
  on_stop();
}

void ApplicationPlugin::on_play(bool b, ::TimeVal t)
{
  // TODO have a on_exit handler to properly stop the scenario.
  if (auto doc = currentDocument())
  {
    if (auto pres = doc->presenter())
    {
      auto scenar = dynamic_cast<Scenario::ScenarioDocumentPresenter*>(
          pres->presenterDelegate());
      if (scenar)
        on_play(scenar->displayedElements.interval(), b, {}, t);
    }
    else
    {
      auto& mod = doc->model().modelDelegate();
      auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
      if (scenar)
      {
        on_play(scenar->baseInterval(), b, {}, t);
      }
    }
  }
}

void ApplicationPlugin::on_transport(TimeVal t)
{
  if (!m_clock)
    return;

  auto itv = m_clock->scenario.baseInterval().OSSIAInterval();
  if (!itv)
    return;

  auto& settings = context.settings<Execution::Settings::Model>();
  auto& ctx = m_clock->context;
  if (settings.getTransportValueCompilation())
  {
    auto execState = m_clock->context.execState;
    ctx.executionQueue.enqueue(
        [execState, itv, time = m_clock->context.time(t)] {
          itv->offset(time);
          execState->commit();
        });
  }
  else
  {
    ctx.executionQueue.enqueue(
        [itv, time = m_clock->context.time(t)] { itv->transport(time); });
  }
}

void ApplicationPlugin::on_play(
    Scenario::IntervalModel& cst,
    bool b,
    exec_setup_fun setup_fun,
    TimeVal t)
{
  auto doc = currentDocument();
  if (!doc)
    return;

  auto plugmodel = doc->context().findPlugin<Execution::DocumentPlugin>();
  if (!plugmodel)
    return;

  if (b)
  {
    if (m_playing)
    {
      SCORE_ASSERT(bool(m_clock));
      if (m_clock->paused())
      {
        m_clock->resume();
        m_paused = false;
      }
    }
    else
    {
      // Here we stop the listening when we start playing the scenario.
      // Get all the selected nodes
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      // Disable listening for everything
      if (explorer
          && !doc->context()
                  .app.settings<Execution::Settings::Model>()
                  .getExecutionListening())
      {
        explorer->deviceModel().listening().stop();
      }

      plugmodel->reload(cst);

      auto& c = plugmodel->context();
      m_clock = makeClock(c);

      if (setup_fun)
      {
        plugmodel->runAllCommands();
        setup_fun(c, plugmodel->baseScenario());
        plugmodel->runAllCommands();
      }

      m_clock->play(t);
      m_paused = false;
    }

    m_playing = true;
    doc->context().execTimer.start();
  }
  else
  {
    if (m_clock)
    {
      m_clock->pause();
      m_paused = true;

      doc->context().execTimer.stop();
    }
  }
}

void ApplicationPlugin::on_record(::TimeVal t)
{
  SCORE_ASSERT(!m_playing);

  // TODO have a on_exit handler to properly stop the scenario.
  if (auto doc = currentDocument())
  {
    auto plugmodel = doc->context().findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;
    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &doc->model().modelDelegate());
    if (!scenar)
      return;

    // Listening isn't stopped here.
    plugmodel->reload(scenar->baseInterval());
    m_clock = makeClock(plugmodel->context());
    m_clock->play(t);

    doc->context().execTimer.start();
    m_playing = true;
    m_paused = false;
  }
}

void ApplicationPlugin::on_stop()
{
  bool wasplaying = m_playing;
  m_playing = false;
  m_paused = false;

  if (m_clock)
  {
    m_clock->stop();
    m_clock.reset();
  }

  if (auto doc = currentDocument())
  {
    doc->context().execTimer.stop();
    auto plugmodel = doc->context().findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;
    else
    {
      // TODO why is this commented
      // plugmodel->clear();
    }
    // If we can we resume listening
    if (!context.docManager.preparingNewDocument())
    {
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      if (explorer)
        explorer->deviceModel().listening().restore();
    }

    if (wasplaying)
    {
      if (auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
              &doc->model().modelDelegate()))
      {
        auto state = Engine::score_to_ossia::state(
            scenar->baseScenario().endState(), plugmodel->context());
        state.launch();
      }
    }

    QTimer::singleShot(50, this, [this] {
      auto doc = currentDocument();
      if (!doc)
        return;
      auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
          &doc->model().modelDelegate());
      if (!scenar)
        return;
      scenar->baseInterval().reset();
      scenar->baseInterval().executionFinished();
      auto procs = doc->findChildren<Scenario::ProcessModel*>();
      for (Scenario::ProcessModel* e : procs)
      {
        for (auto& itv : e->intervals)
        {
          itv.reset();
          itv.executionFinished();
        }
        for (auto& ev : e->events)
        {
          ev.setStatus(Scenario::ExecutionStatus::Editing, *e);
        }
      }

      auto loops = doc->findChildren<Loop::ProcessModel*>();
      for (Loop::ProcessModel* lp : loops)
      {
        lp->interval().reset();
        lp->interval().executionFinished();
        lp->startEvent().setStatus(Scenario::ExecutionStatus::Editing, *lp);
        lp->endEvent().setStatus(Scenario::ExecutionStatus::Editing, *lp);
        lp->startState().setStatus(Scenario::ExecutionStatus::Editing);
        lp->endState().setStatus(Scenario::ExecutionStatus::Editing);
      }
    });
  }
}

void ApplicationPlugin::on_init()
{
  if (auto doc = currentDocument())
  {
    auto plugmodel = doc->context().findPlugin<Execution::DocumentPlugin>();
    if (!plugmodel)
      return;

    auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(
        &doc->model().modelDelegate());
    if (!scenar)
      return;

    auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
    // Disable listening for everything
    if (explorer
        && !doc->context()
                .app.settings<Execution::Settings::Model>()
                .getExecutionListening())
      explorer->deviceModel().listening().stop();

    auto state = Engine::score_to_ossia::state(
        scenar->baseScenario().startState(), plugmodel->context());
    state.launch();

    // If we can we resume listening
    if (!context.docManager.preparingNewDocument())
    {
      auto explorer = Explorer::try_deviceExplorerFromObject(*doc);
      if (explorer)
        explorer->deviceModel().listening().restore();
    }
  }
}

void ApplicationPlugin::initLocalTreeNodes(LocalTree::DocumentPlugin& lt)
{
  auto& appplug = *this;
  auto& root = lt.device().get_root_node();

  {
    auto n = root.create_child("running");
    auto p = n->create_parameter(ossia::val_type::BOOL);
    p->set_value(false);
    p->set_access(ossia::access_mode::GET);

    if (context.applicationSettings.gui)
    {
      auto& play_action = appplug.context.actions.action<Actions::Play>();
      connect(play_action.action(), &QAction::triggered, &lt, [=] {
        p->push_value(true);
      });

      auto& stop_action = context.actions.action<Actions::Stop>();
      connect(stop_action.action(), &QAction::triggered, &lt, [=] {
        p->push_value(false);
      });
    }
  }
  {
    auto local_play_node = root.create_child("play");
    auto local_play_address
        = local_play_node->create_parameter(ossia::val_type::BOOL);
    local_play_address->set_value(bool{false});
    local_play_address->set_access(ossia::access_mode::SET);
    local_play_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [=] {
        if (auto val = v.target<bool>())
        {
          if (!playing() && *val)
          {
            // not playing, play requested
            if (context.applicationSettings.gui)
            {
              auto& play_action = context.actions.action<Actions::Play>();
              play_action.action()->trigger();
            }
            else
            {
              this->on_play(true);
            }
          }
          else if (playing())
          {
            if (paused() == *val)
            {
              // paused, play requested
              // or playing, pause requested

              if (context.applicationSettings.gui)
              {
                auto& play_action = context.actions.action<Actions::Play>();
                play_action.action()->trigger();
              }
              else
              {
                this->on_play(true);
              }
            }
          }
        }
      });
    });
  }

  {
    auto local_play_node = root.create_child("global_play");
    auto local_play_address
        = local_play_node->create_parameter(ossia::val_type::BOOL);
    local_play_address->set_value(bool{false});
    local_play_address->set_access(ossia::access_mode::SET);
    local_play_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [=] {
        if (auto val = v.target<bool>())
        {
          if (!playing() && *val)
          {
            // not playing, play requested
            if (context.applicationSettings.gui)
            {
              auto& play_action
                  = context.actions.action<Actions::PlayGlobal>();
              play_action.action()->trigger();
            }
            else
            {
              if (auto doc = currentDocument())
              {
                auto& mod = doc->model().modelDelegate();
                auto scenar
                    = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
                if (scenar)
                {
                  on_play(scenar->baseInterval(), true, {}, TimeVal{});
                }
              }
            }
          }
          else if (playing())
          {
            if (paused() == *val)
            {
              // paused, play requested
              // or playing, pause requested

              if (context.applicationSettings.gui)
              {
                auto& play_action
                    = context.actions.action<Actions::PlayGlobal>();
                play_action.action()->trigger();
              }
              else
              {
                if (auto doc = currentDocument())
                {
                  auto& mod = doc->model().modelDelegate();
                  auto scenar
                      = dynamic_cast<Scenario::ScenarioDocumentModel*>(&mod);
                  if (scenar)
                  {
                    on_play(scenar->baseInterval(), true, {}, TimeVal{});
                  }
                }
              }
            }
          }
        }
      });
    });
  }

  {
    auto local_transport_node = root.create_child("transport");
    auto local_transport_address
        = local_transport_node->create_parameter(ossia::val_type::FLOAT);
    local_transport_address->set_value(bool{false});
    local_transport_address->set_access(ossia::access_mode::SET);
    local_transport_address->set_unit(ossia::millisecond_u{});
    local_transport_address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(this, [=] {
        on_transport(TimeVal::fromMsecs(ossia::convert<float>(v)));
      });
    });
  }

  {
    auto local_stop_node = root.create_child("stop");
    auto local_stop_address
        = local_stop_node->create_parameter(ossia::val_type::IMPULSE);
    local_stop_address->set_value(ossia::impulse{});
    local_stop_address->set_access(ossia::access_mode::SET);
    local_stop_address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(this, [=] {
        if (context.applicationSettings.gui)
        {
          auto& stop_action = context.actions.action<Actions::Stop>();
          stop_action.action()->trigger();
        }
        else
        {
          this->on_stop();
        }
      });
    });
  }

  {
    auto local_stop_node = root.create_child("reinit");
    auto local_stop_address
        = local_stop_node->create_parameter(ossia::val_type::IMPULSE);
    local_stop_address->set_value(ossia::impulse{});
    local_stop_address->set_access(ossia::access_mode::SET);
    local_stop_address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(this, [=] {
        if (context.applicationSettings.gui)
        {
          auto& stop_action = context.actions.action<Actions::Reinitialize>();
          stop_action.action()->trigger();
        }
        else
        {
          this->on_init();
        }
      });
    });
  }
  {
    auto node = root.create_child("exit");
    auto address = node->create_parameter(ossia::val_type::IMPULSE);
    address->set_value(ossia::impulse{});
    address->set_access(ossia::access_mode::SET);
    address->add_callback([&](const ossia::value&) {
      ossia::qt::run_async(this, [=] {
        if (context.applicationSettings.gui)
        {
          auto& stop_action = context.actions.action<Actions::Stop>();
          stop_action.action()->trigger();
        }
        else
        {
          this->on_stop();
        }

        QTimer::singleShot(500, [] {
          score::GUIApplicationInterface::instance().forceExit();
        });
      });
    });
  }

  {
    auto node = root.create_child("document");
    auto address = node->create_parameter(ossia::val_type::INT);
    address->set_value(0);
    address->set_access(ossia::access_mode::BI);
    address->add_callback([&](const ossia::value& v) {
      int val = v.get<int>();
      ossia::qt::run_async(this, [=] {
        if (context.applicationSettings.gui)
        {
          QTabWidget* docs
              = context.mainWindow->centralWidget()->findChild<QTabWidget*>(
                  "Documents", Qt::FindDirectChildrenOnly);
          SCORE_ASSERT(docs);
          if (docs)
          {
            docs->setCurrentIndex(std::clamp(val, 0, docs->count() - 1));
          }
        }
      });
    });
  }

  {
    auto node = root.create_child("reconnect");
    auto address = node->create_parameter(ossia::val_type::STRING);
    address->set_value("");
    address->set_access(ossia::access_mode::BI);
    address->add_callback([&](const ossia::value& v) {
      auto val = QString::fromStdString(v.get<std::string>());
      ossia::qt::run_async(this, [=, device = std::move(val)] {
        auto doc = currentDocument();
        if(!doc)
          return;

        auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
        plug.reconnect(device);
      });
    });
  }
}

std::unique_ptr<Execution::Clock>
ApplicationPlugin::makeClock(const Execution::Context& ctx)
{
  auto& s = context.settings<Execution::Settings::Model>();
  return s.makeClock(ctx);
}
}
