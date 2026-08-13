# Avalonia 11 Implementation Notes

[Module overview](README.md) · [theory](theory.md) · [C ABI](api.md) · [日本語](../../ja/pid/avalonia-notes.md)

Pitfalls hit while building the PID demo's Avalonia frontend
(`frontend_avalonia`), and how each was solved.

---

## 1. Canvas is fixed-size — it does not follow window resize

### Problem

Fixing the size with `<Canvas Width="720" Height="320">` forces every grid line
and tick position to be hardcoded in pixels, and the drawing area does not
change when the window is resized.

### Solution

Subclass `Control` and override `Render(DrawingContext ctx)`, reading
`Bounds.Width` / `Bounds.Height` for the current size at render time.

```csharp
public override void Render(DrawingContext ctx)
{
    double w = Bounds.Width;
    double h = Bounds.Height;
    if (w < 1 || h < 1) return;
    // ... scale using w/h
}
```

---

## 2. No re-render on resize — put `BoundsProperty` in `AffectsRender`

### Problem

`AffectsRender<PlotControl>(SimTProperty, ...)` alone does not call `Render()`
when the window is resized.

### Solution

**Always** add `BoundsProperty` to `AffectsRender`.

```csharp
static PlotControl()
{
    AffectsRender<PlotControl>(
        SimTProperty, SimThetaProperty,
        YMinProperty, YMaxProperty, XMaxProperty,
        ThetaGoalProperty, TitleProperty,
        BoundsProperty   // ← without this, resizing does not re-render
    );
}
```

---

## 3. `Polyline.Points` requires `AvaloniaList<Point>`

### Problem

In a Canvas-based implementation, binding `Polyline.Points` to `List<Point>` or
`IEnumerable<Point>` does not propagate updates.

### Solution

Use `AvaloniaList<Point>`. Updating in place with `Clear()` → `Add()` raises the
notifications.

```csharp
// ViewModel
public AvaloniaList<Point> ResponsePoints { get; } = new();

// inside Run()
ResponsePoints.Clear();
for (int i = 0; i < sim.T.Length; ++i)
    ResponsePoints.Add(new Point(...));
```

---

## 4. Slider / NumericUpDown bindings need an explicit `Mode=TwoWay`

### Problem

Avalonia's default binding mode is `OneWay`, so moving a slider does not push
the value back to the ViewModel.

### Solution

Always add `Mode=TwoWay` to sliders and input controls.

```xml
<Slider Value="{Binding Kp, Mode=TwoWay}"/>
<NumericUpDown Value="{Binding Kp, Mode=TwoWay}"/>
```

---

## 5. Do not use `HeaderedContentControl`

### Problem

`HeaderedContentControl` was tried for the slider panel header, but Avalonia 11's
Fluent theme does not style it, so the layout came out broken.

### Solution

Use a `TextBlock` + `Border` + `StackPanel` combination instead.

```xml
<StackPanel>
    <TextBlock Text="Parameters" FontWeight="Bold" Margin="2,0,0,2"/>
    <Border BorderBrush="Gray" BorderThickness="1" Padding="8" CornerRadius="2">
        <StackPanel Spacing="4">
            <!-- sliders -->
        </StackPanel>
    </Border>
</StackPanel>
```

---

## 6. Register custom-control properties as `StyledProperty`

### Problem

Plain CLR properties do not support binding, and their changes never reach
`AffectsRender`.

### Solution

Define a StyledProperty with `AvaloniaProperty.Register<TOwner, TValue>()`.

```csharp
public static readonly StyledProperty<double[]?> SimTProperty =
    AvaloniaProperty.Register<PlotControl, double[]?>(nameof(SimT));

public double[]? SimT
{
    get => GetValue(SimTProperty);
    set => SetValue(SimTProperty, value);
}
```

---

## 7. Draw polylines inside `Render()` with `StreamGeometry`

### Problem

The `Polyline` control cannot be used when drawing a point sequence inside
`Render()`.

### Solution

Open a `StreamGeometry`, build the line with `BeginFigure` / `LineTo` /
`EndFigure`, and draw it with `DrawGeometry`.

```csharp
var geo = new StreamGeometry();
using (var gc = geo.Open())
{
    gc.BeginFigure(ToPx(t[0], th[0]), false);
    for (int i = 1; i < t.Length; ++i)
        gc.LineTo(ToPx(t[i], th[i]));
    gc.EndFigure(false);
}
ctx.DrawGeometry(null, new Pen(Brushes.Blue, 1.6), geo);
```

---

## 8. Dashed lines are specified with `DashStyle`

```csharp
var dashStyle = new DashStyle(new[] { 6.0, 4.0 }, 0);
ctx.DrawLine(
    new Pen(Brushes.Red, 1.4, dashStyle),
    new Point(x1, y), new Point(x2, y));
```

---

## 9. `InitializeComponent()` must call `AvaloniaXamlLoader.Load(this)`

When the source generator is not used
(`AvaloniaUseCompiledBindingsByDefault=false`), call it manually.

```csharp
private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
```

Call it in the code-behind constructor. Forgetting it means the AXAML content
never appears.

---

## 10. Copy the native DLL conditionally from the `.csproj`

The extension differs per platform (dll / so / dylib), so branch on
`Condition="Exists(...)"` and copy the right one into the output directory.

```xml
<ItemGroup Condition="Exists('..\..\core\build\pid_core.dll')">
    <None Include="..\..\core\build\pid_core.dll">
        <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
</ItemGroup>
<ItemGroup Condition="Exists('..\..\core\build\libpid_core.so')">
    <None Include="..\..\core\build\libpid_core.so">
        <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
</ItemGroup>
```

---

## 11. A `Line` used as a legend swatch needs an explicit `Width`

A `Line` inside a `StackPanel` breaks the layout unless `Width` is set.

```xml
<Line Stroke="#D62828" StrokeThickness="2"
      StrokeDashArray="3,2"
      StartPoint="0,0" EndPoint="16,0"
      VerticalAlignment="Center" Width="16"/>
```

---

## 12. State number format strings explicitly (G4 / F3 …)

Without an explicit format in `FormattedText` or `NumericUpDown.FormatString`,
the decimal separator can become a comma depending on locale. Always pass
`CultureInfo.InvariantCulture`.

```csharp
var ft = new FormattedText(text, CultureInfo.InvariantCulture,
    FlowDirection.LeftToRight, Typeface.Default, 9, Brushes.Black);
```

---

## Recommended architecture (where this project ended up)

| Concern | Recommendation |
|---------|----------------|
| Plot rendering | `Control` subclass + `Render(DrawingContext)` |
| Properties | `StyledProperty` + `AffectsRender` (including `BoundsProperty`) |
| ViewModel → View data | Raw data arrays (pixel conversion happens in the View) |
| Polylines | `StreamGeometry` |
| Sliders | `Mode=TwoWay` required |
| Group UI | `TextBlock` + `Border` + `StackPanel` (not `HeaderedContentControl`) |
| Native DLL | Conditional copy in the `.csproj` (`Exists` check) |
