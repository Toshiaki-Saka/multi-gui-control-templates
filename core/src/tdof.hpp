// tdof.hpp — internal C++ interface for the 2-DOF comparison core.

#ifndef TDOF_INTERNAL_HPP
#define TDOF_INTERNAL_HPP

#include "tdof_core.h"

#include <Eigen/Dense>
#include <vector>

namespace tdof {

using Vec = Eigen::VectorXd;
using Mat = Eigen::MatrixXd;

// ----- Transfer function ------------------------------------------------
//
// Coefficients are stored highest-power-first, matching numpy /
// python-control:  num[0] s^(n-1) + ... + num[n-1]
//                  -------------------------------------
//                  den[0] s^(m-1) + ... + den[m-1]
struct TransferFunction {
    std::vector<double> num;   // numerator coefficients
    std::vector<double> den;   // denominator coefficients

    TransferFunction() = default;
    TransferFunction(std::vector<double> n, std::vector<double> d)
        : num(std::move(n)), den(std::move(d)) {}
};

// Polynomial multiply (convolution). Highest-power-first in/out.
std::vector<double> poly_mul(const std::vector<double>& a,
                             const std::vector<double>& b);

// Polynomial add, aligned at the constant term (lowest power). Highest-
// power-first in/out; the shorter operand is zero-padded on the left.
std::vector<double> poly_add(const std::vector<double>& a,
                             const std::vector<double>& b);

// Series connection: (a.num*b.num) / (a.den*b.den).
TransferFunction series(const TransferFunction& a, const TransferFunction& b);

// Unity-feedback: feedback(G, 1) = G.num / (G.den + G.num).
TransferFunction feedback_unity(const TransferFunction& g);

// Strip leading (highest-power) coefficients that are ~0 so the leading
// term is meaningful; keeps at least one element.
void normalize_leading(std::vector<double>& p);

// ----- State-space + simulation ----------------------------------------

// Controllable canonical form of a (strictly or non-strictly proper) TF.
// Produces A (n x n), B (n x 1), C (1 x n), D (1 x 1) with n = deg(den).
// Requires deg(num) <= deg(den).
struct StateSpace {
    Mat A;
    Vec B;
    Vec C;   // stored as a column; used as a row via .transpose()
    double D = 0.0;
};

StateSpace tf_to_ss(const TransferFunction& tf);

// Simulate the forced response of a TF to input u sampled on a uniform
// grid of step dt (first-order hold / linear interpolation on the input,
// exact discretisation via matrix exponential — matching
// python-control.forced_response). Returns y of the same length as u.
std::vector<double> forced_response(const TransferFunction& tf,
                                    const std::vector<double>& u,
                                    double dt);

// Same but reusing a precomputed state space (avoids recomputing the
// canonical form for repeated calls with the same system).
std::vector<double> forced_response_ss(const StateSpace& ss,
                                       const std::vector<double>& u,
                                       double dt);

// ----- System builders (from config) -----------------------------------
TransferFunction build_plant(const TdofConfig& cfg);
TransferFunction build_pid(const TdofConfig& cfg);
TransferFunction build_reference_filter(const TdofConfig& cfg);
TransferFunction build_closed_loop(const TdofConfig& cfg);

}  // namespace tdof

#endif  // TDOF_INTERNAL_HPP
