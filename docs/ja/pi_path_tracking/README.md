# pi_path_tracking — PI 経路追従

[ドキュメント索引](../README.md) · [アルゴリズム概要](../algorithms.md) · [English](../../en/pi_path_tracking/README.md)

3 自由度平面運動モデルの車両が「直線 → 90° 円弧 → 直線」の参照経路を追従します。先読み付きの横偏差・方位偏差 PI にヨーレートダンピングを加え、前後方向は独立した速度 PI で制御します。

## ドキュメント

| ドキュメント | 内容 |
|-------------|------|
| [theory.md](theory.md) | 車両運動、参照経路の幾何、偏差の定義、2 つの制御器、状態別 RK4 が前進 Euler に帰着する理由 |
| [api.md](api.md) | `track_core` の C ABI：ライフサイクル、 `TrackConfig` 、参照経路、15 チャネル、集計指標、C の使用例 |
| [python.md](python.md) | Python バインディング： `TrackConfig` / `ReferencePath` / `Simulation` 、パラメータスイープ、2 つの GUI スクリプト |

## 基本情報

| | |
|---|---|
| ソース | `examples/pi_path_tracking/core/` |
| 共有ライブラリ | `track_core`（ `track_core.dll` / `libtrack_core.so` / `libtrack_core.dylib` ） |
| Qt6 実行ファイル | `track_qt` |
| Avalonia プロジェクト | `TrackAvalonia` |
| Python 環境変数 | `TRACK_CORE_LIB` |

## 実行方法

```powershell
# リポジトリ直下から（1 = Qt6, 2 = Avalonia, 3 = Python）
.uild_and_run.ps1 track 1
.uild_and_run.ps1 track       # 3 フロントエンドすべて
```

```sh
# 共通ギャラリー（プラットフォーム非依存）
cd gui/python && python gallery_app.py --example pi_path_tracking
```

ビルドの詳細： [../build-and-run.md](../build-and-run.md)
