# `track_core` — C ABI

[モジュール概要](README.md) · [理論](theory.md) · [Python バインディング](python.md) · [English](../../en/pi_path_tracking/api.md)

`track_core` は純粋な C インターフェースを公開しているため、FFI を持つ言語
（C#、Python、Rust など）から利用できます。すべてのシンボルは Windows では
`__declspec(dllexport)` 、Linux / macOS では
`__attribute__((visibility("default")))` で修飾されています。

---

## ライフサイクル

```
track_core_default_config(&cfg)        ← cfg を既定値で埋める
                                          （必要に応じてフィールドを編集）

ref  = track_core_make_reference(&cfg) ← 参照経路を生成
         … ref の配列を読む …
track_core_free_reference(ref)         ← 解放

sim  = track_core_simulate(&cfg)       ← 閉ループシミュレーションを実行
         … sim の配列と指標を読む …
track_core_free_simulation(sim)        ← 解放
```

ヒープ上のオブジェクトはすべてライブラリ内部で確保されるため、対応する
`_free_*` 関数で**必ず**解放してください。アクセサに `NULL` を渡すのは安全です
（0 または 0.0 を返します）。

---

## 設定 — `TrackConfig`

```c
typedef struct TrackConfig {
    /* プラント */
    double m;                   /* 質量 [kg]                          既定  0.1    */
    double izz;                 /* ヨー慣性 [kg·m²]                   既定  1.0    */
    double cornering_power;     /* fy = -C·β  [N/rad]                 既定 20.0    */

    /* タイミング */
    double h;                   /* 積分刻み [s]                       既定  1e-4   */
    double tc;                  /* 制御更新周期 [s]                    既定  1e-3   */
    double total_time;          /* 総シミュレーション時間 [s]           既定  0.90   */

    /* 速度目標 */
    double target_speed;        /* [m/s]                              既定  1.0    */

    /* 横 / ヨー ゲイン */
    double ky_p;                /* 横偏差 P ゲイン                    既定 400.0   */
    double ky_i;                /* 横偏差 I ゲイン                    既定   0.0   */
    double kpsi_p;              /* 方位 P ゲイン                      既定 200.0   */
    double kpsi_i;              /* 方位 I ゲイン                      既定   0.0   */
    double kr_damping;          /* ヨーレートダンパゲイン              既定  20.0   */

    /* 飽和制限 */
    double n_moment_limit;      /* |N| の制限 [N·m]                   既定 500.0   */
    double fx_limit;            /* |fx| の制限 [N]                    既定   5.0   */
    double error_integral_limit;/* σ_y, σ_ψ のアンチワインドアップ     既定   0.2   */

    /* 経路追従 */
    int32_t lookahead_index;    /* 先読み点数                         既定  60     */

    /* 初期条件 */
    double initial_y_offset;    /* y(0) [m]                           既定 -0.03   */
    double initial_heading_deg; /* ψ(0) [deg]                         既定  3.0    */

    /* 参照経路の形状 */
    double straight1_len;       /* 1 本目の直線 [m]                   既定  0.30   */
    double radius;              /* 円弧半径 [m]                       既定  0.20   */
    double straight2_len;       /* 2 本目の直線 [m]                   既定  0.30   */
    double ds;                  /* 弧長リサンプル刻み [m]              既定  0.002  */
} TrackConfig;
```

数値フィールドはすべて `double` で、整数は `lookahead_index`（ `int32_t` ）
だけです。

---

## 関数

### その他

```c
const char* track_core_version(void);
```

`"track_core 1.0.0"` のような静的文字列を返します。

---

### 設定

```c
void track_core_default_config(TrackConfig* cfg);
```

`*cfg` を上表の既定値で埋めます。スタック上に `TrackConfig` を用意してこれを
呼び、個々のフィールドを変更してから `make_reference` や `simulate` を呼びます。

---

### 参照経路

```c
TrackReferencePath* track_core_make_reference(const TrackConfig* cfg);
void                track_core_free_reference(TrackReferencePath* ref);
```

`make_reference` は（直線 → 円弧 → 直線）の参照経路を確保して返します。
`cfg` が `NULL` の場合や、フィールドが不正な場合（非有限、正であるべき箇所が
非正）は `NULL` を返します。

```c
int32_t track_core_ref_length(const TrackReferencePath* ref);
```

参照点の数 $`N_{\rm ref}`$ 。既定値では 458。

```c
int32_t track_core_ref_copy_x  (const TrackReferencePath*, double* buf, int32_t buf_len);
int32_t track_core_ref_copy_y  (const TrackReferencePath*, double* buf, int32_t buf_len);
int32_t track_core_ref_copy_psi(const TrackReferencePath*, double* buf, int32_t buf_len);
```

$`x`$ 、 $`y`$ 、 $`\psi`$ の配列を、呼び出し側が確保した `ref_length` 以上の
バッファへコピーします。コピーした要素数を返し、エラー時（ `buf_len` 不足、
`NULL` ポインタ）は 0 を返します。

---

### シミュレーション

```c
TrackSimulation* track_core_simulate(const TrackConfig* cfg);
void             track_core_free_simulation(TrackSimulation* sim);
```

`simulate` は閉ループシミュレーション全体を実行し、結果へのハンドルを返します。
設定が不正な場合や確保に失敗した場合は `NULL` を返します。

```c
int32_t track_core_sim_length(const TrackSimulation* sim);
```

記録された時間ステップ数。 `(int)(total_time / h)` に等しく、既定値では 9000。

#### サンプルごとのアクセサ

各関数はシミュレーション 1 ステップにつき `double` 1 個を `buf` へコピーします。
要素数を返し、エラー時は 0 を返します。

```c
int32_t track_core_sim_copy_time   (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_x      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_y      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_psi    (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_u      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_v      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_r      (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_beta   (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_ey     (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_epsi   (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_nmoment(const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_fx     (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_x_ref  (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_y_ref  (const TrackSimulation*, double* buf, int32_t buf_len);
int32_t track_core_sim_copy_psi_ref(const TrackSimulation*, double* buf, int32_t buf_len);
```

| アクセサ接尾辞 | 物理量 | 単位 |
|---------------|--------|------|
| `time` | シミュレーション時刻 $`t`$ | s |
| `x` | 全体座標系の X 位置 | m |
| `y` | 全体座標系の Y 位置 | m |
| `psi` | 方位 $`\psi`$ | rad |
| `u` | 前後速度 | m/s |
| `v` | 横速度 | m/s |
| `r` | ヨーレート | rad/s |
| `beta` | 横すべり角 $`\beta`$ | rad |
| `ey` | 先読み点での横偏差 $`e_y`$ | m |
| `epsi` | 先読み点での方位偏差 $`e_\psi`$ | rad |
| `nmoment` | 印加ヨーモーメント $`N`$ | N·m |
| `fx` | 印加前後力 $`f_x`$ | N |
| `x_ref` | 最近傍参照点の X | m |
| `y_ref` | 最近傍参照点の Y | m |
| `psi_ref` | 最近傍参照点の $`\psi`$ | rad |

> **注意**： `x_ref` / `y_ref` / `psi_ref` は先読み点 $`k_{\rm la}`$ ではなく、
> *最近傍*点 $`k^*`$ の値が記録されます。

#### 集計指標

```c
double track_core_sim_path_error_rms (const TrackSimulation*);
double track_core_sim_path_error_max (const TrackSimulation*);
double track_core_sim_ey_rms         (const TrackSimulation*);
double track_core_sim_ey_max_abs     (const TrackSimulation*);
double track_core_sim_epsi_rms       (const TrackSimulation*);
double track_core_sim_epsi_max_abs   (const TrackSimulation*);
double track_core_sim_nmoment_max_abs(const TrackSimulation*);
```

| 関数 | 定義 |
|------|------|
| `path_error_rms` | $`\sqrt{\frac{1}{N}\sum\|\mathbf{p}_i - \mathbf{p}_{{\rm ref},i}\|^2}`$ |
| `path_error_max` | $`\max_i \|\mathbf{p}_i - \mathbf{p}_{{\rm ref},i}\|`$ |
| `ey_rms` | $`\sqrt{\frac{1}{N}\sum e_{y,i}^2}`$ |
| `ey_max_abs` | $`\max_i \|e_{y,i}\|`$ |
| `epsi_rms` | $`\sqrt{\frac{1}{N}\sum e_{\psi,i}^2}`$ |
| `epsi_max_abs` | $`\max_i \|e_{\psi,i}\|`$ |
| `nmoment_max_abs` | $`\max_i \|N_i\|`$ |

`path_error` は車両位置から*最近傍*参照点までのユークリッド距離です。
`ey` は*先読み*点での横偏差なので、先読みオフセットの分だけ両者は異なります。

---

## 最小の C 使用例

```c
#include "track_core.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    TrackConfig cfg;
    track_core_default_config(&cfg);

    /* 円弧半径を大きくする */
    cfg.radius = 0.30;

    TrackSimulation* sim = track_core_simulate(&cfg);
    if (!sim) { fputs("simulate failed\n", stderr); return 1; }

    int n = track_core_sim_length(sim);
    double* x = malloc(n * sizeof(double));
    double* y = malloc(n * sizeof(double));
    track_core_sim_copy_x(sim, x, n);
    track_core_sim_copy_y(sim, y, n);

    printf("path_error_rms = %.6f m\n", track_core_sim_path_error_rms(sim));

    track_core_free_simulation(sim);
    free(x); free(y);
    return 0;
}
```
