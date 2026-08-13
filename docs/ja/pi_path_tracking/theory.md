# pi_path_tracking — PI 経路追従

[アルゴリズム概要](../algorithms.md) · [C ABI](api.md) · [English](../../en/pi_path_tracking/theory.md)


実装： `examples/pi_path_tracking/core/src/track_core.cpp`
（ `planar_path_tracking_pi_tuned.py` の移植）。
C ABI の詳細： [api.md](api.md) 。

## 1. 状態量

| 記号 | 単位 | 意味 |
|------|------|------|
| $`u`$ | m/s | 車体座標系の前後速度 |
| $`v`$ | m/s | 車体座標系の横速度（左が正） |
| $`r`$ | rad/s | ヨーレート（反時計回りが正） |
| $`x, y`$ | m | 全体座標系の位置 |
| $`\psi`$ | rad | $`+X`$ 軸から測った方位角 |

## 2. プラント — 3 自由度平面運動モデル

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

## 3. 参照経路

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

## 4. 最近傍点と先読み

前回の添字を起点とする**窓付き**線形探索により、1 ステップあたりの計算量を
$`O(1)`$ に抑えつつ、後方の点に吸着するのを防ぎます。

$$k^\* = \arg\min_{k\,\in\,[\max(0,\,k_{\rm prev}-5),\ \min(N_{\rm ref},\,k_{\rm prev}+120))} \bigl\lVert (x,y) - (x_{\rm ref}[k],\,y_{\rm ref}[k]) \bigr\rVert_2$$

後方 5 点の余裕がわずかな後退を許容し、前方 120 点（既定 $`\Delta s`$ で
0.24 m）が 1 ステップで進める上限を与えます。

制御の目標点は**先読み点**です。

$$k_{\rm la} = \min\bigl(k^\* + L_{\rm la},\ N_{\rm ref}-1\bigr), \qquad L_{\rm la} = 60\ \text{点} = 0.12\ \text{m}$$

先読みによって、単なる位置偏差が予見的な操舵指令に変わります。これが無いと
$`-k_{y,p}e_y`$ 項だけでは円弧上で振動します。

## 5. 偏差座標

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

## 6. 制御則

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

## 7. 積分 — 状態別スカラ RK4（実質は Euler）

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

## 8. 1 ステップの処理順序

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

## 9. 評価指標

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
