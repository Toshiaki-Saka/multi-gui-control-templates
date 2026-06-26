/*
 * pid_core.h — C ABI for the interactive PID-response demo.
 *
 * Reproduces pid_advanced_simulation.py:
 *
 *   pid_control(kp, ki, kd, theta_goal, theta_current, error_sum, error_pre):
 *       error      = theta_goal - theta_current
 *       error_sum += error
 *       error_diff = error - error_pre
 *       m          = kp*error + ki*error_sum + kd*error_diff
 *       return (m, error_sum, error)
 *
 *   simulate_pid(theta_start, theta_goal, offset, time_length, kp, ki, kd):
 *       for t in range(1, int(time_length)):
 *           m, error_sum, error = pid_control(...)
 *           theta_current += m
 *           theta_current -= offset
 *           error_pre      = error
 *
 * The output sequence has exactly `time_length` samples (index 0 holds
 * the initial state).
 *
 * Frontends: Qt6 C++ / Avalonia C# / Python (ctypes).
 */
#ifndef PID_CORE_H
#define PID_CORE_H

#ifdef _WIN32
#  ifdef PID_CORE_BUILD
#    define PID_CORE_API __declspec(dllexport)
#  else
#    define PID_CORE_API __declspec(dllimport)
#  endif
#else
#  define PID_CORE_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Configuration --------------------------------------------------- */
typedef struct PidConfig {
    double  theta_start;
    double  theta_goal;
    double  offset;           /* subtracted from theta_current each step */
    int32_t time_length;      /* number of samples (>= 2) */
    double  kp;
    double  ki;
    double  kd;
    double  dt;               /* time step (> 0); scales integral and derivative */
    double  integral_clamp;   /* max |error_sum| for anti-windup; 0 = disabled */
    double  output_clamp;     /* max |m| for actuator saturation; 0 = disabled */
} PidConfig;

/* Default configuration (gentle, well-damped response):
 *   theta_start = 0, theta_goal = 90, offset = 0, time_length = 150,
 *   kp = 0.10, ki = 0.01, kd = 0.0, dt = 1.0,
 *   integral_clamp = 0 (off), output_clamp = 0 (off)
 * The Python reference (pid_advanced_simulation.py) uses the more aggressive
 * kp = 0.10, ki = 0.5, kd = 0.5, which overshoots to ~135 before settling
 * to 90 (see the README screenshot and the smoke test). */
PID_CORE_API void pid_core_default_config(PidConfig* cfg);

/* ----- Simulation ------------------------------------------------------ */
typedef struct PidSimulation PidSimulation;

/* Run the PID response. Returns NULL if cfg is invalid (time_length < 2,
 * non-finite values). The simulation stores `time_length` (time, theta)
 * pairs. */
PID_CORE_API PidSimulation* pid_core_simulate(const PidConfig* cfg);
PID_CORE_API void           pid_core_free_simulation(PidSimulation*);

PID_CORE_API int32_t pid_core_sim_length (const PidSimulation*);

/* Copy time indices (0, 1, 2, ..., N-1). buffer_len >= N. */
PID_CORE_API int32_t pid_core_sim_copy_time  (const PidSimulation*, double* buffer, int32_t buffer_len);
/* Copy the theta response (length N). */
PID_CORE_API int32_t pid_core_sim_copy_theta (const PidSimulation*, double* buffer, int32_t buffer_len);

/* Convenience accessors */
PID_CORE_API double  pid_core_sim_final_theta(const PidSimulation*);
PID_CORE_API double  pid_core_sim_max_theta  (const PidSimulation*);
PID_CORE_API double  pid_core_sim_min_theta  (const PidSimulation*);

/* ----- Misc ------------------------------------------------------------ */
PID_CORE_API const char* pid_core_version(void);

#ifdef __cplusplus
}
#endif

#endif  /* PID_CORE_H */
