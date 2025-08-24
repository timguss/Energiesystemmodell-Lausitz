async function loadContent() {
  const response = await fetch("text.json");
  const data = await response.json();

  // Helper zum Füllen von Listen
  function fillList(elementId, items) {
    const ul = document.getElementById(elementId);
    if (!ul) return;
    ul.innerHTML = "";
    items.forEach(item => {
      const li = document.createElement("li");
      li.textContent = item;
      ul.appendChild(li);
    });
  }

  // Home
  document.getElementById("home-title").textContent = data.home.title;
  document.getElementById("home-text").textContent = data.home.text;

  // Solar
  document.getElementById("solar-header").textContent = data.solar.header;
  document.getElementById("solar-description").textContent = data.solar.description;
  document.getElementById("solar-info-title").textContent = data.solar.info.title;
  document.getElementById("solar-info-text").textContent = data.solar.info.text;
  fillList("solar-info-details", data.solar.info.details);

  // Wind
  document.getElementById("wind-header").textContent = data.wind.header;
  document.getElementById("wind-description").textContent = data.wind.description;
  document.getElementById("wind-info-title").textContent = data.wind.info.title;
  document.getElementById("wind-info-text").textContent = data.wind.info.text;
  fillList("wind-info-details", data.wind.info.details);

  // Coal
  document.getElementById("coal-header").textContent = data.coal.header;
  document.getElementById("coal-description").textContent = data.coal.description;
  document.getElementById("coal-info-title").textContent = data.coal.info.title;
  document.getElementById("coal-info-text").textContent = data.coal.info.text;
  fillList("coal-info-details", data.coal.info.details);

  // Gas
  document.getElementById("gas-header").textContent = data.gas.header;
  document.getElementById("gas-description").textContent = data.gas.description;
  document.getElementById("gas-info-title").textContent = data.gas.info.title;
  document.getElementById("gas-info-text").textContent = data.gas.info.text;
  fillList("gas-info-details", data.gas.info.details);

  // Biogas
  document.getElementById("biogas-header").textContent = data.biogas.header;
  document.getElementById("biogas-description").textContent = data.biogas.description;
  document.getElementById("biogas-info-title").textContent = data.biogas.info.title;
  document.getElementById("biogas-info-text").textContent = data.biogas.info.text;
  fillList("biogas-info-details", data.biogas.info.details);

  // Heatpump
  document.getElementById("heatpump-header").textContent = data.heatpump.header;
  document.getElementById("heatpump-description").textContent = data.heatpump.description;
  document.getElementById("heatpump-info-title").textContent = data.heatpump.info.title;
  document.getElementById("heatpump-info-text").textContent = data.heatpump.info.text;
  fillList("heatpump-info-details", data.heatpump.info.details);

  // Hydrogen
  document.getElementById("hydrogen-header").textContent = data.hydrogen.header;
  document.getElementById("hydrogen-description").textContent = data.hydrogen.description;
  document.getElementById("hydrogen-info-title").textContent = data.hydrogen.info.title;
  document.getElementById("hydrogen-info-text").textContent = data.hydrogen.info.text;
  fillList("hydrogen-info-details", data.hydrogen.info.details);

  // Battery
  document.getElementById("battery-header").textContent = data.battery.header;
  document.getElementById("battery-description").textContent = data.battery.description;
  document.getElementById("battery-info-title").textContent = data.battery.info.title;
  document.getElementById("battery-info-text").textContent = data.battery.info.text;
  fillList("battery-info-details", data.battery.info.details);
}

document.addEventListener("DOMContentLoaded", loadContent);
