using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;

using MsdAvalonia.ViewModels;

namespace MsdAvalonia.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

    private MainWindowViewModel? Vm => DataContext as MainWindowViewModel;

    public void OnRunClick      (object? sender, RoutedEventArgs e) => Vm?.Run();
    public void OnAddClick      (object? sender, RoutedEventArgs e) => Vm?.AddCase();
    public void OnDuplicateClick(object? sender, RoutedEventArgs e) => Vm?.DuplicateSelected();
    public void OnRemoveClick   (object? sender, RoutedEventArgs e) => Vm?.RemoveSelected();
    public void OnResetClick    (object? sender, RoutedEventArgs e) => Vm?.ResetCases();
}
