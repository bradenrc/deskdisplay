// Which screens the desk display moves through by itself, and how long each one stays up.
import { useEffect, useState } from "react";
import { getDisplay, saveDisplay } from "./api";
import type { Rotation } from "./types";

const SCREENS = [
  { id: "dashboard", label: "Home", note: "Clock, agenda, weather and news" },
  { id: "tasks", label: "Tasks", note: "" },
  { id: "markets", label: "Markets", note: "" },
  { id: "photo", label: "Photo", note: "" },
  { id: "aquarium", label: "Aquarium", note: "" },
];
const TIMES = [
  { seconds: 0, label: "Don't change by itself" },
  { seconds: 15, label: "Every 15 seconds" },
  { seconds: 30, label: "Every 30 seconds" },
  { seconds: 60, label: "Every minute" },
  { seconds: 120, label: "Every 2 minutes" },
  { seconds: 300, label: "Every 5 minutes" },
  { seconds: 600, label: "Every 10 minutes" },
  { seconds: 1800, label: "Every 30 minutes" },
];

export default function DisplayDialog({ onClose }: { onClose: () => void }) {
  const [rotation, setRotation] = useState<Rotation | null>(null);
  const [problem, setProblem] = useState("");
  useEffect(() => {
    getDisplay().then(setRotation, e => setProblem(e.message));
  }, []);

  const toggle = (id: string) => setRotation(r => r && {
    ...r, screens: r.screens.includes(id) ? r.screens.filter(s => s !== id) : [...r.screens, id],
  });
  const save = () => {
    if (!rotation) return;
    saveDisplay(rotation).then(onClose, e => setProblem("Couldn't save: " + e.message));
  };
  const off = !rotation || rotation.seconds === 0;
  // A time that is not one of the choices (set some other way) is still shown.
  const times = rotation && !TIMES.some(t => t.seconds === rotation.seconds)
    ? [...TIMES, { seconds: rotation.seconds, label: `Every ${rotation.seconds} seconds` }] : TIMES;

  return (
    <div className="veil" onMouseDown={e => e.target === e.currentTarget && onClose()}
      onKeyDown={e => e.key === "Escape" && onClose()}>
      <form className="dialog" onSubmit={e => { e.preventDefault(); save(); }}>
        <h2>Desk display</h2>
        {!rotation && !problem && <p className="empty">Loading…</p>}
        {rotation && (
          <>
            <label>Change screen
              <select autoFocus value={rotation.seconds}
                onChange={e => setRotation({ ...rotation, seconds: Number(e.target.value) })}>
                {times.map(t => <option key={t.seconds} value={t.seconds}>{t.label}</option>)}
              </select>
            </label>
            <fieldset disabled={off}>
              <legend>Screens to move through</legend>
              {SCREENS.map(s => (
                <label className="check" key={s.id}>
                  <input type="checkbox" checked={rotation.screens.includes(s.id)} onChange={() => toggle(s.id)} />
                  <span>{s.label}{s.note && <small>{s.note}</small>}</span>
                </label>
              ))}
            </fieldset>
            <p className="note">
              Touching the display holds it on the current screen for a while. Screens left unticked
              can still be reached by swiping. With one screen ticked, the display returns to it.
            </p>
          </>
        )}
        {problem && <p className="problem">{problem}</p>}
        <div className="actions">
          <button type="button" onClick={onClose}>Cancel</button>
          <button type="submit" className="primary" disabled={!rotation || (!off && !rotation.screens.length)}>Save</button>
        </div>
      </form>
    </div>
  );
}
