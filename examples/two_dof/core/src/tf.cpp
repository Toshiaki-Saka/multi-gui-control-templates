// tf.cpp — transfer-function polynomial algebra and system builders.

#include "tdof.hpp"

#include <algorithm>
#include <cmath>

namespace tdof {

std::vector<double> poly_mul(const std::vector<double>& a,
                             const std::vector<double>& b)
{
    if (a.empty() || b.empty()) return {};
    std::vector<double> out(a.size() + b.size() - 1, 0.0);
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < b.size(); ++j) {
            out[i + j] += a[i] * b[j];
        }
    }
    return out;
}

std::vector<double> poly_add(const std::vector<double>& a,
                             const std::vector<double>& b)
{
    const std::size_t n = std::max(a.size(), b.size());
    std::vector<double> out(n, 0.0);
    // Align at the constant term (end of each array).
    for (std::size_t i = 0; i < a.size(); ++i) out[n - a.size() + i] += a[i];
    for (std::size_t i = 0; i < b.size(); ++i) out[n - b.size() + i] += b[i];
    return out;
}

void normalize_leading(std::vector<double>& p) {
    std::size_t i = 0;
    while (i + 1 < p.size() && std::abs(p[i]) < 1e-14) ++i;
    if (i > 0) p.erase(p.begin(), p.begin() + static_cast<long>(i));
}

TransferFunction series(const TransferFunction& a, const TransferFunction& b) {
    return TransferFunction(poly_mul(a.num, b.num), poly_mul(a.den, b.den));
}

TransferFunction feedback_unity(const TransferFunction& g) {
    // feedback(G, 1) = G.num / (G.den + G.num)
    TransferFunction out;
    out.num = g.num;
    out.den = poly_add(g.den, g.num);
    normalize_leading(out.den);
    return out;
}

// ----- system builders --------------------------------------------------

TransferFunction build_plant(const TdofConfig& cfg) {
    // P(s) = 1 / (m s^2 + c s + k)
    return TransferFunction({1.0}, {cfg.m, cfg.c, cfg.k});
}

TransferFunction build_pid(const TdofConfig& cfg) {
    // K1(s) = (kd s^2 + kp s + ki) / s
    return TransferFunction({cfg.kd, cfg.kp, cfg.ki}, {1.0, 0.0});
}

TransferFunction build_reference_filter(const TdofConfig& cfg) {
    // K2(s) = (kp s + ki) / (kd s^2 + kp s + ki)
    return TransferFunction({cfg.kp, cfg.ki}, {cfg.kd, cfg.kp, cfg.ki});
}

TransferFunction build_closed_loop(const TdofConfig& cfg) {
    // Gyz = feedback(P * K1, 1)
    const TransferFunction P  = build_plant(cfg);
    const TransferFunction K1 = build_pid(cfg);
    return feedback_unity(series(P, K1));
}

}  // namespace tdof
