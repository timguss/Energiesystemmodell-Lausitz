const container = document.getElementById("system");
const svg       = document.getElementById("grid");
const modalOverlay = document.getElementById("modal-overlay");
const modalClose = document.getElementById("modal-close");

const NODE_HALF = 40;

// Node details for modal
const nodeDetails = {
    coal: {
        subtitle: "Kraftwerk",
        description: "Das Kohlekraftwerk ist eine traditionelle Energieerzeugungsanlage, die Kohle zur Stromproduktion verwendet. Es verfügt über mehrere Relais zur Steuerung der Verbrennungsprozesse, Kühlung und Turbine.",
        scenarios: [
            { name: "Starten", desc: "Kraftwerk hochfahren" },
            { name: "Stoppen", desc: "Kraftwerk herunterfahren" },
            { name: "Testlauf", desc: "Alle Systeme testen" },
            { name: "Not-Aus", desc: "Sofortige Abschaltung" }
        ]
    },
    village: {
        subtitle: "Wohngebiet",
        description: "Das Dorf repräsentiert den lokalen Energieverbrauch. Hier werden Haushalte mit Strom und Wärme versorgt. Der Energiebedarf variiert je nach Tageszeit und Jahreszeit.",
        scenarios: [
            { name: "Nachtmodus", desc: "Reduzierte Versorgung" },
            { name: "Tagmodus", desc: "Normale Versorgung" },
            { name: "Spitzenlast", desc: "Hoher Energiebedarf" }
        ]
    },
    solar: {
        subtitle: "Photovoltaik",
        description: "Die Solarenergieanlage wandelt Sonnenlicht direkt in elektrische Energie um. Die Leistung ist stark abhängig von der Sonneneinstrahlung und Wetterbedingungen.",
        scenarios: [
            { name: "Aktivieren", desc: "Einspeisung starten" },
            { name: "Deaktivieren", desc: "Einspeisung stoppen" },
            { name: "Maximum", desc: "Volle Leistung" }
        ]
    },
    wind: {
        subtitle: "Windkraft",
        description: "Die Windkraftanlage nutzt die Windenergie zur Stromerzeugung. Die Leistung ist abhängig von der Windgeschwindigkeit und kann stark schwanken.",
        scenarios: [
            { name: "Starten", desc: "Turbine starten" },
            { name: "Stoppen", desc: "Turbine stoppen" },
            { name: "Leerlauf", desc: "Minimaler Betrieb" }
        ]
    },
    gridNode: {
        subtitle: "Verteilung",
        description: "Der Netzknoten ist das Herz der Energieverteilung. Hier werden alle Energieflüsse zusammengeführt und an die Verbraucher verteilt.",
        scenarios: [
            { name: "Ausbalancieren", desc: "Netz optimieren" },
            { name: "Notfall", desc: "Sicherheitsmodus" },
            { name: "Diagnose", desc: "Netzwerk prüfen" }
        ]
    },
    external: {
        subtitle: "Übertragung",
        description: "Der Stromnetzanschluss verbindet das lokale Energiesystem mit dem überregionalen Stromnetz. Überschüssige Energie kann eingespeist oder bezogen werden.",
        scenarios: [
            { name: "Einspeisen", desc: "Energie abgeben" },
            { name: "Beziehen", desc: "Energie aufnehmen" },
            { name: "Isoliert", desc: "Autark betreiben" }
        ]
    },
    gas: {
        subtitle: "Kraftwerk",
        description: "Das Gaskraftwerk nutzt Erdgas zur Stromerzeugung. Es ist flexibel und kann schnell hoch- und heruntergefahren werden.",
        scenarios: [
            { name: "Zünden", desc: "Anfahren" },
            { name: "Abstellen", desc: "Herunterfahren" },
            { name: "Teillast", desc: "Reduzierte Leistung" },
            { name: "Volllast", desc: "Maximale Leistung" }
        ]
    },
    elektro: {
        subtitle: "Wasserstoff",
        description: "Die Elektrolyseanlage erzeugt Wasserstoff durch Spaltung von Wasser mittels elektrischer Energie.",
        scenarios: [
            { name: "Starten", desc: "Produktion beginnen" },
            { name: "Stoppen", desc: "Produktion beenden" },
            { name: "Sparmodus", desc: "Minimaler Betrieb" }
        ]
    },
    heatpump: {
        subtitle: "Heizung",
        description: "Die Wärmepumpe nutzt Umweltwärme zur Erzeugung von Heizwärme. Sie ist eine effiziente Methode zur Gebäudeheizung.",
        scenarios: [
            { name: "Heizen", desc: "Heizbetrieb starten" },
            { name: "Kühlen", desc: "Kühlbetrieb aktivieren" },
            { name: "Standby", desc: "Bereitschaftsmodus" },
            { name: "Abtauen", desc: "Frostschutz" }
        ]
    }
};

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
        wrap.dataset.nodeId = id;
        const vis = document.createElement("div");
        vis.className = "node-visual";
        vis.innerHTML = '<span class="node-icon">' + (nodeIcons[id] || '⬡') + '</span>';
        const lbl = document.createElement("div");
        lbl.className = "node-label";
        lbl.textContent = n.label || "";
        wrap.appendChild(vis);
        wrap.appendChild(lbl);

        // Click handler for modal
        wrap.addEventListener("click", () => openModal(id));

        container.appendChild(wrap);
    }
}

// Modal functions
function openModal(nodeId) {
    const details = nodeDetails[nodeId];
    if (!details) return;

    // Fill modal content
    document.getElementById("modal-icon").textContent = nodeIcons[nodeId] || '⬡';
    document.getElementById("modal-title").textContent = nodes[nodeId]?.label || nodeId;
    document.getElementById("modal-subtitle").textContent = details.subtitle;
    document.getElementById("modal-visual-icon").textContent = nodeIcons[nodeId] || '⬡';
    document.getElementById("modal-visual-label").textContent = nodes[nodeId]?.label || nodeId;
    document.getElementById("modal-description").textContent = details.description;

    // Fill scenario buttons
    const scenarioContainer = document.getElementById("scenario-buttons");
    scenarioContainer.innerHTML = '';
    details.scenarios.forEach(scenario => {
        const btn = document.createElement("button");
        btn.className = "scenario-btn";
        btn.innerHTML = `<span class="scenario-name">${scenario.name}</span><span class="scenario-desc">${scenario.desc}</span>`;
        // Not linked yet - just visual
        scenarioContainer.appendChild(btn);
    });

    // Show modal
    modalOverlay.classList.add("active");
    document.body.style.overflow = "hidden";
}

function closeModal() {
    modalOverlay.classList.remove("active");
    document.body.style.overflow = "";
}

// Modal event listeners
modalClose.addEventListener("click", closeModal);
modalOverlay.addEventListener("click", (e) => {
    if (e.target === modalOverlay) closeModal();
});
document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") closeModal();
});

function draw() {
    svg.innerHTML = "";
    const W = container.clientWidth, H = container.clientHeight;
    edges.forEach(e => drawEdge(e, W, H));
}

// Initialize
renderNodes();
draw();
window.addEventListener("resize", draw);