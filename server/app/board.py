"""The browser dashboard at `/`: the same content as the desk screen, for any browser on the
home network. One static page; it reads /api/dashboard every 30 seconds and draws itself.

Everything from outside (headlines, event titles, team names) is put on the page as plain
text, never as HTML, and links are only followed when they start with http:// or https://.
"""

PAGE = r"""<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="referrer" content="no-referrer">
<title>Desk companion</title>
<style>
:root{--bg:#0b0b0b;--panel:#1a1a19;--news:#18222e;--snow:#122a36;--line:#2c2c2a;--text:#f1efe8;
--muted:#b4b2a9;--dim:#888780;--amber:#ef9f27;--ambertext:#fac775;--red:#e24b4a;--redtext:#f09595;
--green:#5dcaa5;--blue:#85b7eb}
@media (prefers-color-scheme:light){:root{--bg:#f6f5f1;--panel:#fff;--news:#eaf1f9;--snow:#e4f1f6;
--line:#dddbd3;--text:#1f1e1c;--muted:#55544f;--dim:#7b7a74;--amber:#b86f08;--ambertext:#854f0b;
--red:#c93433;--redtext:#a32d2d;--green:#0f6e56;--blue:#185fa5}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif}
a{color:inherit}
main{max-width:1320px;margin:0 auto;padding:20px 20px 48px}
header{display:flex;flex-wrap:wrap;align-items:flex-end;justify-content:space-between;gap:16px 32px;margin-bottom:20px}
#date{color:var(--muted);font-size:18px}
#clock{font-size:76px;line-height:1;font-weight:300;letter-spacing:-2px;font-variant-numeric:tabular-nums}
#clock small{font-size:24px;color:var(--muted);letter-spacing:0;margin-left:8px}
#now{display:flex;align-items:center;gap:14px}
#now .icon{font-size:44px}#now .temp{font-size:48px;font-weight:300}
#now .cond{color:var(--muted);font-size:15px;line-height:1.4}
.grid{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1.25fr) minmax(0,1fr);gap:16px;align-items:start}
@media (max-width:1050px){.grid{grid-template-columns:1fr 1fr}.grid>.col:last-child{grid-column:1/-1}}
@media (max-width:700px){.grid{grid-template-columns:1fr}#clock{font-size:56px}}
.col{display:flex;flex-direction:column;gap:16px;min-width:0}
section{background:var(--panel);border-radius:14px;padding:16px 18px;min-width:0}
section.news{background:var(--news)}section.snow{background:var(--snow)}
h2{margin:0 0 10px;font-size:14px;font-weight:500;color:var(--dim);text-transform:uppercase;letter-spacing:.06em}
h2 span{float:right;text-transform:none;letter-spacing:0}
.empty{color:var(--dim);margin:4px 0}
.event{display:flex;gap:14px;padding:9px 10px;margin:0 -10px;border-radius:10px;border:1.5px solid transparent}
.event .when{flex:0 0 78px;color:var(--muted);font-variant-numeric:tabular-nums;white-space:nowrap}
.event .what{flex:1;min-width:0;overflow-wrap:anywhere}
.event .sub{color:var(--dim);font-size:14px}
.event .tag{flex:none;font-size:14px;color:var(--dim);white-space:nowrap}
.event.past{opacity:.45}
.event.soon{border-color:var(--amber)}.event.soon .tag{color:var(--ambertext)}
.event.now .when,.event.now .tag{color:var(--redtext)}
.event.now .tag::after{content:"";display:inline-block;width:9px;height:9px;border-radius:50%;background:var(--red);margin-left:8px}
.sub.soon{color:var(--ambertext)}.sub.now{color:var(--redtext)}
.free{color:var(--green);margin:10px 0 0}
.story{padding:11px 0;border-top:1px solid var(--line)}
.story:first-of-type{border-top:0;padding-top:2px}
.story .meta{color:var(--blue);font-size:13px}
.story .title{display:block;font-size:18px;line-height:1.3;margin:2px 0;text-decoration:none}
a.title:hover{text-decoration:underline}
.story .summary{color:var(--muted);font-size:14px;margin:0}
.hours{display:flex;justify-content:space-between;text-align:center;gap:6px}
.hours div{flex:1;color:var(--muted);font-size:14px}
.hours .icon{font-size:24px;display:block;margin:2px 0}
.hours b{font-weight:500;color:var(--text);font-size:16px}
.hours .wet{color:var(--blue);font-size:12px;display:block}
.facts{display:flex;flex-wrap:wrap;gap:4px 18px;color:var(--muted);font-size:14px;margin-top:12px;
padding-top:10px;border-top:1px solid var(--line)}
.game{display:flex;gap:12px;align-items:center;padding:7px 0}
.game .bars{flex:none;width:10px;height:38px;border-radius:3px}
.game .line1{color:var(--muted);font-size:14px}.game.live .line2{color:var(--redtext)}
.game .tv{margin-left:auto;color:var(--dim);font-size:13px}
.spot{display:flex;align-items:baseline;gap:10px;padding:5px 0}
.spot .new{font-size:24px;min-width:48px}.spot .name{flex:1}.spot .sub{color:var(--muted);font-size:14px}
.stock{display:flex;align-items:center;gap:10px;padding:6px 0}
.stock .name{flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.stock svg{flex:none;width:84px;height:26px}
.stock .price{font-variant-numeric:tabular-nums;text-align:right}
.stock .change{font-size:13px;display:block}
.up{color:var(--green)}.down{color:var(--redtext)}.flat{color:var(--dim)}
#photo img{display:block;width:100%;border-radius:8px;aspect-ratio:16/10;object-fit:cover;background:var(--line)}
#photo figcaption{color:var(--dim);font-size:13px;margin-top:6px}
figure{margin:0}
footer{margin-top:22px;color:var(--dim);font-size:14px;display:flex;gap:18px;flex-wrap:wrap}
#problem{color:var(--ambertext)}
</style>
<main>
<header>
  <div><div id="date">Connecting…</div><div id="clock"></div></div>
  <div id="now"></div>
</header>
<div class="grid">
  <div class="col"><section id="agenda"><h2>Today</h2></section><section id="tomorrow" hidden></section></div>
  <div class="col"><section id="news" class="news"><h2>News</h2></section></div>
  <div class="col">
    <section id="weather" hidden></section>
    <section id="games" hidden></section>
    <section id="snow" class="snow" hidden></section>
    <section id="stocks" hidden></section>
    <section id="photo" hidden></section>
  </div>
</div>
<footer><span id="problem"></span><a href="/setup">Setup</a></footer>
</main>
<script>
"use strict";
const ICONS = {clear:"☀️","clear-night":"🌙",partly:"⛅","partly-night":"☁️",cloudy:"☁️",fog:"🌫️",
  drizzle:"🌦️",rain:"🌧️",snow:"❄️",storm:"⛈️"};
const $ = id => document.getElementById(id);
let zone;  // the server's timezone, so the clock matches the desk screen wherever the browser is

// el("div", "cls", "text" or [children]): text always goes in as text, never as HTML.
function el(tag, cls, content) {
  const node = document.createElement(tag);
  if (cls) node.className = cls;
  if (Array.isArray(content)) node.append(...content.filter(Boolean));
  else if (content != null) node.textContent = content;
  return node;
}
function link(url, cls, text) {
  if (!/^https?:\/\//i.test(url || "")) return null;
  const a = el("a", cls, text);
  a.href = url; a.target = "_blank"; a.rel = "noopener noreferrer";
  return a;
}
function fill(id, title, children, note) {
  const box = $(id);
  const head = el("h2", "", title);
  if (note) head.append(el("span", "", note));
  box.replaceChildren(head, ...children.filter(Boolean));
  box.hidden = false;
}
const hide = id => { $(id).hidden = true; };

function tick() {
  const opts = zone ? {timeZone: zone} : {};
  const now = new Date();
  try {
    const parts = new Intl.DateTimeFormat("en-US", {...opts, hour: "numeric", minute: "2-digit"}).formatToParts(now);
    const get = type => (parts.find(p => p.type === type) || {}).value || "";
    $("clock").replaceChildren(get("hour") + ":" + get("minute"), el("small", "", get("dayPeriod").toLowerCase()));
    if (zone) $("date").textContent = new Intl.DateTimeFormat("en-US",
      {...opts, weekday: "long", month: "long", day: "numeric"}).format(now);
  } catch (e) { zone = undefined; }
}

function eventRow(e, plain) {
  let tag = "";
  if (!plain && e.status === "now") tag = e.minutes_left != null ? e.minutes_left + " min left" : "Now";
  if (!plain && e.status === "soon") tag = "in " + e.minutes_until + " min";
  const what = [el("div", "", e.title)];
  if (e.travel && !plain) {
    what.push(el("div", "sub " + e.travel.urgency,
      `Leave by ${e.travel.leave_label} ${e.travel.leave_ampm} · ${e.travel.drive_minutes} min drive`));
  }
  if (e.location) {
    const join = link(e.location, "", "Join the meeting");
    what.push(el("div", "sub", join ? [join] : e.location));
  }
  return el("div", "event " + (plain ? "" : e.status), [
    el("div", "when", `${e.start_label} ${e.start_ampm}`), el("div", "what", what), tag && el("div", "tag", tag)]);
}

function drawAgenda(a) {
  if (!a) return fill("agenda", "Today", [el("p", "empty", "No calendar connected yet.")]);
  const rows = a.all_day.map(t => el("div", "event", [el("div", "when", "All day"), el("div", "what", t)]));
  rows.push(...a.events.map(e => eventRow(e, false)));
  if (!rows.length) rows.push(el("p", "empty", "Nothing on the calendar today."));
  if (a.free_after) rows.push(el("p", "free", `Free after ${a.free_after.label} ${a.free_after.ampm}`));
  fill("agenda", "Today", rows);
  if (!a.tomorrow.length) return hide("tomorrow");
  const more = a.tomorrow_total - a.tomorrow.length;
  fill("tomorrow", "Tomorrow", [...a.tomorrow.map(e => eventRow(e, true)),
    more > 0 && el("p", "empty", `and ${more} more`)]);
}

function drawNews(items) {
  fill("news", "News", items.length ? items.map(n => el("div", "story", [
    el("div", "meta", [n.source, n.age].filter(Boolean).join(" · ")),
    link(n.link, "title", n.title) || el("span", "title", n.title),
    n.summary && el("p", "summary", n.summary)])) : [el("p", "empty", "No headlines right now.")]);
}

function drawWeather(w, moon) {
  if (!w) { $("now").replaceChildren(); return hide("weather"); }
  const air = w.air ? ` · AQI ${w.air.value} ${w.air.label}` : "";
  $("now").replaceChildren(el("span", "icon", ICONS[w.icon] || ""), el("span", "temp", w.temp + "°"),
    el("span", "cond", [w.condition, el("br"), `H ${w.high}° · L ${w.low}°${air}`]));
  const hours = el("div", "hours", w.hourly.map(h => el("div", "", [h.label,
    el("span", "icon", ICONS[h.icon] || ""), el("b", "", h.temp + "°"),
    h.precip >= 20 && el("span", "wet", h.precip + "%")])));
  const facts = el("div", "facts", [el("span", "", `Sunrise ${w.sunrise_label} am`),
    el("span", "", `Sunset ${w.sunset_label} pm`),
    moon && el("span", "", `${moon.name}, ${moon.illumination}% lit`)]);
  fill("weather", "Weather", [hours, facts]);
}

function drawGames(games) {
  if (!games.length) return hide("games");
  fill("games", "Games", games.map(g => {
    const bars = el("div", "bars");
    const colors = (g.colors || []).filter(c => /^#[0-9a-f]{6}$/i.test(c));
    if (colors.length) bars.style.background = colors.length > 1
      ? `linear-gradient(90deg, ${colors[0]} 50%, ${colors[1]} 50%)` : colors[0];
    return el("div", "game" + (g.state === "in" ? " live" : ""), [bars,
      el("div", "", [el("div", "line1", g.line1), el("div", "line2", g.line2)]), g.tv && el("div", "tv", g.tv)]);
  }));
}

function drawSnow(spots) {
  if (!spots.length) return hide("snow");
  fill("snow", "Snow, 48 hours", spots.map(s => el("div", "spot", [el("span", "new", s.new),
    el("span", "name", s.name), el("span", "sub", [s.detail, s.temp != null && s.temp + "°"].filter(Boolean).join(" · "))])));
}

function spark(points, baseline, direction) {
  const NS = "http://www.w3.org/2000/svg";
  const svg = document.createElementNS(NS, "svg");
  svg.setAttribute("viewBox", "0 0 84 26"); svg.setAttribute("class", direction);
  if (points.length < 2) return svg;
  const y = v => (24 - v * 0.22).toFixed(1);
  if (baseline != null) {
    const base = document.createElementNS(NS, "line");
    for (const [k, v] of Object.entries({x1: 0, x2: 84, y1: y(baseline), y2: y(baseline), stroke: "currentColor",
      "stroke-opacity": .35, "stroke-dasharray": "2 3"})) base.setAttribute(k, v);
    svg.append(base);
  }
  const line = document.createElementNS(NS, "polyline");
  line.setAttribute("points", points.map((v, i) => (i * 84 / (points.length - 1)).toFixed(1) + "," + y(v)).join(" "));
  for (const [k, v] of Object.entries({fill: "none", stroke: "currentColor", "stroke-width": 1.6,
    "stroke-linejoin": "round"})) line.setAttribute(k, v);
  svg.append(line);
  return svg;
}

function drawStocks(s) {
  if (!s || !s.items.length) return hide("stocks");
  fill("stocks", "Markets", s.items.map(i => el("div", "stock", [el("span", "name", i.name || i.symbol),
    spark(i.points || [], i.baseline, i.direction),
    el("span", "price", [i.price, el("span", "change " + i.direction, i.change)])])), s.open ? "" : "Closed");
}

let photos = [], photoAt = 0;
async function drawPhoto(count) {
  if (!count) return hide("photo");
  if (Date.now() - photoAt < 60000) return;
  photoAt = Date.now();
  try {
    if (photos.length !== count) photos = (await (await fetch("/api/photos")).json()).photos;
    const pick = photos[Math.floor(Math.random() * photos.length)];
    if (!pick) return hide("photo");
    const img = el("img");
    img.alt = pick.caption || "Photo"; img.src = `/api/photo/${encodeURIComponent(pick.id)}?w=800&h=500`;
    fill("photo", "Photo", [el("figure", "", [img, pick.caption && el("figcaption", "", pick.caption)])]);
  } catch (e) { hide("photo"); }
}

async function refresh() {
  try {
    const reply = await fetch("/api/dashboard", {cache: "no-store"});
    if (!reply.ok) throw new Error(reply.status);
    const d = await reply.json();
    zone = d.time.timezone;
    tick();
    drawAgenda(d.agenda); drawNews(d.news || []); drawWeather(d.weather, d.moon);
    drawGames(d.games || []); drawSnow(d.snow || []); drawStocks(d.stocks);
    drawPhoto(d.photos ? d.photos.count : 0);
    $("problem").textContent = "";
  } catch (e) {
    $("problem").textContent = "Can't reach the server. Showing what was last received.";
  }
}
tick(); setInterval(tick, 1000);
refresh(); setInterval(refresh, 30000);
</script>
</html>
"""
