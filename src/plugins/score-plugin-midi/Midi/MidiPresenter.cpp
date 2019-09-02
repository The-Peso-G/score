// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Midi/Commands/AddNote.hpp>
#include <Midi/Commands/RemoveNotes.hpp>
#include <Midi/Commands/ScaleNotes.hpp>
#include <Midi/MidiDrop.hpp>
#include <Midi/MidiNoteView.hpp>
#include <Midi/MidiPresenter.hpp>
#include <Midi/MidiProcess.hpp>
#include <Midi/MidiView.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/Layer/LayerContextMenu.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/copy.hpp>

#include <ossia/detail/algorithms.hpp>
#include <QApplication>
#include <QAction>
#include <QInputDialog>
#include <QMenu>

#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#endif
namespace Midi
{
Presenter::Presenter(
    const Midi::ProcessModel& layer,
    View* view,
    const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{ctx, parent}
    , m_layer{layer}
    , m_view{view}
    , m_ongoing{ctx.commandStack}
    , m_zr{1.}
{
  putToFront();

  auto& model = layer;

  con(model, &ProcessModel::notesChanged, this, [&] {
    for (auto note : m_notes)
    {
      delete note;
    }

    m_notes.clear();

    for (auto& note : model.notes)
    {
      on_noteAdded(note);
    }
  });

  con(model, &ProcessModel::rangeChanged, this, [=](int min, int max) {
    m_view->setRange(min, max);
    for (auto note : m_notes)
      updateNote(*note);
  });
  m_view->setRange(model.range().first, model.range().second);
  model.notes.added.connect<&Presenter::on_noteAdded>(this);
  model.notes.removing.connect<&Presenter::on_noteRemoving>(this);

  connect(m_view, &View::doubleClicked, this, [&](QPointF pos) {
    CommandDispatcher<>{context().context.commandStack}.submit(
        new AddNote{layer, m_view->noteAtPos(pos)});
  });

  connect(m_view, &View::pressed, this, [&]() {
    m_context.context.focusDispatcher.focus(this);
  });
  connect(m_view, &View::dropReceived, this, &Presenter::on_drop);

  connect(m_view, &View::deleteRequested, this, [&] {
    CommandDispatcher<>{context().context.commandStack}.submit(
        new RemoveNotes{m_layer, selectedNotes()});
  });

  connect(
      m_view, &View::askContextMenu, this, &Presenter::contextMenuRequested);

  for (auto& note : model.notes)
  {
    on_noteAdded(note);
  }
#if __has_include(<valgrind/callgrind.h>)
  // CALLGRIND_START_INSTRUMENTATION;
#endif
}

Presenter::~Presenter()
{
#if __has_include(<valgrind/callgrind.h>)
  // CALLGRIND_STOP_INSTRUMENTATION;
#endif
}

void Presenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const Process::LayerContextMenuManager& cm)
{
  auto act = menu.addAction(tr("Rescale midi"));
  connect(act, &QAction::triggered, this, [&] {
    bool ok = true;
    double val = QInputDialog::getDouble(
        qApp->activeWindow(),
        tr("Rescale factor"),
        tr("Rescale factor"),
        1.0,
        0.0001,
        100.,
        8,
        &ok);
    if (!ok)
      return;

    CommandDispatcher<> c{context().context.commandStack};
    c.submit<RescaleMidi>(m_layer, val);
  });

  auto act2 = menu.addAction(tr("Rescale all midi"));
  connect(act2, &QAction::triggered, this, [&] {
    bool ok = true;
    double val = QInputDialog::getDouble(
        qApp->activeWindow(),
        tr("Rescale factor"),
        tr("Rescale factor"),
        1.0,
        0.0001,
        100.,
        8,
        &ok);
    if (!ok)
      return;

    MacroCommandDispatcher<RescaleAllMidi> disp{
        context().context.commandStack};
    auto& doc = context().context.document.model();
    auto midi = doc.findChildren<Midi::ProcessModel*>();
    for (auto ptr : midi)
    {
      disp.submit(new RescaleMidi{*ptr, val});
    }
    disp.commit();
  });
}

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
  m_view->setDefaultWidth(m_layer.duration().toPixels(m_zr));
  for (auto note : m_notes)
    updateNote(*note);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
  for (auto note : m_notes)
    updateNote(*note);
}

void Presenter::putToFront()
{
  m_view->setEnabled(true);
}

void Presenter::putBehind()
{
  m_view->setEnabled(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio zr)
{
  m_zr = zr;
  m_view->setDefaultWidth(m_layer.duration().toPixels(m_zr));
  for (auto note : m_notes)
    updateNote(*note);
}

void Presenter::parentGeometryChanged() {}

const Midi::ProcessModel& Presenter::model() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& Presenter::modelId() const
{
  return m_layer.id();
}

void Presenter::setupNote(NoteView& v)
{
  con(v.note, &Note::noteChanged, &v, [&] { updateNote(v); });

  con(v, &NoteView::deselectOtherNotes, this, [&] {
      for(NoteView* n : m_notes)
          n->setSelected(false);
  });
  con(v, &NoteView::noteChangeFinished, this, [&] {
    const auto [min, max] = this->m_layer.range();
    auto newPos = v.pos();
    auto rect = m_view->boundingRect();
    auto height = rect.height();

    // Snap to grid : we round y to the closest multiple of 127
    int note = ossia::clamp(
        int(max
            - (qMin(rect.bottom(), qMax(newPos.y(), rect.top())) / height)
                  * this->m_view->visibleCount()),
        min,
        max);

    auto notes = selectedNotes();
    auto it = ossia::find(notes, v.note.id());
    if (it == notes.end())
    {
      notes = {v.note.id()};
    }

    m_ongoing.submit(
        m_layer,
        notes,
        note - v.note.pitch(),
        newPos.x() / m_view->defaultWidth() - v.note.start());
    m_ongoing.commit();
  });

  con(v, &NoteView::noteScaled, this, [&](double newScale) {
    auto notes = selectedNotes();
    auto it = ossia::find(notes, v.note.id());
    if (it == notes.end())
    {
      notes = {v.note.id()};
    }

    auto dt = newScale - v.note.duration();
    CommandDispatcher<>{context().context.commandStack}.submit(
        new ScaleNotes{m_layer, notes, dt});
  });
}

void Presenter::updateNote(NoteView& v)
{
  const auto noteRect = v.computeRect();
  const auto newPos = noteRect.topLeft();
  if (newPos != v.pos())
    v.setPos(newPos);

  v.setWidth(noteRect.width());
  v.setHeight(noteRect.height());
}

void Presenter::on_noteAdded(const Note& n)
{
  auto v = new NoteView{n, m_view};
  setupNote(*v);
  updateNote(*v);
  m_notes.push_back(v);
}

void Presenter::on_noteRemoving(const Note& n)
{
  auto it = ossia::find_if(
      m_notes, [&](const auto& other) { return &other->note == &n; });
  if (it != m_notes.end())
  {
    delete *it;
    m_notes.erase(it);
  }
}

void Presenter::on_drop(const QPointF& pos, const QMimeData& md)
{
  auto songs = Midi::MidiTrack::parse(md, context().context);
  if (songs.empty())
    return;
  auto& song = songs.front();
  if (song.tracks.empty())
    return;

  auto& track = song.tracks[0];

  CommandDispatcher<> disp{m_context.context.commandStack};
  // Scale notes so that the durations are relative to the ratio of the song
  // duration & constraint duration
  for (auto& note : track.notes)
  {
    note.setStart(
        song.durationInMs * note.start() / m_layer.duration().msec());
    note.setDuration(
        song.durationInMs * note.duration() / m_layer.duration().msec());
  }
  disp.submit<Midi::ReplaceNotes>(
      m_layer, track.notes, track.min, track.max, m_layer.duration());
}

std::vector<Id<Note>> Presenter::selectedNotes() const
{
  using namespace boost::adaptors;

  std::vector<Id<Note>> res;
  boost::copy(
      m_notes | filtered([](NoteView* v) { return v->isSelected(); })
          | transformed([](NoteView* v) { return v->note.id(); }),
      std::back_inserter(res));
  return res;
}
}
