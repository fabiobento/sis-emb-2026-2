/* Painel ao vivo: Socket.IO + Chart.js, sem build, sem framework. */
"use strict";
const UNIDADES = {temp: "°C", umid: "%", dist: "cm", tensao: "V", corrente: "A", potencia: "W", luz: "lux"};
const CORES = ["#1f6fb2", "#d2691e", "#2e7d32", "#b03030", "#6a3d9a", "#00838f"];
const MAX_PONTOS = 120, VELHO_S = 30;

const estado = {nos: {}, series: {}, grandeza: null};
const $ = (id) => document.getElementById(id);
const fmtHora = (ts) => new Date(ts * 1000).toLocaleTimeString("pt-BR");

/* ---------------- tempo real ---------------- */
const sock = io();            // mesma origem: passa pelo nginx em produção
sock.on("connect", () => selo("selo-ws", true));
sock.on("disconnect", () => selo("selo-ws", false));
function selo(id, ok) { $(id).className = "selo " + (ok ? "on" : "off"); }

sock.on("estado", (s) => {    // fotografia inicial, enviada só a este navegador
  estado.nos = s.nos; estado.series = s.series;
  selo("selo-mqtt", s.mqtt);
  desenhaNos(); desenhaEventos(s.eventos); preencheGrandezas(); montaGrafico();
});
sock.on("leitura", (d) => {
  const no = garanteNo(d.bancada);
  no.ultimo[d.grandeza] = {valor: d.valor, ts: d.ts};
  const por = (estado.series[d.grandeza] ||= {});
  const s = (por[d.bancada] ||= []);
  s.push([d.ts, d.valor]); if (s.length > MAX_PONTOS) s.shift();
  selo("selo-mqtt", true);
  desenhaNos(); preencheGrandezas();
  if (d.grandeza === estado.grandeza) atualizaGrafico();
});
sock.on("status", (d) => {
  const no = garanteNo(d.bancada); no.status = d.texto; no.status_ts = d.ts;
  desenhaNos(); eventoNaLista({ts: d.ts, bancada: d.bancada, tipo: "status", texto: d.texto});
});
sock.on("movimento", (d) => {
  garanteNo(d.bancada).movimento = {classe: d.classe, conf: d.conf, ts: d.ts};
  desenhaNos(); eventoNaLista({ts: d.ts, bancada: d.bancada, tipo: "movimento", texto: d.classe, extra: d.conf});
});
sock.on("comando", (d) => eventoNaLista({ts: d.ts, bancada: d.bancada, tipo: "comando", texto: d.texto}));

function garanteNo(b) {
  return (estado.nos[b] ||= {status: "desconhecido", ultimo: {}, movimento: null});
}

/* ---------------- cartões dos nós ---------------- */
function desenhaNos() {
  const nomes = Object.keys(estado.nos).sort();
  const raiz = $("nos");
  if (!nomes.length) return;
  raiz.innerHTML = "";
  for (const b of nomes) {
    const n = estado.nos[b], el = document.createElement("div");
    el.className = "no";
    const valores = Object.keys(n.ultimo).sort().map((g) => {
      const u = n.ultimo[g], idade = Date.now() / 1000 - u.ts;
      return `<div class="valor ${idade > VELHO_S ? "velho" : ""}"><span class="n">${u.valor.toFixed(1)}</span>` +
             `<span class="u">${UNIDADES[g] || g}</span><span class="r">${g} · há ${Math.round(idade)} s</span></div>`;
    }).join("");
    const mov = n.movimento
      ? `<div class="mov">movimento: <span class="chip">${esc(n.movimento.classe)}</span> ${Math.round(n.movimento.conf * 100)}%</div>` : "";
    el.innerHTML =
      `<div class="no-topo"><h3>${esc(b)}</h3><span class="pilula ${esc(n.status)}">${esc(n.status)}</span></div>` +
      `<div class="valores">${valores || '<span class="vazio">sem leituras</span>'}</div>${mov}` +
      `<div class="acoes"><button class="liga" data-b="${esc(b)}" data-v="1">LED on</button>` +
      `<button data-b="${esc(b)}" data-v="0">LED off</button></div>`;
    raiz.appendChild(el);
  }
}
$("nos").addEventListener("click", (ev) => {
  const bt = ev.target.closest("button[data-b]");
  if (bt) comanda(bt.dataset.b, Number(bt.dataset.v));
});
setInterval(() => { if (Object.keys(estado.nos).length) desenhaNos(); }, 1000);   // atualiza o "há X s"

function esc(s) { return String(s).replace(/[&<>"']/g, (c) => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}[c])); }

/* ---------------- comandos (REST + token opcional) ---------------- */
async function comanda(bancada, valor) {
  const token = sessionStorage.getItem("dash_token") || "";
  const r = await fetch("/api/cmd/led", {
    method: "POST",
    headers: {"Content-Type": "application/json", ...(token ? {Authorization: "Bearer " + token} : {})},
    body: JSON.stringify({bancada, valor}),
  });
  if (r.status === 401) { pedeToken(); return; }
  toast(r.ok ? `LED ${valor ? "ligado" : "desligado"} em ${bancada}` : "falha: " + ((await r.json()).erro || r.status));
}
function pedeToken() {
  const dlg = $("dlg-token");
  dlg.showModal();
  dlg.addEventListener("close", () => {
    if (dlg.returnValue === "ok") sessionStorage.setItem("dash_token", $("in-token").value);
  }, {once: true});
}
function toast(t) {
  const el = $("toast"); el.textContent = t; el.classList.add("vis");
  setTimeout(() => el.classList.remove("vis"), 2500);
}

/* ---------------- eventos ---------------- */
function desenhaEventos(lista) { $("eventos").innerHTML = ""; [...lista].reverse().forEach(eventoNaLista); }
function eventoNaLista(e) {
  const ul = $("eventos"), li = document.createElement("li");
  const extra = e.tipo === "movimento" && e.extra != null ? ` (${Math.round(e.extra * 100)}%)` : "";
  li.innerHTML = `<span class="h">${fmtHora(e.ts)}</span><span><strong>${esc(e.bancada)}</strong> · ${esc(e.tipo)}: ${esc(e.texto)}${extra}</span>`;
  ul.prepend(li); while (ul.children.length > 20) ul.lastChild.remove();
}

/* ---------------- gráfico ---------------- */
let grafico = null;
function preencheGrandezas() {
  const sel = $("sel-grandeza"), gs = Object.keys(estado.series).sort();
  if ([...sel.options].map((o) => o.value).join() === gs.join()) return;
  sel.innerHTML = gs.map((g) => `<option>${esc(g)}</option>`).join("");
  if (!estado.grandeza || !gs.includes(estado.grandeza)) estado.grandeza = gs.includes("temp") ? "temp" : gs[0];
  sel.value = estado.grandeza; montaGrafico();
}
$("sel-grandeza").addEventListener("change", (e) => { estado.grandeza = e.target.value; montaGrafico(); });

function datasets() {
  const por = estado.series[estado.grandeza] || {};
  return Object.keys(por).sort().map((b, i) => ({
    label: b, data: por[b].map(([t, v]) => ({x: t, y: v})),
    borderColor: CORES[i % CORES.length], backgroundColor: CORES[i % CORES.length],
    borderWidth: 2, pointRadius: 0, tension: 0.25,
  }));
}
function montaGrafico() {
  if (!estado.grandeza) return;
  if (grafico) grafico.destroy();
  grafico = new Chart($("grafico"), {
    type: "line", data: {datasets: datasets()},
    options: {
      animation: false, responsive: true, maintainAspectRatio: false, parsing: false,
      scales: {
        x: {type: "linear", ticks: {maxTicksLimit: 6, callback: (v) => fmtHora(v)}},
        y: {title: {display: true, text: `${estado.grandeza} (${UNIDADES[estado.grandeza] || "—"})`}},
      },
      plugins: {legend: {position: "bottom"}, tooltip: {callbacks: {title: (it) => fmtHora(it[0].parsed.x)}}},
    },
  });
}
function atualizaGrafico() {
  if (!grafico) return montaGrafico();
  const novos = datasets();
  if (novos.length !== grafico.data.datasets.length) return montaGrafico();
  novos.forEach((d, i) => (grafico.data.datasets[i].data = d.data));
  grafico.update("none");
}
