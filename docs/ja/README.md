# multi-gui-control-templates — ドキュメント（日本語）

English: [../en/README.md](../en/README.md)

本リポジトリは、次の 1 つのパターンを繰り返し示すためのテンプレート集です。

> **1 つの制御コアを C ABI で公開し、3 つの独立した GUI
> （Qt6 C++ / Avalonia C# / Python PySide6）と共通 Python ギャラリーから駆動する。**

このパターンに対して 4 つの制御題材が実装されており、各題材は
`examples/<name>/core` 配下の自己完結した共有ライブラリになっています。

| slug | example ディレクトリ | 題材 | コアライブラリ |
|------|---------------------|------|---------------|
| `pid` | `examples/pid` | PID による 1 自由度姿勢制御 | `pid_core` |
| `track` | `examples/pi_path_tracking` | 3 自由度平面運動モデルの PI 経路追従 | `track_core` |
| `tdof` | `examples/two_dof` | 2 自由度制御と PID のステップ応答比較 | `tdof_core` |
| `msd` | `examples/mass_spring_damper` | 質量-ばね-ダンパ系の強制応答 | `msd_core` |

## ページ一覧

| ページ | 内容 |
|--------|------|
| [build-and-run.md](build-and-run.md) | `build_and_run.ps1` の完全リファレンス、および全コア・全フロントエンド・統合 CMake ビルド・CTest・共通ギャラリーの手動コマンド |
| [architecture.md](architecture.md) | リポジトリ構成、C ABI の設計契約、各フロントエンドのバインド方法、実行時のライブラリ解決、5 つ目の example の追加手順 |
| [algorithms.md](algorithms.md) | アルゴリズム詳細：プラントモデル、制御則、離散化、積分器、評価指標と、各数値的選択の理由 |
| [c-abi-reference.md](c-abi-reference.md) | 4 コアの全エクスポート関数・構造体と、所有権・バッファサイズの規約 |

### 題材固有の実装メモ

ドキュメントはすべて `docs/` 配下に集約されており、 `examples/` 側には置いて
いません。以下は特定の題材・フロントエンドに固有のメモです（英語のみ）。

| ドキュメント | 内容 |
|-------------|------|
| [examples/mass_spring_damper/architecture.md](../examples/mass_spring_damper/architecture.md) | 層構造・データフロー・設計判断 |
| [examples/mass_spring_damper/build.md](../examples/mass_spring_damper/build.md) | ビルドと環境構築の詳細 |
| [examples/mass_spring_damper/avalonia-notes.md](../examples/mass_spring_damper/avalonia-notes.md) | Avalonia 11 の実装メモ |
| [examples/mass_spring_damper/avalonia-debug-polylines.md](../examples/mass_spring_damper/avalonia-debug-polylines.md) | Avalonia ポリライン描画のデバッグ記録 |
| [examples/pi_path_tracking/api.md](../examples/pi_path_tracking/api.md) | `track_core` の C ABI 詳細と使用例 |
| [examples/pi_path_tracking/python.md](../examples/pi_path_tracking/python.md) | `track_core` の Python バインディング解説 |
| [examples/pid/avalonia-notes.md](../examples/pid/avalonia-notes.md) | Avalonia 11 の注意点（DLL 探索・P/Invoke・描画） |

## クイックスタート（Windows）

```powershell
# 全部：4 example × 3 フロントエンド
.\build_and_run.ps1

# 1 example × 1 フロントエンド（1 = Qt6, 2 = Avalonia, 3 = Python）
.\build_and_run.ps1 pid 1
```

## クイックスタート（プラットフォーム非依存：コア + テスト + ギャラリー）

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure

cd gui/python
pip install -r requirements.txt
python gallery_app.py
```

## ライセンス

Apache License 2.0 — [../../LICENSE](../../LICENSE) を参照してください。
