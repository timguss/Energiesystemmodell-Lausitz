const container = document.getElementById("system");
const svg       = document.getElementById("grid");

const NODE_HALF = 40;

// Icons for each node type
const nodeIcons = {
    coal:      '🏭',
    village:  '🏘️',
    solar:     '☀️',
    wind:      '🌬️',
    gridNode:  '⚡',
    external:  '🔌',
    gas:       '🔥',
    elektro:   '⚗️',
    heatpump:  '🌡️'
};

// CSS class suffix for each node
const nodeClasses = {
    coal:      'node-coal',
    village:   'node-village',
    solar:     'node-solar',
    wind:      'node-wind',
    gridNode:  'node-gridNode',
    external:  'node-external',
    gas:       'node-gas',
    elektro:   'node-elektro',
    heatpump:  'node-heatpump'
};

const nodes = {
    coal:        { label:"Kohlekraftwerk",        x:0.10, y:0.50, visible:true },
    village:     { label:"Dorf",                  x:0.38, y:0.55, visible:true },
    solar:       { label:"Solar",                 x:0.52, y:0.58, visible:true },
    wind:        { label:"Windrad",               x:0.60, y:0.67, visible:true },
    gridNode:    { label:"Netzknoten",            x:0.68, y:0.50, visible:true },
    external:    { label:"Stromnetzanschluss",    x:0.68, y:0.18, visible:true },
    gas:         { label:"Gaskraftwerk",          x:0.88, y:0.30, visible:true },
    elektro:     { label:"Elektrolyse",           x:0.42, y:0.82, visible:true },
    heatpump:    { label:"Wärmepumpe",            x:0.68, y:0.88, visible:true },

    r_coal_bend: { x:0.28, y:0.50 },
    r_top_left:  { x:0.38, y:0.38 },
    r_top_right: { x:0.68, y:0.38 },
    r_solar_mid: { x:0.52, y:0.38 },
    r_elek_mid:  { x:0.42, y:0.68 },
    r_heat_mid:  { x:0.68, y:0.68 },
    r_gas_bend:  { x:0.88, y:0.50 },
};

const edges = [
    { from:"coal",        to:"r_coal_bend", color:"grey",             flow:false },
    { from:"r_coal_bend", to:"r_top_left",  color:"grey",             flow:false },
    { from:"r_top_left",  to:"r_solar_mid", color:["green","yellow"], flow:[true,true] },
    { from:"r_solar_mid", to:"r_top_right", color:["green","yellow"], flow:[true,true] },
    { from:"r_top_right", to:"gridNode",    color:["green","yellow"], flow:[true,true] },
    { from:"r_top_left",  to:"village",     color:"grey",             flow:false },
    { from:"village",     to:"r_elek_mid",  color:"grey",             flow:false },
    { from:"solar",       to:"r_solar_mid", color:["green","yellow"], flow:[true,true] },
    { from:"wind",        to:"gridNode",    color:["grey","yellow"],  flow:[false,true] },
    { from:"gridNode",    to:"external",    color:["green","yellow"], flow:[true,true], rev:[false,true] },
    { from:"gas",         to:"r_gas_bend",  color:"green",            flow:false },
    { from:"r_gas_bend",  to:"gridNode",    color:"green",            flow:false },
    { from:"gridNode",    to:"r_heat_mid",  color:"yellow",           flow:true },
    { from:"r_heat_mid",  to:"r_elek_mid",  color:"yellow",           flow:true },
    { from:"r_elek_mid",  to:"elektro",     color:"yellow",           flow:true },
    { from:"r_heat_mid",  to:"heatpump",    color:"yellow",           flow:false },
];

function clipToNode(cx, cy, ox, oy) {
    const dx = cx - ox, dy = cy - oy;
    const adx = Math.abs(dx), ady = Math.abs(dy);
    let t = adx >= ady
        ? (adx - NODE_HALF) / adx
        : (ady - NODE_HALF) / ady;
    t = Math.max(0, Math.min(1, t));
    return { x: ox + dx * t, y: oy + dy * t };
}

function drawEdge(e, W, H) {
    const n1 = nodes[e.from], n2 = nodes[e.to];
    if (!n1 || !n2) return;

    const colors = Array.isArray(e.color) ? e.color : [e.color];
    const flows  = Array.isArray(e.flow)  ? e.flow  : colors.map(() => e.flow ?? false);
    const revs   = Array.isArray(e.rev)   ? e.rev   : colors.map(() => false);
    const N = colors.length;
    const perStripe = N === 1 ? 7 : 6;

    const cx1 = n1.x * W, cy1 = n1.y * H;
    const cx2 = n2.x * W, cy2 = n2.y * H;

    const p1 = n1.visible ? clipToNode(cx1, cy1, cx2, cy2) : { x:cx1, y:cy1 };
    const p2 = n2.visible ? clipToNode(cx2, cy2, cx1, cy1) : { x:cx2, y:cy2 };

    const dx = p2.x - p1.x, dy = p2.y - p1.y;
    const len = Math.sqrt(dx*dx + dy*dy) || 1;
    const px = -dy / len, py = dx / len;

    for (let i = 0; i < N; i++) {
        const offset = (i - (N - 1) / 2) * perStripe;
        const line = document.createElementNS("http://www.w3.org/2000/svg", "line");
        line.setAttribute("x1", p1.x + px * offset);
        line.setAttribute("y1", p1.y + py * offset);
        line.setAttribute("x2", p2.x + px * offset);
        line.setAttribute("y2", p2.y + py * offset);
        line.setAttribute("stroke-width", perStripe - (N > 1 ? 1.5 : 0));
        let cls = "line " + colors[i];
        if (flows[i]) cls += revs[i] ? " flow-rev" : " flow";
        line.setAttribute("class", cls);
        svg.appendChild(line);
    }
}

function renderNodes() {
    document.querySelectorAll(".node").forEach(n => n.remove());
    for (const id in nodes) {
        const n = nodes[id];
        if (!n.visible) continue;
        const wrap = document.createElement("div");
        wrap.className = "node " + (nodeClasses[id] || '');
        wrap.style.left = (n.x * 100) + "%";
        wrap.style.top  = (n.y * 100) + "%";
        const vis = document.createElement("div");
        vis.className = "node-visual";
        vis.innerHTML = '<span class="node-icon">' + (nodeIcons[id] || '⬡') + '</span>';
        const lbl = document.createElement("div");
        lbl.className = "node-label";
        lbl.textContent = n.label || "";
        wrap.appendChild(vis);
        wrap.appendChild(lbl);
        container.appendChild(wrap);
    }
}

function draw() {
    svg.innerHTML = "";
    const W = container.clientWidth, H = container.clientHeight;
    edges.forEach(e => drawEdge(e, W, H));
}

// Initialize
renderNodes();
draw();
window.addEventListener("resize", draw);