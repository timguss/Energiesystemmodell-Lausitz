/**
 * fluesse_utils.js
 * Contains helper functions for coordinate calculations and state lookups.
 */

function clipToNode(cx, cy, ox, oy) {
    const dx = cx - ox,
      dy = cy - oy;
    const adx = Math.abs(dx),
      ady = Math.abs(dy);
    let t = adx >= ady ? (adx - NODE_HALF) / adx : (ady - NODE_HALF) / ady;
    t = Math.max(0, Math.min(1, t));
    return { x: ox + dx * t, y: oy + dy * t };
}

// Helper: Check if a node produces energy (on state or fuel cell mode)
function nodeProducesEnergy(nodeId) {
    const details = nodeDetails[nodeId];
    if (!details) return false;
    return details.currentState === "on" || details.currentState === "on_fuelcell";
}

// Get the edge group for a node (producer or routing node)
function getGroupForNode(nodeId) {
    if (producerNodeToGroup[nodeId]) return producerNodeToGroup[nodeId];
    if (routingNodeToGroup[nodeId]) return routingNodeToGroup[nodeId];
    return null;
}

// Update edge group state based on node states
// Each color stripe can be controlled independently
function updateEdgeGroupStates() {
    const groups = getEdgeGroups();
  
    // Coal group - single grey stripe, flows when coal is active
    const coalActive = nodeProducesEnergy("coal");
    groups.coal.flows = [coalActive];
    groups.coal.revs = [false];
  
    // Solar group - two stripes, both flow together
    const solarActive = nodeProducesEnergy("solar");
    groups.solar.flows = [solarActive, solarActive];
    groups.solar.revs = [false, false]; // Producer: solar -> grid
  
    // Wind group - two stripes
    const windActive = nodeProducesEnergy("wind");
    groups.wind.flows = [windActive, windActive];
    groups.wind.revs = [false, false]; // Producer: wind -> grid
  
    // Gas group - single stripe
    const gasActive = nodeProducesEnergy("gas");
    groups.gas.flows = [gasActive];
    groups.gas.revs = [false]; // Producer: gas -> grid
  
    // Village group - flows when village has demand (default: idle or on)
    const villageActive =
      nodeDetails.village &&
      (nodeDetails.village.currentState === "on" ||
        nodeDetails.village.currentState === "idle");
    groups.village.flows = [villageActive];
    groups.village.revs = [true]; // Consumer: grid -> village
  
    // Grid to external - two stripes
    const fuelcellActive = nodeDetails.elektro.currentState === "on_fuelcell";
    const gridHasPower = solarActive || windActive || gasActive || coalActive || fuelcellActive;
    const externalActive = nodeDetails.external && nodeDetails.external.currentState === "on";
    
    // Stripe 1: Export (green)
    // Stripe 2: Import (yellow)
    groups.gridToExternal.flows = [gridHasPower, externalActive];
    groups.gridToExternal.revs = [false, true];
  
    // Grid to Mid - flows when heatpump draws power OR coal pushes power
    const heatpumpActive = nodeProducesEnergy("heatpump");
    groups.gridToMid.flows = [heatpumpActive || coalActive];
    // Reverse if coal is pushing TO the grid from the mid point
    groups.gridToMid.revs = [coalActive];
  
    // Heatpump
    groups.heatpump.flows = [heatpumpActive];
    groups.heatpump.revs = [false]; // Consumer: mid -> heatpump
  
    // Elektro - single stripe
    const elektroState = nodeDetails.elektro.currentState;
    const elektroActive = elektroState === "on" || elektroState === "on_fuelcell";
    groups.elektro.flows = [elektroActive];
    // Consumer: grid -> elektro (revs=true) when in 'on' (electrolysis) mode
    // Producer: elektro -> grid (revs=false) when in 'on_fuelcell' (fuel cell) mode
    groups.elektro.revs = [elektroState === "on"];
}
