# `msd_core` — C ABI

Back to the [C ABI overview](../c-abi-reference.md) · [theory](theory.md) · [日本語](../../ja/mass_spring_damper/api.md)


Header: `examples/mass_spring_damper/core/include/msd_core.h`.
This core splits the plant (`MsdCase`) from the sampling grid
(`MsdSamplingConfig`) so a frontend can sweep several cases on one grid.

## Structs

```c
typedef struct MsdCase {
    double m;                 /* mass [kg], > 0            */
    double c;                 /* damping [N·s/m]           */
    double k;                 /* stiffness [N/m], >= 0     */
    double force_amplitude;   /* F [N]                     */
    double force_omega;       /* ω [rad/s]                 */
    double x0, v0;            /* initial position/velocity */
} MsdCase;

typedef struct MsdSamplingConfig {
    double dt;    /* step [s], > 0     */
    double stop;  /* end time [s], > 0 */
} MsdSamplingConfig;
```

## Functions

| Function | Description |
|----------|-------------|
| `void msd_core_default_case(MsdCase*)` | Baseline case: m 1, c 2, k 5, F 0.5, ω 2, x₀ = v₀ = 0. |
| `void msd_core_default_sampling(MsdSamplingConfig*)` | dt = 0.001 s, stop = 10 s. |
| `int32_t msd_core_derived(const MsdCase*, double* omega_n, double* zeta)` | $`\omega_n = \sqrt{k/m}`$, $`\zeta = c/(2\sqrt{mk})`$. Returns 0 if `m <= 0` or `k < 0`; ζ reported as 0 when `m·k == 0`. Either output pointer may be `NULL`. |
| `MsdSimulation* msd_core_simulate(const MsdCase*, const MsdSamplingConfig*)` | Fixed-step RK4. `NULL` on invalid input or when fewer than 2 samples result. |
| `void msd_core_free_simulation(MsdSimulation*)` | Releases the handle. |
| `int32_t msd_core_sim_length(const MsdSimulation*)` | $`N = \lfloor (\mathrm{stop}+dt)/dt - 10^{-12}\rfloor + 1`$. |
| `int32_t msd_core_sim_copy_time(…)` | Time axis. |
| `int32_t msd_core_sim_copy_position(…)` | x(t). |
| `int32_t msd_core_sim_copy_velocity(…)` | v(t). |
| `int32_t msd_core_sim_copy_force(…)` | f(t) = F sin(ωt), sampled on the same grid. |
| `double msd_core_sim_final_position(…)` / `…_final_velocity(…)` | Last sample. |
| `double msd_core_sim_max_abs_position(…)` / `…_max_abs_velocity(…)` | Peak magnitude. |
| `const char* msd_core_version(void)` | Static version string. |
