// app.js — Frontend JavaScript for Central UI

let metaCache = {}; // Cache für Meta-Daten
let pendingRequests = new Set(); // Verhindert doppelte Requests
const FETCH_TIMEOUT = 12000; // 12 Sekunden Frontend-Timeout

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
  esp4: false,

};

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
    metaCache[device] = body; // Cachen
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
    updateStatusDisplay(device, !isOffline);

    let body = j.body || j;
    if (isOffline) body.offline = true; // Inject flag
    return body;
  } catch (e) {
    if (e.name !== 'AbortError') console.error('state err', device, e);
    updateStatusDisplay(device, false);
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
      updateStatusDisplay('host', j.host_reachable);
    } else {
      updateStatusDisplay('host', false);
    }
  } catch (e) {
    updateStatusDisplay('host', false);
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

async function buildRelays() {
  // esp1
  const meta1 = await fetchMeta('esp1');
  const state1 = await fetchState('esp1');
  const container1 = document.getElementById('esp1_relays');
  container1.innerHTML = '';

  if (meta1 && meta1.names && state1) {
    for (let i = 0; i < meta1.count; i++) {
      const name = meta1.names[i] || ('Relay ' + (i + 1));
      const st = (state1['r' + i] !== undefined) ? state1['r' + i] : 0;
      container1.appendChild(mkRelayRow(i + 1, name, st));
    }
  } else {
    container1.innerHTML = '<div class="error">keine daten</div>';
  }

  // esp2
  const meta2 = await fetchMeta('esp2');
  const state2 = await fetchState('esp2');
  const container2 = document.getElementById('esp2_relays');
  container2.innerHTML = '';

  if (meta2 && meta2.names && state2) {
    for (let i = 0; i < meta2.count; i++) {
      const name = meta2.names[i] || ('Relay ' + (i + 1));
      const global_idx = 9 + i;
      const st = (state2['r' + i] !== undefined) ? state2['r' + i] : 0;
      container2.appendChild(mkRelayRow(global_idx, name, st));
    }
  } else {
    container2.innerHTML = '<div class="error">keine daten</div>';
  }

  //esp4
  const meta4 = await fetchMeta('esp4');
  const state4 = await fetchState('esp4');
  const container4 = document.getElementById('esp4_relays');
  container4.innerHTML = '';

  if (meta4 && meta4.names && state4) {
    for (let i = 0; i < meta4.count; i++) {
      const name = meta4.names[i] || ('Relay ' + (i + 1));
      const global_idx = 13 + i;
      const st = state4.relays ? state4.relays[i] : 0;
      container4.appendChild(mkRelayRow(global_idx, name, st));
    }
  }
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
      else device = 'esp4';
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

async function refreshEsp3() {
  try {
    let r = await fetchWithTimeout('/api/esp3/state');
    if (!r.ok) throw r;
    let j = await r.json();

    let isOffline = !!j.offline;
    updateStatusDisplay('esp3', !isOffline);

    let body = j.body || j;
    document.getElementById('windState').textContent = body.running ? 'AN' : 'AUS';
    document.getElementById('pwmVal').textContent = body.pwm;
    // Update slider only if not dragging? For now just update.
    const slider = document.getElementById('pwmSlider');
    if (slider && document.activeElement !== slider) {
      slider.value = body.pwm;
    }
  } catch (e) {
    console.error('esp3 err', e);
    updateStatusDisplay('esp3', false);
  }
}

async function setWind(val) {
  await fetch('/api/esp3/set_wind', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ val })
  });
  setTimeout(refreshEsp3, 100);
}

async function setPwm(v) {
  document.getElementById('pwmVal').textContent = v;
  await fetch('/api/esp3/set_pwm', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ pwm: parseInt(v) })
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
  if (!state || !state.sensors) return;
  state.sensors.forEach((sensor, i) => {
    const pEl = document.getElementById('esp4_p' + i);
    const iEl = document.getElementById('esp4_i' + i);
    if (pEl) pEl.textContent = Number(sensor.pressure).toFixed(2);
    if (iEl) iEl.textContent = Number(sensor.current).toFixed(2);
  });
}

// ============================================================================
// POLLING
// ============================================================================

// Each device is fetched ONCE per cycle. Results are reused for relays, temps, sensors.
// Promise.allSettled ensures one slow device doesn't block others.
async function refreshAll() {
  // Fetch all device states independently, each can fail without blocking others
  const [hostResult, esp1Result, esp2Result, esp3Result, esp4Result] =
    await Promise.allSettled([
      checkHostStatus(),
      fetchState('esp1'),
      fetchState('esp2'),
      refreshEsp3(),
      fetchState('esp4'),
    ]);

  // ESP1: update relays + temperature from single fetch
  const state1 = esp1Result.status === 'fulfilled' ? esp1Result.value : null;
  if (state1) {
    refreshRelayCheckboxes('esp1', state1);
    updateTemps('esp1', state1);
  }

  // ESP2: update relays + temperature from single fetch
  const state2 = esp2Result.status === 'fulfilled' ? esp2Result.value : null;
  if (state2) {
    refreshRelayCheckboxes('esp2', state2);
    updateTemps('esp2', state2);
  }

  // ESP4: update relays + sensors from single fetch
  const state4 = esp4Result.status === 'fulfilled' ? esp4Result.value : null;
  if (state4) {
    refreshRelayCheckboxes('esp4', state4);
    updateEsp4Sensors(state4);
  }
}

// Update relay checkboxes from already-fetched state data
function refreshRelayCheckboxes(device, state) {
  const meta = metaCache[device];
  if (!meta || !state) return;

  let startIdx;
  if (device === 'esp1') startIdx = 1;
  else if (device === 'esp2') startIdx = 9;
  else startIdx = 13; // esp4

  const count = meta.count || meta.relayCount || 0;
  for (let i = 0; i < count; i++) {
    const globalIdx = startIdx + i;
    const checkbox = document.getElementById('g' + globalIdx);
    if (checkbox) {
      const newState = device === 'esp4'
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