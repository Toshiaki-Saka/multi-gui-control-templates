// smoke_test.cpp — verifies the core reproduces the Python reference
// (planar_path_tracking_pi_tuned.py) and asserts every metric so it can run as
// a CTest regression gate (non-zero exit on any mismatch).

#include "track_core.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {
int g_failures = 0;

void check_near(const char* what, double actual, double expected, double tol) {
    const bool ok = std::fabs(actual - expected) <= tol;
    std::printf("  [%s] %-16s actual=%.6f expected=%.6f\n",
                ok ? "PASS" : "FAIL", what, actual, expected);
    if (!ok) ++g_failures;
}
void check_eq(const char* what, int actual, int expected) {
    const bool ok = actual == expected;
    std::printf("  [%s] %-16s actual=%d expected=%d\n",
                ok ? "PASS" : "FAIL", what, actual, expected);
    if (!ok) ++g_failures;
}
}  // namespace

int main() {
    std::printf("=== %s ===\n", track_core_version());

    TrackConfig cfg;
    track_core_default_config(&cfg);
    std::printf("Config: m=%.3g izz=%.3g C=%.3g  h=%.1e tc=%.1e T=%.3f  "
                "target=%.3f\n",
                cfg.m, cfg.izz, cfg.cornering_power,
                cfg.h, cfg.tc, cfg.total_time, cfg.target_speed);
    std::printf("Gains : ky_p=%.1f ky_i=%.1f kpsi_p=%.1f kpsi_i=%.1f "
                "kr_damping=%.1f\n",
                cfg.ky_p, cfg.ky_i, cfg.kpsi_p, cfg.kpsi_i, cfg.kr_damping);

    TrackReferencePath* ref = track_core_make_reference(&cfg);
    if (!ref) { std::fprintf(stderr, "make_reference failed\n"); return 1; }
    const int nref = track_core_ref_length(ref);
    std::printf("Reference points: %d\n", nref);
    track_core_free_reference(ref);

    TrackSimulation* sim = track_core_simulate(&cfg);
    if (!sim) { std::fprintf(stderr, "simulate failed\n"); return 1; }

    const int n = track_core_sim_length(sim);
    std::printf("Simulation steps : %d  (expect 9000)\n", n);

    // First / last positions
    std::vector<double> X(n), Y(n);
    track_core_sim_copy_x(sim, X.data(), n);
    track_core_sim_copy_y(sim, Y.data(), n);
    std::printf("First (x,y)  : (%.6f, %.6f)\n", X[0], Y[0]);
    std::printf("Last  (x,y)  : (%.6f, %.6f)\n", X[n-1], Y[n-1]);

    std::printf("path_error_rms = %.6f  (expect 0.013762)\n",
                track_core_sim_path_error_rms(sim));
    std::printf("path_error_max = %.6f  (expect 0.030000)\n",
                track_core_sim_path_error_max(sim));
    std::printf("e_y_rms        = %.6f  (expect 0.020243)\n",
                track_core_sim_ey_rms(sim));
    std::printf("e_y_max        = %.6f  (expect 0.032639)\n",
                track_core_sim_ey_max_abs(sim));
    std::printf("e_psi_rms      = %.6f  (expect 0.300645)\n",
                track_core_sim_epsi_rms(sim));
    std::printf("e_psi_max      = %.6f  (expect 0.579404)\n",
                track_core_sim_epsi_max_abs(sim));
    std::printf("max|n_moment|  = %.6f  (expect 28.495415)\n",
                track_core_sim_nmoment_max_abs(sim));

    // ── Regression checks against the Python reference ──
    std::printf("\nChecks:\n");
    check_eq("ref points",     nref, 458);
    check_eq("sim steps",      n,    9000);
    check_near("path_err_rms", track_core_sim_path_error_rms(sim),  0.013762,  1e-6);
    check_near("path_err_max", track_core_sim_path_error_max(sim),  0.030000,  1e-6);
    check_near("e_y_rms",      track_core_sim_ey_rms(sim),          0.020243,  1e-6);
    check_near("e_y_max",      track_core_sim_ey_max_abs(sim),      0.032639,  1e-6);
    check_near("e_psi_rms",    track_core_sim_epsi_rms(sim),        0.300645,  1e-6);
    check_near("e_psi_max",    track_core_sim_epsi_max_abs(sim),    0.579404,  1e-6);
    check_near("max|n_moment|",track_core_sim_nmoment_max_abs(sim), 28.495415, 1e-5);

    track_core_free_simulation(sim);

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "SMOKE TEST PASS" : "SMOKE TEST FAIL",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
