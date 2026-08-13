# Avalonia 11 実装メモ

[モジュール概要](README.md) · [理論](theory.md) · [C ABI](api.md) · [English](../../en/mass_spring_damper/avalonia-notes.md)

MSD の Avalonia フロントエンドを作る過程で得られた知見です。各項目は
「踏んだ落とし穴」と「その対処」で構成しています。

---

## 1. Canvas は固定サイズ — ウィンドウリサイズに追従しない

**問題**： `<Canvas Width="720" Height="320">` にすると、グリッド線と目盛り位置を
すべてピクセル単位でハードコードする必要があり、ウィンドウをリサイズしても
キャンバスの大きさは変わりません。

**対処**： `Control` を継承して `Render(DrawingContext ctx)` をオーバーライドし、
描画時に `Bounds.Width` / `Bounds.Height` を読んで動的にスケーリングします。

```csharp
public override void Render(DrawingContext ctx)
{
    double w = Bounds.Width;
    double h = Bounds.Height;
    if (w < 1 || h < 1) return;
    // 以降の座標変換にはすべて w, h を使う
}
```

---

## 2. `BoundsProperty` を `AffectsRender` に入れないとリサイズで再描画されない

**問題**： `AffectsRender<PlotControl>(SimTProperty, ...)` だけでは、ウィンドウを
リサイズしても再描画が走りません。

**対処**： `AffectsRender` の呼び出しに `BoundsProperty` を追加します。

```csharp
static PlotControl()
{
    AffectsRender<PlotControl>(
        SimTProperty, SimXProperty,
        YMinProperty, YMaxProperty, XMaxProperty,
        BoundsProperty   // ← リサイズ時の再描画に必須
    );
}
```

---

## 3. `Polyline.Points` には `AvaloniaList<Point>` が必要

**問題**： `Polyline.Points` を `List<Point>` や `IEnumerable<Point>` にバインド
しても更新が反映されません。

**対処**： `AvaloniaList<Point>` を使い、 `Clear()` → `Add()` でインプレースに
更新します。

```csharp
// ViewModel
public AvaloniaList<Point> ResponsePoints { get; } = new();

// Run() の中で
ResponsePoints.Clear();
for (int i = 0; i < sim.T.Length; ++i)
    ResponsePoints.Add(new Point(...));
```

---

## 4. Slider / NumericUpDown のバインドには `Mode=TwoWay` が必要

**問題**： Avalonia の既定バインディングモードは `OneWay` のため、 `Slider` を
動かしたり `NumericUpDown` を編集しても ViewModel に値が伝わりません。

**対処**： 入力コントロールには必ず `Mode=TwoWay` を指定します。

```xml
<Slider Value="{Binding Kp, Mode=TwoWay}"/>
<NumericUpDown Value="{Binding Kp, Mode=TwoWay}"/>
```

---

## 5. `HeaderedContentControl` は Fluent テーマでは使えない

**問題**： Avalonia 11 の `HeaderedContentControl` には Fluent テーマのスタイルが
適用されず、レイアウトが崩れます。

**対処**： `TextBlock` + `Border` + `StackPanel` の組み合わせで代替します。

```xml
<StackPanel>
    <TextBlock Text="Parameters" FontWeight="Bold" Margin="2,0,0,2"/>
    <Border BorderBrush="Gray" BorderThickness="1" Padding="8" CornerRadius="2">
        <StackPanel Spacing="4">
            <!-- コントロール群 -->
        </StackPanel>
    </Border>
</StackPanel>
```

---

## 6. カスタムコントロールのプロパティは `StyledProperty` で登録する

**問題**： 素の CLR プロパティはバインディングにも `AffectsRender` にも対応
しません。

**対処**： `AvaloniaProperty.Register<TOwner, TValue>()` で登録します。

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

## 7. `Render()` 内のポリラインは `StreamGeometry` で描く

**問題**： `Polyline` はコントロールなので `Render()` の中では使えません。

**対処**： `BeginFigure` / `LineTo` / `EndFigure` で `StreamGeometry` を構築します。

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

## 8. 破線は `DashStyle` で指定する

```csharp
var dashStyle = new DashStyle(new[] { 6.0, 4.0 }, 0);
ctx.DrawLine(
    new Pen(Brushes.Red, 1.4, dashStyle),
    new Point(x1, y), new Point(x2, y));
```

---

## 9. `InitializeComponent()` は `AvaloniaXamlLoader.Load(this)` を呼ぶ

コンパイル済みバインディングを無効にしている場合
（ `.csproj` の `AvaloniaUseCompiledBindingsByDefault=false` ）、
`InitializeComponent` は手動で実装する必要があります。

```csharp
private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
```

Window / UserControl のコンストラクタ内で、名前付きコントロールにアクセスする
前に呼んでください。呼び忘れるとウィンドウが真っ白になります。

---

## 10. ネイティブ DLL は `.csproj` の条件付き項目で配置する

プラットフォームごとに拡張子が異なります（ `dll` / `so` / `dylib` ）。
`Condition="Exists(...)"` を使えば、他プラットフォームでビルドを失敗させずに
適切なファイルだけをコピーできます。

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

## 11. `StackPanel` 内の `Line` には明示的な `Width` が必要

`Width` を指定しないと、 `StackPanel` 内の `Line` 要素はレイアウトを崩します。

```xml
<Line Stroke="Red" StrokeThickness="2"
      StartPoint="0,0" EndPoint="16,0"
      VerticalAlignment="Center" Width="16"/>
```

---

## 12. 数値の書式には常に `CultureInfo.InvariantCulture` を使う

指定しないと、英語以外のロケールで小数点記号がカンマになり、
`NumericUpDown` の `FormatString` や `FormattedText` が壊れます。

```csharp
var ft = new FormattedText(
    text, CultureInfo.InvariantCulture,
    FlowDirection.LeftToRight, Typeface.Default, 9, Brushes.Black);
```

---

## 推奨アーキテクチャまとめ

| 関心事 | 推奨 |
|--------|------|
| プロット描画 | `Control` サブクラス + `Render(DrawingContext)` |
| リサイズ対応 | `AffectsRender` に `BoundsProperty` を含める |
| カスタムプロパティ | `StyledProperty` + `AffectsRender` |
| Render 内のポリライン | `StreamGeometry` |
| XAML バインドのポリライン | `AvaloniaList<Point>` |
| 双方向の入力コントロール | すべての `Slider` / `NumericUpDown` に `Mode=TwoWay` |
| グループ UI | `TextBlock` + `Border` + `StackPanel`（ `HeaderedContentControl` は不可） |
| ネイティブ DLL の配置 | `.csproj` の条件付き `<None>` 項目 |
| 数値の書式 | 全箇所で `CultureInfo.InvariantCulture` |

---

関連： 描画されないときの体系的な切り分けは
[avalonia-debug-polylines.md](avalonia-debug-polylines.md) を参照してください。
