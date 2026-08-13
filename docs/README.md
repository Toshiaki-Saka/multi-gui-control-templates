# Documentation / ドキュメント

All documentation for this repository lives here — nothing is kept under
`examples/`.
本リポジトリのドキュメントはすべてここに集約されています（`examples/` 配下には
置きません）。

## Cross-cutting documents / 横断ドキュメント

Available in both languages. 日本語版・英語版の両方があります。

| Page | English | 日本語 |
|------|---------|--------|
| Overview / index | [en/README.md](en/README.md) | [ja/README.md](ja/README.md) |
| Build & run — full `build_and_run.ps1` reference and every frontend command | [en/build-and-run.md](en/build-and-run.md) | [ja/build-and-run.md](ja/build-and-run.md) |
| Architecture — the "1 core × 3 GUIs" pattern | [en/architecture.md](en/architecture.md) | [ja/architecture.md](ja/architecture.md) |
| Algorithms — full derivations and numerics for all four cores | [en/algorithms.md](en/algorithms.md) | [ja/algorithms.md](ja/algorithms.md) |
| C ABI reference — every exported symbol | [en/c-abi-reference.md](en/c-abi-reference.md) | [ja/c-abi-reference.md](ja/c-abi-reference.md) |

The **algorithms** page is the single source of truth for the mathematics of all
four examples — plant models, control laws, discretisation, integrators and
error metrics.
**algorithms** が 4 題材すべての数学（プラントモデル・制御則・離散化・積分器・
評価指標）の正本です。

## Per-example documents / 題材固有のドキュメント

Implementation notes that are specific to one example and one frontend.
Written in English only.
特定の題材・フロントエンドに固有の実装メモです（英語のみ）。

### `examples/mass_spring_damper`

| Document | Contents |
|----------|----------|
| [examples/mass_spring_damper/architecture.md](examples/mass_spring_damper/architecture.md) | Layer diagram, data flow, design decisions |
| [examples/mass_spring_damper/build.md](examples/mass_spring_damper/build.md) | Detailed build and environment setup |
| [examples/mass_spring_damper/avalonia-notes.md](examples/mass_spring_damper/avalonia-notes.md) | Avalonia 11 implementation tips |
| [examples/mass_spring_damper/avalonia-debug-polylines.md](examples/mass_spring_damper/avalonia-debug-polylines.md) | Debugging log for the Avalonia polyline renderer |

### `examples/pi_path_tracking`

| Document | Contents |
|----------|----------|
| [examples/pi_path_tracking/api.md](examples/pi_path_tracking/api.md) | Complete `track_core` C ABI reference with a usage example |
| [examples/pi_path_tracking/python.md](examples/pi_path_tracking/python.md) | Python bindings guide (`TrackConfig`, `Simulation` fields, code examples) |

### `examples/pid`

| Document | Contents |
|----------|----------|
| [examples/pid/avalonia-notes.md](examples/pid/avalonia-notes.md) | Avalonia 11 gotchas: native DLL search path, P/Invoke signatures, rendering |
| [examples/pid/screenshot.png](examples/pid/screenshot.png) | Screenshot used by the example README |

### `examples/two_dof`

No frontend-specific notes; the mathematics is in
[en/algorithms.md §4](en/algorithms.md#4-two_dof--2-dof-control-vs-pid) /
[ja/algorithms.md §4](ja/algorithms.md#4-two_dof--2-自由度制御-vs-pid).
