/*
 * msd_core.h — C ABI for the mass-spring-damper forced-response demo.
 *
 * Reproduces mass_spring_damper_forced_response.py:
 *
 *   Plant:
 *     m·x_ddot + c·x_dot + k·x = f(t)
 *     f(t) = force_amplitude · sin(force_omega · t)
 *
 *   State:
 *     [x_dot]     [   0       1  ] [x]   [    0    ]
 *     [v_dot]  =  [ -k/m    -c/m ] [v] + [ f(t)/m  ]
 *
 *   The Python reference integrates with scipy.integrate.odeint
 *   (LSODA, adaptive step). The C++ core uses fixed-step RK4 with the
 *   same dt; differences are bounded by ~1e-8 for the default cases.
 *
 * Derived quantities (per case):
 *     omega_n = sqrt(k / m)             (natural angular frequency)
 *     zeta    = c / (2 sqrt(m·k))       (damping ratio)
 *
 * Frontends: Qt6 C++ / Avalonia C# / Python (ctypes). Windows-first.
 */
#ifndef MSD_CORE_H
#define MSD_CORE_H

#ifdef _WIN32
#  ifdef MSD_CORE_BUILD
#    define MSD_CORE_API __declspec(dllexport)
#  else
#    define MSD_CORE_API __declspec(dllimport)
#  endif
#else
#  define MSD_CORE_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Per-case parameters -------------------------------------------- */
typedef struct MsdCase {
    double m;                 /* mass [kg] */
    double c;                 /* damping coefficient [N·s/m] */
    double k;                 /* spring constant [N/m] */
    double force_amplitude;   /* [N] */
    double force_omega;       /* [rad/s] */
    double x0;                /* initial position [m] */
    double v0;                /* initial velocity [m/s] */
} MsdCase;

/* Defaults from the Python "baseline" case. */
MSD_CORE_API void msd_core_default_case(MsdCase* c);

/* Derived quantities. omega_n = sqrt(k/m), zeta = c / (2 sqrt(m·k)).
 * Returns 0 on bad inputs (negative m or k, or m == 0). */
MSD_CORE_API int32_t msd_core_derived(const MsdCase* c,
                                      double* out_omega_n,
                                      double* out_zeta);

/* ----- Sampling parameters -------------------------------------------- */
typedef struct MsdSamplingConfig {
    double dt;        /* time step [s], must be > 0 */
    double stop;      /* end time [s], must be > 0 */
} MsdSamplingConfig;

MSD_CORE_API void msd_core_default_sampling(MsdSamplingConfig* s);

/* ----- Simulation ----------------------------------------------------- */
typedef struct MsdSimulation MsdSimulation;

/* Run a fixed-step RK4 simulation of a single case.
 *
 * The output time axis matches the Python reference:
 *     times = np.arange(0.0, stop + dt, dt)
 * which produces samples at 0, dt, 2·dt, ..., k·dt where
 * k·dt <= stop + dt (i.e. an extra trailing sample beyond stop).
 *
 * Returns NULL on bad inputs. */
MSD_CORE_API MsdSimulation* msd_core_simulate(const MsdCase*,
                                              const MsdSamplingConfig*);
MSD_CORE_API void msd_core_free_simulation(MsdSimulation*);

MSD_CORE_API int32_t msd_core_sim_length(const MsdSimulation*);

/* Per-sample accessors. */
MSD_CORE_API int32_t msd_core_sim_copy_time    (const MsdSimulation*, double*, int32_t);
MSD_CORE_API int32_t msd_core_sim_copy_position(const MsdSimulation*, double*, int32_t);
MSD_CORE_API int32_t msd_core_sim_copy_velocity(const MsdSimulation*, double*, int32_t);
MSD_CORE_API int32_t msd_core_sim_copy_force   (const MsdSimulation*, double*, int32_t);

MSD_CORE_API double msd_core_sim_final_position(const MsdSimulation*);
MSD_CORE_API double msd_core_sim_final_velocity(const MsdSimulation*);
MSD_CORE_API double msd_core_sim_max_abs_position(const MsdSimulation*);
MSD_CORE_API double msd_core_sim_max_abs_velocity(const MsdSimulation*);

/* ----- Misc ----------------------------------------------------------- */
MSD_CORE_API const char* msd_core_version(void);

#ifdef __cplusplus
}
#endif

#endif  /* MSD_CORE_H */
