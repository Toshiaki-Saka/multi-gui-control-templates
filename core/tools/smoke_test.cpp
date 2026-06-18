// smoke_test.cpp — verifies the core against the Python reference.

#include "track_core.h"

#include <cmath>
#include <cstdio>
#include <vector>

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

    track_core_free_simulation(sim);
    return 0;
}
