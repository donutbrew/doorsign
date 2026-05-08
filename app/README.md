# Door Sign Web App

Clean static web app for the ESP32 e-paper door sign.

## Files

- `index.html`
- `style.css`
- `app.js`

Upload all three files to GitHub Pages.

## Features

- BLE connect/reconnect/disconnect
- Dynamic preset metadata from firmware
- Dynamic icon metadata from firmware
- Current display status from firmware
- Scheduled message status from firmware
- Dark mode
- Message history
- Preset save / recall
- Manual `SHOWIN`
- Calendar day schedule JSON with `defaultMessage`
- Apply current message + next transition
- Backup / restore

## Calendar schedule JSON

```json
{
  "date": "2026-05-08",
  "timezone": "America/New_York",
  "defaultMessage": "ICON:available|[big]Available[/big]\\n[small]Come on in[/small]",
  "events": [
    {
      "title": "Meeting",
      "start": "2026-05-08T10:00:00-04:00",
      "end": "2026-05-08T10:30:00-04:00",
      "message": "ICON:meeting|[big]In a meeting[/big]\\n[small]Back at 10:30[/small]"
    }
  ]
}
```
