# `tdof_core` — C ABI

Back to the [C ABI overview](../c-abi-reference.md) · [theory](theory.md) · [日本語](../../ja/two_dof/api.md)


Header: `examples/two_dof/core/include/tdof_core.h`.
Requires Eigen at build time; the dependency does not leak into the ABI.

## Config

```c
typedef struct TdofConfig {
    double m, c, k;      /* plant P(s) = 1 / (m s^2 + c s + k) */
    double kp, ki, kd;   /* PID gains                          */
    double ref;          /* step amplitude used for display    */
    double t_end, dt;    /* time grid: [0, t_end) step dt      */
} TdofConfig;
```

`tdof_core_default_config()` fills m = 0.01, c = 0.015, k = 1.0, kp = 2,
ki = 10, kd = 0.1, ref = 10, t_end = 2 s, dt = 0.01 s.
`tdof_core_simulate()` requires `m > 0`, finite gains, `t_end > 0`, `dt > 0` and
`dt < t_end`.

## Transfer-function inspection

```c
int32_t tdof_core_get_tf(const TdofConfig* cfg, int32_t which,
                         double* num, int32_t* num_len,
                         double* den, int32_t* den_len);
```

`which`: `0` = plant $P$, `1` = PID $`K_1`$, `2` = pre-filter $`K_2`$,
`3` = closed loop $`G_{yz}`$. Coefficients come back highest-power-first.

Two-call protocol: on entry `*num_len` / `*den_len` hold the buffer capacities;
on return they hold the actual counts. Pass `NULL` buffers to query sizes only
(returns 1). Returns 0 for a bad `which`, `NULL` config/length pointers, or
buffers that are too small.

## Simulation

| Function | Description |
|----------|-------------|
| `TdofSimulation* tdof_core_simulate(const TdofConfig*)` | Runs both comparisons. `NULL` on invalid config or an internal Eigen failure. |
| `void tdof_core_free_simulation(TdofSimulation*)` | Releases the handle. |
| `int32_t tdof_core_sim_length(const TdofSimulation*)` | $`N = \lceil t_{\rm end}/dt - 10^{-12}\rceil`$ (200 with the defaults). |
| `int32_t tdof_core_sim_copy_time(…)` | Time axis — **not** scaled by `ref`. |
| `int32_t tdof_core_sim_copy_r(…)` | Reference step, scaled by `ref`. |
| `int32_t tdof_core_sim_copy_z(…)` | Pre-filtered reference $z = K_2 r$, scaled. |
| `int32_t tdof_core_sim_copy_y_pid(…)` | 1-DOF PID output $`G_{yz} r`$, scaled. |
| `int32_t tdof_core_sim_copy_y_2dof(…)` | 2-DOF output $`G_{yz} z`$, scaled. |
| `const char* tdof_core_version(void)` | Static version string. |

Signals are stored normalised to a unit step and multiplied by `ref` inside the
copy functions, so changing only the amplitude needs no re-simulation on the
core side.
