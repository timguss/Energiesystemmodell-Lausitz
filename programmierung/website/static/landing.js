// Landing Page JavaScript - Interactive Effects

// ============================================================================
// PARTICLE BACKGROUND
// ============================================================================

function createParticles() {
    const particlesContainer = document.getElementById('particles');
    if (!particlesContainer) return;
    
    const particleCount = 50;
    
    for (let i = 0; i < particleCount; i++) {
        const particle = document.createElement('div');
        particle.className = 'particle';
        
        // Random position
        const x = Math.random() * 100;
        const y = Math.random() * 100;
        
        // Random size
        const size = Math.random() * 3 + 1;
        
        // Random animation duration
        const duration = Math.random() * 20 + 10;
        
        // Random delay
        const delay = Math.random() * 5;
        
        particle.style.cssText = `
            position: absolute;
            left: ${x}%;
            top: ${y}%;
            width: ${size}px;
            height: ${size}px;
            background: radial-gradient(circle, rgba(102, 126, 234, 0.8) 0%, transparent 70%);
            border-radius: 50%;
            animation: float ${duration}s ease-in-out ${delay}s infinite;
            pointer-events: none;
        `;
        
        particlesContainer.appendChild(particle);
    }
    
    // Add CSS animation
    const style = document.createElement('style');
    style.textContent = `
        @keyframes float {
            0%, 100% {
                transform: translate(0, 0) scale(1);
                opacity: 0.3;
            }
            25% {
                transform: translate(10px, -20px) scale(1.1);
                opacity: 0.6;
            }
            50% {
                transform: translate(-10px, -40px) scale(0.9);
                opacity: 0.4;
            }
            75% {
                transform: translate(15px, -20px) scale(1.05);
                opacity: 0.5;
            }
        }
    `;
    document.head.appendChild(style);
}

// ============================================================================
// SMOOTH SCROLLING
// ============================================================================

function initSmoothScroll() {
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function (e) {
            const href = this.getAttribute('href');
            if (href === '#') return;
            
            e.preventDefault();
            const target = document.querySelector(href);
            
            if (target) {
                target.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
            }
        });
    });
}

// ============================================================================
// SCROLL ANIMATIONS
// ============================================================================

function initScrollAnimations() {
    const observerOptions = {
        threshold: 0.1,
        rootMargin: '0px 0px -100px 0px'
    };
    
    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                entry.target.style.opacity = '1';
                entry.target.style.transform = 'translateY(0)';
            }
        });
    }, observerOptions);
    
    // Observe feature cards
    document.querySelectorAll('.feature-card').forEach((card, index) => {
        card.style.opacity = '0';
        card.style.transform = 'translateY(30px)';
        card.style.transition = `all 0.6s ease ${index * 0.1}s`;
        observer.observe(card);
    });
    
    // Observe architecture section
    const archElements = document.querySelectorAll('.architecture-text, .architecture-visual');
    archElements.forEach((el, index) => {
        el.style.opacity = '0';
        el.style.transform = 'translateY(30px)';
        el.style.transition = `all 0.8s ease ${index * 0.2}s`;
        observer.observe(el);
    });
}

// ============================================================================
// NAVBAR SCROLL EFFECT
// ============================================================================

function initNavbarScroll() {
    const navbar = document.querySelector('.navbar');
    if (!navbar) return;
    
    let lastScroll = 0;
    
    window.addEventListener('scroll', () => {
        const currentScroll = window.pageYOffset;
        
        if (currentScroll > 100) {
            navbar.style.background = 'rgba(10, 14, 39, 0.9)';
            navbar.style.backdropFilter = 'blur(10px)';
            navbar.style.borderBottom = '1px solid rgba(255, 255, 255, 0.1)';
        } else {
            navbar.style.background = 'transparent';
            navbar.style.backdropFilter = 'none';
            navbar.style.borderBottom = 'none';
        }
        
        lastScroll = currentScroll;
    });
}

// ============================================================================
// INTERACTIVE DIAGRAM
// ============================================================================

function initDiagram() {
    const nodes = document.querySelectorAll('.diagram-node');
    
    nodes.forEach(node => {
        node.addEventListener('mouseenter', () => {
            // Add glow effect
            node.style.boxShadow = '0 0 30px rgba(102, 126, 234, 0.6)';
        });
        
        node.addEventListener('mouseleave', () => {
            node.style.boxShadow = 'none';
        });
    });
}

// ============================================================================
// STATS COUNTER ANIMATION
// ============================================================================

function animateStats() {
    const stats = document.querySelectorAll('.stat-value');
    
    const observerOptions = {
        threshold: 0.5
    };
    
    const observer = new IntersectionObserver((entries) => {
        entries.forEach(entry => {
            if (entry.isIntersecting && !entry.target.classList.contains('counted')) {
                entry.target.classList.add('counted');
                
                const text = entry.target.textContent;
                if (text === 'Live') return; // Skip non-numeric
                
                const target = parseInt(text.replace('+', ''));
                const hasPlus = text.includes('+');
                let current = 0;
                const increment = target / 30;
                const duration = 1000;
                const stepTime = duration / 30;
                
                const timer = setInterval(() => {
                    current += increment;
                    if (current >= target) {
                        entry.target.textContent = target + (hasPlus ? '+' : '');
                        clearInterval(timer);
                    } else {
                        entry.target.textContent = Math.floor(current) + (hasPlus ? '+' : '');
                    }
                }, stepTime);
            }
        });
    }, observerOptions);
    
    stats.forEach(stat => observer.observe(stat));
}

// ============================================================================
// MOUSE PARALLAX EFFECT
// ============================================================================

function initParallax() {
    const hero = document.querySelector('.hero');
    if (!hero) return;
    
    hero.addEventListener('mousemove', (e) => {
        const { clientX, clientY } = e;
        const { innerWidth, innerHeight } = window;
        
        const xPos = (clientX / innerWidth - 0.5) * 20;
        const yPos = (clientY / innerHeight - 0.5) * 20;
        
        const particles = document.getElementById('particles');
        if (particles) {
            particles.style.transform = `translate(${xPos}px, ${yPos}px)`;
        }
    });
}

// ============================================================================
// INITIALIZATION
// ============================================================================

document.addEventListener('DOMContentLoaded', () => {
    createParticles();
    initSmoothScroll();
    initScrollAnimations();
    initNavbarScroll();
    initDiagram();
    animateStats();
    initParallax();
    
    // Initialize control systems
    loadScenarios();
    loadRelays();
    initWindTrainControls();
    startStatusPolling();
    
    console.log('🚀 Energiesystem Lausitz - Landing Page loaded');
});

// ============================================================================
// CONTROL SYSTEMS - API INTEGRATION
// ============================================================================

let metaCache = {};
let pendingRequests = new Set();

// Notification System
function showNotification(message, type = 'info') {
    const notification = document.createElement('div');
    notification.className = `notification-toast notification-${type}`;
    notification.textContent = message;
    document.body.appendChild(notification);
    
    setTimeout(() => {
        notification.style.animation = 'slideInRight 0.3s ease reverse';
        setTimeout(() => notification.remove(), 300);
    }, 3000);
}

// ============================================================================
// SCENARIO CONTROLS
// ============================================================================

async function loadScenarios() {
    const container = document.getElementById('scenarios-container');
    if (!container) return;
    
    try {
        const response = await fetch('/api/scenarios');
        if (!response.ok) throw new Error('Failed to load scenarios');
        
        const data = await response.json();
        container.innerHTML = '';
        
        if (data.scenarios && Object.keys(data.scenarios).length > 0) {
            for (const [key, scenario] of Object.entries(data.scenarios)) {
                const card = createScenarioCard(key, scenario);
                container.appendChild(card);
            }
        } else {
            container.innerHTML = '<div class="loading-spinner">Keine Szenarien verfügbar</div>';
        }
    } catch (error) {
        console.error('Error loading scenarios:', error);
        container.innerHTML = '<div class="loading-spinner">Fehler beim Laden</div>';
    }
}

function createScenarioCard(key, scenario) {
    const card = document.createElement('div');
    card.className = 'scenario-card';
    
    const header = document.createElement('div');
    header.className = 'scenario-card-header';
    
    const name = document.createElement('div');
    name.className = 'scenario-name';
    name.textContent = scenario.name;
    header.appendChild(name);
    
    const buttons = document.createElement('div');
    buttons.className = 'scenario-buttons';
    
    scenario.states.forEach(state => {
        const btn = document.createElement('button');
        btn.className = 'scenario-state-btn';
        btn.textContent = state.name;
        btn.title = state.description || '';
        btn.onclick = () => executeScenario(key, state.id, state.name, btn);
        buttons.appendChild(btn);
    });
    
    card.appendChild(header);
    card.appendChild(buttons);
    return card;
}

async function executeScenario(scenarioName, state, stateName, buttonElement) {
    const statusBadge = document.getElementById('scenario-status');
    buttonElement.disabled = true;
    const originalText = buttonElement.textContent;
    buttonElement.textContent = stateName + ' ...';
    
    if (statusBadge) statusBadge.textContent = 'Ausführung...';
    
    try {
        const response = await fetch('/api/scenario/execute', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({scenario: scenarioName, state: state})
        });
        
        if (!response.ok) throw new Error('Request failed');
        const result = await response.json();
        
        if (result.success || result.host_response) {
            buttonElement.textContent = stateName + ' ✓';
            if (statusBadge) statusBadge.textContent = 'Erfolgreich';
            showNotification(`Szenario "${stateName}" erfolgreich ausgeführt`, 'success');
            
            // Refresh status displays
            setTimeout(() => {
                refreshAllStatus();
                buttonElement.textContent = originalText;
                buttonElement.disabled = false;
                if (statusBadge) statusBadge.textContent = 'Bereit';
            }, 2000);
        } else {
            throw new Error(result.error || 'Unknown error');
        }
    } catch (error) {
        console.error('Scenario execution error:', error);
        buttonElement.textContent = stateName + ' ✗';
        if (statusBadge) statusBadge.textContent = 'Fehler';
        showNotification(`Fehler: ${error.message}`, 'error');
        
        setTimeout(() => {
            buttonElement.textContent = originalText;
            buttonElement.disabled = false;
            if (statusBadge) statusBadge.textContent = 'Bereit';
        }, 2000);
    }
}

// ============================================================================
// RELAY CONTROLS
// ============================================================================

// ============================================================================
// ============================================================================
// SYSTEM CONTROLS (Coal, Gas, Reserve)
// ============================================================================

// CONFIGURATION: Map Relays to Power Stations
// Change the target container ID to move a relay to a different station.
// Available IDs: 'coal-controls', 'gas-controls', 'reserve-controls'
const RELAY_MAPPING = {
    esp1: {
        0: 'gas-controls',    // Ventil - 1
        1: 'gas-controls',    // Ventil - 2
        2: 'gas-controls',    // Heizstab
        3: 'gas-controls',    // Zünder
        4: 'gas-controls',     // Gasventil
        5: 'gas-controls',    // Kühler
        6: 'gas-controls', // MFC - Reserve
        7: 'reserve-controls'  // Unbelegt
    },
    esp2: {
        0: 'coal-controls', // Reserve 1
        1: 'coal-controls', // Reserve 2
        2: 'reserve-controls', // Reserve 3
        3: 'reserve-controls'  // Reserve 4
    }
};

async function loadRelays() {
    // Clear containers
    const coalContainer = document.getElementById('coal-controls');
    const gasContainer = document.getElementById('gas-controls');
    const reserveContainer = document.getElementById('reserve-controls');
    
    if (coalContainer) coalContainer.innerHTML = '';
    if (gasContainer) gasContainer.innerHTML = '';
    if (reserveContainer) reserveContainer.innerHTML = '';
    
    await Promise.all([
        loadAndDistributeESP1(),
        loadAndDistributeESP2()
    ]);
}

async function loadAndDistributeESP1() {
    try {
        const [metaResp, stateResp] = await Promise.all([
            fetch('/api/device_meta/esp1'),
            fetch('/api/device_state/esp1')
        ]);
        
        if (!metaResp.ok || !stateResp.ok) return;
        
        const metaData = await metaResp.json();
        const stateData = await stateResp.json();
        
        const meta = metaData.body || metaData;
        const state = stateData.body || stateData;
        const isOffline = !!metaData.offline || !!stateData.offline;
        
        if (meta && meta.names) {
            for (let i = 0; i < meta.count; i++) {
                const globalIdx = 1 + i; // ESP1 starts at 1
                const name = meta.names[i] || `Relay ${i}`;
                const val = state[`r${i}`] || 0;
                
                const item = createRelayItem(globalIdx, name, val, isOffline);
                
                // Use Mapping
                const targetId = RELAY_MAPPING.esp1[i] || 'reserve-controls';
                document.getElementById(targetId)?.appendChild(item);
            }
        }
    } catch (e) {
        console.error('Error loading ESP1:', e);
    }
}

async function loadAndDistributeESP2() {
    try {
        const [metaResp, stateResp] = await Promise.all([
            fetch('/api/device_meta/esp2'),
            fetch('/api/device_state/esp2')
        ]);
        
        if (!metaResp.ok || !stateResp.ok) return;
        
        const metaData = await metaResp.json();
        const stateData = await stateResp.json();
        
        const meta = metaData.body || metaData;
        const state = stateData.body || stateData;
        const isOffline = !!metaData.offline || !!stateData.offline;
        
        if (meta && meta.names) {
            for (let i = 0; i < meta.count; i++) {
                const globalIdx = 9 + i; // ESP2 starts at 9
                const name = meta.names[i] || `Relay ${i}`;
                const val = state[`r${i}`] || 0;
                
                const item = createRelayItem(globalIdx, name, val, isOffline);
                
                // Use Mapping
                const targetId = RELAY_MAPPING.esp2[i] || 'reserve-controls';
                document.getElementById(targetId)?.appendChild(item);
            }
        }
    } catch (e) {
        console.error('Error loading ESP2:', e);
    }
}

function createRelayItem(globalIndex, name, state, isOffline = false) {
    const item = document.createElement('div');
    item.className = 'relay-item';
    if (isOffline) item.classList.add('offline');
    
    const info = document.createElement('div');
    info.className = 'relay-info';
    
    const nameEl = document.createElement('div');
    nameEl.className = 'relay-name';
    nameEl.textContent = name;
    
    // Optional: Hide index for cleaner look, or keep small
    // const indexEl = document.createElement('div');
    // indexEl.className = 'relay-index';
    // indexEl.textContent = `#${globalIndex}`;
    
    info.appendChild(nameEl);
    // info.appendChild(indexEl);
    
    const toggle = document.createElement('label');
    toggle.className = 'toggle-switch';
    if (isOffline) toggle.classList.add('disabled');
    
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.id = `relay-${globalIndex}`;
    input.checked = (state === 1);
    input.disabled = isOffline; // Disable
    if (!isOffline) {
        input.onchange = () => toggleRelay(globalIndex, input.checked ? 1 : 0, input);
    }
    
    const slider = document.createElement('span');
    slider.className = 'toggle-slider';
    
    toggle.appendChild(input);
    toggle.appendChild(slider);
    
    item.appendChild(info);
    item.appendChild(toggle);
    
    return item;
}

async function toggleRelay(globalIndex, val, inputElement) {
    const requestKey = 'relay-' + globalIndex;
    if (pendingRequests.has(requestKey)) {
        inputElement.checked = !inputElement.checked;
        return;
    }
    
    inputElement.disabled = true;
    pendingRequests.add(requestKey);
    
    try {
        const response = await fetch('/api/relay/set', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({global_idx: globalIndex, val: val})
        });
        
        if (!response.ok) throw new Error('Request failed');
        
        showNotification(`Schalten erfolgreich`, 'success');
        
        setTimeout(() => {
            inputElement.disabled = false;
        }, 200);
    } catch (error) {
        console.error('Relay toggle error:', error);
        inputElement.checked = !inputElement.checked;
        inputElement.disabled = false;
        showNotification(`Fehler beim Schalten`, 'error');
    } finally {
        pendingRequests.delete(requestKey);
    }
}

function toggleReserveSection() {
    const container = document.getElementById('reserve-controls-container');
    const icon = document.getElementById('reserve-toggle-icon');
    
    if (container.style.display === 'none') {
        container.style.display = 'block';
        icon.textContent = '▲';
    } else {
        container.style.display = 'none';
        icon.textContent = '▼';
    }
}

// ============================================================================
// WIND & TRAIN CONTROLS
// ============================================================================

function initWindTrainControls() {
    refreshWindTrainStatus();
}

async function setWind(val) {
    try {
        const response = await fetch('/api/esp3/set_wind', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({val: val})
        });
        
        if (!response.ok) throw new Error('Request failed');
        
        showNotification(`Wind ${val ? 'eingeschaltet' : 'ausgeschaltet'}`, 'success');
        setTimeout(refreshWindTrainStatus, 200);
    } catch (error) {
        console.error('Wind control error:', error);
        showNotification('Fehler bei Wind-Steuerung', 'error');
    }
}

async function updateTrainPWM(value) {
    const display = document.getElementById('pwm-display');
    if (display) display.textContent = value;
    
    try {
        await fetch('/api/esp3/set_pwm', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({pwm: parseInt(value)})
        });
    } catch (error) {
        console.error('Train PWM error:', error);
    }
}

async function stopTrain() {
    const slider = document.getElementById('train-slider');
    const display = document.getElementById('pwm-display');
    
    try {
        await fetch('/api/esp3/set_pwm', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({pwm: 0})
        });
        
        if (slider) slider.value = 0;
        if (display) display.textContent = '0';
        showNotification('Zug gestoppt', 'success');
    } catch (error) {
        console.error('Train stop error:', error);
        showNotification('Fehler beim Stoppen', 'error');
    }
}

// ============================================================================
// STATUS UPDATES
// ============================================================================

async function refreshWindTrainStatus() {
    try {
        const response = await fetch('/api/esp3/state');
        if (!response.ok) return;
        
        const data = await response.json();
        const state = data.body || data;
        
        const windDisplay = document.getElementById('wind-display');
        const windStatus = document.getElementById('wind-status');
        const pwmDisplay = document.getElementById('pwm-display');
        const trainPwm = document.getElementById('train-pwm');
        const slider = document.getElementById('train-slider');
        
        if (state.running !== undefined) {
            const windText = state.running ? 'AN' : 'AUS';
            if (windDisplay) windDisplay.textContent = windText;
            if (windStatus) windStatus.textContent = windText;
        }
        
        if (state.pwm !== undefined) {
            if (pwmDisplay) pwmDisplay.textContent = state.pwm;
            if (trainPwm) trainPwm.textContent = state.pwm;
            if (slider) slider.value = state.pwm;
        }
    } catch (error) {
        console.error('Wind/Train status error:', error);
    }
}

async function refreshTemperatures() {
    // ESP1
    try {
        const response = await fetch('/api/device_state/esp1');
        if (response.ok) {
            const data = await response.json();
            const state = data.body || data;
            
            const tempEl = document.getElementById('temp-esp1');
            if (tempEl && state.temp !== undefined) {
                tempEl.textContent = state.temp === null ? '--°C' : `${Number(state.temp).toFixed(1)}°C`;
            }
        }
    } catch (error) {
        console.error('ESP1 temp error:', error);
    }
    
    // ESP2
    try {
        const response = await fetch('/api/device_state/esp2');
        if (response.ok) {
            const data = await response.json();
            const state = data.body || data;
            
            const tempEl = document.getElementById('temp-esp2');
            if (tempEl && state.temp !== undefined) {
                tempEl.textContent = state.temp === null ? '--°C' : `${Number(state.temp).toFixed(1)}°C`;
            }
        }
    } catch (error) {
        console.error('ESP2 temp error:', error);
    }
}

async function refreshAllStatus() {
    await Promise.all([
        refreshWindTrainStatus(),
        refreshTemperatures()
    ]);
}

function startStatusPolling() {
    // Initial refresh
    refreshAllStatus();
    
    // Poll every 5 seconds
    setInterval(refreshAllStatus, 5000);
}

// ============================================================================
// INITIALIZATION
// ============================================================================

// ============================================================================
// PERFORMANCE OPTIMIZATION
// ============================================================================

// Reduce animations on low-end devices
if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
    document.querySelectorAll('*').forEach(el => {
        el.style.animation = 'none';
        el.style.transition = 'none';
    });
}
