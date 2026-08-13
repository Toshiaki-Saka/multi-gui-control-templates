# Python バインディング

[モジュール概要](README.md) · [理論](theory.md) · [C ABI](api.md) · [English](../../en/pi_path_tracking/python.md)

`frontend_python/track_core.py` は `ctypes` 経由で C ライブラリをラップします。
3 つの dataclass（ `TrackConfig` 、 `ReferencePath` 、 `Simulation` ）と
2 つのトップレベル関数（ `make_reference` 、 `simulate` ）を公開します。

---

## インストール

```bash
# 1. C コアをビルド
cd core && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j

# 2. Python の依存関係をインストール
cd ../../frontend_python
pip install -r requirements.txt   # numpy, matplotlib, PySide6
```

ローダーは次の順で共有ライブラリを探索します。

1. 環境変数 `$TRACK_CORE_LIB`（絶対パス）
2. `../core/build/`（単一構成ジェネレータ、Linux / macOS）
3. `../core/build/Release/` と `../core/build/Debug/`（MSVC マルチ構成）
4. `ctypes.util.find_library('track_core')`（システムパス）

上書きする場合：

```bash
set TRACK_CORE_LIB=C:\path\to\track_core.dll     # Windows
export TRACK_CORE_LIB=/path/to/libtrack_core.so  # Linux/macOS
```

---

## クイックスタート

```python
import track_core as tc

# 既定パラメータで実行
sim = tc.simulate()

print(f"path_error_rms = {sim.path_error_rms:.6f} m")
print(f"e_y_max        = {sim.ey_max:.6f} m")
print(f"e_psi_max      = {sim.epsi_max:.6f} rad")
```

---

## `TrackConfig`

C 側の `TrackConfig` をそのまま写した `dataclass` です。フィールド名と既定値は
C 構造体と同一です（ [api.md](api.md) を参照）。

```python
@dataclass
class TrackConfig:
    m:               float = 0.1
    izz:             float = 1.0
    cornering_power: float = 20.0
    h:               float = 1e-4
    tc:              float = 1e-3
    total_time:      float = 0.90
    target_speed:    float = 1.0
    ky_p:            float = 400.0
    ky_i:            float = 0.0
    kpsi_p:          float = 200.0
    kpsi_i:          float = 0.0
    kr_damping:      float = 20.0
    n_moment_limit:  float = 500.0
    fx_limit:        float = 5.0
    error_integral_limit: float = 0.2
    lookahead_index: int   = 60
    initial_y_offset:     float = -0.03
    initial_heading_deg:  float =  3.0
    straight1_len:   float = 0.30
    radius:          float = 0.20
    straight2_len:   float = 0.30
    ds:              float = 0.002
```

C ライブラリ自身から既定値を読み出すには `TrackConfig.default()` を使います。

```python
cfg = tc.TrackConfig.default()
cfg.radius = 0.30   # 円弧を広げる
cfg.ky_p   = 600.0  # 横方向ゲインを強める
sim = tc.simulate(cfg)
```

---

## `make_reference`

```python
def make_reference(cfg: TrackConfig | None = None) -> ReferencePath
```

参照経路を NumPy 配列として返します。

```python
ref = tc.make_reference()
print(f"{len(ref.x)} reference points")

import matplotlib.pyplot as plt
plt.plot(ref.x, ref.y)
plt.axis("equal")
plt.show()
```

### `ReferencePath` のフィールド

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `x` | `np.ndarray` (float64) | 全体座標系の X 位置 [m] |
| `y` | `np.ndarray` (float64) | 全体座標系の Y 位置 [m] |
| `psi` | `np.ndarray` (float64) | 参照方位 [rad] |

---

## `simulate`

```python
def simulate(cfg: TrackConfig | None = None) -> Simulation
```

閉ループシミュレーション全体を実行して結果を返します。

### `Simulation` のフィールド

**時系列配列** — 積分ステップごとに 1 要素（ `len = int(total_time / h)` ）：

| フィールド | 型 | 単位 | 説明 |
|-----------|-----|------|------|
| `t` | `np.ndarray` | s | 時刻 |
| `x`, `y` | `np.ndarray` | m | 車両位置（全体座標系） |
| `psi` | `np.ndarray` | rad | 方位 |
| `u`, `v` | `np.ndarray` | m/s | 車体座標系の速度 |
| `r` | `np.ndarray` | rad/s | ヨーレート |
| `beta` | `np.ndarray` | rad | 横すべり角 |
| `ey` | `np.ndarray` | m | 先読み点での横偏差 |
| `epsi` | `np.ndarray` | rad | 先読み点での方位偏差 |
| `n_moment` | `np.ndarray` | N·m | 印加ヨーモーメント |
| `fx` | `np.ndarray` | N | 印加前後力 |
| `x_ref`, `y_ref` | `np.ndarray` | m | 最近傍参照点 |
| `psi_ref` | `np.ndarray` | rad | 最近傍参照点の方位 |

**計算プロパティ：**

```python
@property
def path_error(self) -> np.ndarray:
    return np.hypot(self.x - self.x_ref, self.y - self.y_ref)
```

**集計指標：**

| フィールド | 単位 | 説明 |
|-----------|------|------|
| `path_error_rms` | m | 最近傍参照点までの距離の RMS |
| `path_error_max` | m | 最近傍参照点までの距離の最大値 |
| `ey_rms` | m | 先読み点での横偏差の RMS |
| `ey_max` | m | 横偏差の絶対値の最大 |
| `epsi_rms` | rad | 方位偏差の RMS |
| `epsi_max` | rad | 方位偏差の絶対値の最大 |
| `nmoment_max` | N·m | ヨーモーメントの絶対値の最大 |

---

## パラメータスイープの例

```python
import numpy as np
import track_core as tc

ky_values = np.linspace(200, 800, 13)
results = []
for ky in ky_values:
    cfg = tc.TrackConfig.default()
    cfg.ky_p = ky
    sim = tc.simulate(cfg)
    results.append((ky, sim.path_error_rms, sim.ey_max))

print(f"{'ky_p':>8}  {'path_err_rms':>14}  {'ey_max':>10}")
for ky, rms, mx in results:
    print(f"{ky:8.1f}  {rms:14.6f}  {mx:10.6f}")
```

---

## GUI フロントエンド

| スクリプト | 説明 |
|-----------|------|
| `app_matplotlib.py` | バッチ実行。 `output_path_tracking_pi_tuned/` にプロットを保存 |
| `app_pyside6.py` | パラメータスライダーと 4 つのプロットタブを持つ対話 GUI |

どちらもコアをビルドした後に実行します。

```bash
python app_matplotlib.py   # 図を保存
python app_pyside6.py      # 対話実行
```
