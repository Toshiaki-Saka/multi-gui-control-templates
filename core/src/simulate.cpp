// simulate.cpp — controllable-canonical-form realisation and exact
// (ZOH) forced-response simulation, matching python-control's
// forced_response for a uniform time grid.

#include "tdof.hpp"

#include <unsupported/Eigen/MatrixFunctions>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tdof {

StateSpace tf_to_ss(const TransferFunction& tf_in) {
    TransferFunction tf = tf_in;
    normalize_leading(tf.num);
    normalize_leading(tf.den);

    if (tf.den.empty() || std::abs(tf.den[0]) < 1e-300) {
        throw std::runtime_error("tf_to_ss: invalid denominator");
    }

    // Normalise so den is monic.
    const double a0 = tf.den[0];
    std::vector<double> den(tf.den.size());
    for (std::size_t i = 0; i < tf.den.size(); ++i) den[i] = tf.den[i] / a0;

    std::vector<double> num(tf.num.size());
    for (std::size_t i = 0; i < tf.num.size(); ++i) num[i] = tf.num[i] / a0;

    const int n = static_cast<int>(den.size()) - 1;   // system order
    StateSpace ss;
    if (n == 0) {
        // Pure gain.
        ss.A = Mat::Zero(0, 0);
        ss.B = Vec::Zero(0);
        ss.C = Vec::Zero(0);
        ss.D = num.empty() ? 0.0 : num.back();
        return ss;
    }

    // Pad numerator on the left to length n+1 so we can split off D.
    std::vector<double> num_full(n + 1, 0.0);
    for (std::size_t i = 0; i < num.size(); ++i) {
        num_full[n + 1 - num.size() + i] = num[i];
    }
    // Now num_full and den both have length n+1 (highest power first).
    // D = leading numerator coeff (b0). Strictly proper -> b0 = 0.
    const double D = num_full[0];

    // Controllable canonical form.
    //   A = [0 1 0 ...; 0 0 1 ...; ...; -a_n ... -a_1]
    //   B = [0;0;...;1]
    //   C[i] = b_{n-i} - b0 * a_{n-i}
    // with den = [1, a_1, ..., a_n] (monic), num_full = [b0, b1, ..., bn].
    ss.A = Mat::Zero(n, n);
    for (int i = 0; i < n - 1; ++i) ss.A(i, i + 1) = 1.0;
    for (int j = 0; j < n; ++j) {
        // last row: -a_n, -a_{n-1}, ..., -a_1   (den[n] .. den[1])
        ss.A(n - 1, j) = -den[n - j];
    }
    ss.B = Vec::Zero(n);
    ss.B(n - 1) = 1.0;

    ss.C = Vec::Zero(n);
    for (int i = 0; i < n; ++i) {
        // C[i] corresponds to state x_i; coefficient b_{n-i} - b0 a_{n-i}
        const double b = num_full[n - i];
        const double a = den[n - i];
        ss.C(i) = b - D * a;
    }
    ss.D = D;
    return ss;
}

std::vector<double> forced_response_ss(const StateSpace& ss,
                                       const std::vector<double>& u,
                                       double dt)
{
    const int n = static_cast<int>(ss.A.rows());
    const int N = static_cast<int>(u.size());
    std::vector<double> y(N, 0.0);

    if (n == 0) {
        // Pure gain: y = D * u.
        for (int i = 0; i < N; ++i) y[i] = ss.D * u[i];
        return y;
    }

    // Exact discretisation assuming *linear interpolation* of the input
    // between samples (first-order hold), matching python-control's
    // forced_response. With a single input (m = 1) the augmented matrix is
    //   M = [[A*dt, B*dt, 0],
    //        [0,    0,    1],
    //        [0,    0,    0]]   (size (n+2) x (n+2))
    //   expM = [[Ad, *, Bd1], ...]
    //   Bd1 = expM[:n, n+1]
    //   Bd0 = expM[:n, n] - Bd1
    // and the recurrence is
    //   x[i] = Ad x[i-1] + Bd0 u[i-1] + Bd1 u[i]
    //   y[i] = C x[i] + D u[i]
    const int sz = n + 2;
    Mat M = Mat::Zero(sz, sz);
    M.topLeftCorner(n, n)     = ss.A * dt;
    M.block(0, n, n, 1)       = ss.B * dt;   // A*dt | B*dt | 0
    M(n, n + 1)               = 1.0;         // row n: [0 ... 0 0 1]
    // row n+1 stays all zeros.

    Mat eM = M.exp();
    Mat Ad  = eM.topLeftCorner(n, n);
    Vec Bd1 = eM.block(0, n + 1, n, 1);
    Vec Bd0 = eM.block(0, n,     n, 1) - Bd1;

    Vec x = Vec::Zero(n);                 // zero initial state
    // i = 0: state is zero, output uses u[0].
    y[0] = ss.C.dot(x) + ss.D * u[0];
    for (int i = 1; i < N; ++i) {
        x = Ad * x + Bd0 * u[i - 1] + Bd1 * u[i];
        y[i] = ss.C.dot(x) + ss.D * u[i];
    }
    return y;
}

std::vector<double> forced_response(const TransferFunction& tf,
                                    const std::vector<double>& u,
                                    double dt)
{
    return forced_response_ss(tf_to_ss(tf), u, dt);
}

}  // namespace tdof
