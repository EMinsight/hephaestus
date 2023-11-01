#pragma once
#include "executioner_base.hpp"
#include "steady_state_problem_builder.hpp"

namespace hephaestus {

class SteadyExecutioner : public Executioner {
public:
  SteadyExecutioner() = default;
  explicit SteadyExecutioner(const hephaestus::InputParameters &params);

  void Init() override;

  void Solve() const override;

  void Execute() const override;

  hephaestus::SteadyStateProblem *problem;
};

} // namespace hephaestus