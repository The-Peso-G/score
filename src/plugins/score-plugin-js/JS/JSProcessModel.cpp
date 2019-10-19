// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "JSProcessModel.hpp"

#include "JS/JSProcessMetadata.hpp"

#include <JS/Qml/QmlObjects.hpp>
#include <Process/Dataflow/Port.hpp>
#include <State/Expression.hpp>

#include <score/document/DocumentInterface.hpp>
#include <score/model/Identifier.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/DeleteAll.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlComponent>
#include <QDebug>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(JS::ProcessModel)
namespace JS
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
{
  if (data.isEmpty())
  {
    setScript(
        R"_(import QtQuick 2.0
import Score 1.0
Item {
  ValueInlet { id: in1 }
  ValueOutlet { id: out1 }
  FloatSlider { id: sl; min: 10; max: 100; }

  function onTick(oldtime, time, position, offset) {
    out1.value = in1.value + sl.value * Math.random();
  }
})_");
  }
  else
  {
    setScript(data);
  }
  metadata().setInstanceName(*this);
}

ProcessModel::~ProcessModel()
{
  if (m_dummyObject)
    m_dummyObject->deleteLater();
}

void ProcessModel::setScript(const QString& script)
{
  m_watch.reset();
  if (m_dummyObject)
    m_dummyObject->deleteLater();
  m_dummyObject = nullptr;
  m_dummyComponent.reset();
  m_dummyComponent = std::make_unique<QQmlComponent>(&m_dummyEngine);

  m_script = script;
  scriptChanged(script);
  auto trimmed = script.trimmed();

  QByteArray data = trimmed.toUtf8();

  auto path = score::locateFilePath(
      trimmed, score::IDocument::documentContext(*this));
  if (QFileInfo{path}.exists())
  {
    m_watch = std::make_unique<QFileSystemWatcher>(QStringList{trimmed});
    connect(
        m_watch.get(),
        &QFileSystemWatcher::fileChanged,
        this,
        [=](const QString& path) {
          // Note:
          // https://stackoverflow.com/questions/18300376/qt-qfilesystemwatcher-signal-filechanged-gets-emited-only-once
          QTimer::singleShot(20, this, [this, path] {
            m_watch->addPath(path);
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
            {
              setQmlData(path.toUtf8(), true);
              m_watch->addPath(path);
            }
          });
        });

    setQmlData(path.toUtf8(), true);
  }
  else
  {
    setQmlData(data, false);
  }
}

void ProcessModel::setQmlData(const QByteArray& data, bool isFile)
{
  if (!isFile && !data.startsWith("import"))
    return;

  m_qmlData = data;
  if (m_dummyObject)
    m_dummyObject->deleteLater();
  m_dummyObject = nullptr;
  m_dummyComponent.reset();
  if (isFile)
  {
    m_dummyComponent = std::make_unique<QQmlComponent>(
        &m_dummyEngine, QUrl::fromLocalFile(data));
  }
  else
  {
    m_dummyComponent = std::make_unique<QQmlComponent>(&m_dummyEngine);
    m_dummyComponent->setData(data, QUrl());
  }

  const auto& errs = m_dummyComponent->errors();
  if (!errs.empty())
  {
    const auto& err = errs.first();
    qDebug() << err.line() << err.toString();
    scriptError(err.line(), err.toString());
    return;
  }

  std::vector<State::AddressAccessor> oldInletAddresses, oldOutletAddresses;
  std::vector<std::vector<Path<Process::Cable>>> oldInletCable, oldOutletCable;
  for (Process::Inlet* in : m_inlets)
  {
    oldInletAddresses.push_back(in->address());
    oldInletCable.push_back(in->cables());
  }
  for (Process::Outlet* in : m_outlets)
  {
    oldOutletAddresses.push_back(in->address());
    oldOutletCable.push_back(in->cables());
  }

  score::deleteAndClear(m_inlets);
  score::deleteAndClear(m_outlets);

  m_dummyObject = m_dummyComponent->create();

  {
    auto cld_inlet = m_dummyObject->findChildren<Inlet*>();
    int i = 0;
    for (auto n : cld_inlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      port->setCustomData(n->objectName());
      if (auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_inlets.push_back(port);
    }
  }

  {
    auto cld_outlet = m_dummyObject->findChildren<Outlet*>();
    int i = 0;
    for (auto n : cld_outlet)
    {
      auto port = n->make(Id<Process::Port>(i++), this);
      port->setCustomData(n->objectName());
      if (auto addr = State::parseAddressAccessor(n->address()))
        port->setAddress(std::move(*addr));
      m_outlets.push_back(port);
    }
  }
  scriptOk();

  std::size_t i = 0;
  for (Process::Inlet* in : m_inlets)
  {
    if (i < oldInletAddresses.size())
    {
      if (!oldInletAddresses[i].address.device.isEmpty())
        in->setAddress(oldInletAddresses[i]);
      for (const auto& cbl : oldInletCable[i])
        in->addCable(cbl);
    }
    i++;
  }
  i = 0;
  for (Process::Outlet* out : m_outlets)
  {
    if (i < oldOutletAddresses.size())
    {
      if (!oldOutletAddresses[i].address.device.isEmpty())
        out->setAddress(oldOutletAddresses[i]);
      for (const auto& cbl : oldOutletCable[i])
        out->addCable(cbl);
    }
    i++;
  }

  qmlDataChanged(data);
  inletsChanged();
  outletsChanged();
}
}
