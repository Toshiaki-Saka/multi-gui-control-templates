# mass_spring_damper — 質量-ばね-ダンパの強制応答

[ドキュメント索引](../README.md) · [アルゴリズム概要](../algorithms.md) · [English](../../en/mass_spring_damper/README.md)

正弦加振を受ける 1 自由度の質量-ばね-ダッシュポット系です。固定刻み RK4 で積分し、不足減衰・過減衰・共振近傍にまたがる 5 ケースを掃引します。

## ドキュメント

| ドキュメント | 内容 |
|-------------|------|
| [theory.md](theory.md) | 運動方程式、状態空間表現、派生量、RK4 の導出、ケース掃引、参考文献 |
| [api.md](api.md) | `msd_core` の C ABI： `MsdCase` 、 `MsdSamplingConfig` 、simulate/copy/free |
| [architecture.md](architecture.md) | 層構造の図、3 フロントエンドのデータフロー、設計判断 |
| [build.md](build.md) | フロントエンド別のビルドと環境構築の詳細 |
| [avalonia-notes.md](avalonia-notes.md) | Avalonia 11 の実装メモ |
| [avalonia-debug-polylines.md](avalonia-debug-polylines.md) | プロットのポリラインが描画されないときの体系的な切り分け |

## 基本情報

| | |
|---|---|
| ソース | `examples/mass_spring_damper/core/` |
| 共有ライブラリ | `msd_core`（ `msd_core.dll` / `libmsd_core.so` / `libmsd_core.dylib` ） |
| Qt6 実行ファイル | `msd_qt` |
| Avalonia プロジェクト | `MsdAvalonia` |
| Python 環境変数 | `MSD_CORE_LIB` |

## 実行方法

```powershell
# リポジトリ直下から（1 = Qt6, 2 = Avalonia, 3 = Python）
.uild_and_run.ps1 msd 1
.uild_and_run.ps1 msd       # 3 フロントエンドすべて
```

```sh
# 共通ギャラリー（プラットフォーム非依存）
cd gui/python && python gallery_app.py --example mass_spring_damper
```

ビルドの詳細： [../build-and-run.md](../build-and-run.md)
