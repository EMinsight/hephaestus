#include "steady_state_equation_system_problem_builder.hpp"

namespace hephaestus
{

void
SteadyStateEquationSystemProblemBuilder::SetOperatorGridFunctions()
{
  _problem->GetOperator()->SetGridFunctions();
}

void
SteadyStateEquationSystemProblemBuilder::ConstructOperator()
{
  _problem->ConstructOperator();
}

void
SteadyStateEquationSystemProblemBuilder::ConstructState()
{
  _problem->_f =
      std::make_unique<mfem::BlockVector>(_problem->GetOperator()->_true_offsets); // Vector of dofs
  _problem->GetOperator()->Init(*(_problem->_f)); // Set up initial conditions
}

} // namespace hephaestus
