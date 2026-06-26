# Avalonia 11 Implementation Notes

Lessons learned while building the MSD Avalonia frontend.
Each entry describes a pitfall and its solution.

---

## 1. Canvas is fixed-size — it does not follow window resize

**Problem**: `<Canvas Width="720" Height="320">` requires all grid lines and
tick positions to be hardcoded in pixels. The canvas does not resize with the
window.

**Solution**: Subclass `Control` and override `Render(DrawingContext ctx)`.
Read `Bounds.Width`/`Bounds.Height` at render time for live scaling.

```csharp
public override void Render(DrawingContext ctx)
{
    double w = Bounds.Width;
    double h = Bounds.Height;
    if (w < 1 || h < 1) return;
    // use w, h for all coordinate transforms
}
```

---

## 2. Resize does not trigger re-render unless `BoundsProperty` is in `AffectsRender`

**Problem**: `AffectsRender<PlotControl>(SimTProperty, ...)` alone does not
re-render when the window is resized.

**Solution**: Add `BoundsProperty` to the `AffectsRender` call.

```csharp
static PlotControl()
{
    AffectsRender<PlotControl>(
        SimTProperty, SimXProperty,
        YMinProperty, YMaxProperty, XMaxProperty,
        BoundsProperty   // ← required for resize re-render
    );
}
```

---

## 3. `Polyline.Points` requires `AvaloniaList<Point>`

**Problem**: Binding `Polyline.Points` to `List<Point>` or
`IEnumerable<Point>` does not trigger updates.

**Solution**: Use `AvaloniaList<Point>` and call `Clear()` then `Add()` in-place.

```csharp
// ViewModel
public AvaloniaList<Point> ResponsePoints { get; } = new();

// Inside Run():
ResponsePoints.Clear();
for (int i = 0; i < sim.T.Length; ++i)
    ResponsePoints.Add(new Point(...));
```

---

## 4. Slider / NumericUpDown binding requires `Mode=TwoWay`

**Problem**: Avalonia's default binding mode is `OneWay`; moving a `Slider`
or editing a `NumericUpDown` does not propagate the value to the ViewModel.

**Solution**: Always specify `Mode=TwoWay` for input controls.

```xml
<Slider Value="{Binding Kp, Mode=TwoWay}"/>
<NumericUpDown Value="{Binding Kp, Mode=TwoWay}"/>
```

---

## 5. `HeaderedContentControl` is not usable with the Fluent theme

**Problem**: `HeaderedContentControl` does not receive Fluent theme styles in
Avalonia 11, resulting in broken layout.

**Solution**: Use `TextBlock` + `Border` + `StackPanel` instead.

```xml
<StackPanel>
    <TextBlock Text="Parameters" FontWeight="Bold" Margin="2,0,0,2"/>
    <Border BorderBrush="Gray" BorderThickness="1" Padding="8" CornerRadius="2">
        <StackPanel Spacing="4">
            <!-- controls here -->
        </StackPanel>
    </Border>
</StackPanel>
```

---

## 6. Custom control properties must use `StyledProperty`

**Problem**: Plain CLR properties do not support binding or `AffectsRender`.

**Solution**: Register properties with `AvaloniaProperty.Register<TOwner, TValue>()`.

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

## 7. Drawing polylines in `Render()` — use `StreamGeometry`

**Problem**: `Polyline` is a control and cannot be used inside `Render()`.

**Solution**: Build a `StreamGeometry` with `BeginFigure`/`LineTo`/`EndFigure`.

```csharp
var geo = new StreamGeometry();
using (var gc = geo.Open())
{
    gc.BeginFigure(ToPx(t[0], x[0]), false);
    for (int i = 1; i < t.Length; ++i)
        gc.LineTo(ToPx(t[i], x[i]));
    gc.EndFigure(false);
}
ctx.DrawGeometry(null, new Pen(Brushes.Blue, 1.6), geo);
```

---

## 8. Dashed lines use `DashStyle`

```csharp
var dashStyle = new DashStyle(new[] { 6.0, 4.0 }, 0);
ctx.DrawLine(
    new Pen(Brushes.Red, 1.4, dashStyle),
    new Point(x1, y), new Point(x2, y));
```

---

## 9. `InitializeComponent()` must call `AvaloniaXamlLoader.Load(this)`

When compiled bindings are disabled (`AvaloniaUseCompiledBindingsByDefault=false`
in `.csproj`), `InitializeComponent` must be implemented manually.

```csharp
private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
```

Call it in the Window/UserControl constructor before accessing any named
controls. Omitting it results in a blank window.

---

## 10. Native DLL deployment via conditional `.csproj` items

Platform-specific extensions differ (`dll`/`so`/`dylib`). Use `Condition="Exists(...)"` 
to copy the right file without making the build fail on other platforms.

```xml
<ItemGroup Condition="Exists('..\..\core\build\msd_core.dll')">
    <None Include="..\..\core\build\msd_core.dll">
        <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
</ItemGroup>
<ItemGroup Condition="Exists('..\..\core\build\libmsd_core.so')">
    <None Include="..\..\core\build\libmsd_core.so">
        <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
</ItemGroup>
```

---

## 11. `Line` in a `StackPanel` requires an explicit `Width`

Without `Width`, a `Line` element collapses the layout in a `StackPanel`.

```xml
<Line Stroke="Red" StrokeThickness="2"
      StartPoint="0,0" EndPoint="16,0"
      VerticalAlignment="Center" Width="16"/>
```

---

## 12. Always use `CultureInfo.InvariantCulture` for number formatting

Without it, the decimal separator may become a comma in non-English locales,
breaking `FormatString` in `NumericUpDown` and `FormattedText`.

```csharp
var ft = new FormattedText(
    text, CultureInfo.InvariantCulture,
    FlowDirection.LeftToRight, Typeface.Default, 9, Brushes.Black);
```

---

## Recommended architecture summary

| Concern | Approach |
|---------|----------|
| Plot rendering | `Control` subclass + `Render(DrawingContext)` |
| Resizable plots | `BoundsProperty` in `AffectsRender` |
| Custom properties | `StyledProperty` + `AffectsRender` |
| Polylines in Render | `StreamGeometry` |
| Polylines in XAML binding | `AvaloniaList<Point>` |
| Two-way input controls | `Mode=TwoWay` on all `Slider`/`NumericUpDown` |
| Group UI | `TextBlock` + `Border` + `StackPanel` (not `HeaderedContentControl`) |
| Native DLL deployment | Conditional `<None>` items in `.csproj` |
| Number formatting | `CultureInfo.InvariantCulture` everywhere |
