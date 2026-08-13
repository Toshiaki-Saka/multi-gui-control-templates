# アルゴリズム詳細

English: [../en/algorithms.md](../en/algorithms.md)

各コアが実際に何を計算しているかを詳述します。プラントモデル、制御則、離散化、
積分器、評価指標、そして個々の数値的選択の理由までを扱います。ここに現れる式は
すべて `examples/<name>/core/src/` のコードに対応しています。

- [0. 共通の約束事](#0-共通の約束事)
- [1. `pid` — PID 姿勢制御](#1-pid--pid-姿勢制御)
- [2. `mass_spring_damper` — 強制応答（RK4）](#2-mass_spring_damper--強制応答rk4)
- [3. `pi_path_tracking` — PI 経路追従](#3-pi_path_tracking--pi-経路追従)
- [4. `two_dof` — 2 自由度制御 vs PID](#4-two_dof--2-自由度制御-vs-pid)
- [5. 手法の比較](#5-手法の比較)

---

## 0. 共通の約束事

**飽和関数。** 全コアで同じクランプを使います。

$$\mathrm{sat}(v, a, b) = \max\bigl(a,\ \min(b,\ v)\bigr)$$

範囲が $`[-L, L]`$ のときは $`\mathrm{sat}(v, \pm L)`$ と書きます。
`pid` では上限 0 が「無効」を意味し、 `track` では各上限に正の値が必須です。

**時間格子。** 4 コアのうち 3 つは、参照スクリプトの NumPy `arange` を
再現しています。サンプル数そのものが数値一致の契約に含まれるためです。

| コア | Python の式 | C++ でのサンプル数 |
|------|------------|------------------|
| `pid` | `range(1, time_length)` | $`N = \texttt{time\_length}`$（添字 0 が初期状態） |
| `msd` | `np.arange(0.0, stop + dt, dt)` | $`N = \lfloor (t_{\rm stop}+\Delta t)/\Delta t - 10^{-12}\rfloor + 1`$ |
| `tdof` | `np.arange(0, t_end, dt)` | $`N = \lceil t_{\rm end}/\Delta t - 10^{-12}\rceil`$ |
| `track` | 固定ステップ数 | $`N = \lfloor T/h \rfloor`$ |

$`10^{-12}`$ のガードは浮動小数点誤差を吸収するためのもので、区間長が刻み幅の
整数倍のときに末尾サンプルが 1 つ増減するのを防ぎます。

**入力検証。** すべての `*_simulate()` は、設定が有限でない場合や定義域制約
（ $`m > 0`$ 、 $`dt > 0`$ 、 $`\texttt{time\_length} \ge 2`$ など）を満たさない
場合に、例外ではなく `NULL` を返します。中途半端な結果は生成されません。

**決定性。** コア内部に乱数・適応刻み・スレッドは一切ありません。同じ設定は常に
ビット単位で同一の出力を返します。だからこそスモークテストで小数値を厳密に
固定できます。

---

## 1. `pid` — PID 姿勢制御

実装： `examples/pid/core/src/pid_core.cpp`
（ `pid_advanced_simulation.py` の移植）

### 1.1 ループ構成

P・I・D の効きが見える最小構成として、プラントは純粋な積算器であり、制御出力が
そのまま状態に加算されます。

```
        θ_goal ──▶( Σ )──e──▶[ PID ]──m──▶( Σ )──▶ θ
                    ▲ −                     ▲ −
                    │                       │
                    └──────── θ ◀───────────┘   offset
```

### 1.2 制御則

ステップ $`n`$ において（ $`e_{-1} = 0`$ 、 $`\Sigma_{-1} = 0`$ ）：

$$e_n = \theta_{\rm goal} - \theta_n$$

$$\Sigma_n = \Sigma_{n-1} + e_n\,\Delta t$$

$$d_n = \frac{e_n - e_{n-1}}{\Delta t}$$

$$m_n^{\rm raw} = k_p\,e_n + k_i\,\Sigma_n + k_d\,d_n$$

```math
m_n = \begin{cases}
\mathrm{sat}(m_n^{\rm raw}, \pm M) & M > 0 \\
m_n^{\rm raw} & M = 0\ (\text{無効})
\end{cases}
```

$$\theta_{n+1} = \theta_n + m_n - \mathrm{offset}$$

積分項は**次の**反復に向けてクランプされます。

$$\Sigma_n \leftarrow \mathrm{sat}(\Sigma_n, \pm S) \quad (S > 0)$$

ここで $`M`$ が `output_clamp` 、 $`S`$ が `integral_clamp` です。

### 1.3 実行順序

順序は教科書どおりではなく、参照スクリプトを 1 行ずつ再現したものです。

```text
for n = 1 .. N-1:
    e      = θ_goal − θ                   # 現在の状態から偏差を計算
    Σ     += e · dt                       # クランプ前に積分
    d      = (e − e_prev) / dt
    m_raw  = kp·e + ki·Σ + kd·d
    m      = clamp(m_raw, ±output_clamp)  # アクチュエータ飽和
    θ     += m                            # プラント更新：純粋な積算器
    θ     −= offset                       # 定常外乱 / バイアス
    Σ      = clamp(Σ, ±integral_clamp)    # アンチワインドアップ（使用後に適用）
    e_prev = e
    t[n]   = n · dt ;  θ[n] = θ
```

プロットを読むうえで重要な帰結が 2 つあります。

- **アンチワインドアップは 1 ステップ遅れて効く。** $`\Sigma_n`$ はクランプ前に
  $`m_n`$ の計算に使われるため、クランプが制限するのは「蓄積された積分値」で
  あって当該ステップの出力ではありません。出力を即座に制限するのは出力クランプ
  のほうです。
- **`offset` は無条件に適用される。** 制御更新の後に減算されるため、積分項が
  打ち消すべき定常的なリークを表します。 $`k_i = 0`$ ならループは定常偏差を
  残したまま収束します。

### 1.4 `dt` の役割

$`\Delta t`$ は積分項と微分項をスケールします（ $`\Sigma`$ は $`e\,\Delta t`$
を累積し、 $`d`$ は $`\Delta t`$ で除算）。一方でプラント更新
$`\theta \mathrel{+}= m`$ は**スケールされません**。プラントは構造上
「1 サンプル = 1 制御周期」です。既定の $`\Delta t = 1`$ で参照漸化式と厳密に
一致し、 $`\Delta t`$ を変えると P に対する I・D の相対的な効きが変わります。
GUI のスライダはまさにそれを触るためのものです。

### 1.5 時間軸と出力

添字 0 が初期状態なので、配列長はちょうど `time_length` です。

$$t_n = n\,\Delta t,\qquad \theta_0 = \theta_{\rm start},\qquad n = 0,\dots,N-1$$

補助指標は $`\theta`$ に対する `final_theta`（最終値）、 `max_theta` 、
`min_theta` の 3 つで、配列をコピーしなくてもオーバーシュートと定常偏差を
読み取れます。

### 1.6 妥当性チェック

`pid_core_simulate()` は次をすべて満たさない限り `NULL` を返します。
$`\theta_{\rm start},\theta_{\rm goal},\mathrm{offset},k_p,k_i,k_d`$ が有限、
$`\Delta t > 0`$ 、両クランプが有限かつ $`\ge 0`$ 、
$`\texttt{time\_length} \ge 2`$ 。

### 1.7 既定値と挙動

| パラメータ | 既定値 | 備考 |
|-----------|--------|------|
| $`\theta_{\rm start}`$ | 0 | |
| $`\theta_{\rm goal}`$ | 90 | |
| `offset` | 0 | 無効 |
| `time_length` | 150 | サンプル数 |
| $`k_p`$ | 0.10 | |
| $`k_i`$ | 0.01 | |
| $`k_d`$ | 0.0 | |
| $`\Delta t`$ | 1.0 | |
| `integral_clamp` | 0 | 無効 |
| `output_clamp` | 0 | 無効 |

既定値は緩やかでよく減衰した立ち上がりになります。Python 参照側はより攻めた
$`k_p = 0.10`$ 、 $`k_i = 0.5`$ 、 $`k_d = 0.5`$ を使い、 $`\theta \approx 135`$
までオーバーシュートしてから 90 に収束します。README のスクリーンショットと
スモークテストが固定しているのはこちらの応答です。

プラントが積算器（離散積分器）なので、 $`k_i = k_d = 0`$ のときループは
$`k_p`$ に関する 1 次系になり、極は 1 ステップあたり $`1 - k_p`$ です。
したがって安定範囲は $`0 < k_p < 2`$ で、 $`k_p = 1`$ がデッドビートです。
$`k_i`$ を加えると 2 次系になり、参照ゲインで見られるオーバーシュートが
現れます。

---

## 2. `mass_spring_damper` — 強制応答（RK4）

実装： `examples/mass_spring_damper/core/src/msd_core.cpp`
（ `mass_spring_damper_forced_response.py` / `scipy.integrate.odeint` の移植）。
設計メモ： [../examples/mass_spring_damper/architecture.md](../examples/mass_spring_damper/architecture.md) 。

### 2.1 プラント

ばね $`k`$ とダッシュポット $`c`$ で壁につながれた質量 $`m`$ に、外力
$`f(t)`$ が加わる系です。

```
        k          c
wall ──/\/\/──┬──[==]──┬── → x(t)
              │         │
              └── [m] ──┘
                    ↑
                  f(t)
```

運動方向にニュートンの第 2 法則を適用すると

$$m\,\ddot{x} + c\,\dot{x} + k\,x = f(t), \qquad f(t) = F\sin(\omega t)$$

| 記号 | 単位 | 意味 |
|------|------|------|
| $`x(t)`$ | m | 平衡位置からの変位 |
| $`\dot x(t)`$ | m/s | 速度 |
| $`\ddot x(t)`$ | m/s² | 加速度 |
| $`m`$ | kg | 質量（ $`m > 0`$ ） |
| $`c`$ | N·s/m | 粘性減衰係数（ $`c \ge 0`$ ） |
| $`k`$ | N/m | ばね定数（ $`k \ge 0`$ ） |
| $`F`$ | N | 外力振幅 |
| $`\omega`$ | rad/s | 加振角周波数 |

$`\mathbf{z} = (x, v)^{\mathsf T}`$ 、 $`v = \dot x`$ とおいた 1 階系：

```math
\begin{bmatrix} \dot{x} \\ \dot{v} \end{bmatrix}
= \begin{bmatrix} 0 & 1 \\ -k/m & -c/m \end{bmatrix}
  \begin{bmatrix} x \\ v \end{bmatrix}
+ \begin{bmatrix} 0 \\ F\sin(\omega t)/m \end{bmatrix},
\qquad x(0) = x_0,\ v(0) = v_0
```

### 2.2 派生量

$$\omega_n = \sqrt{\frac{k}{m}}, \qquad \zeta = \frac{c}{2\sqrt{mk}} = \frac{c}{2m\omega_n}$$

$`\omega_n`$ は、減衰も外力も無い場合に系が自由振動する角周波数です。減衰比は
自由応答の振る舞いを次のように分類します。

| 条件 | 領域 | 自由応答 |
|------|------|---------|
| $`0 \le \zeta < 1`$ | 不足減衰 | 振動しながら減衰 |
| $`\zeta = 1`$ | 臨界減衰 | 振動せずに最速で減衰 |
| $`\zeta > 1`$ | 過減衰 | 指数減衰（実数 2 モード） |

不足減衰の場合、振動は減衰固有角周波数

$$\omega_d = \omega_n\sqrt{1-\zeta^2}, \qquad \zeta < 1$$

で起こります。

`msd_core_derived()` は $`\omega_n`$ と $`\zeta`$ を返し、 $`m \le 0`$ または
$`k < 0`$ のとき 0 を返します。 $`mk = 0`$ のときはゼロ除算せず $`\zeta = 0`$
として報告します。

定常強制応答の振幅は、周波数比 $`r = \omega/\omega_n`$ を使って

$$X = \frac{F/k}{\sqrt{(1-r^2)^2 + (2\zeta r)^2}}, \qquad \phi = \arctan\frac{2\zeta r}{1-r^2}$$

となります。既定ケースの「near natural frequency」
（ $`r \approx 0.98`$ 、 $`\zeta = 0.11`$ ）が他より大きく振れるのはこのためです。

### 2.3 積分器 — 古典的なベクトル RK4

参照側は適応刻みの LSODA ですが、コアは状態ベクトル全体に対する**固定刻み
RK4** を使います。上式の右辺を $`\mathbf{f}(t,\mathbf{z})`$ 、 $`h = \Delta t`$
として：

$$\mathbf{k}_1 = \mathbf{f}(t_n,\ \mathbf{z}_n)$$

$$\mathbf{k}_2 = \mathbf{f}\!\left(t_n + \tfrac{h}{2},\ \mathbf{z}_n + \tfrac{h}{2}\mathbf{k}_1\right)$$

$$\mathbf{k}_3 = \mathbf{f}\!\left(t_n + \tfrac{h}{2},\ \mathbf{z}_n + \tfrac{h}{2}\mathbf{k}_2\right)$$

$$\mathbf{k}_4 = \mathbf{f}\!\left(t_n + h,\ \mathbf{z}_n + h\,\mathbf{k}_3\right)$$

$$\mathbf{z}_{n+1} = \mathbf{z}_n + \frac{h}{6}\bigl(\mathbf{k}_1 + 2\mathbf{k}_2 + 2\mathbf{k}_3 + \mathbf{k}_4\bigr)$$

導関数ヘルパには時刻 $`t`$ を明示的に渡しており、 $`\mathbf{k}_2, \mathbf{k}_3`$
は $`t_n + h/2`$ 、 $`\mathbf{k}_4`$ は $`t_n + h`$ で外力を評価します。外力を
$`t_n`$ でしか評価しないと、強制項に関する精度が黙って 2 次に落ちます。

| 性質 | 値 |
|------|-----|
| 局所打ち切り誤差 | $`O(h^5)`$ |
| 大域誤差 | $`O(h^4)`$ |
| `scipy.odeint` との差 | 既定ケース・ $`h = 10^{-3}`$ s で 2 × 10⁻⁷ 以下 |

同次部の安定性には $`|h\lambda|`$ が RK4 の安定領域（負実軸上でおよそ 2.79）
内にある必要がありますが、 $`\omega_n \approx 2.2`$ 、 $`h = 10^{-3}`$ s では
4 桁の余裕があります。

### 2.4 時間格子

$$t_k = k\,\Delta t,\qquad N = \left\lfloor \frac{t_{\rm stop} + \Delta t}{\Delta t} - 10^{-12}\right\rfloor + 1$$

これは `np.arange(0.0, stop + dt, dt)` に対応し、 `stop` の 1 つ先まで
サンプルが出ます。既定の $`\Delta t = 10^{-3}`$ s 、 $`t_{\rm stop} = 10`$ s で
$`N = 10001`$ です。 $`N < 2`$ の場合は拒否されます。

外力も同じ格子上で保存されるため（ $`f_k = F\sin(\omega t_k)`$ ）、
フロントエンドは再計算なしで加振と応答を重ね描きできます。

### 2.5 既定のケーススイープ

全ケース共通： $`m = 1`$ kg 、 $`F = 0.5`$ N 、 $`x_0 = v_0 = 0`$ 。

| 名称 | $`c`$ | $`k`$ | $`\omega`$ | $`\omega_n`$ | $`\zeta`$ | 領域 |
|------|-----|-----|----------|-------------|---------|------|
| baseline | 2.0 | 5.0 | 2.0 | 2.236 | 0.447 | 不足減衰 |
| low damping | 0.5 | 5.0 | 2.0 | 2.236 | 0.112 | 弱減衰・振動的 |
| high damping | 5.0 | 5.0 | 2.0 | 2.236 | 1.118 | 過減衰 |
| stiffer spring | 2.0 | 12.0 | 2.0 | 3.464 | 0.289 | 固有振動数が高い |
| near natural frequency | 0.5 | 5.0 | 2.2 | 2.236 | 0.112 | 共振近傍 |

C++ コアはケース一覧を持ちません。 `default_cases()` は Python バインディング側
にあり、Qt / Avalonia のビューモデルにも同等の表があります。各フロントエンドが
自由にケースを追加・編集できるようにするためです。

$`t = 10`$ s における期待値（スモークテストが固定）：

| 名称 | $`x(10)`$ [m] | $`v(10)`$ [m/s] |
|------|-------------|---------------|
| baseline | −0.021155 | +0.238803 |
| low damping | +0.109906 | +0.709929 |
| high damping | −0.015682 | +0.094431 |
| stiffer spring | +0.035444 | +0.086453 |
| near natural frequency | +0.409217 | −0.121325 |

### 2.6 評価指標

`final_position` 、 `final_velocity` 、 `max_abs_position` 、
`max_abs_velocity` を積分と同じパスで計算します。

### 2.7 参考文献

- Den Hartog, J.P. (1985). *Mechanical Vibrations* (4th ed.). Dover.
- Inman, D.J. (2014). *Engineering Vibration* (4th ed.). Pearson.
- Press, W.H. et al. (2007). *Numerical Recipes in C++* (3rd ed.), §17.1 — Runge-Kutta.

---

## 3. `pi_path_tracking` — PI 経路追従

実装： `examples/pi_path_tracking/core/src/track_core.cpp`
（ `planar_path_tracking_pi_tuned.py` の移植）。
C ABI の詳細： [../examples/pi_path_tracking/api.md](../examples/pi_path_tracking/api.md) 。

### 3.1 状態量

| 記号 | 単位 | 意味 |
|------|------|------|
| $`u`$ | m/s | 車体座標系の前後速度 |
| $`v`$ | m/s | 車体座標系の横速度（左が正） |
| $`r`$ | rad/s | ヨーレート（反時計回りが正） |
| $`x, y`$ | m | 全体座標系の位置 |
| $`\psi`$ | rad | $`+X`$ 軸から測った方位角 |

### 3.2 プラント — 3 自由度平面運動モデル

$$\dot u = \frac{f_x}{m} + r\,v, \qquad \dot v = \frac{f_y}{m} - r\,u, \qquad \dot r = \frac{N}{I_{zz}}$$

$$\dot x = u\cos\psi - v\sin\psi, \qquad \dot y = u\sin\psi + v\cos\psi, \qquad \dot\psi = r$$

$`r v`$ と $`r u`$ の項は、運動量方程式を回転する車体座標系で書いたことで現れる
コリオリ加速度です。

横力は線形すべり角モデル：

$$\beta = \arctan\frac{v}{u}, \qquad f_y = -C\,\beta$$

ここで $`C`$ はコーナリングパワー [N/rad] です。実装には 1 ステップの遅れが
あります。 $`\beta`$ は各ステップの**末尾**で更新されるため、ステップ $`n`$ で
使われる $`f_y`$ はステップ $`n-1`$ のすべり角に基づきます
（ $`\beta_0 = 0`$ ）。これは参照スクリプトの挙動を忠実に再現するための意図的な
仕様です。

既定値： $`m = 0.1`$ kg 、 $`I_{zz} = 1.0`$ kg·m² 、 $`C = 20`$ N/rad 。

### 3.3 参照経路

弧長 $`\Delta s`$ で等間隔にリサンプルした 3 セグメントです。

1. **$`+X`$ 方向の直線** 、 $`s \in [0, L_1)`$ ： $`(s,\ 0)`$ 、
   $`\psi_{\rm ref} = 0`$ 。
2. **90° 左旋回の円弧**（半径 $`R`$ 、中心 $`(L_1, R)`$ ）、
   $`\theta \in [0, \pi/2)`$ 、 $`\Delta\theta = \Delta s / R`$ ：

   $$x = L_1 + R\sin\theta, \qquad y = R(1-\cos\theta), \qquad \psi_{\rm ref} = \theta$$

3. **$`+Y`$ 方向の直線**（円弧終端から）、 $`s \in [0, L_2)`$ ：
   $`(L_1+R,\ R + s)`$ 、 $`\psi_{\rm ref} = \pi/2`$ 。

点数は `np.arange(0, L, ds)` の意味（上端を含まない）に合わせます。

$$N_{\rm seg} = \left\lceil \frac{L}{\Delta s} - 10^{-12} \right\rceil$$

既定の $`L_1 = 0.30`$ m 、 $`R = 0.20`$ m 、 $`L_2 = 0.30`$ m 、
$`\Delta s = 0.002`$ m では 458 点になります。経路は
`track_core_make_reference()` で単独に取得できるため、シミュレーションを
走らせずに描画できます。

### 3.4 最近傍点と先読み

前回の添字を起点とする**窓付き**線形探索により、1 ステップあたりの計算量を
$`O(1)`$ に抑えつつ、後方の点に吸着するのを防ぎます。

$$k^\* = \arg\min_{k\,\in\,[\max(0,\,k_{\rm prev}-5),\ \min(N_{\rm ref},\,k_{\rm prev}+120))} \bigl\lVert (x,y) - (x_{\rm ref}[k],\,y_{\rm ref}[k]) \bigr\rVert_2$$

後方 5 点の余裕がわずかな後退を許容し、前方 120 点（既定 $`\Delta s`$ で
0.24 m）が 1 ステップで進める上限を与えます。

制御の目標点は**先読み点**です。

$$k_{\rm la} = \min\bigl(k^\* + L_{\rm la},\ N_{\rm ref}-1\bigr), \qquad L_{\rm la} = 60\ \text{点} = 0.12\ \text{m}$$

先読みによって、単なる位置偏差が予見的な操舵指令に変わります。これが無いと
$`-k_{y,p}e_y`$ 項だけでは円弧上で振動します。

### 3.5 偏差座標

$`\delta x = x - x_{\rm ref}[k_{\rm la}]`$ 、
$`\delta y = y - y_{\rm ref}[k_{\rm la}]`$ 、
$`\psi_r = \psi_{\rm ref}[k_{\rm la}]`$ として：

$$e_y = -\sin\psi_r\ \delta x + \cos\psi_r\ \delta y$$

$$e_\psi = \mathrm{wrap}_{(-\pi,\pi]}\bigl(\psi_r - \psi\bigr)$$

$`e_y`$ は位置偏差を参照法線 $`(-\sin\psi_r, \cos\psi_r)`$ に射影したもので、
車両が経路の左側にあるとき正になります。角度のラップは

$$\mathrm{wrap}_{(-\pi,\pi]}(\alpha) = \bigl((\alpha + \pi) \bmod 2\pi\bigr) - \pi$$

で、 `std::fmod` と負剰余の補正で実装しています。これにより参照方位が
$`\pm\pi`$ をまたいでも方位偏差が $`2\pi`$ 跳ぶことはありません。

### 3.6 制御則

制御は独自の周期 $`t_c`$ で更新されます。すなわち積分ステップ
$`\lfloor t_c/h \rfloor = 10`$ 回ごとで、更新の合間は $`f_x`$ と $`N`$ が保持
されます（アクチュエータへのゼロ次ホールド）。カウンタは 0 から始まり `>=` で
判定するため、**最初の更新はステップ 10** で起こります。つまり最初の 1 ms は
$`f_x = N = 0`$ の惰行です。

**前後方向 PI**（ゲインは参照に合わせてハードコード）：

$$e_{\rm spd} = v_{\rm target} - \sqrt{u^2+v^2}, \qquad \sigma_{\rm spd} \mathrel{+}= e_{\rm spd}\,t_c$$

$$f_x = \mathrm{sat}\bigl(100\,e_{\rm spd} + 0.1\,\sigma_{\rm spd},\ \pm f_{x,\rm lim}\bigr)$$

**横方向 + ヨー方向 PI（レートダンピング付き）**、積分器はアンチワインドアップ
のためクランプします。

$$\sigma_y = \mathrm{sat}\bigl(\sigma_y + e_y t_c,\ \pm\sigma_{\rm lim}\bigr), \qquad \sigma_\psi = \mathrm{sat}\bigl(\sigma_\psi + e_\psi t_c,\ \pm\sigma_{\rm lim}\bigr)$$

$$N_{\rm raw} = -k_{y,p}e_y - k_{y,i}\sigma_y + k_{\psi,p}e_\psi + k_{\psi,i}\sigma_\psi - k_r\,r$$

$$N = \mathrm{sat}(N_{\rm raw},\ \pm N_{\rm lim})$$

符号の読み方： $`-k_{y,p}e_y`$ が車両を経路へ向けてヨーさせ、
$`+k_{\psi,p}e_\psi`$ が方位を参照接線に合わせ、 $`-k_r r`$ が
ヨーレートダンパとして、前 2 項が円弧上で励起する振動を抑えます。

| ゲイン | 既定値 | 役割 |
|--------|--------|------|
| $`k_{y,p}`$ | 400.0 | 横偏差 比例 |
| $`k_{y,i}`$ | 0.0 | 横偏差 積分（既定では無効） |
| $`k_{\psi,p}`$ | 200.0 | 方位偏差 比例 |
| $`k_{\psi,i}`$ | 0.0 | 方位偏差 積分（既定では無効） |
| $`k_r`$ | 20.0 | ヨーレートダンピング |
| $`N_{\rm lim}`$ | 500.0 N·m | モーメント飽和 |
| $`f_{x,\rm lim}`$ | 5.0 N | 力の飽和 |
| $`\sigma_{\rm lim}`$ | 0.2 | 積分器クランプ |
| $`L_{\rm la}`$ | 60 点 | 先読み |
| $`h`$ / $`t_c`$ / $`T`$ | 10⁻⁴ / 10⁻³ / 0.90 s | 積分刻み / 制御周期 / 総時間 |

初期条件は速度が目標値 $`u = v_{\rm target}`$ 、横オフセット
$`y_0 = -0.03`$ m 、方位誤差 $`\psi_0 = 3°`$ です。つまり最初から抑制すべき
過渡が与えられています。

### 3.7 積分 — 状態別スカラ RK4（実質は Euler）

6 つの状態それぞれを独立のスカラ RK4 で進め、他の状態はステップ開始時の値に
固定します。

$$k_1 = h f(q_n),\quad k_2 = h f(q_n + \tfrac{k_1}{2}),\quad k_3 = h f(q_n + \tfrac{k_2}{2}),\quad k_4 = h f(q_n + k_3)$$

$$q_{n+1} = q_n + \frac{k_1 + 2k_2 + 2k_3 + k_4}{6}$$

ところが**このモデルのどの導関数も、自分自身の積分変数を読みません**。

| 状態 $`q`$ | 導関数 $`\dot q`$ | 自己依存 |
|-----------|------------------|---------|
| $`u`$ | $`f_x/m + r_{\rm old}\,v_{\rm old}`$ | なし |
| $`v`$ | $`f_y/m - r_{\rm old}\,u_{\rm old}`$ | なし |
| $`r`$ | $`N/I_{zz}`$ | なし |
| $`x`$ | $`u_{\rm old}\cos\psi_{\rm old} - v_{\rm old}\sin\psi_{\rm old}`$ | なし |
| $`y`$ | $`u_{\rm old}\sin\psi_{\rm old} + v_{\rm old}\cos\psi_{\rm old}`$ | なし |
| $`\psi`$ | $`r_{\rm old}`$ | なし |

任意の $`\alpha`$ について
$`f(q_n + \alpha k;\,\mathbf{p}) = f(q_n;\,\mathbf{p})`$ が成り立つため、
$`f`$ は 4 つのサブステップを通じて一定であり、
$`k_1 = k_2 = k_3 = k_4 = h f(q_n)`$ 、結合式は

$$q_{n+1} = q_n + h\,f(q_n)$$

すなわち**前進 Euler** に帰着します。大域誤差は $`O(h^4)`$ ではなく
$`O(h)`$ です。ただし $`h = 10^{-4}`$ s が十分小さいため実害はなく、参照との
差は $`5\times10^{-10}`$ m 未満です。

RK4 の外形を残している理由は 2 つあります。1 つは参照とのビット単位の再現性
（同じ浮動小数点演算を同じ順序で行う）、もう 1 つは将来への備えです。
$`\dot v`$ の中で $`v`$ を読む非線形タイヤモデルを入れればサブステップは自明で
なくなり、そのとき構造を書き換えずに精度が得られます。

### 3.8 1 ステップの処理順序

```text
for n = 0 .. num_steps-1:
    k*         = nearest_index(x, y, k_prev から窓探索)
    k_la       = min(k* + lookahead, N_ref-1)
    (e_y, e_ψ) = k_la における偏差
    f_y        = −C · β                        # β は「前ステップ」の値
    if control_counter ≥ steps_per_tc:         # tc 秒ごと
        f_x, N を更新（PI + ダンピング、飽和付き）
    log(t, x, y, ψ, u, v, r, β, e_y, e_ψ, N, f_x, ref[k*], path_err)
    u, v, r, x, y, ψ を積分（他状態は開始時の値に固定）
    t += h ;  β = atan2(v, u)                  # 次ステップ用の β
```

ログは積分の**前**に取られます。したがってサンプル $`n`$ は
$`t = n h`$ における状態と、区間 $`[nh, (n{+}1)h)`$ に適用された指令の組です。

### 3.9 評価指標

`path_err` は先読み点ではなく**最近傍点**に対して測るため、純粋な幾何距離です。

$$\mathrm{path\_err}_n = \bigl\lVert (x_n,y_n) - (x_{\rm ref}[k^\*], y_{\rm ref}[k^\*]) \bigr\rVert_2$$

集計値は `path_err` の RMS と最大、 $`e_y`$ と $`e_\psi`$ の RMS と絶対値最大、
$`N`$ の絶対値最大です。既定値ではスモークテストが次を固定しています。
`path_error_rms` = 0.013762 m 、 `path_error_max` = 0.030000 m 、
`ey_rms` = 0.020243 m 、 `ey_max` = 0.032639 m 、
`epsi_rms` = 0.300645 rad 、 `epsi_max` = 0.579404 rad 、
`max|N|` = 28.495415 N·m 。

$`e_\psi`$ の RMS が大きいのは、先読み点の接線に対して測っているためです。円弧上
では 0.12 m 先の参照方位が現在方位と構造的に異なるので、定常的に 0 でない
$`e_\psi`$ が出るのは誤差ではなく正しい追従挙動です。

---

## 4. `two_dof` — 2 自由度制御 vs PID

実装： `examples/two_dof/core/src/{tf,simulate,tdof_core}.cpp`
（python-control 参照の移植）。

このコアが答える問いは 1 つです。PID のステップ応答オーバーシュートのうち、
どれだけが**目標値経路**に由来し、どれだけがフィードバックループに由来するのか。
同じ閉ループを 2 回、生のステップと前置フィルタ済みステップで走らせます。

### 4.1 対象システム

$$P(s) = \frac{1}{ms^2 + cs + k}, \qquad K_1(s) = k_p + \frac{k_i}{s} + k_d s = \frac{k_d s^2 + k_p s + k_i}{s}$$

$$K_2(s) = \frac{k_p s + k_i}{k_d s^2 + k_p s + k_i}$$

| パラメータ | 記号 | 既定値 | 単位 |
|-----------|------|--------|------|
| 質量 | $`m`$ | 0.01 | kg |
| 粘性減衰 | $`c`$ | 0.015 | N·s/m |
| ばね定数 | $`k`$ | 1.0 | N/m |
| 比例ゲイン | $`k_p`$ | 2.0 | — |
| 積分ゲイン | $`k_i`$ | 10.0 | — |
| 微分ゲイン | $`k_d`$ | 0.1 | — |

$`K_1`$ は分子が 2 次・分母が 1 次（純粋な積分器）なので、単体では**インプロパ**
です。だからこそ単体で状態空間実現されることはなく、必ず $`P`$ との直列結合の
形でのみ実現されます。

$`K_2`$ は目標値前置フィルタで、PID の分子をそのまま自分の分母に使い、分子から
微分項を落とします。これにより $`K_2(0) = 1`$ （定常値は不変）を保ちながら、
目標値が微分モードを励起しなくなります。言い換えると、 $`K_1`$ の零点
（ $`k_d s^2 + k_p s + k_i`$ の根）が $`K_2`$ の極になります。

開ループ：

$$L(s) = P(s)K_1(s) = \frac{k_d s^2 + k_p s + k_i}{m s^3 + c s^2 + k s}$$

単位フィードバック閉ループ：

$$G_{yz}(s) = \frac{L}{1 + L} = \frac{k_d s^2 + k_p s + k_i}{m s^3 + (c + k_d)s^2 + (k + k_p)s + k_i}$$

すなわち**3 次系**です。2 つの実行は $`G_{yz}`$ を共有し、入力だけが異なります。

$$y_{\rm pid} = G_{yz}\,r, \qquad z = K_2\,r, \qquad y_{\rm 2dof} = G_{yz}\,z$$

2 自由度経路の目標値→出力の実効伝達関数は、共通因子がちょうど相殺するため
書き下す価値があります。

$$\frac{Y_{\rm 2dof}(s)}{R(s)} = G_{yz}(s)K_2(s) = \frac{(k_d s^2 + k_p s + k_i)(k_p s + k_i)}{\bigl[m s^3+(c+k_d)s^2+(k+k_p)s+k_i\bigr](k_d s^2+k_p s+k_i)}$$

$$\frac{Y_{\rm 2dof}(s)}{R(s)} = \frac{k_p s + k_i}{m s^3+(c+k_d)s^2+(k+k_p)s+k_i}$$

分母は $`G_{yz}`$ と同一（極が同じ）ですが、分子は 2 次ではなく 1 次になります。
これが前置フィルタの効果のすべてです。目標値経路から微分モードが消える一方で、
フィードバック側のダイナミクスには手を触れていません。

信号フロー：

```
                ┌──────┐     z     ┌───────┐    y_2dof
  r ──────────▶│  K2  │──────────▶│  Gyz  │──────────▶
  │            └──────┘           └───────┘
  │                                    ▲
  └────────────────────────────────────┘  y_pid  （直接経路）
```

### 4.2 多項式代数

係数は最高次から順に格納します（NumPy / python-control の慣習）。
`tf.cpp` の 4 つのプリミティブ：

| 演算 | 定義 |
|------|------|
| `poly_mul` | 畳み込み： $`[A\cdot B]_k = \sum_{i+j=k} a_i b_j`$ |
| `poly_add` | 定数項で桁を揃え、短い側の**左**をゼロ埋め |
| `series(A,B)` | $`(A_{\rm num}B_{\rm num}) / (A_{\rm den}B_{\rm den})`$ |
| `feedback_unity(G)` | $`G_{\rm num} / (G_{\rm den} + G_{\rm num})`$ |

`normalize_leading()` は $`10^{-14}`$ 未満の先頭係数を（最低 1 つは残して）
除去します。これにより退化した設定も破綻しません。 $`k_d = 0`$ のとき
$`K_2`$ は先頭のゼロ係数を状態空間変換に持ち込むのではなく、2 次から 1 次へ
きちんと落ちます。

### 4.3 可制御正準形

プロパな $`H(s) = \dfrac{b_0 s^n + \dots + b_n}{s^n + a_1 s^{n-1} + \dots + a_n}`$
（両多項式を分母の最高次係数で割った後）について、 $`D = b_0`$ 、
$`\tilde b_i = b_i - b_0 a_i`$ とすると：

```math
A = \begin{pmatrix}
0 & 1 & 0 & \cdots & 0 \\
0 & 0 & 1 & \cdots & 0 \\
\vdots & & & \ddots & \vdots \\
0 & 0 & 0 & \cdots & 1 \\
-a_n & -a_{n-1} & -a_{n-2} & \cdots & -a_1
\end{pmatrix},\quad
B = \begin{pmatrix} 0 \\ \vdots \\ 0 \\ 1 \end{pmatrix},\quad
C = \begin{pmatrix} \tilde b_n & \tilde b_{n-1} & \cdots & \tilde b_1 \end{pmatrix}
```

分子はあらかじめ長さ $`n+1`$ まで左ゼロ埋めされるため、厳密プロパな系は
自動的に $`b_0 = 0 \Rightarrow D = 0`$ となります。 $`n = 0`$ の場合は純ゲイン
$`y = D u`$ として特別扱いされます。

既定値では $`G_{yz}`$ は 3 次です。

```math
A = \begin{pmatrix} 0 & 1 & 0 \\ 0 & 0 & 1 \\ -1000 & -300 & -11.5 \end{pmatrix},\quad
B = \begin{pmatrix} 0 \\ 0 \\ 1 \end{pmatrix},\quad
C = \begin{pmatrix} 1000 & 200 & 10 \end{pmatrix},\quad D = 0
```

### 4.4 1 次ホールド（FOH）離散化

`python-control.forced_response` は等間隔格子上で、入力がサンプル間を
区分一定ではなく**線形に**変化すると仮定します。これを合わせることが
約 5 × 10⁻⁹ の一致をもたらしており、目標値 $`z(t)`$ が滑らかな信号である本件
では特に効きます。

FOH の下で次サンプルの厳密解は

$$x[k+1] = A_d x[k] + B_{d0} u[k] + B_{d1} u[k+1], \qquad y[k] = C x[k] + D u[k]$$

定数変化法から $`\tau = t - t_k`$ として

$$x[k+1] = e^{A\Delta t}x[k] + \int_0^{\Delta t}\! e^{A(\Delta t - \tau)} B\left[u[k] + \frac{u[k+1]-u[k]}{\Delta t}\tau\right]\!d\tau$$

となり、 $`A_d = e^{A\Delta t}`$ 、
$`B_{d1} = \frac{1}{\Delta t}\int_0^{\Delta t}\tau e^{A(\Delta t-\tau)}d\tau\,B`$ 、
$`B_{d0} = \left[\int_0^{\Delta t} e^{A(\Delta t-\tau)}d\tau\right]B - B_{d1}`$
が得られます。2 つの積分は拡大した $`(n+2)\times(n+2)`$ 行列の行列指数関数
1 回で同時に求まります。

```math
M = \begin{pmatrix}
A\,\Delta t & B\,\Delta t & 0 \\
0 & 0 & 1 \\
0 & 0 & 0
\end{pmatrix},
\qquad
e^{M} = \begin{pmatrix}
A_d & * & B_{d1} \\
0 & 1 & * \\
0 & 0 & 1
\end{pmatrix}
```

$`B_{d0} = [e^M]_{0:n,\,n} - B_{d1}`$ です。評価には Eigen の
`MatrixBase::exp()`（スケーリング・スクエアリング + パデ近似）を使います。
数値積分は不要で、行列指数関数はステップごとではなく 1 回だけ計算し、
$`A_d`$ 、 $`B_{d0}`$ 、 $`B_{d1}`$ を全 $`N`$ ステップで再利用します。

漸化は零初期状態から始まるため $`y[0] = D\,u[0]`$ です（厳密プロパな系では 0）。

### 4.5 時間格子・スケーリング・再利用

$$t_i = i\,\Delta t, \qquad N = \left\lceil \frac{t_{\rm end}}{\Delta t} - 10^{-12} \right\rceil$$

これは `np.arange(0, t_end, dt)` に対応し、既定の $`t_{\rm end} = 2`$ s 、
$`\Delta t = 0.01`$ s で $`N = 200`$ です。全信号は単位ステップに**正規化**して
保持され、 `copy_r` / `copy_z` / `copy_y_pid` / `copy_y_2dof` が取り出し時に
`ref` を掛けます。したがって GUI で振幅だけを変えても再計算は不要です。
`copy_time` はスケールされません。

`Gyz` の状態空間変換は 1 回だけ行い、 $`y_{\rm pid}`$ と $`y_{\rm 2dof}`$ の
両方で再利用します。個別に変換が必要なのは $`K_2`$ だけです。Eigen が投げる
例外は ABI 境界で捕捉され `NULL` として報告されます。

### 4.6 結果の読み方

既定値 $`m = 0.01`$ 、 $`c = 0.015`$ 、 $`k = 1.0`$ 、 $`k_p = 2`$ 、
$`k_i = 10`$ 、 $`k_d = 0.1`$ 、 `ref` = 10 のとき：

$$G_{yz}(s) = \frac{0.1 s^2 + 2 s + 10}{0.01 s^3 + 0.115 s^2 + 3 s + 10}$$

これは `tdof_core_get_tf(cfg, 3, …)` が返す係数配列（最高次から順）に対応します。

```
num = [0.1,  2,     10]
den = [0.01, 0.115, 3, 10]
```

シミュレーション結果（ $`N = 200`$ サンプル、出力は `ref` = 10 倍）：

| 信号 | ピーク | 最終値 | オーバーシュート |
|------|--------|--------|-----------------|
| $`y_{\rm pid}`$ | 11.397 | 9.996 | 14.0 % |
| $`y_{\rm 2dof}`$ | 12.049 | 9.995 | 20.5 % |

興味深いのは、前置フィルタ側のほうが指令信号 $`z`$ は滑らかなのに、出力の
オーバーシュートは**大きい**という点です。滑らかにした目標値を同じループへ
通せば自動的に出力も穏やかになる、というわけではありません。これこそが本デモの
示したいトレードオフです。読み解く手がかりとして、プラントは弱減衰
（ $`\zeta = 0.075`$ 、 $`\omega_n = 10`$ rad/s ）であり、既定ゲインでは
$`K_2`$ が $`s = -10`$ に二重極を持つ、つまり折れ点周波数がプラントの固有
振動数とちょうど重なっています。 `tdof_core_get_tf()` を使えば 4 つの伝達関数
（ $`P`$ 、 $`K_1`$ 、 $`K_2`$ 、 $`G_{yz}`$ ）を表示できるので、任意のゲイン
設定について極零配置を直接確認できます。

---

## 5. 手法の比較

| | `pid` | `msd` | `track` | `tdof` |
|---|---|---|---|---|
| モデル | 離散積算器 | 2 階 ODE | 6 状態の非線形 ODE | LTI 伝達関数 |
| 制御 | PID + クランプ | なし（開ループ） | 速度 PI + 横/ヨー PI + レートダンピング | PID、前置フィルタ切替 |
| 解法 | 明示的漸化式 | 固定刻みベクトル RK4 | 状態別 RK4 ≡ 前進 Euler | 厳密 FOH 離散化 |
| 次数 | 厳密（モデルそのもの） | 大域 $`O(h^4)`$ | 大域 $`O(h)`$ | FOH 入力に対して厳密 |
| 刻み | $`\Delta t = 1`$（既定） | $`10^{-3}`$ s | $`10^{-4}`$ s（制御 $`10^{-3}`$ s） | $`0.01`$ s |
| サンプル数 | 150 | 10001 | 9000 | 200 |
| 依存 | なし | なし | なし | Eigen 3 |
| 参照との一致 | 漸化式として厳密 | 2 × 10⁻⁷ 以下 | 5 × 10⁻¹⁰ 未満 | 約 5 × 10⁻⁹ |

各コアの `tools/smoke_test.cpp` がこれらの値を検証し、
`tests/test_examples.py` が共通アダプタ経由で 4 コアを再実行します。上記の
アルゴリズムに変更が入れば `ctest` の失敗として現れます。
