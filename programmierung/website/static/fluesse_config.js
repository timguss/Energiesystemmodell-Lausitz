/**
 * fluesse_config.js
 * Contains all configuration and state data for the energy flow diagram.
 */

// Node details for modal
const nodeDetails = {
    coal: {
      subtitle: "Kraftwerk",
      description:
        "Das Kohlekraftwerk ist eine traditionelle Energieerzeugungsanlage, die Kohle zur Stromproduktion verwendet. Es verfügt über mehrere Relais zur Steuerung der Verbrennungsprozesse, Kühlung und Turbine.",
      states: {
        off: { label: "Aus", icon: "🏭" },
        idle: { label: "Bereit", icon: "🏭" },
        on: { label: "Betrieb", icon: "🏭🔥" },
        error: { label: "Störung", icon: "🏭⚠️" },
      },
      currentState: "off",
      scenarios: [
        { name: "Starten", desc: "Kraftwerk hochfahren" },
        { name: "Stoppen", desc: "Kraftwerk herunterfahren" },
        { name: "Testlauf", desc: "Alle Systeme testen" },
        { name: "Not-Aus", desc: "Sofortige Abschaltung" },
      ],
    },
    village: {
      subtitle: "Wohngebiet",
      description:
        "Das Dorf repräsentiert den lokalen Energieverbrauch. Hier werden Haushalte mit Strom und Wärme versorgt. Der Energiebedarf variiert je nach Tageszeit und Jahreszeit.",
      states: {
        off: { label: "Kein Bedarf", icon: "🏘️" },
        idle: { label: "Normal", icon: "🏘️" },
        on: { label: "Hochlast", icon: "🏘️⚡" },
        error: { label: "Störung", icon: "🏘️⚠️" },
      },
      currentState: "idle",
      scenarios: [
        { name: "Nachtmodus", desc: "Reduzierte Versorgung" },
        { name: "Tagmodus", desc: "Normale Versorgung" },
        { name: "Spitzenlast", desc: "Hoher Energiebedarf" },
      ],
    },
    solar: {
      subtitle: "Photovoltaik",
      description:
        "Die Solarenergieanlage wandelt Sonnenlicht direkt in elektrische Energie um. Die Leistung ist stark abhängig von der Sonneneinstrahlung und Wetterbedingungen.",
      states: {
        off: { label: "Inaktiv", icon: "☀️" },
        idle: { label: "Bereit", icon: "☀️" },
        on: { label: "Produktion", icon: "☀️⚡" },
        error: { label: "Störung", icon: "☀️⚠️" },
      },
      currentState: "idle",
      scenarios: [
        { name: "Aktivieren", desc: "Einspeisung starten" },
        { name: "Deaktivieren", desc: "Einspeisung stoppen" },
        { name: "Maximum", desc: "Volle Leistung" },
      ],
    },
    wind: {
      subtitle: "Windkraft",
      description:
        "Die Windkraftanlage nutzt die Windenergie zur Stromerzeugung. Die Leistung ist abhängig von der Windgeschwindigkeit und kann stark schwanken.",
      states: {
        off: { label: "Still", icon: "🌬️" },
        idle: { label: "Leerlauf", icon: "🌬️" },
        on: { label: "Produktion", icon: "🌬️⚡" },
        error: { label: "Störung", icon: "🌬️⚠️" },
      },
      currentState: "idle",
      scenarios: [
        { name: "Starten", desc: "Turbine starten" },
        { name: "Stoppen", desc: "Turbine stoppen" },
        { name: "Leerlauf", desc: "Minimaler Betrieb" },
      ],
    },
    gridNode: {
      subtitle: "Verteilung",
      description:
        "Der Netzknoten ist das Herz der Energieverteilung. Hier werden alle Energieflüsse zusammengeführt und an die Verbraucher verteilt.",
      states: {
        off: { label: "Aus", icon: "⚡" },
        idle: { label: "Bereit", icon: "⚡" },
        on: { label: "Aktiv", icon: "⚡🔋" },
        error: { label: "Störung", icon: "⚡⚠️" },
      },
      currentState: "on",
      scenarios: [
        { name: "Ausbalancieren", desc: "Netz optimieren" },
        { name: "Notfall", desc: "Sicherheitsmodus" },
        { name: "Diagnose", desc: "Netzwerk prüfen" },
      ],
    },
    external: {
      subtitle: "Übertragung",
      description:
        "Der Stromnetzanschluss verbindet das lokale Energiesystem mit dem überregionalen Stromnetz. Überschüssige Energie kann eingespeist oder bezogen werden.",
      states: {
        off: { label: "Getrennt", icon: "🔌" },
        idle: { label: "Bereit", icon: "🔌" },
        on: { label: "Verbunden", icon: "🔌⚡" },
        error: { label: "Störung", icon: "🔌⚠️" },
      },
      currentState: "on",
      scenarios: [
        { name: "Einspeisen", desc: "Energie abgeben" },
        { name: "Beziehen", desc: "Energie aufnehmen" },
        { name: "Isoliert", desc: "Autark betreiben" },
      ],
    },
    gas: {
      subtitle: "Kraftwerk",
      description:
        "Das Gaskraftwerk nutzt Erdgas zur Stromerzeugung. Es ist flexibel und kann schnell hoch- und heruntergefahren werden.",
      states: {
        off: { label: "Aus", icon: "🔥" },
        idle: { label: "Bereit", icon: "🔥" },
        on: { label: "Betrieb", icon: "🔥⚡" },
        error: { label: "Störung", icon: "🔥⚠️" },
      },
      currentState: "off",
      scenarios: [
        { name: "Zünden", desc: "Anfahren" },
        { name: "Abstellen", desc: "Herunterfahren" },
        { name: "Teillast", desc: "Reduzierte Leistung" },
        { name: "Volllast", desc: "Maximale Leistung" },
      ],
    },
    elektro: {
      subtitle: "Wasserstoff",
      description:
        "Die Elektrolyseanlage erzeugt Wasserstoff durch Spaltung von Wasser mittels elektrischer Energie.",
      states: {
        off: { label: "Aus", icon: "⚗️" },
        idle: { label: "Bereit", icon: "⚗️" },
        on: { label: "Produktion", icon: "⚗️💧" },
        error: { label: "Störung", icon: "⚗️⚠️" },
      },
      currentState: "off",
      scenarios: [
        { name: "Starten", desc: "Produktion beginnen" },
        { name: "Stoppen", desc: "Produktion beenden" },
        { name: "Sparmodus", desc: "Minimaler Betrieb" },
      ],
    },
    heatpump: {
      subtitle: "Heizung",
      description:
        "Die Wärmepumpe nutzt Umweltwärme zur Erzeugung von Heizwärme. Sie ist eine effiziente Methode zur Gebäudeheizung.",
      states: {
        off: { label: "Aus", icon: "🌡️" },
        idle: { label: "Standby", icon: "🌡️" },
        on: { label: "Heizen", icon: "🌡️🔥" },
        error: { label: "Störung", icon: "🌡️⚠️" },
      },
      currentState: "idle",
      scenarios: [
        { name: "Heizen", desc: "Heizbetrieb starten" },
        { name: "Kühlen", desc: "Kühlbetrieb aktivieren" },
        { name: "Standby", desc: "Bereitschaftsmodus" },
        { name: "Abtauen", desc: "Frostschutz" },
      ],
    },
  };
  
  // Icons for each node type
  const nodeIcons = {
    coal: "🏭",
    village: "🏘️",
    solar: "☀️",
    wind: "🌬️",
    gridNode: "⚡",
    external: "🔌",
    gas: "🔥",
    elektro: "⚗️",
    heatpump: "🌡️",
  };
  
  // CSS class suffix for each node
  const nodeClasses = {
    coal: "node-coal",
    village: "node-village",
    solar: "node-solar",
    wind: "node-wind",
    gridNode: "node-gridNode",
    external: "node-external",
    gas: "node-gas",
    elektro: "node-elektro",
    heatpump: "node-heatpump",
  };
  
  const nodes = {
    coal: { label: "Kohlekraftwerk", x: 0.1, y: 0.5, visible: true },
    village: { label: "Dorf", x: 0.38, y: 0.55, visible: true },
    solar: { label: "Solar", x: 0.52, y: 0.58, visible: true },
    wind: { label: "Windrad", x: 0.63, y: 0.64, visible: true },
    gridNode: { label: "Netzknoten", x: 0.68, y: 0.5, visible: true },
    external: { label: "Stromnetzanschluss", x: 0.74, y: 0.18, visible: true },
    gas: { label: "Gaskraftwerk", x: 0.88, y: 0.3, visible: true },
    elektro: { label: "Elektrolyse", x: 0.42, y: 0.82, visible: true },
    heatpump: { label: "Wärmepumpe", x: 0.68, y: 0.88, visible: true },
  
    r_coal_bend: { x: 0.2, y: 0.5 },
    r_coal_bend2: { x: 0.28, y: 0.36 },
    r_coal_bend3: { x: 0.35, y: 0.36 },
    r_coal_bend4: { x: 0.42, y: 0.45 },
    r_coal_bend5: { x: 0.42, y: 0.72 },
    r_coal_mid: { x: 0.68, y: 0.72 },
    r_village1: { x: 0.38, y: 0.44 },
    r_village2: { x: 0.44, y: 0.38 },
    r_village3: { x: 0.68, y: 0.38 },
    r_solar1: { x: 0.58, y: 0.42 },
    r_solar2: { x: 0.665, y: 0.42 },
    r_gas_bend: { x: 0.88, y: 0.5 },
    r_elekt1: { x: 0.52, y: 0.82 },
    r_elekt2: { x: 0.6, y: 0.55 },
  };
  
  // Node relationship configuration
  const powerEdgeGroups = {
    coal: {
      colors: ["grey"],
      edges: [
        { from: "coal", to: "r_coal_bend" },
        { from: "r_coal_bend", to: "r_coal_bend2" },
        { from: "r_coal_bend2", to: "r_coal_bend3" },
        { from: "r_coal_bend3", to: "r_coal_bend4" },
        { from: "r_coal_bend4", to: "r_coal_bend5" },
        { from: "r_coal_bend5", to: "r_coal_mid" },
      ],
      flows: [false],
      revs: [false],
    },
    solar: {
      colors: ["green", "yellow"],
      edges: [
        { from: "solar", to: "r_solar1" },
        { from: "r_solar1", to: "r_solar2" },
        { from: "r_solar2", to: "gridNode" },
      ],
      flows: [false, false],
      revs: [false, false],
    },
    wind: {
      colors: ["grey", "yellow"],
      edges: [{ from: "wind", to: "gridNode" }],
      flows: [false, false],
      revs: [false, false],
    },
    gas: {
      colors: ["green"],
      edges: [
        { from: "gas", to: "r_gas_bend" },
        { from: "r_gas_bend", to: "gridNode" },
      ],
      flows: [false],
      revs: [false],
    },
    village: {
      colors: ["grey"],
      edges: [
        { from: "village", to: "r_village1" },
        { from: "r_village1", to: "r_village2" },
        { from: "r_village2", to: "r_village3" },
        { from: "r_village3", to: "gridNode" },
      ],
      flows: [false],
      revs: [false],
    },
    gridToExternal: {
      colors: ["green", "yellow"],
      edges: [{ from: "gridNode", to: "external" }],
      flows: [false, false],
      revs: [false, false],
    },
    gridToMid: {
      colors: ["yellow"],
      edges: [{ from: "gridNode", to: "r_coal_mid" }],
      flows: [false],
      revs: [false],
    },
    heatpump: {
      colors: ["yellow"],
      edges: [{ from: "r_coal_mid", to: "heatpump" }],
      flows: [false],
      revs: [false],
    },
    elektro: {
      colors: ["yellow"],
      edges: [
        { from: "elektro", to: "r_elekt1" },
        { from: "r_elekt1", to: "r_elekt2" },
        { from: "r_elekt2", to: "gridNode" },
      ],
      flows: [false],
      revs: [false],
    },
  };
  
  const heatEdgeGroups = {
    coal: {
      colors: ["grey"],
      edges: powerEdgeGroups.coal.edges,
      flows: [false],
      revs: [false],
    },
    solar: {
      colors: ["blue", "red"],
      edges: powerEdgeGroups.solar.edges,
      flows: [false, false],
      revs: [false, false],
    },
    wind: {
      colors: ["grey", "red"],
      edges: powerEdgeGroups.wind.edges,
      flows: [false, false],
      revs: [false, false],
    },
    gas: {
      colors: ["blue"],
      edges: powerEdgeGroups.gas.edges,
      flows: [false],
      revs: [false],
    },
    village: {
      colors: ["grey"],
      edges: powerEdgeGroups.village.edges,
      flows: [false],
      revs: [false],
    },
    gridToExternal: {
      colors: ["blue", "red"],
      edges: powerEdgeGroups.gridToExternal.edges,
      flows: [false, false],
      revs: [false, false],
    },
    gridToMid: {
      colors: ["red"],
      edges: powerEdgeGroups.gridToMid.edges,
      flows: [false],
      revs: [false],
    },
    heatpump: {
      colors: ["red"],
      edges: powerEdgeGroups.heatpump.edges,
      flows: [false],
      revs: [false],
    },
    elektro: {
      colors: ["red"],
      edges: powerEdgeGroups.elektro.edges,
      flows: [false],
      revs: [false],
    },
  };
  
  const routingNodeToGroup = {
    r_coal_bend: "coal",
    r_coal_bend2: "coal",
    r_coal_bend3: "coal",
    r_coal_bend4: "coal",
    r_coal_bend5: "coal",
    r_coal_mid: "gridToMid",
    r_solar1: "solar",
    r_solar2: "solar",
    r_gas_bend: "gas",
    r_village1: "village",
    r_village2: "solar",
    r_village3: "solar",
    r_elekt1: "elektro",
    r_elekt2: "elektro",
  };
  
  const producerNodeToGroup = {
    coal: "coal",
    solar: "solar",
    wind: "wind",
    gas: "gas",
    elektro: "elektro",
  };
  
  const NODE_HALF = 40;
