using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Input;

namespace WPFWeb.ViewModel
{
    public class ViewModelCommand : ICommand
    {
        private readonly Action<object?> m_executeAction;
        private readonly Predicate<object?>? m_canExecuteAction;

        public ViewModelCommand(Action<object?> executeAction)
        {
            m_executeAction = executeAction;
            m_canExecuteAction = null;
        }

        public ViewModelCommand(Action<object?> executeAction, Predicate<object?> canExecuteAction)
        {
            m_executeAction = executeAction;
            m_canExecuteAction = canExecuteAction;
        }

        // 이벤트
        public event EventHandler? CanExecuteChanged
        {
            add { CommandManager.RequerySuggested += value; }
            remove { CommandManager.RequerySuggested -= value; }
        }

        public bool CanExecute(object? parameter)
        {
            return m_canExecuteAction == null ? true : m_canExecuteAction(parameter);
        }

        public void Execute(object? parameter)
        {
            m_executeAction(parameter);
        }
    }
}
