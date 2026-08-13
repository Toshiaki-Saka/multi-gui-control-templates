# アルゴリズム — 概要

[ドキュメント索引](README.md) · [English](../en/algorithms.md)

4 コアに共通する約束事と、各コアが使う数値解法の比較です。
**各モジュールの完全な導出は、それぞれのフォルダにあります。**

| モジュール | 題材 | 詳細 |
|-----------|------|------|
| [`pid`](pid/theory.md) | PID による 1 自由度姿勢制御 | [pid/theory.md](pid/theory.md) |
| [`mass_spring_damper`](mass_spring_damper/theory.md) | 質量-ばね-ダンパの強制応答 | [mass_spring_damper/theory.md](mass_spring_damper/theory.md) |
| [`pi_path_tracking`](pi_path_tracking/theory.md) | 3 自由度平面車両の PI 経路追従 | [pi_path_tracking/theory.md](pi_path_tracking/theory.md) |
| [`two_dof`](two_dof/theory.md) | 2 自由度制御と PID の比較 | [two_dof/theory.md](two_dof/theory.md) |

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
