// Models/GridLineMarker.cs — render DTOs for the plot.

using Avalonia;

namespace PidAvalonia.Models;

/// <summary>One grid line (StartPoint→EndPoint), used by the plot canvas.</summary>
public sealed class GridLineMarker
{
    public Point Start { get; init; }
    public Point End   { get; init; }
}
