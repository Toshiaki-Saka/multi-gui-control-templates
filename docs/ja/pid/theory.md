# pid — PID 姿勢制御

[アルゴリズム概要](../algorithms.md) · [C ABI](api.md) · [English](../../en/pid/theory.md)


実装： `examples/pid/core/src/pid_core.cpp`
（ `pid_advanced_simulation.py` の移植）

## 1. ループ構成

P・I・D の効きが見える最小構成として、プラントは純粋な積算器であり、制御出力が
そのまま状態に加算されます。

```
        θ_goal ──▶( Σ )──e──▶[ PID ]──m──▶( Σ )──▶ θ
                    ▲ −                     ▲ −
                    │                       │
                    └──────── θ ◀───────────┘   offset
```

## 2. 制御則

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

## 3. 実行順序

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

## 4. `dt` の役割

$`\Delta t`$ は積分項と微分項をスケールします（ $`\Sigma`$ は $`e\,\Delta t`$
を累積し、 $`d`$ は $`\Delta t`$ で除算）。一方でプラント更新
$`\theta \mathrel{+}= m`$ は**スケールされません**。プラントは構造上
「1 サンプル = 1 制御周期」です。既定の $`\Delta t = 1`$ で参照漸化式と厳密に
一致し、 $`\Delta t`$ を変えると P に対する I・D の相対的な効きが変わります。
GUI のスライダはまさにそれを触るためのものです。

## 5. 時間軸と出力

添字 0 が初期状態なので、配列長はちょうど `time_length` です。

$$t_n = n\,\Delta t,\qquad \theta_0 = \theta_{\rm start},\qquad n = 0,\dots,N-1$$

補助指標は $`\theta`$ に対する `final_theta`（最終値）、 `max_theta` 、
`min_theta` の 3 つで、配列をコピーしなくてもオーバーシュートと定常偏差を
読み取れます。

## 6. 妥当性チェック

`pid_core_simulate()` は次をすべて満たさない限り `NULL` を返します。
$`\theta_{\rm start},\theta_{\rm goal},\mathrm{offset},k_p,k_i,k_d`$ が有限、
$`\Delta t > 0`$ 、両クランプが有限かつ $`\ge 0`$ 、
$`\texttt{time\_length} \ge 2`$ 。

## 7. 既定値と挙動

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

既定値でもオーバーシュートします。アプリで実測すると、応答のピークは
$`\theta = 117.651`$ （約 31 %）で、 $`\theta = 89.983`$ に収束します。
[スクリーンショット](../../en/pid/screenshot.png) はこの状態です。
`pid_core.h` のヘッダコメントはこの設定を「緩やかでよく減衰した応答」と
表現していますが、それは控えめすぎます。積分項があるだけでループは 2 次系に
なり、オーバーシュートが生じます。

Python 参照側はより攻めた $`k_p = 0.10`$ 、 $`k_i = 0.5`$ 、 $`k_d = 0.5`$ を
使い、 $`\theta \approx 135`$ までオーバーシュートしてから 90 に収束します。
スモークテストが固定しているのはこちらの応答です。

プラントが積算器（離散積分器）なので、 $`k_i = k_d = 0`$ のときループは
$`k_p`$ に関する 1 次系になり、極は 1 ステップあたり $`1 - k_p`$ です。
したがって安定範囲は $`0 < k_p < 2`$ で、 $`k_p = 1`$ がデッドビートです。
$`k_i`$ を加えると 2 次系になり、参照ゲインで見られるオーバーシュートが
現れます。
