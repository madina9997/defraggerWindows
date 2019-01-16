using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Runtime.InteropServices;
using System.Windows.Threading;
using System.Threading;


namespace Defragger
{

    /// <summary>
    /// Логика взаимодействия для MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        private static PInvokeDefragLibrary.TVolume defragmentingVolume;
        private static DispatcherTimer dispatcherTimer;
        private static bool StopDefrProc=false;
        private static uint blocksAmount = 2000;

        public MainWindow()
        {
            InitializeComponent();
            dispatcherTimer = new System.Windows.Threading.DispatcherTimer();
            dispatcherTimer.Tick += new EventHandler(dispatcherTimer_Tick);
            dispatcherTimer.Interval = new TimeSpan(0, 0, 2);
        }

        private PInvokeDefragLibrary.TVolume GetNameFromPath(string volume)
        {
            PInvokeDefragLibrary.TVolume retVolume; 
            //Class1.TVolume retVolume;
            retVolume.Name = volume;
            var volumesGUID = PInvokeDefragLibrary.PInvoke.GetAllVolumesGUID();
            var volumes = PInvokeDefragLibrary.PInvoke.GetAllVolumes();
            for (int i = 0; i < volumes.Length; i++)
            {
                if (volume.Equals(volumes[i].Name))
                {
                    retVolume.Name = volumesGUID[i].Name;
                    break;
                }
            }
            return retVolume;
        }

        private void dispatcherTimer_Tick(object sender, EventArgs e)
        {
            BitmapDrawing(defragmentingVolume);
        }

        private void BtnDefrag_Click(object sender,RoutedEventArgs e)

        {
            StopDefrProc = false;
            var volumeWithGUID = GetNameFromPath(this.comboBox.SelectedItem.ToString());
            defragmentingVolume = volumeWithGUID;
            UInt64 i = 0;
            BitmapDrawing(volumeWithGUID);

            Thread thread = new Thread(() =>
            {
                //Do somthing and set your value
                PInvokeDefragLibrary.PInvoke.DefragmentFirstStage(volumeWithGUID, i);
                /*while (true)
                {
                    /*if (StopDefrProc) {
                        while (StopDefrProc)
                        {
                        
                        }
                        //pause procedure
                    }*
                    if (Class1.PInvoke.DefragmentFirstStage(volumeWithGUID, i) == 0) break;
                    /*if (StopDefrProc)
                    {
                        while (StopDefrProc)
                        {

                        }
                        //pause procedure
                    }*
                    i = Class1.PInvoke.DefragmentSecondStage(volumeWithGUID, i);
                
                }*/
            });

            thread.Start();
            
            dispatcherTimer.Start();
            //Class1.PInvoke.DefragmentFirstStage(volumeWithGUID, i);
            /*while (true)
            {
                if (Class1.PInvoke.DefragmentFirstStage(volumeWithGUID, i) == 0) break;
                //Class1.PInvoke.DefragmentFirstStage(volumeWithGUID, i);
            //BitmapDrawing(volumeWithGUID);
              i=Class1.PInvoke.DefragmentSecondStage(volumeWithGUID, i);
            //BitmapDrawing(volumeWithGUID);

            }*/
            //BitmapDrawing(volumeWithGUID);
           // dispatcherTimer.Stop();
        }

        private void BitmapDrawing(PInvokeDefragLibrary.TVolume vvolume)
        {
            lbResult.Children.Clear();
            int bloksperline = 58;
            int row = 1;
            int column = 1;
            foreach (var item in PInvokeDefragLibrary.PInvoke.GetColouredBlocks(vvolume, blocksAmount))
            {
                RectangleGeometry myRectangleGeometry = new RectangleGeometry
                {
                    Rect = new Rect(10 + column * 13, row * 13, 10, 10)
                };

                Path myPath = new Path
                {
                    Fill= (item.State==2)? Brushes.Coral:
                        (item.State==0)?Brushes.LemonChiffon:
                        (item.State == 3) ? Brushes.Blue:
                        Brushes.LightGreen,                  
                    Stroke = Brushes.Black,
                    StrokeThickness = 1,
                    Data = myRectangleGeometry
                };
                lbResult.Children.Add(myPath);
                if ((column % bloksperline) == 0) { row++; column = 1; }
                else column++;
            }
        }

        private void BtnClickMe_Click(object sender, RoutedEventArgs e)
        {
            
            if (dispatcherTimer.IsEnabled) dispatcherTimer.Stop();
            var volumeWithGUID = GetNameFromPath(this.comboBox.SelectedItem.ToString());
            var VolumeSizeInBytes = PInvokeDefragLibrary.PInvoke.GetVolumeSizeInBytes(volumeWithGUID);
            VolumeSizeLabel.Content = (VolumeSizeInBytes/(1024*1024)).ToString();
            BlockSizeLabel.Content = (VolumeSizeInBytes / (1024 * blocksAmount)).ToString();
            BitmapDrawing(volumeWithGUID);
            
        }

        private void BtnClickMe_Click2(object sender, RoutedEventArgs e)
        {
            PInvokeDefragLibrary.PInvoke.InterruptDefrag();

        }

        private void cbImportance_Loaded(object sender, RoutedEventArgs e)
        {
            List<string> data = new List<string>();
            var volumes = PInvokeDefragLibrary.PInvoke.GetAllVolumes();

            for (int i=0;i<volumes.Length;i++)
            {
                if (!volumes[i].Name.Equals("")) data.Add(volumes[i].Name);
            }
            var cbImportance = sender as ComboBox;
            cbImportance.ItemsSource = data;
            cbImportance.SelectedIndex = 1;
        }
        /*private void TextBlock_TextChanged(object sender, TextChangedEventArgs e)
        {
            //TextBox textBox = (TextBox)sender;
            //MessageBox.Show(textBox.Text);
            VolumeSizeTextBlock.DataContext = "1";
        }*/


        private void myComboBox_SelectionChanged(object sender, RoutedEventArgs e)
        {

        }

        private void myComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {

        }

        /*private void TextBlock_TextChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            //VolumeSizeTextBlock.Text = "1";
            TextBlock VolumeSizeTextBlock = new TextBlock();
            VolumeSizeTextBlock.Content = "The text contents of this TextBlock.";
        }*/
    }
}
