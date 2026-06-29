// Models/PlotSeries.cs — render DTOs for the plots.

using Avalonia.Collections;
using Avalonia.Media;

namespace TdofAvalonia.Models;

/// <summary>A pixel-space polyline plus its legend label and colour.</summary>
public sealed class PlotSeries
{
    public required string             Label  { get; init; }
    public required IBrush             Stroke { get; init; }
    public required AvaloniaList<Avalonia.Point> Points { get; init; }
}
