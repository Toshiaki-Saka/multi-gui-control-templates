# C ABI リファレンス

English: [../en/c-abi-reference.md](../en/c-abi-reference.md)

4 コアの全エクスポートシンボルです。ヘッダは
`examples/<name>/core/include/<slug>_core.h` にあります。

## 共通規約

- **呼び出し規約**： C（ `extern "C"` 、cdecl）。C# は
  `CallingConvention.Cdecl` を宣言し、Python は `ctypes.CDLL` を使います。
- **境界を越える型**： `double` 、 `int32_t` 、不透明ハンドルポインタ、
  `const char*` のみ。
- **ハンドルは不透明。** `*_simulate()` が返したポインタは、対応する
  `*_free_simulation()` で解放してください。アクセサに `NULL` を渡しても安全で、
  `0` / `0.0` が返ります。
- **バッファは呼び出し側の所有物。** すべての `*_copy_*()` は
  `(handle, double* buffer, int32_t buffer_len)` を取り、書き込んだサンプル数を
  返します。 `buffer_len` がシミュレーション長より小さい場合は何も書かずに `0`
  を返します。先に `*_sim_length()` でサイズを取得してください。
- **エラーは戻り値。** `*_simulate()` は不正・非有限な設定に対して `NULL` を
  返します。境界を越えて例外が飛ぶことはありません。
- **エクスポートマクロ**： `<SLUG>_CORE_API` は、コアのビルド時
  （ `<SLUG>_CORE_BUILD` 定義時）に `__declspec(dllexport)` 、Windows の利用側
  では `__declspec(dllimport)` 、それ以外では
  `__attribute__((visibility("default")))` に展開されます。

C からの典型的な使い方：

```c
PidConfig cfg;
pid_core_default_config(&cfg);
cfg.kp = 0.2;

PidSimulation* sim = pid_core_simulate(&cfg);
if (!sim) { /* 設定が不正 */ }

int32_t n = pid_core_sim_length(sim);
double* t     = malloc(n * sizeof(double));
double* theta = malloc(n * sizeof(double));
pid_core_sim_copy_time (sim, t,     n);
pid_core_sim_copy_theta(sim, theta, n);

pid_core_free_simulation(sim);
```

---

## 1. `pid_core`

ヘッダ： `examples/pid/core/include/pid_core.h`

### 設定構造体

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

### 関数

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

---

## 2. `msd_core`

ヘッダ： `examples/mass_spring_damper/core/include/msd_core.h`
このコアはプラント（ `MsdCase` ）とサンプリング格子
（ `MsdSamplingConfig` ）を分離しており、同一格子で複数ケースを掃引できます。

### 構造体

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

### 関数

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

---

## 3. `track_core`

ヘッダ： `examples/pi_path_tracking/core/include/track_core.h`
2 種類のハンドルを公開する唯一のコアです。参照経路をシミュレーション無しで
生成・描画できます。

### 設定構造体

```c
typedef struct TrackConfig {
    /* プラント */
    double m, izz, cornering_power;
    /* 積分・サンプリング */
    double h, tc, total_time;
    /* 速度制御 */
    double target_speed;
    /* 追従ゲイン */
    double ky_p, ky_i, kpsi_p, kpsi_i, kr_damping;
    /* 制限値 */
    double n_moment_limit, fx_limit, error_integral_limit;
    /* 先読み */
    int32_t lookahead_index;
    /* 初期条件 */
    double initial_y_offset, initial_heading_deg;
    /* 参照経路の形状 */
    double straight1_len, radius, straight2_len, ds;
} TrackConfig;
```

`track_core_default_config()` は参照スクリプトの調整済み値を格納します
（[algorithms.md §3.6](algorithms.md#36-制御則) を参照）。 `m` 、 `izz` 、
`h` 、 `tc` 、 `total_time` 、 `radius` 、 `ds` および 3 つの制限値はすべて正、
`lookahead_index` は 0 以上である必要があります。

### 参照経路

| 関数 | 説明 |
|------|------|
| `TrackReferencePath* track_core_make_reference(const TrackConfig*)` | 直線 → 90° 円弧 → 直線を `ds` 間隔で生成。設定が不正なら `NULL` 。 |
| `void track_core_free_reference(TrackReferencePath*)` | ハンドルを解放。 |
| `int32_t track_core_ref_length(const TrackReferencePath*)` | 点数（既定で 458）。 |
| `int32_t track_core_ref_copy_x / _y / _psi(…)` | 経路座標と接線方位。 |

### シミュレーション

| 関数 | 説明 |
|------|------|
| `TrackSimulation* track_core_simulate(const TrackConfig*)` | 閉ループを実行。参照経路は内部で生成。設定が不正なら `NULL` 。 |
| `void track_core_free_simulation(TrackSimulation*)` | ハンドルを解放。 |
| `int32_t track_core_sim_length(const TrackSimulation*)` | $`N = \lfloor T/h \rfloor`$ （既定で 9000）。 |

サンプルごとのチャネル。いずれも
`int32_t <name>(const TrackSimulation*, double*, int32_t)` 形式です。

| アクセサ | 信号 |
|---------|------|
| `track_core_sim_copy_time` | t [s] |
| `track_core_sim_copy_x` / `_y` / `_psi` | 全体座標系の位置・方位 |
| `track_core_sim_copy_u` / `_v` / `_r` | 車体速度とヨーレート |
| `track_core_sim_copy_beta` | 横すべり角 β |
| `track_core_sim_copy_ey` / `_epsi` | 横偏差・方位偏差（先読み点で評価） |
| `track_core_sim_copy_nmoment` / `_fx` | ヨーモーメント指令と前後力指令 |
| `track_core_sim_copy_x_ref` / `_y_ref` / `_psi_ref` | 各ステップの**最近傍**参照点 |

集計指標（いずれも `double <name>(const TrackSimulation*)` ）：
`track_core_sim_path_error_rms` 、 `_path_error_max` 、 `_ey_rms` 、
`_ey_max_abs` 、 `_epsi_rms` 、 `_epsi_max_abs` 、 `_nmoment_max_abs` 。

`const char* track_core_version(void)` は静的バージョン文字列を返します。

---

## 4. `tdof_core`

ヘッダ： `examples/two_dof/core/include/tdof_core.h`
ビルド時に Eigen が必要ですが、この依存は ABI には漏れません。

### 設定構造体

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

### 伝達関数の取得

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

### シミュレーション

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

---

## 5. 本リポジトリのバインディング層

| 言語 | 場所 | 方式 |
|------|------|------|
| C++（Qt6） | `examples/<name>/frontend_qt/` | CMake ターゲットに直接リンク。ヘッダをそのまま include |
| C#（Avalonia） | `examples/<name>/frontend_avalonia/<Proj>/Native/` | `[DllImport]` + `[StructLayout(LayoutKind.Sequential)]` を `*Solver` クラスで包み、 `double[]` へコピーして `finally` で解放 |
| Python（example ごと） | `examples/<name>/frontend_python/<slug>_core.py` | `restype` / `argtypes` を明示した `ctypes` 。 `np.frombuffer(...).copy()` で NumPy 配列を返す |
| Python（共通ギャラリー） | `gui/python/bindings/` | 同じモジュール群。 `gui/python/libloader.py` が事前にライブラリパスを解決 |

全体の関係は [architecture.md](architecture.md) を参照してください。
