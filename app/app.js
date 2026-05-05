// Door Sign BLE Remote
// Dynamic Web Bluetooth app for Bluefy / compatible browsers.

const DEFAULTS = {
  devicePrefix: "ESP32-EINK-MSG",
  serviceUuid: "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
  writeCharacteristicUuid: "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
  iconsCharacteristicUuid: "6e400003-b5a3-f393-e0a9-e50e24dcca9e",
  presetsCharacteristicUuid: "6e400004-b5a3-f393-e0a9-e50e24dcca9e",
  statusCharacteristicUuid: "6e400005-b5a3-f393-e0a9-e50e24dcca9e",
};

const DEFAULT_ICONS = ["available", "meeting", "no", "out", "soon", "remote", "cranky", "stop", "circle"];

const DEFAULT_PRESETS = [
  { slot: "1", label: "Available" },
  { slot: "2", label: "Meeting" },
  { slot: "3", label: "Do Not Disturb" },
  { slot: "4", label: "Out" },
  { slot: "5", label: "Soon" },
  { slot: "6", label: "Teleworking" },
  { slot: "7", label: "Cranky" },
];

const ICON_LABELS = {
  available: "Available / smiley",
  meeting: "Meeting / phone",
  no: "Do not disturb",
  out: "Out of office",
  soon: "Returning soon",
  remote: "Teleworking / WiFi",
  telework: "Telework",
  teleworking: "Teleworking",
  cranky: "Cranky",
  stop: "Stop sign",
  circle: "Black circle",
};

const TEMPLATES = [
  ["Available", "available", "[big]Available[/big]\\n[small]Come on in[/small]"],
  ["Meeting", "meeting", "[big]In a meeting[/big]\\n[small]Back soon[/small]"],
  ["DND", "no", "[big]{{Do not disturb}}[/big]\\n[small]Working[/small]"],
  ["Out", "out", "[big]Out[/big]\\n[small]Back later[/small]"],
  ["Returning soon", "soon", "[big]Returning soon[/big]"],
  ["Teleworking", "remote", "[big]Teleworking[/big]\\n[small]Available online[/small]"],
  ["Cranky", "cranky", "[big]Cranky[/big]\\n[small]Proceed carefully[/small]"],
];

const STORAGE_KEYS = {
  settings: "doorsign.settings.v2",
  history: "doorsign.customHistory.v1",
  localPresetLabels: "doorsign.localPresetLabels.v2",
  cachedIcons: "doorsign.cachedIcons.v2",
  cachedPresets: "doorsign.cachedPresets.v2",
  lastDeviceId: "doorsign.lastDeviceId.v1",
  lastDeviceName: "doorsign.lastDeviceName.v1",
  theme: "doorsign.theme.v1",
};

let bleDevice = null;
let bleServer = null;
let writeCharacteristic = null;
let iconsCharacteristic = null;
let presetsCharacteristic = null;
let statusCharacteristic = null;

let activeIcons = [...DEFAULT_ICONS];
let activePresets = [...DEFAULT_PRESETS];

const $ = (id) => document.getElementById(id);

function readJson(key, fallback) {
  try { return JSON.parse(localStorage.getItem(key) || JSON.stringify(fallback)); }
  catch { return fallback; }
}

function writeJson(key, value) {
  localStorage.setItem(key, JSON.stringify(value));
}

function loadSettings() {
  const old = readJson("doorsign.settings.v1", {});
  const now = readJson(STORAGE_KEYS.settings, {});
  return { ...DEFAULTS, ...old, ...now };
}

function saveSettings(settings) { writeJson(STORAGE_KEYS.settings, settings); }

function currentSettings() {
  return {
    devicePrefix: $("devicePrefixInput").value.trim() || DEFAULTS.devicePrefix,
    serviceUuid: $("serviceUuidInput").value.trim().toLowerCase() || DEFAULTS.serviceUuid,
    writeCharacteristicUuid: $("characteristicUuidInput").value.trim().toLowerCase() || DEFAULTS.writeCharacteristicUuid,
    iconsCharacteristicUuid: $("iconsUuidInput").value.trim().toLowerCase() || DEFAULTS.iconsCharacteristicUuid,
    presetsCharacteristicUuid: $("presetsUuidInput").value.trim().toLowerCase() || DEFAULTS.presetsCharacteristicUuid,
    statusCharacteristicUuid: $("statusUuidInput").value.trim().toLowerCase() || DEFAULTS.statusCharacteristicUuid,
  };
}

function populateSettings() {
  const s = loadSettings();
  $("devicePrefixInput").value = s.devicePrefix;
  $("serviceUuidInput").value = s.serviceUuid;
  $("characteristicUuidInput").value = s.writeCharacteristicUuid;
  $("iconsUuidInput").value = s.iconsCharacteristicUuid;
  $("presetsUuidInput").value = s.presetsCharacteristicUuid;
  $("statusUuidInput").value = s.statusCharacteristicUuid;
}

function setStatus(connected, text) {
  const pill = $("statusPill");
  pill.className = `status ${connected ? "connected" : "disconnected"}`;
  pill.textContent = text || (connected ? "Connected" : "Disconnected");
}

function setAction(text) { $("lastAction").textContent = text; }

function flashButton(button, text = "Sent ✓") {
  if (!button) return;
  const oldText = button.textContent;
  button.textContent = text;
  button.classList.add("sent");
  setTimeout(() => {
    button.textContent = oldText;
    button.classList.remove("sent");
  }, 1000);
}

function updateDeviceInfo() {
  if (bleDevice) {
    $("deviceInfo").textContent = `Device: ${bleDevice.name || "(unnamed)"}${bleDevice.id ? " · " + bleDevice.id : ""}`;
  } else {
    const last = localStorage.getItem(STORAGE_KEYS.lastDeviceName);
    $("deviceInfo").textContent = last ? `Last device: ${last}` : "No device connected.";
  }
}

function bluetoothAvailable() { return !!navigator.bluetooth; }

function decodeDataView(view) {
  return new TextDecoder().decode(view.buffer);
}

function parseDelimitedList(value) {
  return value.split("|").map(x => x.trim()).filter(Boolean);
}

function parsePresetMetadata(value) {
  const parts = parseDelimitedList(value);
  const presets = [];
  for (let i = 0; i + 1 < parts.length; i += 2) {
    presets.push({ slot: parts[i], label: parts[i + 1] });
  }
  return presets.length ? presets : [...DEFAULT_PRESETS];
}

async function loadDeviceMetadata() {
  let usedDeviceData = false;

  if (iconsCharacteristic) {
    try {
      const raw = decodeDataView(await iconsCharacteristic.readValue());
      const icons = parseDelimitedList(raw);
      if (icons.length) {
        activeIcons = icons;
        writeJson(STORAGE_KEYS.cachedIcons, icons);
        usedDeviceData = true;
      }
    } catch (err) {
      console.warn("Could not read icon metadata:", err);
    }
  }

  if (presetsCharacteristic) {
    try {
      const raw = decodeDataView(await presetsCharacteristic.readValue());
      const presets = parsePresetMetadata(raw);
      if (presets.length) {
        activePresets = presets;
        writeJson(STORAGE_KEYS.cachedPresets, presets);
        usedDeviceData = true;
      }
    } catch (err) {
      console.warn("Could not read preset metadata:", err);
    }
  }

  renderIconSelect();
  renderPresetButtons();
  renderSlotSelect();

  $("metadataNote").textContent = usedDeviceData
    ? "Using preset/icon metadata read from device."
    : "Using cached/local defaults.";

  if (presetsCharacteristic?.startNotifications) {
    try {
      await presetsCharacteristic.startNotifications();
      presetsCharacteristic.addEventListener("characteristicvaluechanged", (event) => {
        const raw = decodeDataView(event.target.value);
        activePresets = parsePresetMetadata(raw);
        writeJson(STORAGE_KEYS.cachedPresets, activePresets);
        renderPresetButtons();
        renderSlotSelect();
        $("metadataNote").textContent = "Preset metadata updated from device.";
      });
    } catch (err) {
      console.warn("Preset notifications unavailable:", err);
    }
  }
}

async function connectWithDevice(device) {
  const settings = currentSettings();

  bleDevice = device;
  localStorage.setItem(STORAGE_KEYS.lastDeviceId, device.id || "");
  localStorage.setItem(STORAGE_KEYS.lastDeviceName, device.name || "ESP32-EINK-MSG");

  bleDevice.addEventListener("gattserverdisconnected", () => {
    writeCharacteristic = null;
    iconsCharacteristic = null;
    presetsCharacteristic = null;
    statusCharacteristic = null;
    bleServer = null;
    setStatus(false, "Disconnected");
    setAction("Disconnected.");
    updateDeviceInfo();
  });

  setStatus(false, "Connecting...");
  setAction("Connecting to device...");
  updateDeviceInfo();

  bleServer = await bleDevice.gatt.connect();
  const service = await bleServer.getPrimaryService(settings.serviceUuid);

  writeCharacteristic = await service.getCharacteristic(settings.writeCharacteristicUuid);

  try { iconsCharacteristic = await service.getCharacteristic(settings.iconsCharacteristicUuid); }
  catch { iconsCharacteristic = null; }

  try { presetsCharacteristic = await service.getCharacteristic(settings.presetsCharacteristicUuid); }
  catch { presetsCharacteristic = null; }

  try { statusCharacteristic = await service.getCharacteristic(settings.statusCharacteristicUuid); }
  catch { statusCharacteristic = null; }

  setStatus(true, "Connected");
  setAction("Connected. Reading metadata...");
  updateDeviceInfo();

  await loadDeviceMetadata();

  setAction("Connected.");
}

async function connect() {
  if (!bluetoothAvailable()) {
    alert("Web Bluetooth is not available in this browser. Try Bluefy on iOS.");
    return;
  }

  const settings = currentSettings();
  saveSettings(settings);

  const device = await navigator.bluetooth.requestDevice({
    filters: [
      { namePrefix: settings.devicePrefix },
      { services: [settings.serviceUuid] },
    ],
    optionalServices: [settings.serviceUuid],
  });

  await connectWithDevice(device);
}

async function reconnectLast() {
  if (!bluetoothAvailable()) {
    alert("Web Bluetooth is not available in this browser.");
    return;
  }

  if (navigator.bluetooth.getDevices) {
    const settings = currentSettings();
    const lastId = localStorage.getItem(STORAGE_KEYS.lastDeviceId);
    const lastName = localStorage.getItem(STORAGE_KEYS.lastDeviceName);

    const devices = await navigator.bluetooth.getDevices();
    const candidate =
      devices.find(d => lastId && d.id === lastId) ||
      devices.find(d => lastName && d.name === lastName) ||
      devices.find(d => d.name && d.name.startsWith(settings.devicePrefix));

    if (candidate) {
      await connectWithDevice(candidate);
      return;
    }
  }

  await connect();
}

function disconnect() {
  if (bleDevice?.gatt?.connected) bleDevice.gatt.disconnect();
  writeCharacteristic = null;
  iconsCharacteristic = null;
  presetsCharacteristic = null;
  statusCharacteristic = null;
  bleServer = null;
  setStatus(false, "Disconnected");
  setAction("Disconnected.");
  updateDeviceInfo();
}

async function writeValue(value, button = null) {
  if (!writeCharacteristic) {
    setAction("Connecting before send...");
    await reconnectLast();
  }

  if (!writeCharacteristic) throw new Error("Not connected.");

  setAction(`Sending: ${value}`);
  await writeCharacteristic.writeValue(new TextEncoder().encode(value));
  setAction("Sent ✓");
  flashButton(button);
}

function loadHistory() { return readJson(STORAGE_KEYS.history, []); }
function saveHistory(history) { writeJson(STORAGE_KEYS.history, history.slice(0, 20)); }

function addHistory(message) {
  const clean = message.trim();
  if (!clean) return;
  const history = loadHistory().filter(item => item !== clean);
  history.unshift(clean);
  saveHistory(history);
  renderHistory();
}

function buildMessage() {
  const icon = $("iconSelect").value;
  const text = $("messageInput").value.trim();
  if (!text) return "";
  return icon ? `ICON:${icon}|${text}` : text;
}

function updatePreview() { $("messagePreview").textContent = buildMessage() || "(nothing yet)"; }

function wrapSelection(textarea, before, after) {
  const start = textarea.selectionStart;
  const end = textarea.selectionEnd;
  const selected = textarea.value.slice(start, end);
  const replacement = `${before}${selected || "text"}${after}`;
  textarea.setRangeText(replacement, start, end, "select");
  textarea.focus();
  if (!selected) {
    textarea.selectionStart = start + before.length;
    textarea.selectionEnd = start + before.length + 4;
  }
  updatePreview();
}

function renderIconSelect() {
  const current = $("iconSelect").value;
  const root = $("iconSelect");
  root.innerHTML = "";

  const none = document.createElement("option");
  none.value = "";
  none.textContent = "No icon";
  root.appendChild(none);

  activeIcons.forEach(icon => {
    const opt = document.createElement("option");
    opt.value = icon;
    opt.textContent = ICON_LABELS[icon] || icon;
    root.appendChild(opt);
  });

  if ([...root.options].some(o => o.value === current)) root.value = current;
  updatePreview();
}

function localLabelsMap() {
  return readJson(STORAGE_KEYS.localPresetLabels, {});
}

function saveLocalLabelsMap(labels) {
  writeJson(STORAGE_KEYS.localPresetLabels, labels);
}

function displayLabelForPreset(preset) {
  const local = localLabelsMap();
  return local[preset.slot] || preset.label;
}

function renderPresetButtons() {
  const root = $("presetButtons");
  root.innerHTML = "";

  activePresets.forEach((preset) => {
    const btn = document.createElement("button");
    btn.className = "preset-button";
    btn.innerHTML = `Preset ${preset.slot}<span>${displayLabelForPreset(preset)}</span>`;
    btn.addEventListener("click", async () => {
      try { await writeValue(preset.slot, btn); }
      catch (err) { alert(`Could not send preset ${preset.slot}: ${err.message}`); }
    });
    root.appendChild(btn);
  });

  renderPresetLabelEditor();
}

function renderSlotSelect() {
  const current = $("slotSelect").value;
  const root = $("slotSelect");
  root.innerHTML = "";

  activePresets.forEach((preset) => {
    const opt = document.createElement("option");
    opt.value = preset.slot;
    opt.textContent = `Preset ${preset.slot}: ${displayLabelForPreset(preset)}`;
    root.appendChild(opt);
  });

  if ([...root.options].some(o => o.value === current)) root.value = current;
}

function renderPresetLabelEditor() {
  const root = $("presetLabelEditor");
  const local = localLabelsMap();
  root.innerHTML = "";

  activePresets.forEach((preset) => {
    const row = document.createElement("div");
    row.className = "label-editor-row";

    const slot = document.createElement("strong");
    slot.textContent = `Preset ${preset.slot}`;

    const input = document.createElement("input");
    input.value = local[preset.slot] || preset.label;
    input.placeholder = preset.label;
    input.addEventListener("change", () => {
      const latest = localLabelsMap();
      const val = input.value.trim();
      if (val && val !== preset.label) latest[preset.slot] = val;
      else delete latest[preset.slot];
      saveLocalLabelsMap(latest);
      renderPresetButtons();
      renderSlotSelect();
      $("presetLabelEditor").classList.remove("hidden");
    });

    row.append(slot, input);
    root.appendChild(row);
  });
}

function renderTemplates() {
  const root = $("templateButtons");
  root.innerHTML = "";

  TEMPLATES.forEach(([label, icon, text]) => {
    const btn = document.createElement("button");
    btn.className = "template-button";
    btn.innerHTML = `${label}<span>${icon ? "ICON:" + icon : "No icon"}</span>`;
    btn.addEventListener("click", () => {
      $("iconSelect").value = icon;
      $("messageInput").value = text;
      updatePreview();
      $("messageInput").scrollIntoView({ behavior: "smooth", block: "center" });
    });
    root.appendChild(btn);
  });
}

function renderHistory() {
  const root = $("historyList");
  const history = loadHistory();
  root.innerHTML = "";

  if (!history.length) {
    root.innerHTML = `<p class="muted">No custom messages yet.</p>`;
    return;
  }

  history.forEach((message) => {
    const item = document.createElement("div");
    item.className = "history-item";

    const text = document.createElement("div");
    text.className = "history-text";
    text.textContent = message;

    const sendBtn = document.createElement("button");
    sendBtn.textContent = "Send";
    sendBtn.addEventListener("click", async () => {
      try { await writeValue(message, sendBtn); }
      catch (err) { alert(`Could not send message: ${err.message}`); }
    });

    const editBtn = document.createElement("button");
    editBtn.textContent = "Edit";
    editBtn.addEventListener("click", () => {
      const match = message.match(/^ICON:([^|]+)\|(.*)$/s);
      if (match) {
        $("iconSelect").value = match[1];
        $("messageInput").value = match[2];
      } else {
        $("iconSelect").value = "";
        $("messageInput").value = message;
      }
      updatePreview();
      $("messageInput").scrollIntoView({ behavior: "smooth", block: "center" });
    });

    item.append(text, sendBtn, editBtn);
    root.appendChild(item);
  });
}

function exportBackup() {
  const payload = {
    exportedAt: new Date().toISOString(),
    settings: loadSettings(),
    localPresetLabels: localLabelsMap(),
    cachedIcons: activeIcons,
    cachedPresets: activePresets,
    history: loadHistory(),
    theme: localStorage.getItem(STORAGE_KEYS.theme) || loadTheme(),
  };
  $("backupText").value = JSON.stringify(payload, null, 2);
}

function importBackup() {
  let payload;
  try { payload = JSON.parse($("backupText").value); }
  catch { alert("That does not look like valid JSON."); return; }

  if (payload.settings) saveSettings({ ...DEFAULTS, ...payload.settings });
  if (payload.localPresetLabels) saveLocalLabelsMap(payload.localPresetLabels);
  if (Array.isArray(payload.cachedIcons)) {
    activeIcons = payload.cachedIcons;
    writeJson(STORAGE_KEYS.cachedIcons, activeIcons);
  }
  if (Array.isArray(payload.cachedPresets)) {
    activePresets = payload.cachedPresets;
    writeJson(STORAGE_KEYS.cachedPresets, activePresets);
  }
  if (Array.isArray(payload.history)) saveHistory(payload.history);
  if (payload.theme === "dark" || payload.theme === "light") applyTheme(payload.theme);

  populateSettings();
  renderIconSelect();
  renderPresetButtons();
  renderSlotSelect();
  renderHistory();
  alert("Imported local app settings.");
}


function applyTheme(theme) {
  const chosen = theme === "dark" ? "dark" : "light";
  document.documentElement.setAttribute("data-theme", chosen);
  localStorage.setItem(STORAGE_KEYS.theme, chosen);

  const btn = $("themeToggle");
  if (btn) {
    btn.textContent = chosen === "dark" ? "Light mode" : "Dark mode";
  }
}

function loadTheme() {
  const saved = localStorage.getItem(STORAGE_KEYS.theme);
  if (saved === "dark" || saved === "light") {
    return saved;
  }

  if (window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches) {
    return "dark";
  }

  return "light";
}

function toggleTheme() {
  const current = document.documentElement.getAttribute("data-theme") || "light";
  applyTheme(current === "dark" ? "light" : "dark");
}

function bindEvents() {
  $("themeToggle").addEventListener("click", toggleTheme);
  $("connectBtn").addEventListener("click", () => connect().catch(err => alert(err.message)));
  $("reconnectBtn").addEventListener("click", () => reconnectLast().catch(err => alert(err.message)));
  $("disconnectBtn").addEventListener("click", disconnect);
  $("refreshMetadataBtn").addEventListener("click", () => loadDeviceMetadata().catch(err => alert(err.message)));

  $("saveSettingsBtn").addEventListener("click", () => {
    saveSettings(currentSettings());
    alert("Settings saved in this browser.");
  });

  $("resetSettingsBtn").addEventListener("click", () => {
    saveSettings(DEFAULTS);
    populateSettings();
    alert("Settings reset.");
  });

  $("editLabelsToggle").addEventListener("click", () => {
    $("presetLabelEditor").classList.toggle("hidden");
  });

  $("messageInput").addEventListener("input", updatePreview);
  $("iconSelect").addEventListener("change", updatePreview);

  document.querySelectorAll("[data-wrap]").forEach(btn => {
    btn.addEventListener("click", () => {
      const [before, after] = btn.dataset.wrap.split("|");
      wrapSelection($("messageInput"), before, after);
    });
  });

  $("newlineBtn").addEventListener("click", () => {
    const t = $("messageInput");
    t.setRangeText("\\n", t.selectionStart, t.selectionEnd, "end");
    t.focus();
    updatePreview();
  });

  $("sendCustomBtn").addEventListener("click", async (event) => {
    const message = buildMessage();
    if (!message) { alert("Write a message first."); return; }

    try {
      await writeValue(message, event.currentTarget);
      addHistory(message);
    } catch (err) {
      alert(`Could not send message: ${err.message}`);
    }
  });

  $("savePresetBtn").addEventListener("click", async (event) => {
    const message = buildMessage();
    const slot = $("slotSelect").value;
    if (!message) { alert("Write a message first."); return; }

    try {
      await writeValue(`SET${slot}:${message}`, event.currentTarget);
      addHistory(message);
      setTimeout(() => loadDeviceMetadata().catch(console.warn), 500);
    } catch (err) {
      alert(`Could not save preset: ${err.message}`);
    }
  });

  $("saveAndRecallBtn").addEventListener("click", async (event) => {
    const message = buildMessage();
    const slot = $("slotSelect").value;
    if (!message) { alert("Write a message first."); return; }

    try {
      await writeValue(`SET${slot}:${message}`, event.currentTarget);
      await new Promise(resolve => setTimeout(resolve, 250));
      await writeValue(slot, event.currentTarget);
      addHistory(message);
      setTimeout(() => loadDeviceMetadata().catch(console.warn), 700);
    } catch (err) {
      alert(`Could not save and recall preset: ${err.message}`);
    }
  });

  $("clearEditorBtn").addEventListener("click", () => {
    $("messageInput").value = "";
    $("iconSelect").value = "";
    updatePreview();
  });

  $("clearHistoryBtn").addEventListener("click", () => {
    if (confirm("Clear local custom-message history?")) {
      saveHistory([]);
      renderHistory();
    }
  });

  $("exportBtn").addEventListener("click", exportBackup);
  $("importBtn").addEventListener("click", importBackup);

  $("resetPresetsBtn").addEventListener("click", async (event) => {
    if (!confirm("Send RESETPRESETS to the ESP32?")) return;
    try {
      await writeValue("RESETPRESETS", event.currentTarget);
      setTimeout(() => loadDeviceMetadata().catch(console.warn), 700);
    } catch (err) {
      alert(`Could not reset presets: ${err.message}`);
    }
  });
}

function boot() {
  applyTheme(loadTheme());
  populateSettings();

  activeIcons = readJson(STORAGE_KEYS.cachedIcons, DEFAULT_ICONS);
  activePresets = readJson(STORAGE_KEYS.cachedPresets, DEFAULT_PRESETS);

  renderIconSelect();
  renderPresetButtons();
  renderSlotSelect();
  renderTemplates();
  renderHistory();
  bindEvents();
  updatePreview();
  updateDeviceInfo();

  if (!bluetoothAvailable()) setStatus(false, "No Web Bluetooth");
}

boot();
