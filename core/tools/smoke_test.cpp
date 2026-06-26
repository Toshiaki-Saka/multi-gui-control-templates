// smoke_test.cpp — regression test for the PID core.
//
// Verifies the discrete PID against the Python reference (pid_advanced_simulation.py)
// and exercises the dt / clamp / invalid-config paths. Exits non-zero on any
// failure so `ctest` turns a regression into a red build.

#include "pid_core.h"

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

}  // namespace

int main() {
    std::printf("=== %s ===\n", pid_core_version());

    // (1) Golden reference: kp=0.1, ki=0.5, kd=0.5 reproduces the Python
    //     reference response exactly (overshoot to 135.09, settle to 90).
    std::printf("\n[1] Golden reference (kp=0.1, ki=0.5, kd=0.5)\n");
    {
        PidConfig cfg;
        pid_core_default_config(&cfg);
        cfg.ki = 0.5;
        cfg.kd = 0.5;

        PidSimulation* sim = pid_core_simulate(&cfg);
        if (!sim) { std::fprintf(stderr, "FAIL: simulate golden\n"); return 1; }

        const int n = pid_core_sim_length(sim);
        std::vector<double> th(n);
        pid_core_sim_copy_theta(sim, th.data(), n);

        const double expected_first5[5] = {0.0, 99.0, 89.1, 135.09, 125.991};
        check(n == 150, "150 samples");
        for (int i = 0; i < 5; ++i) {
            char lbl[32];
            std::snprintf(lbl, sizeof lbl, "theta[%d]", i);
            close(th[i], expected_first5[i], 1e-9, lbl);
        }
        close(pid_core_sim_final_theta(sim), 90.0,    1e-3, "final theta -> 90 (settled)");
        close(pid_core_sim_max_theta(sim),   135.09,  1e-9, "max theta = 135.09");
        close(pid_core_sim_min_theta(sim),   0.0,     1e-12, "min theta = 0 (start)");
        pid_core_free_simulation(sim);
    }

    // (2) Default config converges to the goal (gentler gains, kd=0).
    std::printf("\n[2] Default config converges to goal\n");
    {
        PidConfig cfg;
        pid_core_default_config(&cfg);
        PidSimulation* sim = pid_core_simulate(&cfg);
        if (!sim) { std::fprintf(stderr, "FAIL: simulate default\n"); return 1; }
        close(pid_core_sim_final_theta(sim), 90.0, 0.1, "default final near goal");
        check(pid_core_sim_max_theta(sim) >= 90.0, "default overshoots the goal");
        pid_core_free_simulation(sim);
    }

    // (3) Output clamp limits the per-step command (actuator saturation).
    std::printf("\n[3] Output clamp caps the response\n");
    {
        PidConfig cfg;
        pid_core_default_config(&cfg);
        cfg.ki = 0.5; cfg.kd = 0.5;        // would overshoot to 135 unclamped
        cfg.output_clamp = 5.0;            // |m| <= 5 per step
        PidSimulation* sim = pid_core_simulate(&cfg);
        if (!sim) { std::fprintf(stderr, "FAIL: simulate clamp\n"); return 1; }
        const int n = pid_core_sim_length(sim);
        std::vector<double> th(n);
        pid_core_sim_copy_theta(sim, th.data(), n);
        double max_step = 0.0;
        for (int i = 1; i < n; ++i)
            max_step = std::max(max_step, std::fabs(th[i] - th[i - 1]));
        check(max_step <= 5.0 + 1e-9, "no step exceeds output_clamp (5.0)");
        pid_core_free_simulation(sim);
    }

    // (4) Invalid configs return NULL.
    std::printf("\n[4] Invalid configs are rejected\n");
    {
        check(pid_core_simulate(nullptr) == nullptr, "NULL config -> NULL");

        PidConfig cfg;
        pid_core_default_config(&cfg);
        cfg.time_length = 1;
        check(pid_core_simulate(&cfg) == nullptr, "time_length < 2 -> NULL");

        pid_core_default_config(&cfg);
        cfg.dt = 0.0;
        check(pid_core_simulate(&cfg) == nullptr, "dt <= 0 -> NULL");

        pid_core_default_config(&cfg);
        cfg.kp = std::nan("");
        check(pid_core_simulate(&cfg) == nullptr, "non-finite gain -> NULL");
    }

    std::printf("\n%s (%d failure(s))\n",
                g_failures ? "REGRESSION TEST FAILED" : "ALL CHECKS PASSED",
                g_failures);
    return g_failures ? 1 : 0;
}
