# multi-gui-control-templates — ドキュメント

**日本語** · [English](../en/README.md)

本リポジトリは、次の 1 つのパターンを繰り返し示すためのテンプレート集です。

> **1 つの制御コアを C ABI で公開し、3 つの独立した GUI
> （Qt6 C++ / Avalonia C# / Python PySide6）と共通 Python ギャラリーから駆動する。**

このパターンに対して 4 つの制御題材が実装されており、各題材は
`examples/<name>/core` 配下の自己完結した共有ライブラリになっています。

## モジュール

各モジュールはフォルダ単位で、理論・C ABI・フロントエンド実装メモを
ひととおり揃えています。

| モジュール | 題材 | ドキュメント |
|-----------|------|-------------|
| **pid** | PID による 1 自由度姿勢制御 | [pid/](pid/README.md) |
| **mass_spring_damper** | 質量-ばね-ダンパの強制応答 | [mass_spring_damper/](mass_spring_damper/README.md) |
| **pi_path_tracking** | 3 自由度平面車両の PI 経路追従 | [pi_path_tracking/](pi_path_tracking/README.md) |
| **two_dof** | 2 自由度制御と PID の比較 | [two_dof/](two_dof/README.md) |

## 横断ドキュメント

| ページ | 内容 |
|--------|------|
| [build-and-run.md](build-and-run.md) | `build_and_run.ps1` の完全リファレンス、および全コア・全フロントエンド・統合 CMake ビルド・CTest・共通ギャラリーの手動コマンド |
| [architecture.md](architecture.md) | リポジトリ構成、C ABI の設計契約、各フロントエンドのバインド方法、実行時のライブラリ解決、5 つ目の example の追加手順 |
| [algorithms.md](algorithms.md) | 4 コア共通の約束事と数値解法の比較（モジュールごとの導出は各フォルダ） |
| [c-abi-reference.md](c-abi-reference.md) | 4 コア共通の規約とバインディング層（モジュールごとのシンボル詳細は各フォルダ） |

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
