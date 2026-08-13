# multi-gui-control-templates

> 「**1つの制御コア（C ABI）× Qt6 / Avalonia / Python の3GUI**」という設計パターン自体を見せる
> 最小例集。個々の題材は教科書的でも、**同一パターンを複数題材で示すテンプレ集**として価値を持たせる。

小さな似たデモを単独で乱立させると希釈するため、「**マルチGUIバインディングの実装テンプレ**」という
一点へまとめ直したリポジトリです。各題材は C ABI を公開する共有ライブラリ（`examples/<name>/core`）として
独立してビルドされ、ルートの共通シェル（ビルド／CI／GUI ランチャ）から駆動されます。

| example | 由来モジュール | 題材 |
|---|---|---|
| **pid** | `examples/pid` | PID 制御による1自由度姿勢制御 |
| **pi_path_tracking** | `examples/pi_path_tracking` | PI 制御による経路追従 |
| **two_dof** | `examples/two_dof` | 2自由度制御（目標応答と外乱応答の分離） |
| **mass_spring_damper** | `examples/mass_spring_damper` | 質量・ばね・ダンパ系のステップ応答 |

## 設計方針：1コア × 3GUI（土台の共通化）

```
multi-gui-control-templates/
├── README.md / LICENSE / CMakeLists.txt / .github/workflows/ci.yml   # 共通シェル(1セット)
├── gui/
│   └── python/             # 共通の GUI ランチャ（題材をドロップダウンで切替）
│       ├── adapters.py     #   各 core の C ABI を共通 RunResult へ正規化
│       └── gallery_app.py  #   時系列をプロットするギャラリー
├── examples/               # ★差し替え部分（各制御コア = 旧repo由来・履歴保持）
│   ├── pid/
│   ├── pi_path_tracking/
│   ├── two_dof/
│   └── mass_spring_damper/
├── tests/                  # 4題材を横断シミュレートして健全性確認（CTest）
└── docs/                   # 全ドキュメント（en/ ja/ = 横断、examples/ = 題材固有）
```

各 core は `<name>_core_simulate()` 系の C ABI を公開し、ctypes（Python）/ P/Invoke（Avalonia）/
直接リンク（Qt6）から同一バイナリを共有して呼べます。本リポジトリは Python ランチャを同梱し、
Qt6 / Avalonia 版は同じ C ABI に対する薄いバインディングとして各 example の歴史に保持されています。

## ビルド & 実行

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

cd gui/python
pip install -r requirements.txt
python gallery_app.py            # 4題材をドロップダウンで切替表示
```

Windows では、ルートの `build_and_run.ps1` が「コア + 3フロントエンドのビルド〜起動」を
1コマンドで行います（`.\build_and_run.ps1 pid 1` で pid の Qt6 のみ、など）。

## ドキュメント

| ページ | English | 日本語 |
|---|---|---|
| ビルドと実行（`build_and_run.ps1` 完全リファレンス／全フロントエンドのコマンド） | [docs/en/build-and-run.md](docs/en/build-and-run.md) | [docs/ja/build-and-run.md](docs/ja/build-and-run.md) |
| アーキテクチャ（1コア×3GUI パターン） | [docs/en/architecture.md](docs/en/architecture.md) | [docs/ja/architecture.md](docs/ja/architecture.md) |
| アルゴリズム詳細（4コアの導出と数値解法） | [docs/en/algorithms.md](docs/en/algorithms.md) | [docs/ja/algorithms.md](docs/ja/algorithms.md) |
| C ABI リファレンス | [docs/en/c-abi-reference.md](docs/en/c-abi-reference.md) | [docs/ja/c-abi-reference.md](docs/ja/c-abi-reference.md) |

題材固有の実装メモ（英語のみ）は [docs/examples/](docs/examples/) 配下にあります。
ドキュメントはすべて [docs/](docs/) に集約されており、 `examples/` 配下には置いていません。

## ライセンス

Apache License 2.0 — see [LICENSE](LICENSE).

---
*このリポジトリは、旧 `pid` / `track` / `tdof` / `msd` の4リポジトリを統合・再構成したものです
（各制御コアの git 履歴は `examples/` 配下に保持）。*
