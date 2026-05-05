// Door Sign BLE Remote
// Static Web Bluetooth app for Bluefy / compatible browsers.

const DEFAULTS = {
  devicePrefix: "ESP32-EINK-MSG",
  serviceUuid: "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
  characteristicUuid: "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
};

const PRESET_LABELS = [
  ["1", "Available"],
  ["2", "Meeting"],
  ["3", "Do Not Disturb"],
  ["4", "Out"],
  ["5", "Soon"],
  ["6", "Teleworking"],
  ["7", "Cranky"],
];

const STORAGE_KEYS = {
  settings: "doorsign.settings.v1",
  history: "doorsign.customHistory.v1",
  lastDeviceId: "doorsign.lastDeviceId.v1",
  lastDeviceName: "doorsign.lastDeviceName.v1",
};

let bleDevice = null;
let bleServer = null;
let writeCharacteristic = null;

const $ = (id) => document.getElementById(id);

function loadSettings() {
  try {
    return { ...DEFAULTS, ...JSON.parse(localStorage.getItem(STORAGE_KEYS.settings) || "{}") };
  } catch {
    return { ...DEFAULTS };
  }
}

function saveSettings(settings) {
  localStorage.setItem(STORAGE_KEYS.settings, JSON.stringify(settings));
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
    updateDeviceInfo();
  });

  setStatus(false, "Connecting...");
  updateDeviceInfo();

  bleServer = await bleDevice.gatt.connect();
  const service = await bleServer.getPrimaryService(settings.serviceUuid);
  writeCharacteristic = await service.getCharacteristic(settings.characteristicUuid);

  setStatus(true, "Connected");
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
  updateDeviceInfo();
}

async function writeValue(value) {
  if (!writeCharacteristic) {
    await reconnectLast();
  }

  if (!writeCharacteristic) {
    throw new Error("Not connected.");
  }

  await writeCharacteristic.writeValue(new TextEncoder().encode(value));
}

function loadHistory() {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEYS.history) || "[]");
  } catch {
    return [];
  }
}

function saveHistory(history) {
  localStorage.setItem(STORAGE_KEYS.history, JSON.stringify(history.slice(0, 20)));
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
  root.innerHTML = "";

  PRESET_LABELS.forEach(([num, label]) => {
    const btn = document.createElement("button");
    btn.className = "preset-button";
    btn.innerHTML = `Preset ${num}<span>${label}</span>`;
    btn.addEventListener("click", async () => {
      try {
        await writeValue(num);
      } catch (err) {
        alert(`Could not send preset ${num}: ${err.message}`);
      }
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
        await writeValue(message);
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

  $("sendCustomBtn").addEventListener("click", async () => {
    const message = buildMessage();
    if (!message) {
      alert("Write a message first.");
      return;
    }

    try {
      await writeValue(message);
      addHistory(message);
    } catch (err) {
      alert(`Could not send message: ${err.message}`);
    }
  });

  $("savePresetBtn").addEventListener("click", async () => {
    const message = buildMessage();
    const slot = $("slotSelect").value;

    if (!message) {
      alert("Write a message first.");
      return;
    }

    try {
      await writeValue(`SET${slot}:${message}`);
      addHistory(message);
    } catch (err) {
      alert(`Could not save preset: ${err.message}`);
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

  $("resetPresetsBtn").addEventListener("click", async () => {
    if (!confirm("Send RESETPRESETS to the ESP32?")) return;
    try {
      await writeValue("RESETPRESETS");
    } catch (err) {
      alert(`Could not reset presets: ${err.message}`);
    }
  });
}

function boot() {
  populateSettings();
  renderPresetButtons();
  renderHistory();
  bindEvents();
  updatePreview();
  updateDeviceInfo();

  if (!bluetoothAvailable()) {
    setStatus(false, "No Web Bluetooth");
  }
}

boot();
