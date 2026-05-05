# Door Sign BLE Web App — Dynamic Metadata Version

Static Web Bluetooth control panel for the ESP32 e-ink door sign.

## Files

- `index.html`
- `style.css`
- `app.js`

Host these on GitHub Pages and open the site in Bluefy on iOS.

## New in this version

- Reads icon list from ESP32 characteristic `6E400003-...`
- Reads preset slot/label list from ESP32 characteristic `6E400004-...`
- Builds preset buttons and icon dropdown dynamically
- Caches last-read icons/presets locally
- Keeps local label overrides available
- Still works with defaults before the device connects

## Delimited metadata format

Icons:
`available|meeting|no|out|soon|remote|cranky|stop|circle`

Presets:
`1|Available|2|Meeting|3|Do not disturb`

## Notes

Safari/iOS does not support Web Bluetooth directly. Use Bluefy or another iOS browser with Web Bluetooth support.
