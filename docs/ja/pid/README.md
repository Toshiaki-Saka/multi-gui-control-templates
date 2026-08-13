# pid — PID による 1 自由度姿勢制御

[ドキュメント索引](../README.md) · [アルゴリズム概要](../algorithms.md) · [English](../../en/pid/README.md)

プラントが純粋な積算器である 1 自由度姿勢制御ループです。P・I・D の効きが見える最小構成で、アンチワインドアップとアクチュエータ飽和を任意で有効にできます。

![PID interactive demo](../../en/pid/screenshot.png)

## ドキュメント

| ドキュメント | 内容 |
|-------------|------|
| [theory.md](theory.md) | 制御則、実行順序、`dt` の役割、妥当性チェック、既定値と安定性 |
| [api.md](api.md) | `pid_core` の C ABI： `PidConfig` 、simulate/copy/free、補助指標 |
| [avalonia-notes.md](avalonia-notes.md) | C# フロントエンド構築時に踏んだ Avalonia 11 の落とし穴 |

## 基本情報

| | |
|---|---|
| ソース | `examples/pid/core/` |
| 共有ライブラリ | `pid_core`（ `pid_core.dll` / `libpid_core.so` / `libpid_core.dylib` ） |
| Qt6 実行ファイル | `pid_qt` |
| Avalonia プロジェクト | `PidAvalonia` |
| Python 環境変数 | `PID_CORE_LIB` |

## 実行方法

```powershell
# リポジトリ直下から（1 = Qt6, 2 = Avalonia, 3 = Python）
.uild_and_run.ps1 pid 1
.uild_and_run.ps1 pid       # 3 フロントエンドすべて
```

```sh
# 共通ギャラリー（プラットフォーム非依存）
cd gui/python && python gallery_app.py --example pid
```

ビルドの詳細： [../build-and-run.md](../build-and-run.md)
