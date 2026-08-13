# アーキテクチャ — 1 コア × 3 GUI

English: [../en/architecture.md](../en/architecture.md)

## 1. パターン

本リポジトリの example はすべて同じ構造で作られています。

```
                      ┌───────────────────────────────┐
                      │  <slug>_core  (C++17, 共有ライブラリ) │
                      │  シミュレーション + 評価指標          │
                      └──────────────┬────────────────┘
                                     │  フラットな C ABI (extern "C")
             ┌───────────────────────┼───────────────────────┐
             │                       │                       │
       直接リンク (C++)          P/Invoke (C#)            ctypes (Python)
             │                       │                       │
      ┌──────┴──────┐        ┌───────┴────────┐      ┌───────┴────────┐
      │  Qt6 widget │        │ Avalonia MVVM  │      │ PySide6 GUI    │
      │  <slug>_qt  │        │ <Title>Avalonia│      │ app_pyside6.py │
      └─────────────┘        └────────────────┘      └───────┬────────┘
                                                             │
                                                     ┌───────┴────────┐
                                                     │ 共通ギャラリー   │
                                                     │ gallery_app.py │
                                                     └────────────────┘
```

数値計算は**すべて**コアが持ちます。フロントエンドは制御則も積分器も評価指標も
再実装せず、描画方法とパラメータ入力方法だけが異なります。これによって 4 つの
example が横並びで比較可能になります。コアを差し替えれば物理が入れ替わり、
3 つの GUI バインディング層は構造的に同一のまま残ります。

## 2. リポジトリ構成

```
multi-gui-control-templates/
├── CMakeLists.txt              # 統合ビルド：4 コア -> build/lib、+ CTest
├── build_and_run.ps1           # Windows ワンショット：ビルド + 任意フロントエンド起動
├── docs/                       # 本ドキュメント（en / ja）
├── gui/python/                 # 全 example 共通のランチャ
│   ├── libloader.py            #   各 <NAME>_CORE_LIB をインポート前に解決
│   ├── bindings/               #   コアごとの ctypes モジュール
│   ├── adapters.py             #   ネイティブ出力 -> 共通 RunResult へ正規化
│   └── gallery_app.py          #   matplotlib ギャラリー
├── tests/test_examples.py      # 横断スモークテスト（CTest ターゲット）
└── examples/                   # 差し替え部分。各コアは git 履歴を保持
    └── <name>/
        ├── core/               #   C++17 共有ライブラリ + C ABI ヘッダ
        │   ├── include/<slug>_core.h
        │   ├── src/
        │   └── tools/smoke_test.cpp
        ├── frontend_qt/        #   Qt6 Widgets アプリ（コアを直接リンク）
        ├── frontend_avalonia/  #   .NET 8 + Avalonia 11（P/Invoke）
        ├── frontend_python/    #   PySide6 GUI + matplotlib バッチ版
        ├── docs/               #   example 固有の理論 / API ノート
        └── build_and_run.ps1   #   example ごとの実行スクリプト
```

## 3. C ABI の設計契約

各コアは同じ 5 要素の形を持ちます（`<slug>` は `pid` / `track` / `tdof` /
`msd`）。

| 要素 | シグネチャの形 | 目的 |
|------|---------------|------|
| 設定構造体 | `typedef struct <Name>Config { double …; int32_t …; }` | POD のみ。ポインタも C++ 型も含まない |
| 既定値 | `void <slug>_core_default_config(<Name>Config*)` | 全フロントエンドが同じ初期値から始まる |
| 実行 | `<Name>Simulation* <slug>_core_simulate(const <Name>Config*)` | 不透明ハンドルを返し、入力が不正なら `NULL` |
| 読み出し | `int32_t <slug>_core_sim_length(...)` と `int32_t <slug>_core_sim_copy_<field>(..., double* buf, int32_t cap)` | 呼び出し側がバッファを確保。`cap < length` なら 0 を返す |
| 解放 | `void <slug>_core_free_simulation(<Name>Simulation*)` | 所有権をコアへ返す |

3 言語からバインドできるようにするための設計則：

- **境界を越える型は `double` / `int32_t` / 不透明ポインタ / `const char*`
  のみ。** `std::vector` も `bool` も、値返しの構造体も使いません。これにより
  C# の `[StructLayout(LayoutKind.Sequential)]` と Python の
  `ctypes.Structure` がフィールド単位でそのまま対応します。
- **出力バッファは呼び出し側の所有物。** `copy_*` は確保を行わず、
  `sim_length()` で確保済みのバッファを埋めるだけです。アロケータをまたいだ
  解放は、Windows で P/Invoke や ctypes のバインディングがクラッシュする典型
  パターンなので、構造的に避けています。
- **失敗は例外ではなく `NULL` / `0`。** 内部で例外を投げうるのは Eigen を使う
  `two_dof` のみで、ABI 境界ですべて捕捉して `NULL` に変換します。
- **シンボル可視性は明示。** Windows では `<SLUG>_CORE_BUILD` 定義下で
  `__declspec(dllexport)`、それ以外は
  `__attribute__((visibility("default")))`。加えて
  `CMAKE_CXX_VISIBILITY_PRESET hidden` により他のシンボルは漏れません。
- **`<slug>_core_version()`** は静的文字列を返します。フロントエンドが意図した
  ライブラリを読み込めているかの確認に使えます。

全シグネチャは [c-abi-reference.md](c-abi-reference.md) を参照してください。

## 4. 各フロントエンドのバインド方法

### Qt6（C++）

`frontend_qt/CMakeLists.txt` は、コアターゲットが未定義なら
`add_subdirectory(../core)` を実行し、
`target_link_libraries(<slug>_qt PRIVATE <slug>_core Qt6::Core Qt6::Gui Qt6::Widgets)`
でリンクします。ヘッダをそのまま利用するため、マーシャリング層は存在しません。
描画は `Widgets.cpp` の手書き `QPainter` コードで、チャートライブラリへの依存は
ありません。

Windows ではコアと exe を同じディレクトリに出力してローダが DLL を見つけられる
ようにし、Linux では `BUILD_RPATH "$ORIGIN;$ORIGIN/../core"` を設定します。

### Avalonia（C#）

`Native/<Name>CoreNative.cs` が生の `[DllImport("<slug>_core")]` 宣言と
`[StructLayout(LayoutKind.Sequential)]` の設定構造体を持ち、
`Native/<Name>Solver.cs` がそれを扱いやすい API に包みます。Solver は結果を
`double[]` にコピーし、`finally` でハンドルを解放します。ビューモデルはその配列に
バインドし、ビューは Avalonia のプリミティブ（`Polyline` / `Path`）で描画します。
ここでもチャートパッケージは使いません。

`.csproj` は `..\..\core\build\<slug>_core.dll`（および `.so` / `.dylib`）を
`Exists(...)` 条件付きで `CopyToOutputDirectory=PreserveNewest` により出力
ディレクトリへコピーします。`build_and_run.ps1` がコア DLL をこのフラットな
パスへコピーしているのはこのためで、さらに保険として `bin\...` にも直接
コピーします。

### Python（PySide6 + matplotlib）

`frontend_python/<slug>_core.py` は自己完結した ctypes バインディングです。
全エントリポイントに `restype` / `argtypes` を宣言し、C 構造体に対応する
dataclass を定義し、`np.frombuffer(buf, dtype=np.float64).copy()` で NumPy
配列を返します。この `.copy()` は重要で、ctypes バッファはスコープを抜けると
解放されるため、コピーしないビューはダングリングになります。

バインディングの探索順：

1. `$<SLUG>_CORE_LIB`
2. `../core/build/`、`../core/build/Release/`、`../core/build/Debug/`
3. バインディング自身のディレクトリとその親
4. `ctypes.util.find_library("<slug>_core")`

### 共通ギャラリー

`gui/python/` は上記をさらに一段抽象化します。

- `libloader.py` がインポート時に実行され、example ごとのビルドツリーと統合
  ビルドの `build/lib` の両方を探索し、**mtime が最も新しい**ものを選んで
  4 つの `<NAME>_CORE_LIB` を設定します。
- `bindings/` に各 example の ctypes モジュールのコピーを置くことで、
  `examples/*/frontend_python` を `sys.path` に載せる必要をなくしています。
- `adapters.py` が全コアの出力を 1 つの構造へ正規化します。

  ```python
  RunResult(name, description, plots=[Plot(title, xlabel, ylabel,
                                           traces=[Trace(label, x, y, style)],
                                           equal_aspect=False)],
            metrics={...})
  ```

  これがテンプレートの要点です。`gallery_app.py` のコードには PID もヨーレートも
  ばね定数も登場せず、`RunResult` を描画するだけです。

## 5. 数値的一致という契約

各コアは Python 参照スクリプトの移植であり、移植した数値を固定するスモーク
テストを保持しています。

| example | 参照スクリプト | 一致度 |
|---------|---------------|--------|
| `pid` | `pid_advanced_simulation.py` | 漸化式を 1 ステップずつ厳密に再現 |
| `pi_path_tracking` | `planar_path_tracking_pi_tuned.py` | 軌跡が 5 × 10⁻¹⁰ m 以内、集計指標 6 種は小数 6 桁まで一致 |
| `two_dof` | `two_degree_of_freedom…py`（python-control） | ステップ応答で約 5 × 10⁻⁹ |
| `mass_spring_damper` | `mass_spring_damper_forced_response.py`（`scipy.odeint`） | `dt` = 10⁻³ s で 2 × 10⁻⁷ 以下 |

このため、C++ としては冗長に見える構造が意図的に残されています。最も顕著なのが
`pi_path_tracking` の状態別 RK4 で、これは前進 Euler に帰着することが証明できます。
理由は [algorithms.md](algorithms.md) の §3.7 を参照してください。

## 6. 5 つ目の example を追加する

1. `examples/<name>/core` に `include/<slug>_core.h`、`src/`、`CMakeLists.txt`
   を作成します（共有ライブラリ、hidden visibility、`<SLUG>_CORE_BUILD`、
   `if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)` でガードした
   スモークテスト。既存のものを雛形にしてください）。
2. §3 の ABI 形状に従います —
   `default_config` / `simulate` / `sim_length` / `sim_copy_*` /
   `free_simulation` / `version`。
3. ルートの `CMakeLists.txt` に `add_subdirectory(examples/<name>/core)` を追加。
4. `build_and_run.ps1` 冒頭の `$Examples` テーブルに slug を追加
   （`Dir` / `Avalonia` / `EnvVar`）。スクリプト中で example を意識しているのは
   このテーブルだけです。
5. `gui/python/libloader.py::_EXAMPLES` に追記し、ctypes モジュールを
   `gui/python/bindings/` に置き、`gui/python/adapters.py` に `RunResult` を返す
   `run_<name>()` アダプタを書きます。
6. フロントエンドは既存の `frontend_qt` / `frontend_avalonia` /
   `frontend_python` をコピーして名前を差し替えます。コピーする価値があるのは
   描画コードで、バインディング層は機械的な作業です。

4 と 5 を行うことで、`build_and_run.ps1 all`、ギャラリーのドロップダウン、
`ctest` の 3 つに自動的に現れるようになります。
