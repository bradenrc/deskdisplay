// The edit window for one widget: its title, and for feed and link widgets the list inside.
import { useState } from "react";
import { findFeed, hostOf, isWeb } from "./api";
import type { Feed, LinkItem, Widget } from "./types";
import { Favicon } from "./widgets";

function move<T>(list: T[], from: number, by: number): T[] {
  const to = from + by;
  if (to < 0 || to >= list.length) return list;
  const next = [...list];
  [next[from], next[to]] = [next[to], next[from]];
  return next;
}

function RowButtons({ index, count, onMove, onRemove }: {
  index: number; count: number; onMove: (by: number) => void; onRemove: () => void;
}) {
  return (
    <span className="rowbuttons">
      <button type="button" title="Move up" disabled={index === 0} onClick={() => onMove(-1)}>↑</button>
      <button type="button" title="Move down" disabled={index === count - 1} onClick={() => onMove(1)}>↓</button>
      <button type="button" title="Remove" onClick={onRemove}>✕</button>
    </span>
  );
}

export default function EditDialog({ widget, onSave, onCancel }: {
  widget: Widget; onSave: (w: Widget) => void; onCancel: () => void;
}) {
  const [title, setTitle] = useState(widget.title);
  const [feeds, setFeeds] = useState<Feed[]>(widget.feeds || []);
  const [links, setLinks] = useState<LinkItem[]>(widget.links || []);
  const [address, setAddress] = useState("");
  const [name, setName] = useState("");
  const [busy, setBusy] = useState(false);
  const [problem, setProblem] = useState("");

  async function addFeed() {
    if (!address.trim()) return;
    setBusy(true);
    setProblem("");
    try {
      const found = await findFeed(address);
      if (feeds.some(f => f.url === found.url)) setProblem("That feed is already in the list.");
      else setFeeds([...feeds, found]);
      setAddress("");
    } catch (e) {
      setProblem((e as Error).message);
    } finally {
      setBusy(false);
    }
  }

  function addLink() {
    let url = address.trim();
    if (!url) return;
    if (!isWeb(url)) url = "https://" + url;
    const host = hostOf(url);
    if (!host) return setProblem("That doesn't look like a web address.");
    setLinks([...links, { name: name.trim() || host, url }]);
    setAddress("");
    setName("");
    setProblem("");
  }

  const add = widget.type === "feeds" ? addFeed : addLink;
  const save = () => {
    const next: Widget = { type: widget.type, title: title.trim() || widget.title };
    if (widget.type === "feeds") next.feeds = feeds.filter(f => f.name.trim());
    if (widget.type === "links") next.links = links.filter(l => l.name.trim() && isWeb(l.url));
    onSave(next);
  };

  return (
    <div className="veil" onMouseDown={e => e.target === e.currentTarget && onCancel()}
      onKeyDown={e => e.key === "Escape" && onCancel()}>
      <form className="dialog" onSubmit={e => { e.preventDefault(); save(); }}>
        <h2>Edit widget</h2>
        <label>Title<input value={title} maxLength={60} onChange={e => setTitle(e.target.value)} /></label>

        {widget.type !== "feeds" && widget.type !== "links" ? null : (
          <>
            <div className="adder">
              <input value={address} placeholder={widget.type === "feeds" ? "Paste a site or feed address" : "Web address"}
                onChange={e => setAddress(e.target.value)}
                onKeyDown={e => { if (e.key === "Enter") { e.preventDefault(); add(); } }} />
              {widget.type === "links" && (
                <input value={name} placeholder="Name (optional)" maxLength={40} onChange={e => setName(e.target.value)}
                  onKeyDown={e => { if (e.key === "Enter") { e.preventDefault(); add(); } }} />
              )}
              <button type="button" disabled={busy} onClick={add}>{busy ? "Looking…" : "Add"}</button>
            </div>
            {problem && <p className="problem">{problem}</p>}
          </>
        )}

        <div className="rows">
          {widget.type === "feeds" && feeds.map((f, i) => (
            <div className="row" key={f.url}>
              <Favicon domain={f.site || hostOf(f.url)} name={f.name} />
              <span className="fields">
                <input value={f.name} maxLength={40} aria-label="Feed name"
                  onChange={e => setFeeds(feeds.map((x, j) => (j === i ? { ...x, name: e.target.value } : x)))} />
                <small>{f.url}</small>
              </span>
              <RowButtons index={i} count={feeds.length} onMove={by => setFeeds(move(feeds, i, by))}
                onRemove={() => setFeeds(feeds.filter((_, j) => j !== i))} />
            </div>
          ))}
          {widget.type === "links" && links.map((l, i) => (
            <div className="row" key={i}>
              <Favicon domain={hostOf(l.url)} name={l.name} />
              <span className="fields">
                <input value={l.name} maxLength={40} aria-label="Link name"
                  onChange={e => setLinks(links.map((x, j) => (j === i ? { ...x, name: e.target.value } : x)))} />
                <input className="small" value={l.url} aria-label="Link address"
                  onChange={e => setLinks(links.map((x, j) => (j === i ? { ...x, url: e.target.value } : x)))} />
              </span>
              <RowButtons index={i} count={links.length} onMove={by => setLinks(move(links, i, by))}
                onRemove={() => setLinks(links.filter((_, j) => j !== i))} />
            </div>
          ))}
        </div>

        <div className="actions">
          <button type="button" onClick={onCancel}>Cancel</button>
          <button type="submit" className="primary">Save</button>
        </div>
      </form>
    </div>
  );
}
