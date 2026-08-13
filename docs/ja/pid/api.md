# `pid_core` — C ABI

[C ABI 概要](../c-abi-reference.md) · [理論](theory.md) · [English](../../en/pid/api.md)


ヘッダ： `examples/pid/core/include/pid_core.h`

## 設定構造体

```c
typedef struct PidConfig {
    double  theta_start;     /* 初期状態                                  */
    double  theta_goal;      /* 目標値                                    */
    double  offset;          /* 毎ステップ theta から減算されるバイアス      */
    int32_t time_length;     /* サンプル数（>= 2）                        */
    double  kp, ki, kd;      /* ゲイン                                    */
    double  dt;              /* 時間刻み（> 0）。I 項と D 項をスケール      */
    double  integral_clamp;  /* |error_sum| の上限。0 = 無効               */
    double  output_clamp;    /* |m| の上限。       0 = 無効               */
} PidConfig;
```

## 関数

| 関数 | 説明 |
|------|------|
| `void pid_core_default_config(PidConfig*)` | 既定値を格納（θ 0 → 90、N = 150、kp 0.10、ki 0.01、kd 0、dt 1、クランプ無効）。 |
| `PidSimulation* pid_core_simulate(const PidConfig*)` | ループを実行。非有限な値、 `dt <= 0` 、負のクランプ、 `time_length < 2` のいずれかで `NULL` 。 |
| `void pid_core_free_simulation(PidSimulation*)` | ハンドルを解放。 |
| `int32_t pid_core_sim_length(const PidSimulation*)` | サンプル数（= `time_length` ）。 |
| `int32_t pid_core_sim_copy_time(const PidSimulation*, double*, int32_t)` | 時間軸 $`t_n = n\,\Delta t`$ 。 |
| `int32_t pid_core_sim_copy_theta(const PidSimulation*, double*, int32_t)` | 応答 θ 。 |
| `double pid_core_sim_final_theta(const PidSimulation*)` | 最終サンプル。 |
| `double pid_core_sim_max_theta(const PidSimulation*)` | 実行中の最大値。 |
| `double pid_core_sim_min_theta(const PidSimulation*)` | 実行中の最小値。 |
| `const char* pid_core_version(void)` | 静的文字列（例： `"pid_core 1.0.0"` ）。 |
