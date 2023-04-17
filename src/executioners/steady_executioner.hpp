#pragma once
#include "executioner.hpp"

namespace hephaestus {

class SteadyExecutioner : public ExecutionerBase {
private:
public:
  SteadyExecutioner() = default;
  explicit SteadyExecutioner(const hephaestus::InputParameters &params);

  void Init() override;

  void Solve() const override;

  hephaestus::HertzFormulation *formulation;
};

} // namespace hephaestus
