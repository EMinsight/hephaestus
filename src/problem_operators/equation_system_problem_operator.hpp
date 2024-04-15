#pragma once
#include "problem_operator.hpp"
#include "problem_operator_interface.hpp"

namespace hephaestus
{
/// Steady-state problem operator with an equation system.
class EquationSystemProblemOperator : public ProblemOperator, public EquationSystemInterface
{
public:
  EquationSystemProblemOperator(hephaestus::Problem &) = delete;

  EquationSystemProblemOperator(hephaestus::Problem & problem,
                                std::unique_ptr<EquationSystem> equation_system)
    : ProblemOperator(problem), _equation_system(std::move(equation_system))
  {
  }

  ~EquationSystemProblemOperator() override = default;

  [[nodiscard]] EquationSystem * GetEquationSystem() const override
  {
    if (!_equation_system)
    {
      MFEM_ABORT("No equation system has been added to ProblemOperator.");
    }

    return _equation_system.get();
  }

private:
  std::unique_ptr<EquationSystem> _equation_system{nullptr};
};

} // namespace hephaestus