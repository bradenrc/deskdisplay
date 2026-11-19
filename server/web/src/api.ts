import type { Board, Dash, Feed, FeedReply } from "./types";

async function json<T>(reply: Response): Promise<T> {
  if (!reply.ok) {
    let detail = `Error ${reply.status}`;
    try { detail = (await reply.json()).detail || detail; } catch { /* not JSON */ }
    throw new Error(detail);
  }
  return reply.json();
}

export const getBoard = () => fetch("/api/start").then(r => json<Board>(r));
export const getDash = () => fetch("/api/dashboard", { cache: "no-store" }).then(r => json<Dash>(r));
export const saveBoard = (board: Board) =>
  fetch("/api/start", {
    method: "PUT", headers: { "Content-Type": "application/json" }, body: JSON.stringify(board),
  }).then(r => json<Board>(r));
export const findFeed = (address: string) =>
  fetch("/api/feed/find", {
    method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ address }),
  }).then(r => json<Feed>(r));

// Feeds are kept for ten minutes, matching the server, so switching tabs is instant.
const kept = new Map<string, { at: number; reply: Promise<FeedReply> }>();
export function getFeed(url: string): Promise<FeedReply> {
  const hit = kept.get(url);
  if (hit && Date.now() - hit.at < 600_000) return hit.reply;
  const reply = fetch("/api/feed?url=" + encodeURIComponent(url)).then(r => json<FeedReply>(r));
  kept.set(url, { at: Date.now(), reply });
  reply.catch(() => kept.delete(url));
  return reply;
}

export function hostOf(url: string): string {
  try { return new URL(url).hostname.replace(/^www\./, ""); } catch { return ""; }
}
export const isWeb = (url: string) => /^https?:\/\//i.test(url || "");
