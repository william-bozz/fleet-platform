const API = {
  health: "/health",
  trucks: "/api/trucks",
  trailers: "/api/trailers",
  drivers: "/api/drivers",
  loads: "/api/loads",
  fuel: "/api/fuel_entries",
  km: "/api/km_logs",
  pay: "/api/driver_payments",
  stats: "/api/stats/summary",
};

const entities = {
  trucks: {
    title: "Nuevo camión",
    endpoint: API.trucks,
    tableId: "tableTrucks",
    recentTableId: "recentTrucks",
    columns: [
      ["id","ID"], ["unit_number","Unidad"], ["year","Año"], ["make","Marca"], ["model","Modelo"],
      ["engine","Motor"], ["current_km","KM"], ["status","Estatus"]
    ],
    form: [
      { k:"unit_number", label:"unit_number", type:"text", required:true },
      { k:"vin", label:"vin", type:"text" },
      { k:"year", label:"year", type:"number" },
      { k:"make", label:"make", type:"text" },
      { k:"model", label:"model", type:"text" },
      { k:"engine", label:"engine", type:"text" },
      { k:"current_km", label:"current_km", type:"number" },
      { k:"status", label:"status", type:"text" }
    ],
  },
  trailers: {
    title: "Nuevo trailer",
    endpoint: API.trailers,
    tableId: "tableTrailers",
    columns: [
      ["id","ID"], ["unit_number","Unidad"], ["type","Tipo"], ["current_km","KM"], ["status","Estatus"]
    ],
    form: [
      { k:"unit_number", label:"unit_number", type:"text", required:true },
      { k:"vin", label:"vin", type:"text" },
      { k:"type", label:"type", type:"select", required:true, options:["reefer","dry_van","flatbed"] },
      { k:"current_km", label:"current_km", type:"number" },
      { k:"status", label:"status", type:"text" }
    ],
  },
  drivers: {
    title: "Nuevo conductor",
    endpoint: API.drivers,
    tableId: "tableDrivers",
    columns: [
      ["id","ID"], ["name","Nombre"], ["license","Licencia"], ["phone","Teléfono"],
      ["pay_type","pay_type"], ["pay_rate","pay_rate"], ["status","Estatus"]
    ],
    form: [
      { k:"name", label:"name", type:"text", required:true },
      { k:"license", label:"license", type:"text" },
      { k:"phone", label:"phone", type:"text" },
      { k:"pay_type", label:"pay_type", type:"select", options:["", "per_km","percent","salary"] },
      { k:"pay_rate", label:"pay_rate", type:"number" },
      { k:"status", label:"status", type:"text" }
    ],
  },
  loads: {
    title: "Nueva carga",
    endpoint: API.loads,
    tableId: "tableLoads",
    recentTableId: "recentLoads",
    columns: [
      ["id","ID"], ["reference","Referencia"], ["status","Estatus"],
      ["truck_id","Truck"], ["trailer_id","Trailer"], ["driver_id","Driver"],
      ["pickup_location","Pickup"], ["delivery_location","Delivery"], ["distance_km","KM"],
      ["rate","Rate"], ["currency","Moneda"]
    ],
    form: [
      { k:"reference", label:"reference", type:"text", required:true },
      { k:"status", label:"status", type:"select", options:["", "planned","in_transit","delivered","cancelled"] },
      { k:"truck_id", label:"truck_id", type:"number", required:true },
      { k:"trailer_id", label:"trailer_id", type:"number", required:true },
      { k:"driver_id", label:"driver_id", type:"number", required:true },
      { k:"shipper", label:"shipper", type:"text" },
      { k:"pickup_location", label:"pickup_location", type:"text" },
      { k:"delivery_location", label:"delivery_location", type:"text" },
      { k:"pickup_date", label:"pickup_date", type:"text", placeholder:"YYYY-MM-DD" },
      { k:"delivery_date", label:"delivery_date", type:"text", placeholder:"YYYY-MM-DD" },
      { k:"commodity", label:"commodity", type:"text" },
      { k:"weight_kg", label:"weight_kg", type:"number" },
      { k:"distance_km", label:"distance_km", type:"number" },
      { k:"rate", label:"rate", type:"number" },
      { k:"currency", label:"currency", type:"text", placeholder:"USD/MXN" }
    ],
  },
  fuel_entries: {
    title: "Registrar fuel",
    endpoint: API.fuel,
    tableId: "tableFuel",
    recentTableId: "recentFuel",
    columns: [
      ["id","ID"], ["truck_id","Truck"], ["liters","Litros"], ["total_cost","Costo"],
      ["currency","Moneda"], ["odometer_km","OdoKM"], ["location","Ubicación"],
      ["driver_id","Driver"], ["load_id","Load"], ["fueled_at","Fecha"]
    ],
    form: [
      { k:"truck_id", label:"truck_id", type:"number", required:true },
      { k:"liters", label:"liters", type:"number", required:true },
      { k:"total_cost", label:"total_cost", type:"number" },
      { k:"currency", label:"currency", type:"text", placeholder:"USD/MXN" },
      { k:"odometer_km", label:"odometer_km", type:"number" },
      { k:"location", label:"location", type:"text" },
      { k:"driver_id", label:"driver_id", type:"number" },
      { k:"load_id", label:"load_id", type:"number" },
      { k:"fueled_at", label:"fueled_at", type:"text", placeholder:"YYYY-MM-DD HH:MM" }
    ],
  },
  km_logs: {
    title: "Registrar km",
    endpoint: API.km,
    tableId: "tableKm",
    columns: [
      ["id","ID"], ["truck_id","Truck"], ["km","KM"], ["odometer_start","Odo start"],
      ["odometer_end","Odo end"], ["load_id","Load"], ["logged_at","Fecha"]
    ],
    form: [
      { k:"truck_id", label:"truck_id", type:"number", required:true },
      { k:"km", label:"km", type:"number", required:true },
      { k:"odometer_start", label:"odometer_start", type:"number" },
      { k:"odometer_end", label:"odometer_end", type:"number" },
      { k:"load_id", label:"load_id", type:"number" },
      { k:"logged_at", label:"logged_at", type:"text", placeholder:"YYYY-MM-DD HH:MM" }
    ],
  },
  driver_payments: {
    title: "Registrar pago",
    endpoint: API.pay,
    tableId: "tablePay",
    columns: [
      ["id","ID"], ["driver_id","Driver"], ["amount","Monto"], ["currency","Moneda"],
      ["method","Método"], ["load_id","Load"], ["paid_at","Fecha"], ["notes","Notas"]
    ],
    form: [
      { k:"driver_id", label:"driver_id", type:"number", required:true },
      { k:"amount", label:"amount", type:"number", required:true },
      { k:"currency", label:"currency", type:"text", placeholder:"USD/MXN" },
      { k:"method", label:"method", type:"text" },
      { k:"load_id", label:"load_id", type:"number" },
      { k:"paid_at", label:"paid_at", type:"text", placeholder:"YYYY-MM-DD HH:MM" },
      { k:"notes", label:"notes", type:"text" }
    ],
  },
};

function $(id){ return document.getElementById(id); }

function escapeHtml(s){
  return String(s ?? "")
    .replaceAll("&","&amp;")
    .replaceAll("<","&lt;")
    .replaceAll(">","&gt;")
    .replaceAll('"',"&quot;")
    .replaceAll("'","&#039;");
}

function toast(kind, title, msg){
  const wrap = $("toasts");
  const el = document.createElement("div");
  el.className = `toast ${kind}`;
  el.innerHTML = `<div class="toast-title">${escapeHtml(title)}</div><div class="toast-msg">${escapeHtml(msg)}</div>`;
  wrap.appendChild(el);
  setTimeout(() => el.remove(), 3200);
}

async function fetchJson(url, opts){
  const r = await fetch(url, opts);
  const text = await r.text();
  let data = null;
  try{ data = text ? JSON.parse(text) : null; }catch{ data = { raw: text }; }
  if(!r.ok){
    const msg = data?.error ? `error: ${data.error}` : `HTTP ${r.status}`;
    throw new Error(msg);
  }
  return data;
}

function formatNumber(n){
  const x = Number(n) || 0;
  try{ return new Intl.NumberFormat("es-MX", { maximumFractionDigits: 2 }).format(x); }
  catch{ return String(x); }
}

function sumBy(arr, key){
  if(!Array.isArray(arr)) return 0;
  return arr.reduce((a, it) => a + (Number(it?.[key]) || 0), 0);
}

function buildTable(tableEl, columns, rows, maxRows = 200){
  const safeRows = Array.isArray(rows) ? rows.slice(0, maxRows) : [];
  const thead = `<thead><tr>${columns.map(c => `<th>${escapeHtml(c[1])}</th>`).join("")}</tr></thead>`;
  const tbody = `<tbody>${
    safeRows.map(r => `<tr>${
      columns.map(c => `<td>${escapeHtml(r?.[c[0]] ?? "")}</td>`).join("")
    }</tr>`).join("")
  }</tbody>`;
  tableEl.innerHTML = thead + tbody;
}

async function loadEntity(key, { recent=false } = {}){
  const ent = entities[key];
  if(!ent) return;
  const data = await fetchJson(ent.endpoint);
  if(ent.tableId && $(ent.tableId)) buildTable($(ent.tableId), ent.columns, data);
  if(recent && ent.recentTableId && $(ent.recentTableId)) buildTable($(ent.recentTableId), ent.columns, data, 6);
  return data;
}

async function refreshStats(){
  const data = await fetchJson(API.stats);
  if($("statsRaw")) $("statsRaw").textContent = JSON.stringify(data, null, 2);

  $("kpiLiters").textContent = formatNumber(sumBy(data?.fuel_by_truck, "liters_total"));
  $("kpiFuelCost").textContent = formatNumber(sumBy(data?.fuel_by_truck, "cost_total"));
  $("kpiKm").textContent = formatNumber(sumBy(data?.km_by_truck, "km_total"));
  $("kpiPay").textContent = formatNumber(sumBy(data?.pay_by_driver, "pay_total"));
}

async function refreshHealth(){
  const pill = $("healthPill");
  const text = $("healthText");
  try{
    const data = await fetchJson(API.health);
    pill.classList.remove("bad"); pill.classList.add("ok");
    text.textContent = data?.version ? `OK v${data.version}` : "OK";
  }catch{
    pill.classList.remove("ok"); pill.classList.add("bad");
    text.textContent = "Sin conexión";
  }
}

function switchView(view){
  document.querySelectorAll(".nav-item").forEach(b => {
    b.classList.toggle("is-active", b.dataset.view === view);
  });
  document.querySelectorAll(".view").forEach(v => {
    v.classList.toggle("is-active", v.id === `view-${view}`);
  });
}

const modal = {
  el: $("modal"),
  title: $("modalTitle"),
  form: $("modalForm"),
  hint: $("modalHint"),
  submit: $("modalSubmit"),
  currentKey: null,
};

function openModalFor(key){
  const ent = entities[key];
  if(!ent) return;

  modal.currentKey = key;
  modal.title.textContent = ent.title || "Nuevo";
  modal.hint.textContent = ent.endpoint;

  modal.form.innerHTML = ent.form.map(f => {
    const id = `f_${key}_${f.k}`;
    const req = f.required ? "required" : "";
    const placeholder = f.placeholder ? `placeholder="${escapeHtml(f.placeholder)}"` : "";
    if(f.type === "select"){
      const opts = (f.options || []).map(o => {
        const label = o === "" ? "(vacío)" : o;
        return `<option value="${escapeHtml(o)}">${escapeHtml(label)}</option>`;
      }).join("");
      return `
        <div class="field">
          <div class="label">${escapeHtml(f.label)}${f.required ? " (requerido)" : ""}</div>
          <select class="select" id="${id}" ${req}>${opts}</select>
        </div>`;
    }
    const type = f.type === "number" ? "number" : "text";
    return `
      <div class="field">
        <div class="label">${escapeHtml(f.label)}${f.required ? " (requerido)" : ""}</div>
        <input class="input" id="${id}" type="${type}" ${placeholder} ${req} />
      </div>`;
  }).join("");

  modal.el.setAttribute("aria-hidden", "false");
}

function closeModal(){
  modal.el.setAttribute("aria-hidden", "true");
  modal.currentKey = null;
  modal.form.innerHTML = "";
}

function numOrUndef(v){
  if(v === "" || v == null) return undefined;
  const n = Number(v);
  return Number.isFinite(n) ? n : undefined;
}
function strOrUndef(v){
  if(v == null) return undefined;
  const s = String(v).trim();
  return s ? s : undefined;
}

function collectPayload(key){
  const ent = entities[key];
  const payload = {};
  for(const f of ent.form){
    const el = document.getElementById(`f_${key}_${f.k}`);
    const raw = el ? el.value : "";

    if(f.required && String(raw).trim() === ""){
      throw new Error(`Falta: ${f.k}`);
    }

    if(f.type === "number"){
      const n = numOrUndef(raw);
      if(n !== undefined) payload[f.k] = n;
    }else{
      const s = strOrUndef(raw);
      if(s !== undefined) payload[f.k] = s;
    }
  }
  return payload;
}

async function submitModal(){
  const key = modal.currentKey;
  if(!key) return;
  const ent = entities[key];

  try{
    const payload = collectPayload(key);
    const data = await fetchJson(ent.endpoint, {
      method: "POST",
      headers: {"Content-Type":"application/json"},
      body: JSON.stringify(payload)
    });
    toast("ok", "Guardado", `ID: ${data?.id ?? "ok"}`);
    closeModal();

    await loadEntity(key);
    await refreshStats();
  }catch(e){
    toast("bad", "No se pudo guardar", e.message);
  }
}

function wireUI(){
  document.querySelectorAll(".nav-item").forEach(b => {
    b.addEventListener("click", () => switchView(b.dataset.view));
  });

  document.querySelectorAll("[data-open-create]").forEach(b => {
    b.addEventListener("click", () => openModalFor(b.dataset.openCreate));
  });

  document.querySelectorAll("[data-refresh]").forEach(b => {
    b.addEventListener("click", async () => {
      const key = b.dataset.refresh;
      try{
        await loadEntity(key);
        await refreshStats();
        toast("ok", "Actualizado", key);
      }catch(e){
        toast("bad", "Error", e.message);
      }
    });
  });

  $("btnRefreshStats")?.addEventListener("click", async () => {
    try{ await refreshStats(); toast("ok","KPIs","Actualizados"); }catch(e){ toast("bad","KPIs", e.message); }
  });
  $("btnRefreshStats2")?.addEventListener("click", async () => {
    try{ await refreshStats(); toast("ok","KPIs","Actualizados"); }catch(e){ toast("bad","KPIs", e.message); }
  });

  $("btnRefreshRecent")?.addEventListener("click", async () => {
    try{
      await loadEntity("trucks", { recent:true });
      await loadEntity("loads", { recent:true });
      await loadEntity("fuel_entries", { recent:true });
      toast("ok","Recientes","Actualizados");
    }catch(e){
      toast("bad","Recientes", e.message);
    }
  });

  modal.submit.addEventListener("click", submitModal);
  modal.el.addEventListener("click", (e) => {
    if(e.target && e.target.hasAttribute("data-close-modal")) closeModal();
  });
  document.querySelectorAll("[data-close-modal]").forEach(x => x.addEventListener("click", closeModal));
  document.addEventListener("keydown", (e) => { if(e.key === "Escape") closeModal(); });
}

async function initialLoad(){
  try{
    await refreshHealth();
    await loadEntity("trucks", { recent:true });
    await loadEntity("trailers");
    await loadEntity("drivers");
    await loadEntity("loads", { recent:true });
    await loadEntity("fuel_entries", { recent:true });
    await loadEntity("km_logs");
    await loadEntity("driver_payments");
    await refreshStats();
  }catch(e){
    toast("bad", "Carga inicial", e.message);
  }
}

wireUI();
initialLoad();
setInterval(refreshHealth, 6000);
