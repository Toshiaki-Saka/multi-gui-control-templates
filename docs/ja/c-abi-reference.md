# C ABI — 概要

[ドキュメント索引](README.md) · [English](../en/c-abi-reference.md)

4 コアに共通する規約と、各言語からのバインド方法です。
**モジュールごとのシンボル詳細は、それぞれのフォルダにあります。**

| ライブラリ | モジュール | リファレンス |
|-----------|-----------|-------------|
| `pid_core` | `pid` | [pid/api.md](pid/api.md) |
| `msd_core` | `mass_spring_damper` | [mass_spring_damper/api.md](mass_spring_damper/api.md) |
| `track_core` | `pi_path_tracking` | [pi_path_tracking/api.md](pi_path_tracking/api.md) |
| `tdof_core` | `two_dof` | [two_dof/api.md](two_dof/api.md) |

---

## 共通規約

- **呼び出し規約**： C（ `extern "C"` 、cdecl）。C# は
  `CallingConvention.Cdecl` を宣言し、Python は `ctypes.CDLL` を使います。
- **境界を越える型**： `double` 、 `int32_t` 、不透明ハンドルポインタ、
  `const char*` のみ。
- **ハンドルは不透明。** `*_simulate()` が返したポインタは、対応する
  `*_free_simulation()` で解放してください。アクセサに `NULL` を渡しても安全で、
  `0` / `0.0` が返ります。
- **バッファは呼び出し側の所有物。** すべての `*_copy_*()` は
  `(handle, double* buffer, int32_t buffer_len)` を取り、書き込んだサンプル数を
  返します。 `buffer_len` がシミュレーション長より小さい場合は何も書かずに `0`
  を返します。先に `*_sim_length()` でサイズを取得してください。
- **エラーは戻り値。** `*_simulate()` は不正・非有限な設定に対して `NULL` を
  返します。境界を越えて例外が飛ぶことはありません。
- **エクスポートマクロ**： `<SLUG>_CORE_API` は、コアのビルド時
  （ `<SLUG>_CORE_BUILD` 定義時）に `__declspec(dllexport)` 、Windows の利用側
  では `__declspec(dllimport)` 、それ以外では
  `__attribute__((visibility("default")))` に展開されます。

C からの典型的な使い方：

```c
PidConfig cfg;
pid_core_default_config(&cfg);
cfg.kp = 0.2;

PidSimulation* sim = pid_core_simulate(&cfg);
if (!sim) { /* 設定が不正 */ }

int32_t n = pid_core_sim_length(sim);
double* t     = malloc(n * sizeof(double));
double* theta = malloc(n * sizeof(double));
pid_core_sim_copy_time (sim, t,     n);
pid_core_sim_copy_theta(sim, theta, n);

pid_core_free_simulation(sim);
```

---

## 本リポジトリのバインディング層

| 言語 | 場所 | 方式 |
|------|------|------|
| C++（Qt6） | `examples/<name>/frontend_qt/` | CMake ターゲットに直接リンク。ヘッダをそのまま include |
| C#（Avalonia） | `examples/<name>/frontend_avalonia/<Proj>/Native/` | `[DllImport]` + `[StructLayout(LayoutKind.Sequential)]` を `*Solver` クラスで包み、 `double[]` へコピーして `finally` で解放 |
| Python（example ごと） | `examples/<name>/frontend_python/<slug>_core.py` | `restype` / `argtypes` を明示した `ctypes` 。 `np.frombuffer(...).copy()` で NumPy 配列を返す |
| Python（共通ギャラリー） | `gui/python/bindings/` | 同じモジュール群。 `gui/python/libloader.py` が事前にライブラリパスを解決 |

全体の関係は [architecture.md](architecture.md) を参照してください。
