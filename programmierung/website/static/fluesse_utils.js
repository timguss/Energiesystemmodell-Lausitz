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

// Helper: Check if a node produces energy (on state)
function nodeProducesEnergy(nodeId) {
    const details = nodeDetails[nodeId];
    if (!details) return false;
    return details.currentState === "on";
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
    groups.coal.flows = [nodeProducesEnergy("coal")];
    groups.coal.revs = [false];
  
    // Solar group - two stripes, both flow together
    const solarActive = nodeProducesEnergy("solar");
    groups.solar.flows = [solarActive, solarActive];
    groups.solar.revs = [false, false];
  
    // Wind group - two stripes
    const windActive = nodeProducesEnergy("wind");
    groups.wind.flows = [windActive, windActive];
    groups.wind.revs = [false, false];
  
    // Gas group - single stripe
    const gasActive = nodeProducesEnergy("gas");
    groups.gas.flows = [gasActive];
    groups.gas.revs = [false];
  
    // Village group - flows when village has demand
    const villageActive =
      nodeDetails.village &&
      (nodeDetails.village.currentState === "on" ||
        nodeDetails.village.currentState === "idle");
    groups.village.flows = [villageActive];
    groups.village.revs = [false];
  
    // Grid to external - two stripes, can have independent flow direction
    const gridHasPower =
      nodeProducesEnergy("solar") ||
      nodeProducesEnergy("wind") ||
      nodeProducesEnergy("gas") ||
      nodeProducesEnergy("coal");
    const externalActive =
      nodeDetails.external && nodeDetails.external.currentState === "on";
    groups.gridToExternal.flows = [gridHasPower, externalActive];
    groups.gridToExternal.revs = [false, true];
  
    // Grid to Mid - flows when heatpump draws power
    const heatpumpActive = nodeProducesEnergy("heatpump");
    groups.gridToMid.flows = [heatpumpActive];
    groups.gridToMid.revs = [false];
  
    // Heatpump
    groups.heatpump.flows = [heatpumpActive];
    groups.heatpump.revs = [false];
  
    // Elektro - single stripe
    const elektroActive = nodeProducesEnergy("elektro");
    groups.elektro.flows = [elektroActive];
    groups.elektro.revs = [false];
}
