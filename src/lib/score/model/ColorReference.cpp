// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ColorReference.hpp"

#include <score/tools/Debug.hpp>

namespace score
{
optional<ColorRef> ColorRef::ColorFromString(const QString& txt) noexcept
{
  auto res = score::Skin::instance().fromString(txt);

  if (res)
    return ColorRef(res);
  else
    return {};
}

optional<ColorRef> ColorRef::SimilarColor(QColor other) noexcept
{
  SCORE_TODO_("Load similar colors");
  return {};
}
}
