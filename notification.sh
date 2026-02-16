# Nebula-Tidy Notification Script
Add-Type -AssemblyName System.Windows.Forms
[System.Windows.Forms.NotifyIcon]::new().ShowBalloonTip(1000, 'Nebula-Tidy', 'Operation Started...', [System.Windows.Forms.ToolTipIcon]::Info)
