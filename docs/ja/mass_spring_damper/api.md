# `msd_core` — C ABI

[C ABI 概要](../c-abi-reference.md) · [理論](theory.md) · [English](../../en/mass_spring_damper/api.md)


ヘッダ： `examples/mass_spring_damper/core/include/msd_core.h`
このコアはプラント（ `MsdCase` ）とサンプリング格子
（ `MsdSamplingConfig` ）を分離しており、同一格子で複数ケースを掃引できます。

## 構造体

```c
typedef struct MsdCase {
    double m;                 /* 質量 [kg]、> 0        */
    double c;                 /* 減衰 [N·s/m]          */
    double k;                 /* ばね定数 [N/m]、>= 0  */
    double force_amplitude;   /* F [N]                 */
    double force_omega;       /* ω [rad/s]             */
    double x0, v0;            /* 初期位置・初期速度      */
} MsdCase;

typedef struct MsdSamplingConfig {
    double dt;    /* 刻み [s]、> 0     */
    double stop;  /* 終了時刻 [s]、> 0 */
} MsdSamplingConfig;
```

## 関数

| 関数 | 説明 |
|------|------|
| `void msd_core_default_case(MsdCase*)` | baseline ケース： m 1、c 2、k 5、F 0.5、ω 2、x₀ = v₀ = 0。 |
| `void msd_core_default_sampling(MsdSamplingConfig*)` | dt = 0.001 s、stop = 10 s。 |
| `int32_t msd_core_derived(const MsdCase*, double* omega_n, double* zeta)` | $`\omega_n = \sqrt{k/m}`$ 、 $`\zeta = c/(2\sqrt{mk})`$ 。 `m <= 0` または `k < 0` で 0 を返す。 `m·k == 0` のとき ζ は 0 として報告。出力ポインタは `NULL` 可。 |
| `MsdSimulation* msd_core_simulate(const MsdCase*, const MsdSamplingConfig*)` | 固定刻み RK4。入力不正、またはサンプル数が 2 未満になる場合は `NULL` 。 |
| `void msd_core_free_simulation(MsdSimulation*)` | ハンドルを解放。 |
| `int32_t msd_core_sim_length(const MsdSimulation*)` | $`N = \lfloor (\mathrm{stop}+dt)/dt - 10^{-12}\rfloor + 1`$ 。 |
| `int32_t msd_core_sim_copy_time(…)` | 時間軸。 |
| `int32_t msd_core_sim_copy_position(…)` | x(t)。 |
| `int32_t msd_core_sim_copy_velocity(…)` | v(t)。 |
| `int32_t msd_core_sim_copy_force(…)` | f(t) = F sin(ωt)。同じ格子でサンプリング。 |
| `double msd_core_sim_final_position(…)` / `…_final_velocity(…)` | 最終サンプル。 |
| `double msd_core_sim_max_abs_position(…)` / `…_max_abs_velocity(…)` | 絶対値の最大。 |
| `const char* msd_core_version(void)` | 静的バージョン文字列。 |
