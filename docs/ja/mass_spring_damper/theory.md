# mass_spring_damper — 強制応答（RK4）

[アルゴリズム概要](../algorithms.md) · [C ABI](api.md) · [English](../../en/mass_spring_damper/theory.md)


実装： `examples/mass_spring_damper/core/src/msd_core.cpp`
（ `mass_spring_damper_forced_response.py` / `scipy.integrate.odeint` の移植）。
設計メモ： [architecture.md](architecture.md) 。

## 1. プラント

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

## 2. 派生量

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

## 3. 積分器 — 古典的なベクトル RK4

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

## 4. 時間格子

$$t_k = k\,\Delta t,\qquad N = \left\lfloor \frac{t_{\rm stop} + \Delta t}{\Delta t} - 10^{-12}\right\rfloor + 1$$

これは `np.arange(0.0, stop + dt, dt)` に対応し、 `stop` の 1 つ先まで
サンプルが出ます。既定の $`\Delta t = 10^{-3}`$ s 、 $`t_{\rm stop} = 10`$ s で
$`N = 10001`$ です。 $`N < 2`$ の場合は拒否されます。

外力も同じ格子上で保存されるため（ $`f_k = F\sin(\omega t_k)`$ ）、
フロントエンドは再計算なしで加振と応答を重ね描きできます。

## 5. 既定のケーススイープ

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

## 6. 評価指標

`final_position` 、 `final_velocity` 、 `max_abs_position` 、
`max_abs_velocity` を積分と同じパスで計算します。

## 7. 参考文献

- Den Hartog, J.P. (1985). *Mechanical Vibrations* (4th ed.). Dover.
- Inman, D.J. (2014). *Engineering Vibration* (4th ed.). Pearson.
- Press, W.H. et al. (2007). *Numerical Recipes in C++* (3rd ed.), §17.1 — Runge-Kutta.
