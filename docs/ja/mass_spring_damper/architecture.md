# 実装アーキテクチャ

[モジュール概要](README.md) · [理論](theory.md) · [C ABI](api.md) · [English](../../en/mass_spring_damper/architecture.md)

## 全体像

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     msd_core  (C++17 共有ライブラリ)                         │
│                                                                             │
│  msd_core_simulate()  ─── RK4 固定刻み積分器                                 │
│  msd_core_derived()   ─── (m, c, k) から ωn, ζ                              │
│  msd_core_version()   ─── "msd_core 1.0.0"                                  │
│                                                                             │
│  純粋な C ABI。msd_core.dll / libmsd_core.so / libmsd_core.dylib として構築  │
└────────┬──────────────────────┬──────────────────────┬──────────────────────┘
         │ 直接リンク (C++)      │ P/Invoke (C#)        │ ctypes (Python)
         │                      │                      │
┌────────▼──────────┐  ┌────────▼────────────────┐  ┌─▼───────────────────────┐
│ frontend_qt/      │  │ frontend_avalonia/       │  │ frontend_python/        │
│ (Qt6 Widgets)     │  │ MsdAvalonia (C#/.NET)    │  │ msd_core.py  (bindings) │
│                   │  │                          │  │                         │
│ MainWindow.cpp    │  │ Native/                  │  │ app_matplotlib.py (CLI) │
│ Widgets.cpp       │  │   MsdCoreNative.cs       │  │ app_pyside6.py    (GUI) │
│ (OverlayPlot)     │  │   MsdSolver.cs           │  └─────────────────────────┘
└───────────────────┘  │                          │
                       │ MVVM                     │
                       │   ViewModels/            │
                       │   MainWindowViewModel    │
                       │                          │
                       │ View                     │
                       │   Views/MainWindow       │
                       │   (AXAML + code-behind)  │
                       └──────────────────────────┘
```

---

## 第 1 層 — C++ コア（`core/`）

### 公開 C ABI（`include/msd_core.h`）

コアは**純粋な C ABI** を公開しているため、FFI を持つ言語であれば実行時に
ロードできます。C++ の型・例外・名前マングリングはライブラリ境界を越えません。

主要な構造体：

```c
typedef struct MsdCase {
    double m, c, k;
    double force_amplitude, force_omega;
    double x0, v0;
} MsdCase;

typedef struct MsdSamplingConfig {
    double dt;    /* 時間刻み [s] */
    double stop;  /* 終了時刻 [s] */
} MsdSamplingConfig;

typedef struct MsdSimulation MsdSimulation;   /* 不透明ハンドル */
```

`MsdSimulation` ハンドルは `msd_core_simulate()` がヒープ上に確保するので、
`msd_core_free_simulation()` で解放する必要があります。

### ソルバ（`src/msd_core.cpp`）

- **固定刻み 4 次ルンゲ・クッタ**を使用します（[theory.md](theory.md) 参照）。
- 導関数 `deriv()` は外力を正しいサブステップ時刻で評価するため、RK4 の中点
  評価が正確になります。
- 時刻の添字は `+= dt` の累積ではなく `i * dt` で計算し、浮動小数点加算の
  繰り返しによるドリフトを避けています。
- $`n = \lfloor (stop + dt) / dt - \varepsilon \rfloor + 1`$ により NumPy の
  `arange(0, stop + dt, dt)` と同じサンプル数を再現します。

### スモークテスト（`tools/smoke_test.cpp`）

既定の 5 ケースすべてを実行し、 $`|x(final) - expected| \le 10^{-5}`$ を検証
します。期待値は `scipy.odeint` を使った Python スクリプトで生成したものです。
許容誤差は 1e-5 ですが、実際の誤差は 10⁻⁸〜10⁻⁷ 程度です。

---

## 第 2a 層 — Qt6 フロントエンド（`frontend_qt/`）

`msd_core` に直接リンクします（同一の C++ ツールチェーン）。FFI のオーバー
ヘッドはありません。

### UI 構造

```
MainWindow (QMainWindow)
  ├── QListWidget           — 有効/無効チェックボックス付きのケース一覧
  ├── 編集パネル             — m, c, k, F, ω, x₀, v₀ の QDoubleSpinBox
  │     └─ valueChanged     — 50 ms デバウンス → onRun()
  ├── 派生量パネル           — ωₙ, ζ, x(final), v(final), max|x| の QLabel
  └── OverlayPlot           — カスタム QWidget、QPainter で描画
```

`OverlayPlot` は `paintEvent` をオーバーライドしたカスタム `QWidget` です。
`MainWindow` から `setData()` でシミュレーション結果を受け取り、軸線・グリッド・
有効なケースごとのポリラインを `QPainter::drawPolyline` で描画します。

デバウンスタイマ（50 ms の `QTimer::singleShot`）により、スピンボックスへの
入力 1 打鍵ごとにシミュレーションが走るのを防いでいます。

---

## 第 2b 層 — Python バインディング（`frontend_python/msd_core.py`）

Python 標準の `ctypes` モジュールのみを使い、コンパイル済み拡張は不要です。

### ライブラリ探索順

1. 環境変数 `$MSD_CORE_LIB`
2. `../core/build/`、 `../core/build/Release/`、 `../core/build/Debug/`
   （単一構成ジェネレータと MSVC マルチ構成の両方をカバー）
3. `ctypes.util.find_library("msd_core")`（システムパス）

### 高水準 API

```python
case = mc.MsdCase(m=1.0, c=2.0, k=5.0, force_amplitude=0.5, force_omega=2.0)
sampling = mc.SamplingConfig(dt=0.001, stop=10.0)
sim = mc.simulate(case, sampling)
# sim.t, sim.x, sim.v, sim.force  →  numpy 配列
# sim.final_x, sim.max_abs_x      →  float スカラ
```

---

## 第 2c 層 — C# / Avalonia フロントエンド（`frontend_avalonia/MsdAvalonia/`）

### P/Invoke 層（`Native/`）

`MsdCoreNative.cs` — `CallingConvention.Cdecl` を指定した生の `[DllImport]` 宣言。

`MsdSolver.cs` — マネージドラッパ。 `MsdCase` / `MsdSamplingConfig` をネイティブ
構造体へ変換し、 `msd_core_simulate` を呼び、配列をマネージドな `double[]` へ
コピーして、 `finally` ブロックでネイティブハンドルを解放します。

### MVVM 構造

```
MainWindowViewModel
  ├── Cases              : AvaloniaList<CaseEntry>
  ├── SelectedIndex      : int  （エディタを駆動）
  ├── Editor*            : double プロパティ（EditorM, EditorC, …）
  │     └─ SetField(syncToCase:true) → SyncEditorToCase() → Run()
  ├── Dt / Stop          : double（SetField runOnChange:true → Run()）
  ├── Run()              : 全ケースをシミュレート → RebuildPlot()
  ├── GridLines          : AvaloniaList<GridLineMarker>   （グリッド線）
  └── Series             : AvaloniaList<SeriesPolyline>   （データ系列）
```

**自動再実行の設計**：パラメータが変わるたびに `SetField` 経由で `Run()` が
自動的に呼ばれます。 **Run** ボタンは手動再実行用（ `dt` / `stop` を Enter
なしで編集した場合など）に残していますが、それ以外では冗長です。

### プロット描画

プロットキャンバスは固定サイズの `<Canvas Width="700" Height="460">` です。
グリッド線とデータのポリラインは `ItemsControl` バインディングで描画します。

- `GridLines` → `<Line StartPoint=… EndPoint=…>`
- `Series` → `<Polyline Points=… Stroke=…>`

`Polyline.Points` には `AvaloniaList<Point>` を使います。 `Clear()` / `Add()`
がバインディング通知を発火させるためで、素の `List<Point>` では通知されません。

軸の目盛ラベルは、プロット枠のマージンに合わせた `Canvas.Left` / `Canvas.Top`
のハードコード座標に配置しています。ラベルの値（ `RefX0`〜`RefX100` 、
`RefY0`〜`RefY100` ）はデータ範囲が変わるたびに `RebuildPlot()` 内で再計算
されます。

---

## 設計判断

### なぜコアを C ABI にするのか

純粋な C ABI は言語間 FFI の最小公倍数だからです。

- Python は `ctypes` で読み込めます（コンパイル不要）。
- C# は `[DllImport]` / P/Invoke で読み込めます。
- 他の言語（Rust、Julia、Lua など）でも同様です。

### なぜ固定刻み RK4 なのか

対象は線形で良条件な ODE です。 `dt = 1 ms` の固定刻み RK4 で誤差は 10⁻⁷ 程度
であり、プロット上では区別できません。 `scipy.odeint` が使う LSODA のような
適応刻みソルバは、この用途では体感できる利点がないまま複雑さだけが増します。

### なぜパラメータ変更のたびに自動再実行するのか

既定の 10001 サンプルのケースでもシミュレーションは 1 ms 未満で終わるため、
体感的な遅延がありません。パラメータ変更を再実行に直結させることで、ユーザーが
Run を押し忘れる余地をなくし、プロットが常にエディタの内容と一致する状態を
保てます。
