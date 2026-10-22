/* Página de histórico: consulta /api/historico (SQLite reamostrado no servidor). */
"use strict";
const $ = (id) => document.getElementById(id);
const fmt = (ts, horas) => new Date(ts * 1000).toLocaleString("pt-BR",
  horas > 24 ? {day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit"} : {hour: "2-digit", minute: "2-digit"});
let nos = {}, grafico = null;

async function inicia() {
  nos = await (await fetch("/api/nos")).json();
  const bs = Object.keys(nos).sort();
  $("f-bancada").innerHTML = bs.map((b) => `<option>${b}</option>`).join("");
  atualizaGrandezas(); carrega();
}
function atualizaGrandezas() {
  const gs = (nos[$("f-bancada").value] || {grandezas: []}).grandezas;
  $("f-grandeza").innerHTML = gs.map((g) => `<option>${g}</option>`).join("");
}
function params() {
  return new URLSearchParams({bancada: $("f-bancada").value, grandeza: $("f-grandeza").value,
                              horas: $("f-horas").value, pontos: 300});
}
async function carrega() {
  if (!$("f-bancada").value || !$("f-grandeza").value) { $("resumo").textContent = "Sem dados no banco ainda."; return; }
  const p = params(), horas = Number($("f-horas").value);
  $("baixar").href = "/api/historico.csv?" + p;
  const r = await (await fetch("/api/historico?" + p)).json();
  const pts = r.pontos;
  if (grafico) grafico.destroy();
  const base = {pointRadius: 0, borderWidth: 2, tension: 0.2, parsing: false};
  grafico = new Chart($("grafico-hist"), {
    type: "line",
    data: {datasets: [
      {...base, label: "máximo", data: pts.map((q) => ({x: q.t, y: q.max})), borderColor: "#d2691e66", borderDash: [4, 3], borderWidth: 1},
      {...base, label: "média", data: pts.map((q) => ({x: q.t, y: q.media})), borderColor: "#1f6fb2"},
      {...base, label: "mínimo", data: pts.map((q) => ({x: q.t, y: q.min})), borderColor: "#2e7d3266", borderDash: [4, 3], borderWidth: 1},
    ]},
    options: {animation: false, responsive: true, maintainAspectRatio: false,
      scales: {x: {type: "linear", ticks: {maxTicksLimit: 8, callback: (v) => fmt(v, horas)}}},
      plugins: {legend: {position: "bottom"}, tooltip: {callbacks: {title: (it) => fmt(it[0].parsed.x, horas)}}}},
  });
  if (!pts.length) { $("resumo").textContent = "Nenhuma leitura neste período."; return; }
  const n = pts.reduce((a, q) => a + q.n, 0);
  const med = pts.reduce((a, q) => a + q.media * q.n, 0) / n;
  $("resumo").textContent = `${n} leituras em ${pts.length} pontos (balde de ${r.bucket_s} s) · ` +
    `média ${med.toFixed(2)} · mín ${Math.min(...pts.map((q) => q.min)).toFixed(2)} · máx ${Math.max(...pts.map((q) => q.max)).toFixed(2)}`;
}
$("f-bancada").addEventListener("change", () => { atualizaGrandezas(); carrega(); });
$("f-grandeza").addEventListener("change", carrega);
$("f-horas").addEventListener("change", carrega);
(async () => {   // selo do MQTT; esta página não usa WebSocket, então o outro selo some
  const ws = $("selo-ws"); if (ws) ws.remove();
  try { const s = await (await fetch("/api/saude")).json(); $("selo-mqtt").className = "selo " + (s.mqtt ? "on" : "off"); } catch (e) {}
})();
inicia();
