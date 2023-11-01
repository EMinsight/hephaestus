#include "open_coil.hpp"

namespace hephaestus {

double highV(const mfem::Vector &x, double t) { return 1.0; }
double lowV(const mfem::Vector &x, double t) { return 0.0; }

///// THESE FUNCTIONS WILL EVENTUALLY GO INTO A UTILS FILE ///////////

double calcFlux(mfem::GridFunction *v_field, int face_attr) {

  double flux = 0.0;
  double area = 0.0;

  mfem::FiniteElementSpace *FES = v_field->FESpace();
  mfem::Mesh *mesh = FES->GetMesh();

  mfem::Vector local_dofs, normal_vec;
  mfem::DenseMatrix dshape;
  mfem::Array<int> dof_ids;

  for (int i = 0; i < mesh->GetNBE(); i++) {

    if (mesh->GetBdrAttribute(i) != face_attr)
      continue;

    mfem::FaceElementTransformations *FTr =
        mesh->GetFaceElementTransformations(mesh->GetBdrFace(i));
    if (FTr == nullptr)
      continue;

    const mfem::FiniteElement &elem = *FES->GetFE(FTr->Elem1No);
    const int int_order = 2 * elem.GetOrder() + 3;
    const mfem::IntegrationRule &ir =
        mfem::IntRules.Get(FTr->FaceGeom, int_order);

    FES->GetElementDofs(FTr->Elem1No, dof_ids);
    v_field->GetSubVector(dof_ids, local_dofs);
    const int space_dim = FTr->Face->GetSpaceDim();
    normal_vec.SetSize(space_dim);
    dshape.SetSize(elem.GetDof(), space_dim);

    for (int j = 0; j < ir.GetNPoints(); j++) {

      const mfem::IntegrationPoint &ip = ir.IntPoint(j);
      mfem::IntegrationPoint eip;
      FTr->Loc1.Transform(ip, eip);
      FTr->Face->SetIntPoint(&ip);
      double face_weight = FTr->Face->Weight();
      double val = 0.0;
      FTr->Elem1->SetIntPoint(&eip);
      elem.CalcVShape(*FTr->Elem1, dshape);
      mfem::CalcOrtho(FTr->Face->Jacobian(), normal_vec);
      val += dshape.InnerProduct(normal_vec, local_dofs) / face_weight;

      // Measure the area of the boundary
      area += ip.weight * face_weight;

      // Integrate alpha * n.Grad(x) + beta * x
      flux += val * ip.weight * face_weight;
    }
  }

  double total_flux;
  MPI_Allreduce(&flux, &total_flux, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  return total_flux;
}

void SubdomainToArray(const std::vector<hephaestus::Subdomain> &sd,
                      mfem::Array<int> &arr) {
  arr.DeleteAll();
  for (auto s : sd)
    arr.Append(s.id);
}

void SubdomainToArray(const hephaestus::Subdomain &sd, mfem::Array<int> &arr) {
  arr.DeleteAll();
  arr.Append(sd.id);
}

template <typename T> void ifDelete(T *ptr) {
  if (ptr != nullptr)
    delete ptr;
}

/////////////////////////////////////////////////////////////////////

OpenCoilSolver::OpenCoilSolver(
    const hephaestus::InputParameters &params,
    const std::vector<hephaestus::Subdomain> &coil_dom,
    const std::pair<int, int> electrodes, const int order)
    : J_gf_name_(params.GetParam<std::string>("SourceName")),
      V_gf_name_(params.GetParam<std::string>("PotentialName")),
      I_coef_name_(params.GetParam<std::string>("IFuncCoefName")),
      coil_domains_(coil_dom), order_(order), elec_attrs_(electrodes),
      coef1_(nullptr), coef0_(nullptr), mesh_parent_(nullptr),
      J_parent_(nullptr), V_parent_(nullptr), J_(nullptr), V_(nullptr),
      high_src_(nullptr), low_src_(nullptr) {

  coef1_ = new mfem::ConstantCoefficient(1.0);
  coef0_ = new mfem::ConstantCoefficient(0.0);
  ref_face_ = elec_attrs_.first;
}

OpenCoilSolver::~OpenCoilSolver() {

  ifDelete(coef1_);
  ifDelete(coef0_);

  ifDelete(mesh_);
  ifDelete(H1_Collection_);
  ifDelete(HCurl_Collection_);
  ifDelete(H1FESpace_);
  ifDelete(HCurlFESpace_);

  ifDelete(J_);
  ifDelete(V_);

  ifDelete(high_src_);
  ifDelete(low_src_);

  ifDelete(sps_);
  ifDelete(sps_params_);
  ifDelete(current_solver_options_);

  ifDelete(gridfunctions_);
  ifDelete(fespaces_);
  ifDelete(bc_maps_);
  ifDelete(coefs_);
  ifDelete(high_DBC_);
  ifDelete(low_DBC_);
}

void OpenCoilSolver::Init(hephaestus::GridFunctions &gridfunctions,
                          const hephaestus::FESpaces &fespaces,
                          hephaestus::BCMap &bc_map,
                          hephaestus::Coefficients &coefficients) {

  Itotal_ = coefficients.scalars.Get(I_coef_name_);
  if (Itotal_ == nullptr) {
    const std::string error_message = I_coef_name_ +
                                      " not found in coefficients when "
                                      "creating OpenCoilSolver\n";
    mfem::mfem_error(error_message.c_str());
  }

  J_parent_ = gridfunctions.Get(J_gf_name_);
  if (J_parent_ == nullptr) {
    const std::string error_message = J_gf_name_ +
                                      " not found in gridfunctions when "
                                      "creating OpenCoilSolver\n";
    mfem::mfem_error(error_message.c_str());
  }

  V_parent_ = gridfunctions.Get(V_gf_name_);
  if (V_parent_ == nullptr) {
    std::cout << V_gf_name_ + " not found in gridfunctions when "
                              "creating OpenCoilSolver. ";
  }

  mesh_parent_ = J_parent_->ParFESpace()->GetParMesh();

  initChildMesh();
  makeFESpaces();
  makeGridFunctions();
  setBCs();
  SPSCurrent();
}

void OpenCoilSolver::Apply(mfem::ParLinearForm *lf) {

  // The transformation and integration points themselves are not relevant, it's
  // just so we can call Eval
  mfem::ElementTransformation *Tr = mesh_parent_->GetElementTransformation(0);
  const mfem::IntegrationPoint &ip =
      mfem::IntRules.Get(J_parent_->ParFESpace()->GetFE(0)->GetGeomType(), 1)
          .IntPoint(0);

  double I = Itotal_->Eval(*Tr, ip);
  *J_ *= I;
  mesh_->Transfer(*J_, *J_parent_);
  *J_ /= I;

  if (V_parent_ != nullptr) {
    *V_ *= I;
    mesh_->Transfer(*V_, *V_parent_);
    *V_ /= I;
  }

  lf->Add(1.0, *J_parent_);
}

void OpenCoilSolver::SubtractSource(mfem::ParGridFunction *gf) {}

void OpenCoilSolver::initChildMesh() {

  mfem::Array<int> doms_array;
  SubdomainToArray(coil_domains_, doms_array);
  mesh_ = new mfem::ParSubMesh(
      mfem::ParSubMesh::CreateFromDomain(*mesh_parent_, doms_array));
}

void OpenCoilSolver::makeFESpaces() {

  H1_Collection_ = new mfem::H1_FECollection(order_, mesh_->Dimension());
  HCurl_Collection_ = new mfem::ND_FECollection(order_, mesh_->Dimension());
  H1FESpace_ = new mfem::ParFiniteElementSpace(mesh_, H1_Collection_);
  HCurlFESpace_ = new mfem::ParFiniteElementSpace(mesh_, HCurl_Collection_);
}

void OpenCoilSolver::makeGridFunctions() {

  if (V_ == nullptr)
    V_ = new mfem::ParGridFunction(H1FESpace_);

  if (J_ == nullptr)
    J_ = new mfem::ParGridFunction(HCurlFESpace_);

  *V_ = 0.0;
  *J_ = 0.0;
}

void OpenCoilSolver::setBCs() {

  if (high_terminal_.Size() == 0)
    high_terminal_.Append(elec_attrs_.first);
  if (low_terminal_.Size() == 0)
    low_terminal_.Append(elec_attrs_.second);

  if (high_src_ == nullptr)
    high_src_ = new mfem::FunctionCoefficient(highV);
  if (low_src_ == nullptr)
    low_src_ = new mfem::FunctionCoefficient(lowV);

  high_DBC_ = new hephaestus::FunctionDirichletBC(std::string("V"),
                                                  high_terminal_, high_src_);
  low_DBC_ = new hephaestus::FunctionDirichletBC(std::string("V"),
                                                 low_terminal_, low_src_);

  bc_maps_ = new hephaestus::BCMap;
  bc_maps_->Register("high_potential", high_DBC_, true);
  bc_maps_->Register("low_potential", low_DBC_, true);
}

void OpenCoilSolver::SPSCurrent() {

  fespaces_ = new hephaestus::FESpaces;
  fespaces_->Register(std::string("HCurl"), HCurlFESpace_, true);
  fespaces_->Register(std::string("H1"), H1FESpace_, true);

  gridfunctions_ = new hephaestus::GridFunctions;
  gridfunctions_->Register(std::string("source"), J_, true);
  gridfunctions_->Register(std::string("V"), V_, true);

  current_solver_options_ = new hephaestus::InputParameters;
  current_solver_options_->SetParam("Tolerance", float(1.0e-9));
  current_solver_options_->SetParam("MaxIter", (unsigned int)1000);
  current_solver_options_->SetParam("PrintLevel", 1);

  sps_params_ = new hephaestus::InputParameters;
  sps_params_->SetParam("SourceName", std::string("source"));
  sps_params_->SetParam("PotentialName", std::string("V"));
  sps_params_->SetParam("HCurlFESpaceName", std::string("HCurl"));
  sps_params_->SetParam("H1FESpaceName", std::string("H1"));
  sps_params_->SetParam("SolverOptions", *current_solver_options_);
  sps_params_->SetParam("ConductivityCoefName",
                        std::string("magnetic_permeability"));

  coefs_ = new hephaestus::Coefficients;
  coefs_->scalars.Register("magnetic_permeability", coef1_, false);

  sps_ = new hephaestus::ScalarPotentialSource(*sps_params_);
  sps_->Init(*gridfunctions_, *fespaces_, *bc_maps_, *coefs_);

  mfem::ParLinearForm dummy(HCurlFESpace_);
  sps_->Apply(&dummy);

  // Clean the divergence of the two J fields
  cleanDivergence(gridfunctions_, std::string("source"), std::string("V"),
                  bc_maps_);

  // Normalise the current through the wedges and use them as a reference
  double flux = calcFlux(J_, ref_face_);
  *J_ /= abs(flux);
  if (V_) *V_ /= abs(flux);
}

void OpenCoilSolver::cleanDivergence(hephaestus::GridFunctions *gridfunctions,
                                     std::string J_name, std::string V_name,
                                     hephaestus::BCMap *bc_map) {

  hephaestus::InputParameters pars;
  hephaestus::FESpaces fes;

  pars.SetParam("VectorGridFunctionName", J_name);
  pars.SetParam("ScalarGridFunctionName", V_name);
  hephaestus::HelmholtzProjector projector(pars);
  projector.Project(*gridfunctions, fes, *bc_map);
}

void OpenCoilSolver::setRefFace(const int face) { ref_face_ = face; }

} // namespace hephaestus