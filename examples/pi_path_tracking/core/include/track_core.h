/*
 * track_core.h — C ABI for the planar-motion + PI path-tracking demo.
 *
 * Reproduces planar_path_tracking_pi_tuned.py:
 *
 *   Plant (3-DOF planar vehicle):
 *     u_dot = fx/m + r*v
 *     v_dot = fy/m - r*u
 *     r_dot = n_moment/izz
 *     x_dot = u cos(psi) - v sin(psi)
 *     y_dot = u sin(psi) + v cos(psi)
 *     psi_dot = r
 *     fy      = -cornering_power * beta,   beta = atan2(v, u)
 *
 *   Controller (updated every tc seconds, integrator step h):
 *     speed_err = target_speed - sqrt(u^2 + v^2)
 *     speed_int += speed_err * tc
 *     fx        = saturate(100*speed_err + 0.1*speed_int, ±fx_limit)
 *
 *     (e_y, e_psi) computed at the look-ahead reference point
 *     ey_int   += e_y   * tc        (clamped to ±error_integral_limit)
 *     epsi_int += e_psi * tc
 *     n_raw     = -ky_p*e_y - ky_i*ey_int
 *               + kpsi_p*e_psi + kpsi_i*epsi_int
 *               - kr_damping * r
 *     n_moment  = saturate(n_raw, ±n_moment_limit)
 *
 *   Reference path: straight (X) -> 90° left arc -> straight (Y),
 *   resampled by arc length at step ds.
 *
 * Frontends: Qt6 C++ / Avalonia C# / Python (ctypes). Windows-first.
 */
#ifndef TRACK_CORE_H
#define TRACK_CORE_H

#ifdef _WIN32
#  ifdef TRACK_CORE_BUILD
#    define TRACK_CORE_API __declspec(dllexport)
#  else
#    define TRACK_CORE_API __declspec(dllimport)
#  endif
#else
#  define TRACK_CORE_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Configuration --------------------------------------------------- */
typedef struct TrackConfig {
    /* Plant */
    double m;                 /* mass [kg] */
    double izz;               /* yaw inertia [kg*m^2] */
    double cornering_power;   /* fy = -C * beta */

    /* Integrator / sampling */
    double h;                 /* integration step [s] */
    double tc;                /* control update interval [s] */
    double total_time;        /* total simulation time [s] */

    /* Speed control */
    double target_speed;      /* [m/s] */

    /* Tracking gains */
    double ky_p;
    double ky_i;
    double kpsi_p;
    double kpsi_i;
    double kr_damping;

    /* Limits */
    double n_moment_limit;
    double fx_limit;
    double error_integral_limit;

    /* Look-ahead */
    int32_t lookahead_index;

    /* Initial conditions */
    double initial_y_offset;
    double initial_heading_deg;

    /* Reference-path generator parameters */
    double straight1_len;     /* default 0.30 */
    double radius;            /* default 0.20 */
    double straight2_len;     /* default 0.30 */
    double ds;                /* default 0.002 */
} TrackConfig;

/* Defaults from the Python reference. */
TRACK_CORE_API void track_core_default_config(TrackConfig* cfg);

/* ----- Reference path -------------------------------------------------- */
typedef struct TrackReferencePath TrackReferencePath;

/* Generate the (straight, arc, straight) reference path. NULL on bad input. */
TRACK_CORE_API TrackReferencePath* track_core_make_reference(const TrackConfig*);
TRACK_CORE_API void                track_core_free_reference(TrackReferencePath*);

TRACK_CORE_API int32_t track_core_ref_length(const TrackReferencePath*);

/* Copy x_ref / y_ref / psi_ref arrays.  buffer_len must be >= length. */
TRACK_CORE_API int32_t track_core_ref_copy_x  (const TrackReferencePath*, double*, int32_t);
TRACK_CORE_API int32_t track_core_ref_copy_y  (const TrackReferencePath*, double*, int32_t);
TRACK_CORE_API int32_t track_core_ref_copy_psi(const TrackReferencePath*, double*, int32_t);

/* ----- Simulation ------------------------------------------------------ */
typedef struct TrackSimulation TrackSimulation;

/* Run the full closed-loop tracking simulation. NULL on bad input. */
TRACK_CORE_API TrackSimulation* track_core_simulate(const TrackConfig*);
TRACK_CORE_API void             track_core_free_simulation(TrackSimulation*);

TRACK_CORE_API int32_t track_core_sim_length(const TrackSimulation*);

/* Per-sample accessors. Each copy_* function fills the buffer with one
 * double per simulation step. */
TRACK_CORE_API int32_t track_core_sim_copy_time   (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_x      (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_y      (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_psi    (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_u      (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_v      (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_r      (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_beta   (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_ey     (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_epsi   (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_nmoment(const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_fx     (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_x_ref  (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_y_ref  (const TrackSimulation*, double*, int32_t);
TRACK_CORE_API int32_t track_core_sim_copy_psi_ref(const TrackSimulation*, double*, int32_t);

/* Aggregate metrics. */
TRACK_CORE_API double track_core_sim_path_error_rms(const TrackSimulation*);
TRACK_CORE_API double track_core_sim_path_error_max(const TrackSimulation*);
TRACK_CORE_API double track_core_sim_ey_rms        (const TrackSimulation*);
TRACK_CORE_API double track_core_sim_ey_max_abs    (const TrackSimulation*);
TRACK_CORE_API double track_core_sim_epsi_rms      (const TrackSimulation*);
TRACK_CORE_API double track_core_sim_epsi_max_abs  (const TrackSimulation*);
TRACK_CORE_API double track_core_sim_nmoment_max_abs(const TrackSimulation*);

/* ----- Misc ------------------------------------------------------------ */
TRACK_CORE_API const char* track_core_version(void);

#ifdef __cplusplus
}
#endif

#endif  /* TRACK_CORE_H */
