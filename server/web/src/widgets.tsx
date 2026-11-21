// What goes inside each kind of widget. The frame around it (title, drag handle, edit and
// remove buttons) is in App.tsx.
import { useEffect, useState } from "react";
import { getFeed, hostOf, isWeb } from "./api";
import type { AgendaEvent, Dash, FeedReply, Widget } from "./types";

const WEATHER: Record<string, string> = {
  clear: "☀️", "clear-night": "🌙", partly: "⛅", "partly-night": "☁️", cloudy: "☁️", fog: "🌫️",
  drizzle: "🌦️", rain: "🌧️", snow: "❄️", storm: "⛈️",
};
const TILE_COLORS = ["#c2553d", "#2f7d6b", "#3b6fb5", "#8a5bb0", "#b5822a", "#4f7f3a", "#b0456f"];

// A site's icon from the server's cache. Sites without one get a coloured letter.
export function Favicon({ domain, name }: { domain: string; name: string }) {
  const [failed, setFailed] = useState(false);
  useEffect(() => setFailed(false), [domain]);
  if (!domain || failed) {
    let sum = 0;
    for (const ch of name) sum += ch.charCodeAt(0);
    return (
      <span className="favicon letter" style={{ background: TILE_COLORS[sum % TILE_COLORS.length] }}>
        {(name.trim()[0] || "?").toUpperCase()}
      </span>
    );
  }
  return (
    <img className="favicon" alt="" loading="lazy" onError={() => setFailed(true)}
      src={"/api/favicon?domain=" + encodeURIComponent(domain)} />
  );
}

function EventRow({ e, plain }: { e: AgendaEvent; plain?: boolean }) {
  let tag = "";
  if (!plain && e.status === "now") tag = e.minutes_left != null ? `${e.minutes_left} min left` : "Now";
  if (!plain && e.status === "soon") tag = `in ${e.minutes_until} min`;
  return (
    <div className={"event " + (plain ? "" : e.status)}>
      <div className="when">{e.start_label} {e.start_ampm}</div>
      <div className="what">
        <div>{e.title}</div>
        {e.travel && !plain && (
          <div className={"sub " + e.travel.urgency}>
            Leave by {e.travel.leave_label} {e.travel.leave_ampm} · {e.travel.drive_minutes} min drive
          </div>
        )}
        {e.location && (
          <div className="sub">
            {isWeb(e.location) ? <a href={e.location} target="_blank" rel="noopener noreferrer">Join the meeting</a> : e.location}
          </div>
        )}
      </div>
      {tag && <div className="tag">{tag}</div>}
    </div>
  );
}

function Agenda({ dash }: { dash: Dash }) {
  const a = dash.agenda;
  if (!a) return <p className="empty">No calendar connected yet. <a href="/setup">Set one up</a>.</p>;
  const more = a.tomorrow_total - a.tomorrow.length;
  return (
    <>
      {a.all_day.map(t => (
        <div className="event" key={t}><div className="when">All day</div><div className="what">{t}</div></div>
      ))}
      {a.events.map((e, i) => <EventRow e={e} key={i} />)}
      {!a.events.length && !a.all_day.length && <p className="empty">Nothing on the calendar today.</p>}
      {a.free_after && <p className="free">Free after {a.free_after.label} {a.free_after.ampm}</p>}
      {a.tomorrow.length > 0 && (
        <>
          <h3>Tomorrow</h3>
          {a.tomorrow.map((e, i) => <EventRow e={e} plain key={i} />)}
          {more > 0 && <p className="empty">and {more} more</p>}
        </>
      )}
    </>
  );
}

function Weather({ dash }: { dash: Dash }) {
  const w = dash.weather;
  if (!w) return <p className="empty">No forecast yet.</p>;
  return (
    <>
      <div className="now">
        <span className="icon">{WEATHER[w.icon] || ""}</span>
        <span className="temp">{w.temp}°</span>
        <span className="cond">
          {w.condition}<br />H {w.high}° · L {w.low}°{w.air ? ` · AQI ${w.air.value} ${w.air.label}` : ""}
        </span>
      </div>
      <div className="hours">
        {w.hourly.map(h => (
          <div key={h.label}>
            {h.label}<span className="icon">{WEATHER[h.icon] || ""}</span><b>{h.temp}°</b>
            {h.precip >= 20 && <span className="wet">{h.precip}%</span>}
          </div>
        ))}
      </div>
      <div className="facts">
        <span>Sunrise {w.sunrise_label} am</span><span>Sunset {w.sunset_label} pm</span>
        {dash.moon && <span>{dash.moon.name}, {dash.moon.illumination}% lit</span>}
      </div>
    </>
  );
}

function Games({ dash }: { dash: Dash }) {
  if (!dash.games.length) return <p className="empty">No games coming up.</p>;
  return (
    <>
      {dash.games.map((g, i) => {
        const colors = g.colors.filter(c => /^#[0-9a-f]{6}$/i.test(c));
        const background = colors.length > 1
          ? `linear-gradient(90deg, ${colors[0]} 50%, ${colors[1]} 50%)` : colors[0];
        return (
          <div className={"game" + (g.state === "in" ? " live" : "")} key={i}>
            <div className="bars" style={{ background }} />
            <div><div className="line1">{g.line1}</div><div className="line2">{g.line2}</div></div>
            {g.tv && <div className="tv">{g.tv}</div>}
          </div>
        );
      })}
    </>
  );
}

function Snow({ dash }: { dash: Dash }) {
  if (!dash.snow.length) return <p className="empty">No new snow in the last 48 hours.</p>;
  return (
    <>
      {dash.snow.map(s => (
        <div className="spot" key={s.name}>
          <span className="new">{s.new}</span><span className="name">{s.name}</span>
          <span className="sub">{[s.detail, s.temp != null ? s.temp + "°" : ""].filter(Boolean).join(" · ")}</span>
        </div>
      ))}
    </>
  );
}

function Spark({ points, baseline, direction }: { points: number[]; baseline: number | null; direction: string }) {
  const y = (v: number) => 24 - v * 0.22;
  return (
    <svg viewBox="0 0 84 26" className={direction}>
      {baseline != null && (
        <line x1="0" x2="84" y1={y(baseline)} y2={y(baseline)} stroke="currentColor" strokeOpacity=".35" strokeDasharray="2 3" />
      )}
      {points.length > 1 && (
        <polyline fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinejoin="round"
          points={points.map((v, i) => `${((i * 84) / (points.length - 1)).toFixed(1)},${y(v).toFixed(1)}`).join(" ")} />
      )}
    </svg>
  );
}

function Markets({ dash }: { dash: Dash }) {
  const s = dash.stocks;
  if (!s || !s.items.length) return <p className="empty">No symbols set in the server's config.</p>;
  return (
    <>
      {!s.open && <p className="empty">Markets are closed.</p>}
      {s.items.map(i => (
        <div className="stock" key={i.symbol}>
          <span className="name">{i.name || i.symbol}</span>
          <Spark points={i.points || []} baseline={i.baseline} direction={i.direction} />
          <span className="price">{i.price}<span className={"change " + i.direction}>{i.change}</span></span>
        </div>
      ))}
    </>
  );
}

function Photo({ dash }: { dash: Dash }) {
  const [shown, setShown] = useState<{ id: string; caption: string } | null>(null);
  const count = dash.photos.count;
  useEffect(() => {
    if (!count) return;
    let list: { id: string; caption: string }[] = [];
    let stop = false;
    const next = async () => {
      try {
        if (!list.length) list = (await (await fetch("/api/photos")).json()).photos;
        if (!stop && list.length) setShown(list[Math.floor(Math.random() * list.length)]);
      } catch { /* keep the photo already showing */ }
    };
    next();
    const timer = setInterval(next, 60_000);
    return () => { stop = true; clearInterval(timer); };
  }, [count]);
  if (!count) return <p className="empty">No photos yet. <a href="/setup">Choose a folder</a>.</p>;
  if (!shown) return null;
  return (
    <figure>
      <img alt={shown.caption || "Photo"} src={`/api/photo/${encodeURIComponent(shown.id)}?w=900&h=560`} />
      {shown.caption && <figcaption>{shown.caption}</figcaption>}
    </figure>
  );
}

function Tasks({ dash }: { dash: Dash }) {
  const t = dash.tasks;
  if (!t) return <p className="empty">Google Tasks isn't connected yet. <a href="/setup">Set it up</a>.</p>;
  if (!t.lists.length) return <p className="empty">No open tasks.</p>;
  return (
    <>
      {t.lists.map((list, n) => (
        <div key={n}>
          {(t.lists.length > 1 || n > 0) && <h3>{list.title}</h3>}
          {list.items.map((item, i) => (
            <a className={"task" + (item.indent ? " sub" : "")} key={i} title={item.notes || undefined}
              href={isWeb(item.link || "") ? item.link : undefined} target="_blank" rel="noopener noreferrer">
              <span className="ring" /><span className="name">{item.title}</span>
              {item.due && <i className={item.tone}>{item.due}</i>}
            </a>
          ))}
        </div>
      ))}
      {t.more > 0 && <p className="empty">and {t.more} more</p>}
    </>
  );
}

function Feeds({ id, widget }: { id: string; widget: Widget }) {
  const feeds = widget.feeds || [];
  const key = "tab:" + id;
  const [tab, setTab] = useState(() => {
    try { return localStorage.getItem(key) || ""; } catch { return ""; }
  });
  const active = feeds.find(f => f.url === tab) || feeds[0];
  const [state, setState] = useState<{ url: string; reply?: FeedReply; error?: string }>({ url: "" });
  const url = active?.url;
  useEffect(() => {
    if (!url) return;
    let stop = false;
    const load = () =>
      getFeed(url).then(
        reply => !stop && setState({ url, reply }),
        e => !stop && setState({ url, error: e.message }),
      );
    load();
    const timer = setInterval(load, 600_000);
    return () => { stop = true; clearInterval(timer); };
  }, [url]);
  if (!active) return <p className="empty">No feeds yet. Use Customize to add some.</p>;
  const current = state.url === url ? state : { url };
  const items = current.reply?.items || [];
  const pick = (to: string) => {
    setTab(to);
    try { localStorage.setItem(key, to); } catch { /* private window */ }
  };
  return (
    <>
      <div className="tabs" role="tablist">
        {feeds.map(f => (
          <button key={f.url} role="tab" aria-selected={f.url === url} onClick={() => pick(f.url)}>
            <Favicon domain={f.site || hostOf(f.url)} name={f.name} />{f.name}
          </button>
        ))}
      </div>
      {current.error && <p className="empty">Couldn't read this feed: {current.error}</p>}
      {!current.error && !current.reply && <p className="empty">Loading…</p>}
      {items.slice(0, 2).map((item, i) => (
        <a className="lead" key={i} href={isWeb(item.link) ? item.link : undefined} target="_blank" rel="noopener noreferrer">
          {isWeb(item.image) && <img alt="" loading="lazy" src={item.image} onError={e => (e.currentTarget.hidden = true)} />}
          <span>
            <b>{item.title}</b>
            <span className="summary">{item.age && <i>{item.age}{item.summary && " – "}</i>}{item.summary}</span>
          </span>
        </a>
      ))}
      {items.slice(2, 12).map((item, i) => (
        <a className="line" key={i} href={isWeb(item.link) ? item.link : undefined} target="_blank" rel="noopener noreferrer">
          <span>{item.title}</span><i>{item.age}</i>
        </a>
      ))}
    </>
  );
}

function Links({ widget }: { widget: Widget }) {
  const links = widget.links || [];
  if (!links.length) return <p className="empty">No links yet. Use Customize to add some.</p>;
  return (
    <div className="chips">
      {links.map((l, i) => (
        <a key={i} href={isWeb(l.url) ? l.url : undefined} rel="noopener noreferrer">
          <Favicon domain={hostOf(l.url)} name={l.name} />{l.name}
        </a>
      ))}
    </div>
  );
}

export function WidgetBody({ id, widget, dash }: { id: string; widget: Widget; dash: Dash | null }) {
  if (widget.type === "feeds") return <Feeds id={id} widget={widget} />;
  if (widget.type === "links") return <Links widget={widget} />;
  if (!dash) return <p className="empty">Loading…</p>;
  switch (widget.type) {
    case "agenda": return <Agenda dash={dash} />;
    case "weather": return <Weather dash={dash} />;
    case "games": return <Games dash={dash} />;
    case "snow": return <Snow dash={dash} />;
    case "tasks": return <Tasks dash={dash} />;
    case "markets": return <Markets dash={dash} />;
    case "photo": return <Photo dash={dash} />;
  }
}
