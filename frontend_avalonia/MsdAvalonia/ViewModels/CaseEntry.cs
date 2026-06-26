// ViewModels/CaseEntry.cs

using System.ComponentModel;
using System.Runtime.CompilerServices;

using MsdAvalonia.Native;

namespace MsdAvalonia.ViewModels;

/// <summary>One row in the case-list. Wraps an MsdCase with an enabled flag.</summary>
public sealed class CaseEntry : INotifyPropertyChanged
{
    public CaseEntry(MsdCase c) { _case = c; }

    private MsdCase _case;
    public MsdCase Case => _case;

    public string Name
    {
        get => _case.Name;
        set { if (_case.Name != value) { _case.Name = value; OnChanged(); } }
    }

    private bool _enabled = true;
    public bool Enabled
    {
        get => _enabled;
        set { if (_enabled != value) { _enabled = value; OnChanged(); } }
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnChanged([CallerMemberName] string? n = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(n));
}
