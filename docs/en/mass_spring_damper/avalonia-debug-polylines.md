# Troubleshooting: signal polylines are not drawn

[Module overview](README.md) · [Avalonia notes](avalonia-notes.md) · [日本語](../../ja/mass_spring_damper/avalonia-debug-polylines.md)

A systematic guide to why the `Polyline`s in the Series `ItemsControl` of
`MainWindow.axaml` may fail to render, and how to fix each cause.

---

## Symptom checklist

| Symptom | First suspect |
|---|---|
| Grid lines appear but no signal lines | Simulation failed / `Enabled=false` / empty Points |
| Neither grid nor signal lines appear | DataContext not set / native DLL not found |
| Lines appear at first, then vanish after Run | `_sims` dictionary inconsistency / thread violation |
| Only some cases are missing | Per-case simulation exception |
| Lines appear at startup but vanish after a parameter change | The `SyncEditorToCase` → `Run` chain is broken |

---

## Cause 1 — native DLL not found / simulation failed

### Cause

If `MsdCoreNative.Simulate` returns `IntPtr.Zero` or throws inside
`MsdSolver.Simulate`, the catch block in `Run()` sets `_sims[entry] = null`.
`RebuildPlot` skips `null` entries, so **no lines are drawn at all**.

```csharp
// MsdSolver.cs:94-96
var handle = MsdCoreNative.Simulate(ref cn, ref sn);
if (handle == IntPtr.Zero)
    throw new InvalidOperationException("msd_core_simulate failed");
```

### How to check

1. Look at **StatusMessage** at the bottom of the window.
   - `"Simulation failed for N case(s)"` means the problem is on the DLL side.
2. Check that `msd_core.dll` (or `libmsd_core.so`) exists in the build output
   directory.

```powershell
# inspect the build output folder
Get-ChildItem "bin\Debug\net8.0\" -Filter "msd_core*"
```

### Fix

- Add the following to the `.csproj` so the DLL is copied at build time.

```xml
<ItemGroup>
  <Content Include="..\..\..\core\build\msd_core.dll">
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
  </Content>
</ItemGroup>
```

- If it still fails while the DLL is present, suspect a CPU architecture
  mismatch (x64 vs ARM64).

---

## Cause 2 — every case has `Enabled == false`

### Cause

`RebuildPlot` unconditionally skips cases whose `Enabled` is false.

```csharp
// MainWindowViewModel.cs:263
if (!entry.Enabled) continue;
```

If the CheckBox default is `false`, or state from a previous session persists,
every line disappears.

### How to check

Visually confirm that the CheckBoxes for all cases in the left pane are ticked.

### Fix

Always default `CaseEntry.Enabled` to `true`.

```csharp
// ViewModels/CaseEntry.cs
public bool Enabled { get; set; } = true;   // do not set this to false
```

---

## Cause 3 — the simulation produced 0 samples

### Cause

If `dt` is larger than `stop`, `sim.T.Length == 0` and an empty `pts` is handed
to `SeriesPolyline`. A `Polyline` with an empty `Points` list draws nothing.

### How to check

- Check the `dt` (NumericUpDown) and `stop` values.
- StatusMessage reading `"OK — N cases, 0 samples each"` confirms it.

### Fix

Add validation at the top of `Run()`.

```csharp
public void Run()
{
    if (Dt <= 0 || Stop <= 0 || Dt >= Stop)
    {
        StatusMessage = $"Error: dt={Dt:G4} must be > 0 and < stop={Stop:G4}";
        return;
    }
    // ...
}
```

---

## Cause 4 — the Y range is degenerate

### Cause

When every case's response is constant (e.g. only `x(t) = 0`),
`yMax - yMin < 1e-12`. The code detects this and adds padding, but if
`dy = yMax - yMin` is near zero the coordinate transform
`fy = (sim.X[i] - yMin) / dy` can produce `NaN` or `Inf`.

```csharp
// MainWindowViewModel.cs:273
if (yMax - yMin < 1e-12) { yMin -= 0.5; yMax += 0.5; }
```

### How to check

Most likely when `k` is very large and `F ≈ 0` for every case (static
equilibrium = 0).

### Fix (a safe extra guard on top of the current code)

```csharp
double dy = yMax - yMin;
if (!double.IsFinite(dy) || dy < 1e-12) dy = 1.0;   // safe fallback
```

---

## Cause 5 — `Polyline.Points` binding type mismatch

### Cause

Avalonia's `Polyline.Points` accepts **`AvaloniaList<Point>`** or `IList<Point>`.
Passing a plain `List<Point>` or an array binds successfully, but
**collection-change notifications never arrive**, so the UI may not update.

### Current code (correct)

```csharp
// MainWindowViewModel.cs:323
var pts = new AvaloniaList<Point>();   // uses AvaloniaList → OK
```

### Caution

If `SeriesPolyline.Points` is ever changed to `List<Point>` or `Point[]`, change
it back to `AvaloniaList<Point>`.

```csharp
// SeriesPolyline.cs — do not change
public required AvaloniaList<Avalonia.Point> Points { get; init; }
```

---

## Cause 6 — the `ItemsControl`'s `ItemsPanel` is not a Canvas, or its size differs

### Cause

```xml
<!-- MainWindow.axaml:181-189 -->
<ItemsControl ItemsSource="{Binding Series}">
    <ItemsControl.ItemsPanel>
        <ItemsPanelTemplate>
            <Canvas Width="700" Height="460"/>   <!-- must match the outer Canvas -->
        </ItemsPanelTemplate>
    </ItemsControl.ItemsPanel>
```

If the `ItemsPanelTemplate` Canvas size differs from the outer
`<Canvas Width="700" Height="460">`, the coordinate systems disagree and every
line falls outside the clipping region.

### How to check

Confirm that `PlotWidthPx = 700` and `PlotHeightPx = 460` (constants in
`MainWindowViewModel`) match the Canvas dimensions in the AXAML.

### Fix

Keep all three Canvas sizes identical — the outer Canvas, the GridLines
`ItemsPanelTemplate`, and the Series `ItemsPanelTemplate`. Change all three
together.

---

## Cause 7 — UI thread violation

### Cause

If `RebuildPlot()` is called from an async task or another thread, writes to
`AvaloniaList` are no longer thread-safe: rendering may not update, or an
exception is thrown.

### How to check

While debugging, watch the output window for
`System.InvalidOperationException: "Call from invalid thread"`.

### Fix

Always call `Run()` and `RebuildPlot()` from the UI thread (Avalonia's
Dispatcher thread). When doing async work, marshal back to the Dispatcher:

```csharp
await Avalonia.Threading.Dispatcher.UIThread.InvokeAsync(() => RebuildPlot());
```

---

## Cause 8 — DataContext is not set

### Cause

`App.axaml.cs` sets `DataContext = new MainWindowViewModel()`; if that
assignment is lost during an edit, every binding becomes inert.

### How to check

```csharp
// App.axaml.cs — make sure this is present
desktop.MainWindow = new MainWindow
{
    DataContext = new MainWindowViewModel(),
};
```

If `d:DataContext` is specified in the AXAML, check whether it is overriding the
runtime DataContext.

---

## Cause 9 — `AvaloniaXamlLoader.Load` vs the source generator

### Cause

The current implementation uses `AvaloniaXamlLoader.Load(this)` (the
non-source-generator approach). Avalonia 11 and later recommend the source
generator (an auto-generated `InitializeComponent()`); mixing the two can leave
AXAML changes unreflected at runtime.

### How to check

Check whether the `.csproj` contains:

```xml
<AvaloniaUseCompiledBindingsByDefault>true</AvaloniaUseCompiledBindingsByDefault>
```

With compiled bindings enabled, untyped bindings such as `{Binding Stroke}`
become compile errors, so you must specify `DataType` or use
`{ReflectionBinding}`.

---

## Debugging procedure (flow when reproducing)

```
1. Check StatusMessage
   → "Simulation failed"  → Cause 1 (DLL / parameters)
   → "OK — 0 samples"     → Cause 3 (dt/stop settings)
   → "OK — N cases, ..."  → check Cause 2 onward

2. Check the case list checkboxes
   → all unchecked        → Cause 2

3. Break on RebuildPlot() in the debugger
   → check Series.Count   → 0 means Cause 1/2/3
   → check Points.Count   → 0 means Cause 3/4
   → check pts coordinates → NaN/Inf means Cause 4

4. Enable the Avalonia diagnostic overlay
   → add .With<AvaloniaDiagnostics>() to the AppBuilder for a visual check
```

---

## Checklist when changing this code

Verify the following every time polyline-related code is touched.

- [ ] `SeriesPolyline.Points` is still typed `AvaloniaList<Avalonia.Point>`
- [ ] `Canvas Width/Height` matches `PlotWidthPx`/`PlotHeightPx` in `MainWindowViewModel`
- [ ] `RebuildPlot()` is called from the UI thread
- [ ] `msd_core.dll` is present in the build output directory
- [ ] StatusMessage reads `"OK"` after `Run()`
- [ ] At least one case has `Enabled == true`
