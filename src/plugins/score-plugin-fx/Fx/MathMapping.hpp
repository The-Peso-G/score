#pragma once
#include <Fx/MathGenerator.hpp>
namespace Nodes
{
namespace MathMapping
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Custom Mapping (Value)";
    static const constexpr auto objectKey = "MathMapping";
    static const constexpr auto category = "Control";
    static const constexpr auto author = "ossia score, ExprTK (Arash Partow)";
    static const constexpr auto kind = Process::ProcessCategory::Mapping;
    static const constexpr auto description
        = "Applies a math expression to an input.\n"
          "Available variables: a,b,c, t (samples), dt (delta), pos (position in parent), x (value)\n"
          "See the documentation at http://www.partow.net/programming/exprtk";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid
        = make_uuid("ae84e8b6-74ff-4259-aeeb-305d95cdfcab");

    static const constexpr value_in value_ins[]{"in"};
    static const constexpr value_out value_outs[]{"out"};

    static const constexpr auto controls = std::make_tuple(
        Control::LineEdit("Expression (ExprTK)", "cos(t) + log(pos * x / dt)"),
        Control::FloatSlider("Param (a)", 0., 1., 0.5),
        Control::FloatSlider("Param (b)", 0., 1., 0.5),
        Control::FloatSlider("Param (c)", 0., 1., 0.5));
  };
  struct State
  {
    State()
    {
      syms.add_variable("x", cur_value);
      syms.add_variable("t", cur_time);
      syms.add_variable("dt", cur_deltatime);
      syms.add_variable("pos", cur_pos);
      syms.add_variable("a", p1);
      syms.add_variable("b", p2);
      syms.add_variable("c", p3);
      syms.add_constants();

      expr.register_symbol_table(syms);
    }
    double cur_value{};
    double cur_time{};
    double cur_deltatime{};
    double cur_pos{};
    double p1{}, p2{}, p3{};
    exprtk::symbol_table<double> syms;
    exprtk::expression<double> expr;
    exprtk::parser<double> parser;
    std::string cur_expr_txt;
    bool ok = false;
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(const ossia::value_port& input,
      const std::string& expr,
      float a,
      float b,
      float c,
      ossia::value_port& output,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& self)
  {
    if (!updateExpr(self, expr))
      return;

    for (const ossia::timed_value& v : input.get_data())
    {
      self.cur_value = ossia::convert<double>(v.value);
      self.cur_time = tk.date.impl;
      self.cur_deltatime = tk.date.impl - tk.prev_date.impl;
      self.cur_pos = tk.position();
      self.p1 = a;
      self.p2 = b;
      self.p3 = c;

      auto res = self.expr.value();
      output.write_value(res, v.timestamp);
    }
  }

  template<typename... Args>
  static void item(Args&&... args)
  {
    Nodes::mathItem(Metadata::controls, std::forward<Args>(args)...);
  }
};
}

namespace MathAudioFilter
{
struct Node
{
  struct Metadata : Control::Meta_base
  {
    static const constexpr auto prettyName = "Audio Filter";
    static const constexpr auto objectKey = "MathAudioFilter";
    static const constexpr auto category = "Audio";
    static const constexpr auto author = "ossia score, ExprTK (Arash Partow)";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto kind = Process::ProcessCategory::AudioEffect;
    static const constexpr auto description
        = "Applies a math expression to an audio input.\n"
          "Available variables: a,b,c, t (samples), fs (sampling frequency), \n"
          "x (value), px (previous value)\n"
          "See the documentation at http://www.partow.net/programming/exprtk";
    static const constexpr auto uuid
        = make_uuid("13e1f4b0-1c2c-40e6-93ad-dfc91aac5335");

    static const constexpr audio_in audio_ins[]{"in"};
    static const constexpr audio_out audio_outs[]{"out"};

    static const constexpr auto controls = std::make_tuple(
        Control::LineEdit("Expression (ExprTK)", "a * x"),
        Control::FloatSlider("Param (a)", 0., 1., 0.5),
        Control::FloatSlider("Param (b)", 0., 1., 0.5),
        Control::FloatSlider("Param (c)", 0., 1., 0.5));
  };

  struct State
  {
    State()
    {
      syms.add_variable("x", cur_in);
      syms.add_variable("px", prev_in);
      syms.add_variable("t", cur_time);
      syms.add_variable("a", p1);
      syms.add_variable("b", p2);
      syms.add_variable("c", p3);
      syms.add_variable("fs", fs);
      syms.add_constants();

      expr.register_symbol_table(syms);
    }
    double cur_in{};
    double prev_in{};
    double cur_time{};
    double p1{}, p2{}, p3{};
    double fs{44100};
    exprtk::symbol_table<double> syms;
    exprtk::expression<double> expr;
    exprtk::parser<double> parser;
    std::string cur_expr_txt;
    bool ok = false;
  };

  using control_policy = ossia::safe_nodes::last_tick;
  static void
  run(const ossia::audio_port& input,
      const std::string& expr,
      float a,
      float b,
      float c,
      ossia::audio_port& output,
      ossia::token_request tk,
      ossia::exec_state_facade st,
      State& self)
  {
    if (tk.date > tk.prev_date)
    {
      auto count = tk.date - tk.prev_date;
      if (!updateExpr(self, expr))
        return;

      if (input.samples.empty()
          || (int64_t)input.samples[0].size() < tk.offset + count)
        return;

      output.samples.resize(1);
      auto& out = output.samples[0];
      if ((int64_t)out.size() < tk.offset + count)
        out.resize(tk.offset + count);

      self.p1 = a;
      self.p2 = b;
      self.p3 = c;
      self.fs = st.sampleRate();
      for (int64_t i = 0; i < count; i++)
      {
        self.cur_in = input.samples[0][tk.offset + i];
        self.cur_time = tk.prev_date + i;
        out[tk.offset + i] = self.expr.value();
        self.prev_in = self.cur_in;
      }
    }
  }

  template<typename... Args>
  static void item(Args&&... args)
  {
    Nodes::mathItem(Metadata::controls, std::forward<Args>(args)...);
  }
};
}
}

namespace Control{
template <>
struct HasCustomUI<Nodes::MathAudioFilter::Node> : std::true_type { };
template <>
struct HasCustomUI<Nodes::MathAudioGenerator::Node> : std::true_type { };
template <>
struct HasCustomUI<Nodes::MathMapping::Node> : std::true_type { };
template <>
struct HasCustomUI<Nodes::MathGenerator::Node> : std::true_type { };
}
