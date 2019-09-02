// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/ApplicationPlugin.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/messages/MessagesPanel.hpp>

#include <ossia-qt/qt_logger.hpp>
#include <ossia/detail/logger.hpp>

#include <QDockWidget>

#include <vector>

struct VisitorVariant;

namespace Explorer
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

void ApplicationPlugin::setupConnections(
    score::MessagesPanelDelegate& messages,
    Device::DeviceList& devices)
{
  m_inbound = QObject::connect(
      &devices,
      &Device::DeviceList::logInbound,
      &messages,
      [&messages](const QString& str) {
        messages.push(str, score::log::dark1);
      },
      Qt::QueuedConnection);

  m_outbound = QObject::connect(
      &devices,
      &Device::DeviceList::logOutbound,
      &messages,
      [&messages](const QString& str) {
        messages.push(str, score::log::dark2);
      },
      Qt::QueuedConnection);

  auto qt_sink
      = dynamic_cast<ossia::qt::log_sink*>(&*ossia::logger().sinks()[1]);
  if (qt_sink)
  {
    m_error = QObject::connect(
        qt_sink,
        &ossia::qt::log_sink::l,
        &messages,
        [&messages](spdlog::level::level_enum l, const QString& m) {
          messages.push(m, score::log::dark3);
        },
        Qt::QueuedConnection);
  }
}

void ApplicationPlugin::on_newDocument(score::Document& doc)
{
  score::addDocumentPlugin<DeviceDocumentPlugin>(doc);
}

void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc,
    score::Document* newdoc)
{
  disableConnections();
  QObject::disconnect(m_visible);

  if (olddoc)
  {
    auto& doc_plugin = olddoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(false);
  }

  if (newdoc)
  {
    // Connect devices
    auto& doc_plugin = newdoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(true);

    // Set-up logging
    auto& devices = doc_plugin.list();

    auto messages = context.findPanel<score::MessagesPanelDelegate>();
    if (messages)
    {
      if (auto qw = messages->dock())
      {
        auto func = [=, &devices](bool visible) {
          disableConnections();
          if (visible)
          {
            setupConnections(*messages, devices);
          }
          devices.setLogging(visible);
        };

        m_visible = QObject::connect(
            qw, &QDockWidget::visibilityChanged, messages, func);

        func(qw->isVisible());
      }
    }
  }
}
}
