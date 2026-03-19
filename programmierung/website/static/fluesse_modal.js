/**
 * fluesse_modal.js
 * Contains all logic for displaying and interacting with node modals.
 */

const modalOverlay = document.getElementById("modal-overlay");
const modalClose = document.getElementById("modal-close");

let currentNodeId = null;

function openModal(nodeId) {
  const details = nodeDetails[nodeId];
  if (!details) return;

  currentNodeId = nodeId;
  updateModalState(nodeId);

  // Show modal
  modalOverlay.classList.add("active");
  document.body.style.overflow = "hidden";
}

// Function to refresh modal content without closing/reopening
function updateModalState(nodeId) {
  if (currentNodeId !== nodeId) return;
  const details = nodeDetails[nodeId];
  if (!details) return;

  const currentState = details.currentState || "off";
  const stateInfo = details.states?.[currentState] || {
    label: "Unbekannt",
    icon: "⬡",
  };

  // Fill modal content
  document.getElementById("modal-icon").textContent = stateInfo.icon;
  document.getElementById("modal-title").textContent =
    nodes[nodeId]?.label || nodeId;
  document.getElementById("modal-subtitle").textContent = details.subtitle;
  document.getElementById("modal-visual-icon").textContent = stateInfo.icon;
  document.getElementById("modal-visual-label").textContent =
    nodes[nodeId]?.label || nodeId;
  document.getElementById("modal-description").textContent =
    details.description;

  // Update state indicator
  const stateDot = document.getElementById("state-dot");
  const stateLabel = document.getElementById("state-label");
  stateDot.className = "state-dot " + currentState;
  stateLabel.textContent = stateInfo.label;

  // Update visual glow based on state
  const visualIcon = document.getElementById("modal-visual-icon");
  visualIcon.classList.remove("active-glow", "warning-glow", "error-glow");
  if (currentState === "on") visualIcon.classList.add("active-glow");
  else if (currentState === "idle") visualIcon.classList.add("warning-glow");
  else if (currentState === "error") visualIcon.classList.add("error-glow");

  // Fill state buttons
  const stateButtonsContainer = document.getElementById("state-buttons");
  stateButtonsContainer.innerHTML = "";
  if (details.states) {
    Object.keys(details.states).forEach((stateKey) => {
      const btn = document.createElement("button");
      btn.className =
        "state-btn" + (stateKey === currentState ? " active" : "");
      btn.textContent = details.states[stateKey].label;
      btn.onclick = () => setNodeState(nodeId, stateKey);
      stateButtonsContainer.appendChild(btn);
    });
  }

  // Fill scenario buttons
  const scenarioContainer = document.getElementById("scenario-buttons");
  scenarioContainer.innerHTML = "";
  if (details.scenarios) {
    details.scenarios.forEach((scenario) => {
      const btn = document.createElement("button");
      btn.className = "scenario-btn";
      btn.innerHTML = `<span class="scenario-name">${scenario.name}</span><span class="scenario-desc">${scenario.desc}</span>`;
      btn.onclick = () => runScenario(scenario.id);
      scenarioContainer.appendChild(btn);
    });
  }
}

async function runScenario(scenarioId) {
    if (!currentNodeId) return;
    const sid = scenarioId.toLowerCase();
    console.log(`Executing optical scenario: ${sid} for ${currentNodeId}`);
    
    // Purely visual logic
    if (sid === "tank_in_brennstoffzelle") {
         nodeDetails[currentNodeId].currentState = "on_fuelcell";
    } else if (sid.includes("startup") || sid.includes("_an") || sid.includes("import") || sid.includes("peak") || sid.includes("day") || sid.includes("heizen")) {
        nodeDetails[currentNodeId].currentState = "on";
    } else if (sid.includes("shutdown") || sid.includes("_aus") || sid.includes("notaus") || sid.includes("off") || sid.includes("night") || sid.includes("standby")) {
        nodeDetails[currentNodeId].currentState = "off";
    }
    
    updateModalState(currentNodeId);
    draw();
}

function setNodeState(nodeId, newState) {
  const details = nodeDetails[nodeId];
  if (!details || !details.states) return;

  // Optimization: If it's a manual state change, we could send a relay command.
  // But for now, let's just update local state and let polling or manual refresh happen.
  // Most nodes are controlled via scenarios.
  details.currentState = newState;
  updateModalState(nodeId); 
  draw(); 
}

function closeModal() {
  modalOverlay.classList.remove("active");
  document.body.style.overflow = "";
  currentNodeId = null;
}

// Modal event listeners
if (modalClose) {
    modalClose.addEventListener("click", closeModal);
}

if (modalOverlay) {
    modalOverlay.addEventListener("click", (e) => {
        if (e.target === modalOverlay) closeModal();
    });
}

document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") closeModal();
});
