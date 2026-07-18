using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;

using TdofAvalonia.ViewModels;

namespace TdofAvalonia.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

    private MainWindowViewModel? Vm => DataContext as MainWindowViewModel;

    public void OnRunClick  (object? sender, RoutedEventArgs e) => Vm?.Run();
    public void OnResetClick(object? sender, RoutedEventArgs e) => Vm?.ResetAndRun();
}
