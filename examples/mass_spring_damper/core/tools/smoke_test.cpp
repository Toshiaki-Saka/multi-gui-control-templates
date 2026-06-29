// smoke_test.cpp — verifies the core against the Python reference.
//
// The reference uses scipy.integrate.odeint (LSODA, adaptive step);
// the core uses fixed-step RK4. Differences are bounded by ~1e-7.

#include "msd_core.h"

#include <cmath>
#include <cstdio>
#include <vector>

struct ExpectedCase {
    const char* name;
    MsdCase cfg;
    double expected_final_x;
    double expected_final_v;
};

int main() {
    std::printf("=== %s ===\n", msd_core_version());

    MsdSamplingConfig sp;
    msd_core_default_sampling(&sp);
    std::printf("Sampling: dt=%.4f, stop=%.2f\n", sp.dt, sp.stop);

    // Reference values produced by the Python script:
    const ExpectedCase cases[] = {
        {"baseline",      {1.0, 2.0,  5.0, 0.5, 2.0, 0.0, 0.0}, -0.021154934, 0.238803425},
        {"low damping",   {1.0, 0.5,  5.0, 0.5, 2.0, 0.0, 0.0},  0.109905870, 0.709929385},
        {"high damping",  {1.0, 5.0,  5.0, 0.5, 2.0, 0.0, 0.0}, -0.015682450, 0.094430891},
        {"stiffer spring",{1.0, 2.0, 12.0, 0.5, 2.0, 0.0, 0.0},  0.035444030, 0.086453475},
        {"near resonance",{1.0, 0.5,  5.0, 0.5, 2.2, 0.0, 0.0},  0.409217408,-0.121325271},
    };

    int failures = 0;
    for (const auto& tc : cases) {
        MsdSimulation* sim = msd_core_simulate(&tc.cfg, &sp);
        if (!sim) { std::fprintf(stderr, "FAIL: simulate %s\n", tc.name); ++failures; continue; }

        const int n = msd_core_sim_length(sim);
        const double fx = msd_core_sim_final_position(sim);
        const double fv = msd_core_sim_final_velocity(sim);
        const double dx = std::fabs(fx - tc.expected_final_x);
        const double dv = std::fabs(fv - tc.expected_final_v);

        double omega_n, zeta;
        msd_core_derived(&tc.cfg, &omega_n, &zeta);

        std::printf("--- %-18s n=%-5d  x(end)=%+11.7f  v(end)=%+11.7f  "
                    "ωn=%.4f  ζ=%.4f  (Δx=%.1e  Δv=%.1e)\n",
                    tc.name, n, fx, fv, omega_n, zeta, dx, dv);

        if (dx > 1e-5 || dv > 1e-5) {
            std::printf("    FAIL: expected x=%.7f v=%.7f\n",
                        tc.expected_final_x, tc.expected_final_v);
            ++failures;
        }
        msd_core_free_simulation(sim);
    }

    std::printf("\nExpected sample count: 10001\n");
    if (failures == 0) std::printf("ALL OK.\n");
    else               std::printf("FAILURES: %d\n", failures);
    return failures ? 1 : 0;
}
