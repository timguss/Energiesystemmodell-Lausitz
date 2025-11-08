// app.js — Frontend JavaScript for Central UI

let metaCache = {}; // Cache für Meta-Daten
let pendingRequests = new Set(); // Verhindert doppelte Requests

async function fetchMeta(device) {
  if (metaCache[device]) return metaCache[device]; // Aus Cache
  if (pendingRequests.has("meta-" + device)) return null; // Request läuft bereits
  
  pendingRequests.add("meta-" + device);
  try {
    let r = await fetch("/api/device_meta/" + device);
    if (!r.ok) throw r;
    let j = await r.json(); 
    if (j.error) throw j.error;
    let body = j.body || j;
    metaCache[device] = body; // Cachen
    return body;
  } catch (e) { 
    console.error("meta err", device, e);
    return null; 
  } finally {
    pendingRequests.delete("meta-" + device);
  }
}

async function fetchState(device) {
  if (pendingRequests.has('state-' + device)) return null; // Request läuft bereits
  
  pendingRequests.add('state-' + device);
  try {
    let r = await fetch('/api/device_state/' + device);
    if (!r.ok) throw r;
    let j = await r.json(); 
    if (j.error) throw j.error;
    return j.body || j;
  } catch (e) { 
    console.error('state err', device, e); 
    return null; 
  } finally {
    pendingRequests.delete('state-' + device);
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
}

async function toggleRelay(global_idx, val, inputElement) {
  // Sofort visuelles Feedback
  inputElement.disabled = true;
  
  try {
    let r = await fetch('/api/relay/set', {
      method: 'POST', 
      headers: {'Content-Type': 'application/json'}, 
      body: JSON.stringify({global_idx, val})
    });
    if (!r.ok) throw new Error('Request failed');
    
    // Nach 100ms State neu laden (gibt Zeit für ESP zu reagieren)
    setTimeout(async () => {
      inputElement.disabled = false;
      // Nur den betroffenen Device neu laden
      const device = global_idx <= 8 ? 'esp1' : 'esp2';
      await refreshDeviceState(device);
    }, 100);
  } catch (e) { 
    alert('Fehler beim Schalten: ' + e); 
    inputElement.checked = !inputElement.checked; // Zurücksetzen
    inputElement.disabled = false;
  }
}

// Nur einen Device-State neu laden
async function refreshDeviceState(device) {
  const state = await fetchState(device);
  if (!state) return;
  
  const meta = metaCache[device];
  if (!meta) return;
  
  // Update nur die Checkboxen
  const startIdx = device === 'esp1' ? 1 : 9;
  for (let i = 0; i < meta.count; i++) {
    const globalIdx = startIdx + i;
    const checkbox = document.getElementById('g' + globalIdx);
    if (checkbox) {
      const newState = state['r' + i] === 1;
      if (checkbox.checked !== newState) checkbox.checked = newState;
    }
  }
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
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({scenario: scenarioName, state: state})
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
    let r = await fetch('/api/esp3/state'); 
    if (!r.ok) throw r;
    let j = await r.json();
    let body = j.body || j;
    document.getElementById('windState').textContent = body.running ? 'AN' : 'AUS';
    document.getElementById('pwmVal').textContent = body.pwm;
    document.getElementById('pwmSlider').value = body.pwm;
  } catch (e) {
    console.error('esp3 err', e);
  }
}

async function setWind(val) { 
  await fetch('/api/esp3/set_wind', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({val})
  }); 
  setTimeout(refreshEsp3, 100);
}

async function setPwm(v) { 
  document.getElementById('pwmVal').textContent = v; 
  await fetch('/api/esp3/set_pwm', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({pwm: parseInt(v)})
  }); 
}

async function stopTrain() { 
  await fetch('/api/esp3/set_pwm', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({pwm: 0})
  }); 
  document.getElementById('pwmVal').textContent = '0'; 
  document.getElementById('pwmSlider').value = 0; 
}

// ============================================================================
// RS232
// ============================================================================

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
      headers: {'Content-Type': 'application/json'}, 
      body: JSON.stringify({cmd, timeout})
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

async function refreshTemps() {
  // ESP1 Temperatur
  try {
    let r = await fetch('/api/device_state/esp1'); 
    if (!r.ok) return;
    let j = await r.json(); 
    let body = j.body || j;
    if (body.temp !== undefined) {
      document.getElementById('temp1').textContent = 
        (body.temp === null ? '--' : Number(body.temp).toFixed(2));
    }
    if (body.rntc !== undefined) {
      document.getElementById('rntc1').textContent = 
        (body.rntc === null ? '--' : Number(body.rntc).toFixed(0));
    }
  } catch (e) {
    console.error('esp1 temp err', e);
  }
  
  // ESP2 Temperatur
  try {
    let r = await fetch('/api/device_state/esp2'); 
    if (!r.ok) return;
    let j = await r.json(); 
    let body = j.body || j;
    if (body.temp !== undefined) {
      document.getElementById('temp2').textContent = 
        (body.temp === null ? '--' : Number(body.temp).toFixed(2));
    }
    if (body.rntc !== undefined) {
      document.getElementById('rntc2').textContent = 
        (body.rntc === null ? '--' : Number(body.rntc).toFixed(0));
    }
  } catch (e) {
    console.error('esp2 temp err', e);
  }
}

// ============================================================================
// POLLING
// ============================================================================

// Optimiertes Polling: nur States, nicht Meta
async function refreshAll() {
  await Promise.all([
    refreshDeviceState('esp1'),
    refreshDeviceState('esp2'),
    refreshEsp3(),
    refreshTemps()
  ]);
}

// ============================================================================
// INIT
// ============================================================================

// Initial: Meta laden und Relays bauen
buildRelays();
loadScenarios();

// Danach nur noch States updaten (alle 3 Sekunden)
setInterval(refreshAll, 3000);