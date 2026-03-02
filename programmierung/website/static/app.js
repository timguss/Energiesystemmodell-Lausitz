// app.js — Frontend JavaScript for Central UI

let metaCache = {}; // Cache für Meta-Daten
let pendingRequests = new Set(); // Verhindert doppelte Requests
const FETCH_TIMEOUT = 12000; // 12 Sekunden Frontend-Timeout

// ============================================================================
// SELECTIVE POLLING STATE
// ============================================================================
const BOOT_WINDOW_MS = 10000; // 10 seconds of trying all ESPs on startup
const FIRST_BOOT_TIME = Date.now();

// true once an ESP has responded successfully at least once
const espConnectedHistory = { esp1: false, esp2: false, esp3: false, esp4: false };

// true when user manually clicks "Connect" for a suspended ESP
const espManualTry = { esp1: false, esp2: false, esp3: false, esp4: false };

// Returns true if we should poll the given ESP this cycle
function shouldPollEsp(device) {
  if (Date.now() - FIRST_BOOT_TIME < BOOT_WINDOW_MS) return true;
  if (espConnectedHistory[device]) return true;
  if (espManualTry[device]) return true;
  return false;
}

// Called from the Connect button — re-enables polling for a suspended ESP
function connectEsp(device) {
  espManualTry[device] = true;
  updateConnectButtonVisibility(device);
  refreshDeviceState(device);
}

// Shows/hides the Connect button based on current polling state
function updateConnectButtonVisibility(device) {
  const btn = document.getElementById('connect-btn-' + device);
  if (!btn) return;
  const isSuspended = !shouldPollEsp(device);
  btn.style.display = isSuspended ? 'inline-block' : 'none';
}

// Fetch with timeout to prevent hanging requests
function fetchWithTimeout(url, options = {}, timeout = FETCH_TIMEOUT) {
  const controller = new AbortController();
  const id = setTimeout(() => controller.abort(), timeout);
  return fetch(url, { ...options, signal: controller.signal })
    .finally(() => clearTimeout(id));
}

// View Control
let showAllDevices = false;
let deviceStatus = {
  host: false,
  esp1: false,
  esp2: false,
  esp3: false,
  esp4: false
};
let deviceFailCount = {
  host: 0,
  esp1: 0,
  esp2: 0,
  esp3: 0,
  esp4: 0
};
const MAX_FAILS = 3; // Gerät wird erst nach 3 Fehlern als offline markiert

function toggleViewMode() {
  const toggle = document.getElementById('viewToggle');
  showAllDevices = toggle.checked;
  updateVisibility();
}

function updateVisibility() {
  const cards = {
    'esp1': ['card_esp1_relays', 'card_rs232'],
    'esp2': ['card_esp2_relays'],
    'esp3': ['card_esp3'],
    'esp4': ['card_esp4_relays', 'card_esp4_sensors']
    // Temp card serves both, keep visible if any is online or showAll? 
    // For simplicity: unique temp cards or just keep always visible for now, or hide if both offline.
  };

  // Helper to set display
  const setDisplay = (id, show) => {
    const el = document.getElementById(id);
    if (el) el.style.display = show ? 'block' : 'none';
  };

  // ESP1
  const showEsp1 = showAllDevices || deviceStatus.esp1;
  cards.esp1.forEach(id => setDisplay(id, showEsp1));

  // ESP2
  const showEsp2 = showAllDevices || deviceStatus.esp2;
  cards.esp2.forEach(id => setDisplay(id, showEsp2));

  // ESP3
  const showEsp3 = showAllDevices || deviceStatus.esp3;
  cards.esp3.forEach(id => setDisplay(id, showEsp3));

  // ESP4
  const showEsp4 = showAllDevices || deviceStatus.esp4;
  cards.esp4.forEach(id => setDisplay(id, showEsp4));

  // Special case for Temp (mixed)
  const showTemp = showAllDevices || deviceStatus.esp1 || deviceStatus.esp2;
  setDisplay('card_temp', showTemp);
}

async function fetchMeta(device) {
  if (metaCache[device]) return metaCache[device]; // Aus Cache
  if (pendingRequests.has("meta-" + device)) return null; // Request läuft bereits

  pendingRequests.add("meta-" + device);
  try {
    let r = await fetchWithTimeout("/api/device_meta/" + device);
    if (!r.ok) throw r;
    let j = await r.json();
    if (j.error) throw j.error;
    let body = j.body || j;
    if (!j.offline) metaCache[device] = body; // Nur echte Daten cachen
    return body;
  } catch (e) {
    if (e.name !== 'AbortError') console.error("meta err", device, e);
    return null;
  } finally {
    pendingRequests.delete("meta-" + device);
  }
}

async function fetchState(device) {
  if (pendingRequests.has('state-' + device)) return null; // Request läuft bereits

  pendingRequests.add('state-' + device);
  try {
    let r = await fetchWithTimeout('/api/device_state/' + device);
    if (!r.ok) throw r;
    let j = await r.json();
    if (j.error) throw j.error;

    // Check if device is marked as offline by backend
    let isOffline = !!j.offline;

    if (isOffline) {
      deviceFailCount[device]++;
    } else {
      deviceFailCount[device] = 0; // Reset counter on success
      // Lock in permanent polling once connected
      if (!espConnectedHistory[device]) {
        espConnectedHistory[device] = true;
        espManualTry[device] = false;
        updateConnectButtonVisibility(device);
      }
    }

    // Only update status if we reach MAX_FAILS or go from offline to online
    if (deviceFailCount[device] >= MAX_FAILS) {
      updateStatusDisplay(device, false);
      updateConnectButtonVisibility(device); // may need to reveal Connect button
    } else if (deviceFailCount[device] === 0) {
      updateStatusDisplay(device, true);
    }

    let body = j.body || j;
    if (isOffline) body.offline = true; // Inject flag
    return body;
  } catch (e) {
    if (e.name !== 'AbortError') console.error('state err', device, e);

    deviceFailCount[device]++;
    if (deviceFailCount[device] >= MAX_FAILS) {
      updateStatusDisplay(device, false);
    }

    return null;
  } finally {
    pendingRequests.delete('state-' + device);
  }
}

function updateStatusDisplay(device, isOnline) {
  if (deviceStatus[device] !== isOnline) {
    deviceStatus[device] = isOnline;
    // Also update visibility when status changes
    updateVisibility();
  }

  const el = document.getElementById('status-' + device);
  if (el) {
    el.className = 'status-indicator ' + (isOnline ? 'online' : 'offline');
    el.title = isOnline ? 'Verbunden' : 'Nicht erreichbar';
  }
}

async function checkHostStatus() {
  try {
    let r = await fetchWithTimeout('/api/debug/host');
    if (r.ok) {
      let j = await r.json();
      if (j.host_reachable) {
        deviceFailCount.host = 0;
        updateStatusDisplay('host', true);
      } else {
        deviceFailCount.host++;
      }
    } else {
      deviceFailCount.host++;
    }

    if (deviceFailCount.host >= MAX_FAILS) {
      updateStatusDisplay('host', false);
    }
  } catch (e) {
    deviceFailCount.host++;
    if (deviceFailCount.host >= MAX_FAILS) {
      updateStatusDisplay('host', false);
    }
  }
}

function mkRelayRow(global_idx, name, state) {
  const row = document.createElement('div');
  row.className = 'relay-row';

  const label = document.createElement('div');
  label.className = 'label';
  label.innerHTML = '<strong>' + name + '</strong><div class="small">#' + global_idx + '</div>';

  const sw = document.createElement('label');
  sw.className = 'switch';

  const input = document.createElement('input');
  input.type = 'checkbox';
  input.id = 'g' + global_idx;
  input.checked = (state == 1);
  input.onchange = () => toggleRelay(global_idx, input.checked ? 1 : 0, input);

  const span = document.createElement('span');
  span.className = 'slider';

  sw.appendChild(input);
  sw.appendChild(span);
  row.appendChild(label);
  row.appendChild(sw);

  return row;
}

async function initDeviceRelays(device, existingState = null) {
  const containerId = (device === 'esp3') ? 'esp3_relays' : device + '_relays';
  const container = document.getElementById(containerId);
  if (!container) return;

  // Wenn bereits Relais da sind, nichts tun (außer Refresh via Checkboxen später)
  if (container.querySelector('.relay-row')) return;

  const state = existingState || await fetchState(device);
  const meta = await fetchMeta(device);

  if (!meta || !state || state.offline) {
    if (container.innerHTML === '' || container.innerHTML === 'lade...') {
      container.innerHTML = '<div class="error">Warte auf Verbindung...</div>';
    }
    return;
  }

  container.innerHTML = '';

  let startIdx;
  if (device === 'esp1') startIdx = 1;
  else if (device === 'esp2') startIdx = 9;
  else if (device === 'esp4') startIdx = 13;
  else startIdx = 18;

  const count = meta.count || meta.relayCount || 0;
  const names = meta.names || [];

  for (let i = 0; i < count; i++) {
    const name = names[i] || ('Relay ' + (i + 1));
    const globalIdx = startIdx + i;

    let st = 0;
    if (device === 'esp4' || device === 'esp3') {
      st = (state.relays && state.relays[i] !== undefined) ? state.relays[i] : 0;
    } else {
      st = (state['r' + i] !== undefined) ? state['r' + i] : 0;
    }

    container.appendChild(mkRelayRow(globalIdx, name, st));
  }
}

async function buildRelays() {
  // Versuche alle Geräte parallel zu initialisieren (ESP4 leicht priorisiert)
  await Promise.all([
    initDeviceRelays('esp4'),
    initDeviceRelays('esp1'),
    initDeviceRelays('esp2'),
    initDeviceRelays('esp3')
  ]);
}

async function toggleRelay(global_idx, val, inputElement) {
  // Prevent duplicate requests
  const requestKey = 'relay-' + global_idx;
  if (pendingRequests.has(requestKey)) {
    console.log('Relay request already pending for', global_idx);
    inputElement.checked = !inputElement.checked; // Revert
    return;
  }

  // Sofort visuelles Feedback
  inputElement.disabled = true;
  pendingRequests.add(requestKey);

  try {
    let r = await fetchWithTimeout('/api/relay/set', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ global_idx, val })
    });
    if (!r.ok) throw new Error('Request failed');

    // Nach 100ms State neu laden (gibt Zeit für ESP zu reagieren)
    setTimeout(async () => {
      inputElement.disabled = false;
      // Nur den betroffenen Device neu laden
      let device;
      if (global_idx <= 8) device = 'esp1';
      else if (global_idx <= 12) device = 'esp2';
      else if (global_idx <= 17) device = 'esp4';
      else device = 'esp3';
      await refreshDeviceState(device);
    }, 100);
  } catch (e) {
    alert('Fehler beim Schalten: ' + e);
    inputElement.checked = !inputElement.checked; // Zurücksetzen
    inputElement.disabled = false;
  } finally {
    pendingRequests.delete(requestKey);
  }
}

// Nur einen Device-State neu laden (für toggleRelay / Szenarien)
async function refreshDeviceState(device) {
  const state = await fetchState(device);
  if (!state) return;
  refreshRelayCheckboxes(device, state);
  if (device === 'esp3') updateEsp3LegacyUI(state);
}

// ============================================================================
// SZENARIEN - Buttons für jedes Szenario mit States
// ============================================================================

async function loadScenarios() {
  try {
    let r = await fetch('/api/scenarios');
    if (!r.ok) throw r;
    let j = await r.json();

    const container = document.getElementById('scenarios');
    container.innerHTML = '';

    if (j.scenarios && Object.keys(j.scenarios).length > 0) {
      // Für jedes Szenario eine Gruppe erstellen
      for (const [scenarioKey, scenarioData] of Object.entries(j.scenarios)) {
        const group = document.createElement('div');
        group.className = 'scenario-group';

        const title = document.createElement('div');
        title.className = 'scenario-title';
        title.textContent = scenarioData.name;
        group.appendChild(title);

        const btnContainer = document.createElement('div');
        btnContainer.className = 'scenario-buttons';

        // Button für jeden State
        scenarioData.states.forEach(stateInfo => {
          const btn = document.createElement('button');
          btn.className = 'scenario-btn';
          btn.textContent = stateInfo.name;
          btn.title = stateInfo.description || '';
          btn.onclick = () => executeScenario(scenarioKey, stateInfo.id, stateInfo.name, btn);
          btnContainer.appendChild(btn);
        });

        group.appendChild(btnContainer);
        container.appendChild(group);
      }
    } else {
      container.innerHTML = '<div class="small">Keine Szenarien verfügbar</div>';
    }
  } catch (e) {
    console.error('scenarios err', e);
    document.getElementById('scenarios').innerHTML = '<div class="error">Fehler beim Laden</div>';
  }
}

async function executeScenario(scenarioName, state, stateName, buttonElement) {
  buttonElement.disabled = true;
  const originalText = buttonElement.textContent;
  buttonElement.textContent = stateName + ' ...';

  try {
    let r = await fetch('/api/scenario/execute', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ scenario: scenarioName, state: state })
    });

    if (!r.ok) throw new Error('Request failed');
    let j = await r.json();

    if (j.success) {
      buttonElement.textContent = stateName + ' ✓';

      // Kontinuierlich States aktualisieren (ESP führt Szenario aus)
      let refreshCount = 0;
      const maxRefresh = 20; // Max 20 Sekunden Updates
      const refreshInterval = setInterval(async () => {
        await Promise.all([
          refreshDeviceState('esp1'),
          refreshDeviceState('esp2')
        ]);
        refreshCount++;
        if (refreshCount >= maxRefresh) {
          clearInterval(refreshInterval);
        }
      }, 1000); // Jede Sekunde updaten

      // Button nach 2 Sekunden wieder aktivieren
      setTimeout(() => {
        buttonElement.textContent = originalText;
        buttonElement.disabled = false;
      }, 2000);

    } else {
      buttonElement.textContent = stateName + ' ✗';
      console.error('Scenario error:', j);
      alert('Fehler beim Ausführen:\n' + (j.error || JSON.stringify(j)));
      setTimeout(() => {
        buttonElement.textContent = originalText;
        buttonElement.disabled = false;
      }, 2000);
    }
  } catch (e) {
    alert('Fehler: ' + e);
    buttonElement.textContent = originalText;
    buttonElement.disabled = false;
  }
}

// ============================================================================
// ESP3
// ============================================================================



function updateEsp3LegacyUI(state3) {
  if (!state3) return;
  const elWind = document.getElementById('windState');
  if (elWind) elWind.textContent = state3.running ? 'AN' : 'AUS';

  const elPwm = document.getElementById('pwmVal');
  if (elPwm) elPwm.textContent = state3.pwm;

  const slider = document.getElementById('pwmSlider');
  if (slider && document.activeElement !== slider) {
    slider.value = state3.pwm;
  }
  const dirBox = document.getElementById('trainDir');
  if (dirBox) {
    // If state3.forward is true, 'Rückwärts' checkbox should be UNCHECKED
    dirBox.checked = !state3.forward;
  }
}

async function setWind(val) {
  await fetch('/api/esp3/set_wind', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ val })
  });
  setTimeout(() => refreshDeviceState('esp3'), 100);
}

async function setPwm(v) {
  const isBackward = document.getElementById('trainDir').checked;
  const pwm = parseInt(v);
  document.getElementById('pwmVal').textContent = v;

  await fetch('/api/esp3/set_pwm', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      pwm: pwm,
      dir: isBackward ? 0 : 1 // 1=Forward, 0=Backward
    })
  });
}

async function stopTrain() {
  await fetch('/api/esp3/set_pwm', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ pwm: 0 })
  });
  document.getElementById('pwmVal').textContent = '0';
  document.getElementById('pwmSlider').value = 0;
}

// ============================================================================
// RS232
// ============================================================================

function updateRsCommand(percent) {
  // Konvertiere Prozent (0-100) zu Wert (0-32000)
  const value = Math.round((percent / 100) * 32000);

  // Konvertiere zu Hex (4 Zeichen, uppercase)
  const hexValue = value.toString(16).toUpperCase().padStart(4, '0');

  // Aktualisiere Anzeigen
  document.getElementById('rs_percent').textContent = percent;
  document.getElementById('rs_hex_value').textContent = hexValue;

  // Generiere Command: :0603010121[HEX]\r\n
  // Format: :06 (Länge) 03 (Node) 01 (Befehl) 01 (Prozess) 21 (Parameter) [HEX]
  const cmd = ':0603010121' + hexValue;
  document.getElementById('rs_cmd').value = cmd;
}

async function sendRs() {
  let cmd = document.getElementById('rs_cmd').value.trim();
  let timeout = parseInt(document.getElementById('rs_timeout').value) || 500;
  if (!cmd) {
    alert('Bitte Befehl');
    return;
  }

  document.getElementById('rs_reply').textContent = 'sende...';
  try {
    let r = await fetch('/api/rs232', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd, timeout })
    });
    let j = await r.json();
    let body = j.body || j;
    if (typeof body === 'object') body = JSON.stringify(body, null, 2);
    document.getElementById('rs_reply').textContent = (body || JSON.stringify(j));
  } catch (e) {
    document.getElementById('rs_reply').textContent = 'Fehler: ' + e;
  }
}

// ============================================================================
// TEMPERATUR
// ============================================================================

// Update temp display from already-fetched state (no extra HTTP request)
function updateTemps(device, state) {
  if (!state) return;
  const suffix = device === 'esp1' ? '1' : '2';
  if (state.temp !== undefined) {
    const el = document.getElementById('temp' + suffix);
    if (el) el.textContent = (state.temp === null ? '--' : Number(state.temp).toFixed(2));
  }
  if (state.rntc !== undefined) {
    const el = document.getElementById('rntc' + suffix);
    if (el) el.textContent = (state.rntc === null ? '--' : Number(state.rntc).toFixed(0));
  }
}

// Update ESP4 sensors from already-fetched state (no extra HTTP request)
function updateEsp4Sensors(state) {
  if (!state) return;
  if (state.sensors) {
    state.sensors.forEach((sensor, i) => {
      const pEl = document.getElementById('esp4_p' + i);
      const iEl = document.getElementById('esp4_i' + i);
      if (pEl) pEl.textContent = Number(sensor.pressure).toFixed(2);
      if (iEl) iEl.textContent = Number(sensor.current).toFixed(2);
    });
  }
  if (state.flow !== undefined) {
    const fEl = document.getElementById('esp4_flow');
    if (fEl) fEl.textContent = Number(state.flow).toFixed(2);
  }
}

// ============================================================================
// POLLING
// ============================================================================

// Each device is fetched ONCE per cycle. Results are reused for relays, temps, sensors.
async function refreshAll() {
  await checkHostStatus();
  ['esp1', 'esp2', 'esp3', 'esp4'].forEach(d => updateConnectButtonVisibility(d));
  if (shouldPollEsp('esp4')) {
    const state4 = await fetchState('esp4');
    if (state4) { await initDeviceRelays('esp4', state4); refreshRelayCheckboxes('esp4', state4); updateEsp4Sensors(state4); }
  }
  if (shouldPollEsp('esp1')) {
    const state1 = await fetchState('esp1');
    if (state1) { await initDeviceRelays('esp1', state1); refreshRelayCheckboxes('esp1', state1); updateTemps('esp1', state1); }
  }
  if (shouldPollEsp('esp2')) {
    const state2 = await fetchState('esp2');
    if (state2) { await initDeviceRelays('esp2', state2); refreshRelayCheckboxes('esp2', state2); updateTemps('esp2', state2); }
  }
  if (shouldPollEsp('esp3')) {
    const state3 = await fetchState('esp3');
    if (state3) { await initDeviceRelays('esp3', state3); refreshRelayCheckboxes('esp3', state3); updateEsp3LegacyUI(state3); }
  }
}

// Update relay checkboxes from already-fetched state data
function refreshRelayCheckboxes(device, state) {
  const meta = metaCache[device];
  if (!meta || !state) return;

  let startIdx;
  if (device === 'esp1') startIdx = 1;
  else if (device === 'esp2') startIdx = 9;
  else if (device === 'esp4') startIdx = 13;
  else startIdx = 18; // esp3

  const count = meta.count || meta.relayCount || 0;
  for (let i = 0; i < count; i++) {
    const globalIdx = startIdx + i;
    const checkbox = document.getElementById('g' + globalIdx);
    if (checkbox) {
      const newState = (device === 'esp4' || device === 'esp3')
        ? (state.relays ? state.relays[i] === 1 : false)
        : state['r' + i] === 1;
      if (checkbox.checked !== newState) checkbox.checked = newState;
    }
  }
}

// setTimeout-based polling prevents overlapping cycles
const POLL_INTERVAL = 3000;
async function pollLoop() {
  try {
    await refreshAll();
  } catch (e) {
    console.error('Poll error:', e);
  }
  setTimeout(pollLoop, POLL_INTERVAL);
}

// ============================================================================
// INIT
// ============================================================================

// Initial: Meta laden und Relays bauen
buildRelays();
loadScenarios();
updateVisibility(); // Initial visibility check

// Start non-overlapping poll loop
setTimeout(pollLoop, POLL_INTERVAL);
