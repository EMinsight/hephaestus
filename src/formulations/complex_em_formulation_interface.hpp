#pragma once
#include "mfem.hpp"

namespace hephaestus {

// Specifies interface specific to EM formulations.
class ComplexEMFormulationInterface {
public:
  ComplexEMFormulationInterface(){};

  // Enable auxiliary calculation of J ∈ H(div)
  virtual void registerCurrentDensityAux(const std::string &j_field_real_name,
                                         const std::string &j_field_imag_name) {
    MFEM_ABORT("Current density auxsolver not available for this formulation");
  };

  // Enable auxiliary calculation of B ∈ H(div)
  virtual void
  registerMagneticFluxDensityAux(const std::string &b_field_real_name,
                                 const std::string &b_field_imag_name) {
    MFEM_ABORT(
        "Magnetic flux density auxsolver not available for this formulation");
  };

  // Enable auxiliary calculation of E ∈ H(curl)
  virtual void registerElectricFieldAux(const std::string &e_field_real_name,
                                        const std::string &e_field_imag_name) {
    MFEM_ABORT("Electric field auxsolver not available for this formulation");
  };

  // Enable auxiliary calculation of H ∈ H(curl)
  virtual void registerMagneticFieldAux(const std::string &h_field_real_name,
                                        const std::string &h_field_imag_name) {
    MFEM_ABORT("Magnetic field auxsolver not available for this formulation");
  };

  // Enable auxiliary calculation of P ∈ L2
  virtual void
  registerJouleHeatingDensityAux(const std::string &p_field_name,
                                 const std::string &e_field_real_name,
                                 const std::string &e_field_imag_name,
                                 const std::string &conductivity_coef_name) {
    MFEM_ABORT("Joule heating auxsolver not available for this formulation");
  };
};
} // namespace hephaestus