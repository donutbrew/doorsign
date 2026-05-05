// Door Sign BLE Remote
// Static Web Bluetooth app for Bluefy / compatible browsers.

const DEFAULTS = {
  devicePrefix: "ESP32-EINK-MSG",
  serviceUuid: "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
  characteristicUuid: "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
};

const DEFAULT_PRESET_LABELS = [
  "Available",
  "Meeting",
  "Do Not Disturb",
  "Out",
  "Soon",
  "Teleworking",
  "Cranky",
];

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
  settings: "doorsign.settings.v1",
  history: "doorsign.customHistory.v1",
  presetLabels: "doorsign.presetLabels.v1",
  lastDeviceId: "doorsign.lastDeviceId.v1",
  lastDeviceName: "doorsign.lastDeviceName.v1",
};

let bleDevice = null;
let bleServer = null;
let writeCharacteristic = null;

const $ = (id) => document.getElementById(id);

function readJson(key, fallback) {
  try {
    return JSON.parse(localStorage.getItem(key) || JSON.stringify(fallback));
  } catch {
    return fallback;
  }
}

function writeJson(key, value) {
  localStorage.setItem(key, JSON.stringify(value));
}

function loadSettings() {
  return { ...DEFAULTS, ...readJson(STORAGE_KEYS.settings, {}) };
}

function saveSettings(settings) {
  writeJson(STORAGE_KEYS.settings, settings);
}

function loadPresetLabels() {
  const labels = readJson(STORAGE_KEYS.presetLabels, DEFAULT_PRESET_LABELS);
  return DEFAULT_PRESET_LABELS.map((fallback, i) => labels[i] || fallback);
}

function savePresetLabels(labels) {
  writeJson(STORAGE_KEYS.presetLabels, labels.slice(0, 7));
}

function currentSettings() {
  return {
    devicePrefix: $("devicePrefixInput").value.trim() || DEFAULTS.devicePrefix,
    serviceUuid: $("serviceUuidInput").value.trim().toLowerCase() || DEFAULTS.serviceUuid,
    characteristicUuid: $("characteristicUuidInput").value.trim().toLowerCase() || DEFAULTS.characteristicUuid,
  };
}

function populateSettings() {
  const settings = loadSettings();
  $("devicePrefixInput").value = settings.devicePrefix;
  $("serviceUuidInput").value = settings.serviceUuid;
  $("characteristicUuidInput").value = settings.characteristicUuid;
}

function setStatus(connected, text) {
  const pill = $("statusPill");
  pill.className = `status ${connected ? "connected" : "disconnected"}`;
  pill.textContent = text || (connected ? "Connected" : "Disconnected");
}

function setAction(text) {
  $("lastAction").textContent = text;
}

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

function bluetoothAvailable() {
  return !!navigator.bluetooth;
}

async function connectWithDevice(device) {
  const settings = currentSettings();

  bleDevice = device;
  localStorage.setItem(STORAGE_KEYS.lastDeviceId, device.id || "");
  localStorage.setItem(STORAGE_KEYS.lastDeviceName, device.name || "ESP32-EINK-MSG");

  bleDevice.addEventListener("gattserverdisconnected", () => {
    writeCharacteristic = null;
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
  writeCharacteristic = await service.getCharacteristic(settings.characteristicUuid);

  setStatus(true, "Connected");
  setAction("Connected.");
  updateDeviceInfo();
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
  if (bleDevice?.gatt?.connected) {
    bleDevice.gatt.disconnect();
  }
  writeCharacteristic = null;
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

  if (!writeCharacteristic) {
    throw new Error("Not connected.");
  }

  setAction(`Sending: ${value}`);
  await writeCharacteristic.writeValue(new TextEncoder().encode(value));
  setAction("Sent ✓");
  flashButton(button);
}

function loadHistory() {
  return readJson(STORAGE_KEYS.history, []);
}

function saveHistory(history) {
  writeJson(STORAGE_KEYS.history, history.slice(0, 20));
}

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

function updatePreview() {
  $("messagePreview").textContent = buildMessage() || "(nothing yet)";
}

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

function renderPresetButtons() {
  const root = $("presetButtons");
  const labels = loadPresetLabels();
  root.innerHTML = "";

  labels.forEach((label, i) => {
    const num = String(i + 1);
    const btn = document.createElement("button");
    btn.className = "preset-button";
    btn.innerHTML = `Preset ${num}<span>${label}</span>`;
    btn.addEventListener("click", async () => {
      try {
        await writeValue(num, btn);
      } catch (err) {
        alert(`Could not send preset ${num}: ${err.message}`);
      }
    });
    root.appendChild(btn);
  });

  renderPresetLabelEditor();
}

function renderPresetLabelEditor() {
  const root = $("presetLabelEditor");
  const labels = loadPresetLabels();
  root.innerHTML = "";

  labels.forEach((label, i) => {
    const row = document.createElement("div");
    row.className = "label-editor-row";

    const slot = document.createElement("strong");
    slot.textContent = `Preset ${i + 1}`;

    const input = document.createElement("input");
    input.value = label;
    input.addEventListener("input", () => {
      const latest = loadPresetLabels();
      latest[i] = input.value.trim() || DEFAULT_PRESET_LABELS[i];
      savePresetLabels(latest);
      renderPresetButtons();
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
      try {
        await writeValue(message, sendBtn);
      } catch (err) {
        alert(`Could not send message: ${err.message}`);
      }
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
    presetLabels: loadPresetLabels(),
    history: loadHistory(),
  };
  $("backupText").value = JSON.stringify(payload, null, 2);
}

function importBackup() {
  let payload;
  try {
    payload = JSON.parse($("backupText").value);
  } catch {
    alert("That does not look like valid JSON.");
    return;
  }

  if (payload.settings) saveSettings({ ...DEFAULTS, ...payload.settings });
  if (Array.isArray(payload.presetLabels)) savePresetLabels(payload.presetLabels);
  if (Array.isArray(payload.history)) saveHistory(payload.history);

  populateSettings();
  renderPresetButtons();
  renderHistory();
  alert("Imported local app settings.");
}

function bindEvents() {
  $("connectBtn").addEventListener("click", () => connect().catch(err => alert(err.message)));
  $("reconnectBtn").addEventListener("click", () => reconnectLast().catch(err => alert(err.message)));
  $("disconnectBtn").addEventListener("click", disconnect);

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
    if (!message) {
      alert("Write a message first.");
      return;
    }

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

    if (!message) {
      alert("Write a message first.");
      return;
    }

    try {
      await writeValue(`SET${slot}:${message}`, event.currentTarget);
      addHistory(message);
    } catch (err) {
      alert(`Could not save preset: ${err.message}`);
    }
  });

  $("saveAndRecallBtn").addEventListener("click", async (event) => {
    const message = buildMessage();
    const slot = $("slotSelect").value;

    if (!message) {
      alert("Write a message first.");
      return;
    }

    try {
      await writeValue(`SET${slot}:${message}`, event.currentTarget);
      await new Promise(resolve => setTimeout(resolve, 250));
      await writeValue(slot, event.currentTarget);
      addHistory(message);
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
    } catch (err) {
      alert(`Could not reset presets: ${err.message}`);
    }
  });
}

function boot() {
  populateSettings();
  renderPresetButtons();
  renderTemplates();
  renderHistory();
  bindEvents();
  updatePreview();
  updateDeviceInfo();

  if (!bluetoothAvailable()) {
    setStatus(false, "No Web Bluetooth");
  }
}

boot();
