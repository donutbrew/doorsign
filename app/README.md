# Door Sign BLE Web App

Static Web Bluetooth control panel for the ESP32 e-ink door sign.

## Files

- `index.html`
- `style.css`
- `app.js`

Host these on GitHub Pages and open the site in Bluefy on iOS.

## Features

- Connects to `ESP32-EINK-MSG`
- Sends preset recalls `1` through `7`
- Sends custom messages
- Saves presets with `SET1:` through `SET7:`
- Save + Recall workflow
- Stores the last 20 custom messages in browser `localStorage`
- Editable local preset labels
- Built-in templates
- Export/import backup JSON
- Clear send/connection feedback
- Remembers BLE UUID settings and tries to reconnect to the last granted device when supported

## Notes

Safari/iOS does not support Web Bluetooth directly. Use Bluefy or another iOS browser with Web Bluetooth support.

The ESP32 firmware must support presets `1` through `7` and `SET1:` through `SET7:` for all controls to work.
