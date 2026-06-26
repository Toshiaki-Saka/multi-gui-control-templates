/*
 * tdof_core.h - C ABI for a 1-DOF PID vs 2-DOF-like control comparison.
 *
 * Reproduces two_degree_of_freedom...py:
 *
 *   Plant            P(s)  = 1 / (m s^2 + c s + k)
 *   PID controller   K1(s) = (kd s^2 + kp s + ki) / s
 *   Reference filter K2(s) = (kp s + ki) / (kd s^2 + kp s + ki)
 *   Closed loop      Gyz   = feedback(P * K1, 1)
 *
 * The comparison feeds a step reference r through:
 *   - the closed loop directly         -> y_pid    (1-DOF PID)
 *   - the reference filter then loop   -> y_2dof   (2-DOF-like)
 *
 * Frontends: Qt6 C++ / Avalonia C# / Python (ctypes).
 */
#ifndef TDOF_CORE_H
#define TDOF_CORE_H

#ifdef _WIN32
#  ifdef TDOF_CORE_BUILD
#    define TDOF_CORE_API __declspec(dllexport)
#  else
#    define TDOF_CORE_API __declspec(dllimport)
#  endif
#else
#  define TDOF_CORE_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Configuration --------------------------------------------------- */
typedef struct TdofConfig {
    /* Plant: P(s) = 1 / (m s^2 + c s + k) */
    double m;
    double c;
    double k;

    /* PID gains */
    double kp;
    double ki;
    double kd;

    /* Step reference amplitude (output is scaled by this for display) */
    double ref;

    /* Time grid: t in [0, t_end) with step dt */
    double t_end;
    double dt;
} TdofConfig;

/* Values from the Python reference:
 *   m = 0.01, c = 0.015, k = 1.0
 *   kp = 2.0, ki = 10.0, kd = 0.1
 *   ref = 10.0, t_end = 2.0, dt = 0.01  */
TDOF_CORE_API void tdof_core_default_config(TdofConfig* cfg);

/* ----- Transfer-function inspection ----------------------------------- */
/*
 * Copy the numerator/denominator polynomial coefficients (highest power
 * first, matching python-control / numpy convention) of one of the
 * systems into caller-provided buffers.
 *
 * `which` selects the system:
 *   0 = Plant P
 *   1 = PID K1
 *   2 = Reference filter K2
 *   3 = Closed loop Gyz = feedback(P*K1, 1)
 *
 * On entry *num_len / *den_len give the buffer capacities; on return they
 * hold the actual coefficient counts. Pass NULL buffers to just query the
 * sizes. Returns 1 on success, 0 on bad input / insufficient buffer.
 */
TDOF_CORE_API int32_t tdof_core_get_tf(
    const TdofConfig* cfg,
    int32_t           which,
    double*           num, int32_t* num_len,
    double*           den, int32_t* den_len);

/* ----- Simulation ------------------------------------------------------ */
typedef struct TdofSimulation TdofSimulation;

/*
 * Run the full comparison:
 *   t       : uniform grid [0, t_end) step dt
 *   r       : unit step (all ones)
 *   z       : K2 * r        (filtered reference)
 *   y_pid   : Gyz * r        (1-DOF PID closed-loop output)
 *   y_2dof  : Gyz * z        (2-DOF-like closed-loop output)
 *
 * All signals are stored in *normalised* form (unit step). Multiply by
 * cfg->ref for the display-scale values, or use the *_scaled accessors.
 *
 * Returns NULL on bad input.
 */
TDOF_CORE_API TdofSimulation* tdof_core_simulate(const TdofConfig* cfg);

TDOF_CORE_API void    tdof_core_free_simulation(TdofSimulation*);
TDOF_CORE_API int32_t tdof_core_sim_length(const TdofSimulation*);

/* Each copies `length` doubles into `buffer` (buffer_len must be >= length).
 * Returns the number of samples copied, 0 on error. The *_scaled variants
 * multiply by cfg->ref. */
TDOF_CORE_API int32_t tdof_core_sim_copy_time   (const TdofSimulation*, double* buffer, int32_t buffer_len);
TDOF_CORE_API int32_t tdof_core_sim_copy_r      (const TdofSimulation*, double* buffer, int32_t buffer_len);
TDOF_CORE_API int32_t tdof_core_sim_copy_z      (const TdofSimulation*, double* buffer, int32_t buffer_len);
TDOF_CORE_API int32_t tdof_core_sim_copy_y_pid  (const TdofSimulation*, double* buffer, int32_t buffer_len);
TDOF_CORE_API int32_t tdof_core_sim_copy_y_2dof (const TdofSimulation*, double* buffer, int32_t buffer_len);

/* ----- Misc ------------------------------------------------------------ */
TDOF_CORE_API const char* tdof_core_version(void);

#ifdef __cplusplus
}
#endif

#endif  /* TDOF_CORE_H */
