# `pid_core` — C ABI

Back to the [C ABI overview](../c-abi-reference.md) · [theory](theory.md) · [日本語](../../ja/pid/api.md)


Header: `examples/pid/core/include/pid_core.h`.

## Config

```c
typedef struct PidConfig {
    double  theta_start;     /* initial state                              */
    double  theta_goal;      /* setpoint                                   */
    double  offset;          /* subtracted from theta each step (bias)     */
    int32_t time_length;     /* number of samples (>= 2)                   */
    double  kp, ki, kd;      /* gains                                      */
    double  dt;              /* time step (> 0); scales the I and D terms  */
    double  integral_clamp;  /* max |error_sum|; 0 = disabled              */
    double  output_clamp;    /* max |m|;         0 = disabled              */
} PidConfig;
```

## Functions

| Function | Description |
|----------|-------------|
| `void pid_core_default_config(PidConfig*)` | Fills the defaults (θ 0 → 90, N = 150, kp 0.10, ki 0.01, kd 0, dt 1, clamps off). |
| `PidSimulation* pid_core_simulate(const PidConfig*)` | Runs the loop. `NULL` if any field is non-finite, `dt <= 0`, a clamp is negative, or `time_length < 2`. |
| `void pid_core_free_simulation(PidSimulation*)` | Releases the handle. |
| `int32_t pid_core_sim_length(const PidSimulation*)` | Sample count (= `time_length`). |
| `int32_t pid_core_sim_copy_time(const PidSimulation*, double*, int32_t)` | Time axis $`t_n = n\,\Delta t`$. |
| `int32_t pid_core_sim_copy_theta(const PidSimulation*, double*, int32_t)` | Response θ. |
| `double pid_core_sim_final_theta(const PidSimulation*)` | Last sample. |
| `double pid_core_sim_max_theta(const PidSimulation*)` | Maximum over the run. |
| `double pid_core_sim_min_theta(const PidSimulation*)` | Minimum over the run. |
| `const char* pid_core_version(void)` | Static string, e.g. `"pid_core 1.0.0"`. |
