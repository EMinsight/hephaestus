#include "scaled_vector_gridfunction_aux.hpp"

namespace hephaestus
{

ScaledVectorGridFunctionAux::ScaledVectorGridFunctionAux(
    const std::string & input_gf_name,
    const std::string & scaled_gf_name,
    const std::string & coef_name,
    const double & aConst,
    const hephaestus::InputParameters & solver_options)
  : AuxSolver(),
    _input_gf_name(input_gf_name),
    _scaled_gf_name(scaled_gf_name),
    _coef_name(coef_name),
    _aConst(aConst),
    _solver_options(solver_options),
    coef(nullptr),
    input_gf(nullptr),
    scaled_gf(nullptr),
    test_fes(nullptr),
    trial_fes(nullptr),
    a(nullptr),
    a_mixed(nullptr),
    a_mat(nullptr),
    mixed_mat(nullptr),
    solver(nullptr)
{
}

void
ScaledVectorGridFunctionAux::Init(const hephaestus::GridFunctions & gridfunctions,
                                  hephaestus::Coefficients & coefficients)
{
  input_gf = gridfunctions.Get(_input_gf_name);
  if (input_gf == nullptr)
  {
    MFEM_ABORT("GridFunction " << _input_gf_name
                               << " not found when initializing ScaledVectorGridFunctionAux");
  }
  scaled_gf = gridfunctions.Get(_scaled_gf_name);
  if (scaled_gf == nullptr)
  {
    MFEM_ABORT("GridFunction " << _scaled_gf_name
                               << " not found when initializing ScaledVectorGridFunctionAux");
  }
  coef = dynamic_cast<mfem::Coefficient *>(coefficients.scalars.Get(_coef_name));
  if (coef == nullptr)
  {
    MFEM_ABORT("Coefficient " << _coef_name
                              << " not found when initializing ScaledVectorGridFunctionAux");
  }

  test_fes = scaled_gf->ParFESpace();
  trial_fes = input_gf->ParFESpace();
  buildBilinearForm();
  buildMixedBilinearForm();
  a_mat = std::unique_ptr<mfem::HypreParMatrix>(a->ParallelAssemble());

  solver = std::make_unique<hephaestus::DefaultJacobiPCGSolver>(_solver_options, *a_mat);
}

void
ScaledVectorGridFunctionAux::buildBilinearForm()
{
  a = std::make_unique<mfem::ParBilinearForm>(test_fes);
  a->AddDomainIntegrator(new mfem::VectorFEMassIntegrator());
  a->Assemble();
  a->Finalize();
}

void
ScaledVectorGridFunctionAux::buildMixedBilinearForm()
{
  a_mixed = std::make_unique<mfem::ParMixedBilinearForm>(trial_fes, test_fes);
  a_mixed->AddDomainIntegrator(new mfem::MixedVectorMassIntegrator(*coef));
  a_mixed->Assemble();
  a_mixed->Finalize();
}

void
ScaledVectorGridFunctionAux::Solve(double t)
{
  mfem::Vector B(test_fes->GetTrueVSize());  // Linear form true DOFs
  mfem::Vector X(test_fes->GetTrueVSize());  // H(Div) gridfunction true DOFs
  mfem::Vector P(trial_fes->GetTrueVSize()); // H(Curl) gridfunction true DOFs
  B = 0.0;
  input_gf->GetTrueDofs(P);

  // Reassemble in case coef has changed
  a_mixed->Update();
  a_mixed->Assemble();
  a_mixed->Finalize();

  mixed_mat = std::unique_ptr<mfem::HypreParMatrix>(a_mixed->ParallelAssemble());
  mixed_mat->AddMult(P, B, _aConst);

  X = 0.0;
  solver->Mult(B, X);
  scaled_gf->SetFromTrueDofs(X);
}

ScaledVectorGridFunctionAux::~ScaledVectorGridFunctionAux() {}

} // namespace hephaestus
