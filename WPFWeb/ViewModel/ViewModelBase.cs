using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Text;

namespace WPFWeb.ViewModel
{
    public class ViewModelBase : INotifyPropertyChanged
    {
        // 변화감지 상속
        public event PropertyChangedEventHandler? PropertyChanged;

        public void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
}
