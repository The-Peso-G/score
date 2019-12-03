#pragma once
#include <Media/Effect/EffectProcessMetadata.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <score/model/ObjectRemover.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/tools/std/IndirectContainer.hpp>
#include <ossia/detail/algorithms.hpp>

#include <score_plugin_media_export.h>
#include <verdigris>
namespace Media
{
namespace Effect
{
class ProcessModel;

template <typename T>
class EntityList
{
public:
  // The real interface starts here
  using value_type = T;
  auto begin() const { return score::make_indirect_iterator(m_list.begin()); }
  auto cbegin() const
  {
    return score::make_indirect_iterator(m_list.cbegin());
  }
  auto end() const { return score::make_indirect_iterator(m_list.end()); }
  auto cend() const { return score::make_indirect_iterator(m_list.cend()); }
  auto size() const { return m_list.size(); }
  bool empty() const { return m_list.empty(); }
  auto& unsafe_list() { return m_list; }
  const auto& list() const { return m_list; }
  const auto& get() const { return m_list.get(); }
  T& at(const Id<T>& id)
  {
    auto it = find(id);
    SCORE_ASSERT(it != m_list.end());
    return *it;
  }
  T& at(const Id<T>& id) const
  {
    auto it = find(id);
    SCORE_ASSERT(it != m_list.end());
    return **it;
  }
  T& at_pos(std::size_t n) const
  {
    SCORE_ASSERT(n < m_list.size());
    auto it = m_list.begin();
    std::advance(it, n);
    return **it;
  }
  auto find(const Id<T>& id) const
  {
    return ossia::find_if(m_list, [&](auto ptr) { return ptr->id() == id; });
  }
  auto find(const Id<T>& id)
  {
    return ossia::find_if(m_list, [&](auto ptr) { return ptr->id() == id; });
  }

  auto index(const Id<T>& id) const
  {
    auto it
        = ossia::find_if(m_list, [&](auto ptr) { return ptr->id() == id; });
    ;
    SCORE_ASSERT(it != m_list.end());
    return std::distance(m_list.begin(), it);
  }

  // public:
  mutable Nano::Signal<void(T&)> mutable_added;
  mutable Nano::Signal<void(const T&)> added;
  mutable Nano::Signal<void(const T&)> removing;
  mutable Nano::Signal<void(const T&)> removed;
  mutable Nano::Signal<void()> orderChanged;

  void add(T* t)
  {
    SCORE_ASSERT(t);
    unsafe_list().push_back(t);

    mutable_added(*t);
    added(*t);
  }

  void erase(T& elt)
  {
    auto it = ossia::find(m_list, &elt);
    SCORE_ASSERT(it != m_list.end());

    removing(elt);
    m_list.erase(it);
    removed(elt);
  }

  void remove(T& elt)
  {
    erase(elt);
    delete &elt;
  }

  void remove(T* elt) { remove(*elt); }

  void remove(const Id<T>& id)
  {
    auto it = find(id);
    SCORE_ASSERT(it != m_list.end());
    auto& elt = **it;

    removing(elt);
    m_list.erase(it);
    removed(elt);
    delete &elt;
  }

  void clear()
  {
    while (!m_list.empty())
    {
      remove(*m_list.begin());
    }
  }

  void insert_at(std::size_t pos, T* t)
  {
    SCORE_ASSERT(pos <= m_list.size());
    auto it = m_list.begin();
    std::advance(it, pos);

    m_list.insert(it, t);
    mutable_added(*t);
    added(*t);
  }
  void move(const Id<T>& id, std::size_t pos)
  {
    auto new_it = m_list.begin();
    std::advance(new_it, pos);
    auto it1 = find(id);
    m_list.splice(new_it, m_list, it1);

    orderChanged();
  }

private:
  std::list<T*> m_list;
};

/**
 * @brief The Media::Effect::ProcessModel class
 *
 * This class represents an effect chain.
 * Each effect should provide a component that will create
 * the corresponding LibMediaStream effect.
 *
 * Chaining two effects blocks [A] -> [B] is akin to
 * doing :
 *
 * MakeEffectSound(MakeEffectSound(Original sound, A, 0, 0), B, 0, 0)
 *
 */
class SCORE_PLUGIN_MEDIA_EXPORT ProcessModel final
    : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Effect::ProcessModel)

  W_OBJECT(ProcessModel)

public:
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ~ProcessModel() override;

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init()
  {
    m_inlets.push_back(inlet.get());
    m_outlets.push_back(outlet.get());
  }
  const EntityList<Process::ProcessModel>& effects() const
  {
    return m_effects;
  }

  void insertEffect(Process::ProcessModel* eff, int pos);
  void removeEffect(const Id<Process::ProcessModel>&);
  void moveEffect(const Id<Process::ProcessModel>&, int new_pos);

  int effectPosition(const Id<Process::ProcessModel>& e) const;

  std::unique_ptr<Process::Inlet> inlet{};
  std::unique_ptr<Process::AudioOutlet> outlet{};

  void checkChaining();
  bool badChaining() const { return m_badChaining; }

public:
  void setBadChaining(bool badChaining)
  {
    if (m_badChaining == badChaining)
      return;

    m_badChaining = badChaining;
    badChainingChanged(m_badChaining);
  };
  W_SLOT(setBadChaining)

public:
  void effectsChanged() W_SIGNAL(effectsChanged);

  void badChainingChanged(bool badChaining)
      W_SIGNAL(badChainingChanged, badChaining);

private:
  Selection selectableChildren() const noexcept override;
  Selection selectedChildren() const noexcept override;
  void setSelection(const Selection& s) const noexcept override;
  // The actual effect instances
  EntityList<Process::ProcessModel> m_effects;
  bool m_badChaining{false};

  W_PROPERTY(
      bool,
      badChaining READ badChaining WRITE setBadChaining NOTIFY
          badChainingChanged)
};

class EffectRemover : public score::ObjectRemover
{
  SCORE_CONCRETE("d26887f9-f17c-4a8f-957c-77645144c8af")
  bool remove(const Selection& s, const score::DocumentContext& ctx) override;
};
}
}
