# two_dof — 2 自由度制御と PID の比較

[ドキュメント索引](../README.md) · [アルゴリズム概要](../algorithms.md) · [English](../../en/two_dof/README.md)

PID のステップ応答オーバーシュートのうち、どれだけが目標値経路に由来するのか。同じ閉ループを、生のステップと前置フィルタ経由の 2 通りで実行して比較します。

## ドキュメント

| ドキュメント | 内容 |
|-------------|------|
| [theory.md](theory.md) | プラント/PID/前置フィルタの伝達関数、多項式代数、可制御正準形、拡大行列指数関数による 1 次ホールド離散化、結果の読み方 |
| [api.md](api.md) | `tdof_core` の C ABI： `TdofConfig` 、2 段階の伝達関数取得、simulate/copy/free |

## 基本情報

| | |
|---|---|
| ソース | `examples/two_dof/core/` |
| 共有ライブラリ | `tdof_core`（ `tdof_core.dll` / `libtdof_core.so` / `libtdof_core.dylib` ） |
| Qt6 実行ファイル | `tdof_qt` |
| Avalonia プロジェクト | `TdofAvalonia` |
| Python 環境変数 | `TDOF_CORE_LIB` |

## 実行方法

```powershell
# リポジトリ直下から（1 = Qt6, 2 = Avalonia, 3 = Python）
.uild_and_run.ps1 tdof 1
.uild_and_run.ps1 tdof       # 3 フロントエンドすべて
```

```sh
# 共通ギャラリー（プラットフォーム非依存）
cd gui/python && python gallery_app.py --example two_dof
```

ビルドの詳細： [../build-and-run.md](../build-and-run.md)
