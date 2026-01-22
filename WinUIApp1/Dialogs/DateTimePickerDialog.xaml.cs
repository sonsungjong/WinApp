using System;
using System.Collections.Generic;
using System.Linq;
using Microsoft.UI.Xaml.Controls;

namespace WinUIApp1.Dialogs;

/// <summary>
/// 날짜/시간 선택 다이얼로그
/// </summary>
public sealed partial class DateTimePickerDialog : ContentDialog
{
    private List<DateTime> _availableDates = new();

    /// <summary>
    /// 선택된 날짜/시간
    /// </summary>
    public DateTime SelectedDateTime { get; private set; } = DateTime.Now;

    /// <summary>
    /// 선택이 유효한지 여부
    /// </summary>
    public bool IsValidSelection => _datePicker?.Date != null;

    private CalendarDatePicker? _datePicker;
    private TimePicker? _timePicker;
    private TextBlock? _previewText;
    private TextBlock? _availableDatesText;

    public DateTimePickerDialog()
    {
        InitializeComponent();

        // XAML에서 이름으로 참조된 컨트롤 찾기
        var content = this.Content as Microsoft.UI.Xaml.FrameworkElement;
        if (content != null)
        {
            _datePicker = content.FindName("DatePicker") as CalendarDatePicker;
            _timePicker = content.FindName("TimePicker") as TimePicker;
            _previewText = content.FindName("PreviewText") as TextBlock;
            _availableDatesText = content.FindName("AvailableDatesText") as TextBlock;
        }

        // 기본값: 현재 시간
        if (_datePicker != null)
        {
            _datePicker.Date = DateTimeOffset.Now;
            _datePicker.DateChanged += DatePicker_DateChanged;
        }

        if (_timePicker != null)
        {
            _timePicker.Time = DateTime.Now.TimeOfDay;
            _timePicker.TimeChanged += TimePicker_TimeChanged;
        }

        UpdatePreview();
    }

    private void TimePicker_TimeChanged(object sender, TimePickerValueChangedEventArgs e)
    {
        UpdatePreview();
    }

    /// <summary>
    /// 녹화 가능한 날짜 설정
    /// </summary>
    public void SetAvailableDates(List<DateTime> dates)
    {
        _availableDates = dates;

        if (_availableDatesText != null)
        {
            if (dates.Count > 0)
            {
                var minDate = dates.Min();
                var maxDate = dates.Max();
                _availableDatesText.Text = $"녹화 가능 기간: {minDate:yyyy-MM-dd} ~ {maxDate:yyyy-MM-dd}";
            }
            else
            {
                _availableDatesText.Text = "녹화된 영상이 없습니다.";
            }
        }
    }

    /// <summary>
    /// 초기 시간 설정
    /// </summary>
    public void SetInitialDateTime(DateTime dateTime)
    {
        if (_datePicker != null)
        {
            _datePicker.Date = new DateTimeOffset(dateTime.Date);
        }

        if (_timePicker != null)
        {
            _timePicker.Time = dateTime.TimeOfDay;
        }

        UpdatePreview();
    }

    private void DatePicker_DateChanged(CalendarDatePicker sender, CalendarDatePickerDateChangedEventArgs args)
    {
        UpdatePreview();
    }

    private void UpdatePreview()
    {
        if (_datePicker?.Date != null && _timePicker != null)
        {
            var date = _datePicker.Date.Value.Date;
            var time = _timePicker.Time;
            SelectedDateTime = date.Add(time);

            if (_previewText != null)
            {
                _previewText.Text = SelectedDateTime.ToString("yyyy-MM-dd HH:mm:ss");
            }
        }
        else if (_previewText != null)
        {
            _previewText.Text = "-";
        }
    }

    private void ContentDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
    {
        if (!IsValidSelection)
        {
            args.Cancel = true;
            return;
        }

        UpdatePreview();
    }
}
