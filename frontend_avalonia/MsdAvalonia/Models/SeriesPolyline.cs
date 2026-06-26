// Models/SeriesPolyline.cs — DTO for one plotted case (color + points).

using Avalonia.Collections;
using Avalonia.Media;

namespace MsdAvalonia.Models;

public sealed class SeriesPolyline
{
    public required string Label  { get; init; }
    public required IBrush Stroke { get; init; }
    public required AvaloniaList<Avalonia.Point> Points { get; init; }
}
