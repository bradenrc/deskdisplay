// The start page: header (clock, search), then the widgets in one to three columns.
// "Customize" turns on drag handles, edit and remove buttons, the column picker and Add.
import {
  DndContext, KeyboardSensor, PointerSensor, closestCorners, useDroppable, useSensor, useSensors,
  type DragEndEvent, type DragOverEvent,
} from "@dnd-kit/core";
import {
  SortableContext, arrayMove, sortableKeyboardCoordinates, useSortable, verticalListSortingStrategy,
} from "@dnd-kit/sortable";
import { CSS } from "@dnd-kit/utilities";
import { useCallback, useEffect, useRef, useState, type ReactNode } from "react";
import { getBoard, getDash, saveBoard } from "./api";
import EditDialog from "./EditDialog";
import type { Board, Dash, Layout, Widget, WidgetType } from "./types";
import { WidgetBody } from "./widgets";

const LAYOUTS: { id: Layout; label: string; bars: number[] }[] = [
  { id: "1", label: "One column", bars: [1] },
  { id: "2", label: "Two equal columns", bars: [1, 1] },
  { id: "2-left", label: "Narrow left, wide right", bars: [1, 2] },
  { id: "2-right", label: "Wide left, narrow right", bars: [2, 1] },
  { id: "3", label: "Three columns", bars: [1, 1, 1] },
];
const SEARCH = [
  { name: "Google", url: "https://www.google.com/search?q=" },
  { name: "Google AI", url: "https://www.google.com/search?udm=50&q=" },
  { name: "Claude", url: "https://claude.ai/new?q=" },
  { name: "DuckDuckGo", url: "https://duckduckgo.com/?q=" },
];
const KINDS: { type: WidgetType; label: string; many?: boolean }[] = [
  { type: "feeds", label: "News feeds", many: true },
  { type: "links", label: "Links", many: true },
  { type: "agenda", label: "Agenda" },
  { type: "weather", label: "Weather" },
  { type: "games", label: "Games" },
  { type: "snow", label: "Snow" },
  { type: "markets", label: "Markets" },
  { type: "photo", label: "Photo" },
];
const columnCount = (layout: Layout) => Number(layout[0]);

function Clock({ zone }: { zone?: string }) {
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const timer = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);
  const opts = zone ? { timeZone: zone } : {};
  const parts = new Intl.DateTimeFormat("en-US", { ...opts, hour: "numeric", minute: "2-digit" }).formatToParts(now);
  const get = (type: string) => parts.find(p => p.type === type)?.value || "";
  return (
    <div className="clock">
      <div className="date">
        {new Intl.DateTimeFormat("en-US", { ...opts, weekday: "long", month: "long", day: "numeric" }).format(now)}
      </div>
      <div className="time">{get("hour")}:{get("minute")}<small>{get("dayPeriod").toLowerCase()}</small></div>
    </div>
  );
}

function Search() {
  const [text, setText] = useState("");
  const go = (url: string) => {
    if (text.trim()) window.location.href = url + encodeURIComponent(text.trim());
  };
  return (
    <form className="search" onSubmit={e => { e.preventDefault(); go(SEARCH[0].url); }}>
      <input autoFocus value={text} placeholder="Search" aria-label="Search" onChange={e => setText(e.target.value)} />
      <div>
        {SEARCH.map((s, i) => (
          <button key={s.name} type={i === 0 ? "submit" : "button"} onClick={i === 0 ? undefined : () => go(s.url)}>
            {s.name}
          </button>
        ))}
      </div>
    </form>
  );
}

function Column({ index, ids, children }: { index: number; ids: string[]; children: ReactNode }) {
  const { setNodeRef } = useDroppable({ id: "col:" + index });
  return (
    <SortableContext items={ids} strategy={verticalListSortingStrategy}>
      <div className="col" ref={setNodeRef}>{children}</div>
    </SortableContext>
  );
}

function Frame({ id, widget, editing, onEdit, onRemove, children }: {
  id: string; widget: Widget; editing: boolean; onEdit: () => void; onRemove: () => void; children: ReactNode;
}) {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({ id, disabled: !editing });
  return (
    <section ref={setNodeRef} className={`widget ${widget.type}${isDragging ? " dragging" : ""}`}
      style={{ transform: CSS.Translate.toString(transform), transition }}>
      <header>
        {editing && <button className="grip" title="Drag to move" {...attributes} {...listeners}>⠿</button>}
        <h2>{widget.title}</h2>
        {editing && (
          <>
            <button title="Edit" onClick={onEdit}>✎</button>
            <button title="Remove" onClick={onRemove}>✕</button>
          </>
        )}
      </header>
      {children}
    </section>
  );
}

export default function App() {
  const [board, setBoard] = useState<Board | null>(null);
  const [dash, setDash] = useState<Dash | null>(null);
  const [editing, setEditing] = useState(false);
  const [dialog, setDialog] = useState<string | null>(null);
  const [adding, setAdding] = useState(false);
  const [problem, setProblem] = useState("");
  const dirty = useRef(false);

  useEffect(() => {
    getBoard().then(setBoard, () => setProblem("Can't reach the server."));
    const load = () => getDash().then(d => { setDash(d); setProblem(p => (p.startsWith("Can't reach") ? "" : p)); },
      () => setProblem("Can't reach the server. Showing what was last received."));
    load();
    const timer = setInterval(load, 30_000);
    return () => clearInterval(timer);
  }, []);

  // Every change to the page is saved to the server a moment later.
  useEffect(() => {
    if (!board || !dirty.current) return;
    const timer = setTimeout(() => {
      dirty.current = false;
      saveBoard(board).then(() => setProblem(""), e => setProblem("Couldn't save the page: " + e.message));
    }, 500);
    return () => clearTimeout(timer);
  }, [board]);

  const change = useCallback((fn: (b: Board) => Board) => {
    dirty.current = true;
    setBoard(b => (b ? fn(b) : b));
  }, []);

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 6 } }),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates }),
  );

  if (!board) return <main><p className="empty">{problem || "Loading…"}</p></main>;
  const count = columnCount(board.layout);
  const where = (b: Board, id: string) =>
    id.startsWith("col:") ? Number(id.slice(4)) : b.columns.findIndex(c => c.includes(id));

  function dragOver({ active, over }: DragOverEvent) {
    if (!over) return;
    const a = String(active.id), o = String(over.id);
    change(b => {
      const from = where(b, a), to = where(b, o);
      if (from < 0 || to < 0 || from === to) return b;
      const columns = b.columns.map(c => c.filter(id => id !== a));
      const at = o.startsWith("col:") ? columns[to].length : columns[to].indexOf(o);
      columns[to].splice(at < 0 ? columns[to].length : at, 0, a);
      return { ...b, columns };
    });
  }
  function dragEnd({ active, over }: DragEndEvent) {
    if (!over || active.id === over.id) return;
    const a = String(active.id), o = String(over.id);
    change(b => {
      const col = where(b, a);
      if (col < 0 || col !== where(b, o) || o.startsWith("col:")) return b;
      const columns = [...b.columns];
      columns[col] = arrayMove(columns[col], columns[col].indexOf(a), columns[col].indexOf(o));
      return { ...b, columns };
    });
  }

  // Fewer columns: whatever was in the columns that go away moves to the last one left.
  const setLayout = (layout: Layout) => change(b => {
    const keep = columnCount(layout);
    const columns = b.columns.map((c, i) => (i < keep ? [...c] : []));
    b.columns.slice(keep).forEach(c => columns[keep - 1].push(...c));
    return { ...b, layout, columns };
  });

  const remove = (id: string) => {
    const w = board.widgets[id];
    const holds = (w.feeds?.length || 0) + (w.links?.length || 0);
    if (holds && !window.confirm(`Remove "${w.title}" and the ${holds} items in it?`)) return;
    change(b => {
      const widgets = { ...b.widgets };
      delete widgets[id];
      return { ...b, widgets, columns: b.columns.map(c => c.filter(x => x !== id)) };
    });
  };

  const add = (type: WidgetType, label: string) => {
    const id = `${type}-${Math.random().toString(36).slice(2, 8)}`;
    const widget: Widget = { type, title: label };
    if (type === "feeds") widget.feeds = [];
    if (type === "links") widget.links = [];
    change(b => ({
      ...b, widgets: { ...b.widgets, [id]: widget },
      columns: b.columns.map((c, i) => (i === 0 ? [id, ...c] : c)),
    }));
    setAdding(false);
    if (type === "feeds" || type === "links") setDialog(id);
  };
  const present = new Set(Object.values(board.widgets).map(w => w.type));

  return (
    <main className={editing ? "editing" : ""}>
      <header className="top">
        <Clock zone={dash?.time.timezone} />
        <Search />
        <button className="customize" onClick={() => { setEditing(!editing); setAdding(false); }}>
          {editing ? "Done" : "Customize"}
        </button>
      </header>

      {editing && (
        <div className="toolbar">
          <span>Columns</span>
          {LAYOUTS.map(l => (
            <button key={l.id} title={l.label} aria-label={l.label} aria-pressed={board.layout === l.id}
              className="layout" onClick={() => setLayout(l.id)}>
              {l.bars.map((grow, i) => <i key={i} style={{ flexGrow: grow }} />)}
            </button>
          ))}
          <span className="addwrap">
            <button onClick={() => setAdding(!adding)}>Add widget ▾</button>
            {adding && (
              <span className="menu">
                {KINDS.filter(k => k.many || !present.has(k.type)).map(k => (
                  <button key={k.type} onClick={() => add(k.type, k.label)}>{k.label}</button>
                ))}
              </span>
            )}
          </span>
          <span className="hint">Drag a widget by its ⠿ handle to move it.</span>
        </div>
      )}
      {problem && <p className="problem">{problem}</p>}

      <DndContext sensors={sensors} collisionDetection={closestCorners} onDragOver={dragOver} onDragEnd={dragEnd}>
        <div className={"grid layout-" + board.layout}>
          {board.columns.slice(0, count).map((ids, index) => (
            <Column key={index} index={index} ids={ids}>
              {ids.map(id => (
                <Frame key={id} id={id} widget={board.widgets[id]} editing={editing}
                  onEdit={() => setDialog(id)} onRemove={() => remove(id)}>
                  <WidgetBody id={id} widget={board.widgets[id]} dash={dash} />
                </Frame>
              ))}
              {editing && !ids.length && <p className="dropzone">Drop a widget here</p>}
            </Column>
          ))}
        </div>
      </DndContext>

      <footer><a href="/setup">Server setup</a></footer>

      {dialog && board.widgets[dialog] && (
        <EditDialog widget={board.widgets[dialog]} onCancel={() => setDialog(null)}
          onSave={w => {
            // Saved straight away: the server only reads feeds that are on the saved page.
            const next = { ...board, widgets: { ...board.widgets, [dialog]: w } };
            setDialog(null);
            saveBoard(next).then(setBoard, e => setProblem("Couldn't save the page: " + e.message));
          }} />
      )}
    </main>
  );
}
