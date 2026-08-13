# `tdof_core` — C ABI

[C ABI 概要](../c-abi-reference.md) · [理論](theory.md) · [English](../../en/two_dof/api.md)


ヘッダ： `examples/two_dof/core/include/tdof_core.h`
ビルド時に Eigen が必要ですが、この依存は ABI には漏れません。

## 設定構造体

```c
typedef struct TdofConfig {
    double m, c, k;      /* プラント P(s) = 1 / (m s^2 + c s + k) */
    double kp, ki, kd;   /* PID ゲイン                            */
    double ref;          /* 表示用のステップ振幅                    */
    double t_end, dt;    /* 時間格子： [0, t_end) 刻み dt          */
} TdofConfig;
```

`tdof_core_default_config()` は m = 0.01、c = 0.015、k = 1.0、kp = 2、
ki = 10、kd = 0.1、ref = 10、t_end = 2 s、dt = 0.01 s を格納します。
`tdof_core_simulate()` は `m > 0` 、有限なゲイン、 `t_end > 0` 、 `dt > 0` 、
`dt < t_end` を要求します。

## 伝達関数の取得

```c
int32_t tdof_core_get_tf(const TdofConfig* cfg, int32_t which,
                         double* num, int32_t* num_len,
                         double* den, int32_t* den_len);
```

`which` ： `0` = プラント $`P`$ 、 `1` = PID $`K_1`$ 、
`2` = 前置フィルタ $`K_2`$ 、 `3` = 閉ループ $`G_{yz}`$ 。係数は最高次から
順に返されます。

2 段階の呼び出し規約です。入力時に `*num_len` / `*den_len` はバッファ容量を、
戻り時には実際の係数個数を保持します。バッファに `NULL` を渡すとサイズ照会のみ
（戻り値 1）。 `which` が不正、設定や長さポインタが `NULL` 、バッファ不足の場合
は 0 を返します。

## シミュレーション

| 関数 | 説明 |
|------|------|
| `TdofSimulation* tdof_core_simulate(const TdofConfig*)` | 2 通りの比較を実行。設定不正または内部の Eigen 例外で `NULL` 。 |
| `void tdof_core_free_simulation(TdofSimulation*)` | ハンドルを解放。 |
| `int32_t tdof_core_sim_length(const TdofSimulation*)` | $`N = \lceil t_{\rm end}/dt - 10^{-12}\rceil`$ （既定で 200）。 |
| `int32_t tdof_core_sim_copy_time(…)` | 時間軸。 `ref` を**掛けない**。 |
| `int32_t tdof_core_sim_copy_r(…)` | 目標ステップ（ `ref` 倍）。 |
| `int32_t tdof_core_sim_copy_z(…)` | 前置フィルタ後の目標値 $`z = K_2 r`$ （ `ref` 倍）。 |
| `int32_t tdof_core_sim_copy_y_pid(…)` | 1 自由度 PID 出力 $`G_{yz} r`$ （ `ref` 倍）。 |
| `int32_t tdof_core_sim_copy_y_2dof(…)` | 2 自由度出力 $`G_{yz} z`$ （ `ref` 倍）。 |
| `const char* tdof_core_version(void)` | 静的バージョン文字列。 |

信号は単位ステップに正規化して保持され、コピー関数の中で `ref` が掛けられます。
そのため振幅だけを変更する場合、コア側の再計算は不要です。
