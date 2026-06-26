// smoke_test.cpp — regression test for the 2-DOF comparison core.
//
// Pins the Python-reference scenario (closed-loop transfer function and the
// PID vs 2-DOF step responses) and checks the invalid-config path. Exits
// non-zero on any failure so `ctest` turns a regression into a red build.

#include "tdof_core.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

void close(double value, double expected, double tol, const char* what) {
    const bool ok = std::fabs(value - expected) <= tol;
    if (ok) {
        std::printf("  [PASS] %s (%.6f ~ %.6f)\n", what, value, expected);
    } else {
        std::printf("  [FAIL] %s: got %.6f, expected %.6f +/- %.1e\n",
                    what, value, expected, tol);
        ++g_failures;
    }
}

bool tf_matches(const TdofConfig* cfg, int which,
                const std::vector<double>& en, const std::vector<double>& ed) {
    int32_t nN = 16, nD = 16;
    std::vector<double> num(nN), den(nD);
    if (!tdof_core_get_tf(cfg, which, num.data(), &nN, den.data(), &nD)) return false;
    if (nN != static_cast<int32_t>(en.size())) return false;
    if (nD != static_cast<int32_t>(ed.size())) return false;
    for (int i = 0; i < nN; ++i) if (std::fabs(num[i] - en[i]) > 1e-9) return false;
    for (int i = 0; i < nD; ++i) if (std::fabs(den[i] - ed[i]) > 1e-9) return false;
    return true;
}

}  // namespace

int main() {
    std::printf("=== %s ===\n", tdof_core_version());

    TdofConfig cfg;
    tdof_core_default_config(&cfg);

    // (1) Closed-loop transfer function Gyz (reference-to-output).
    std::printf("\n[1] Closed-loop Gyz coefficients\n");
    check(tf_matches(&cfg, 3, {0.1, 2.0, 10.0}, {0.01, 0.115, 3.0, 10.0}),
          "Gyz num=[0.1,2,10] den=[0.01,0.115,3,10]");

    // (2) PID vs 2-DOF step responses.
    std::printf("\n[2] Step responses (PID vs 2-DOF)\n");
    {
        TdofSimulation* sim = tdof_core_simulate(&cfg);
        if (!sim) { std::fprintf(stderr, "FAIL: simulate\n"); return 1; }

        const int n = tdof_core_sim_length(sim);
        std::vector<double> z(n), ypid(n), y2(n);
        tdof_core_sim_copy_z     (sim, z.data(),    n);
        tdof_core_sim_copy_y_pid (sim, ypid.data(), n);
        tdof_core_sim_copy_y_2dof(sim, y2.data(),   n);

        double pid_max = 0.0, two_max = 0.0;
        for (int i = 0; i < n; ++i) {
            pid_max = std::max(pid_max, ypid[i]);
            two_max = std::max(two_max, y2[i]);
        }

        check(n == 200, "200 samples");
        close(z[n - 1],   cfg.ref,  1e-6, "reference reaches 10");
        close(pid_max,    11.39699, 1e-3, "PID max = 11.397");
        close(two_max,    12.04883, 1e-3, "2-DOF max = 12.049");
        close(ypid[n - 1], cfg.ref, 0.02, "PID settles near reference");
        close(y2[n - 1],   cfg.ref, 0.02, "2-DOF settles near reference");
        tdof_core_free_simulation(sim);
    }

    // (3) Invalid config returns NULL.
    std::printf("\n[3] Invalid config\n");
    {
        check(tdof_core_simulate(nullptr) == nullptr, "NULL config -> NULL");
        TdofConfig bad;
        tdof_core_default_config(&bad);
        bad.dt = 0.0;
        check(tdof_core_simulate(&bad) == nullptr, "dt = 0 -> NULL");
    }

    std::printf("\n%s (%d failure(s))\n",
                g_failures ? "REGRESSION TEST FAILED" : "ALL CHECKS PASSED",
                g_failures);
    return g_failures ? 1 : 0;
}
