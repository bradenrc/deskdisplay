export type Layout = "1" | "2" | "2-left" | "2-right" | "3";
export type WidgetType =
  | "agenda" | "weather" | "games" | "snow" | "markets" | "photo" | "feeds" | "links";

export interface Feed { name: string; url: string; site: string }
export interface LinkItem { name: string; url: string }
// desk: whether the desk display shows it too (agenda, games, snow), or for a feeds widget
// whether its feeds supply the display's news tile.
export interface Widget { type: WidgetType; title: string; desk?: boolean; feeds?: Feed[]; links?: LinkItem[] }
export const DESK_TYPES: WidgetType[] = ["agenda", "games", "snow", "feeds"];
export interface Board { layout: Layout; columns: string[][]; widgets: Record<string, Widget> }

export interface FeedItem {
  title: string; link: string; summary: string; image: string; age: string;
}
export interface FeedReply { title: string; site: string; items: FeedItem[] }

export interface AgendaEvent {
  title: string; location: string; start_label: string; start_ampm: string; status: string;
  minutes_until: number | null; minutes_left: number | null;
  travel: null | { drive_minutes: number; leave_label: string; leave_ampm: string; urgency: string };
}
export interface Dash {
  time: { timezone: string };
  agenda: null | {
    events: AgendaEvent[]; all_day: string[]; tomorrow: AgendaEvent[]; tomorrow_total: number;
    free_after: null | { label: string; ampm: string };
  };
  weather: null | {
    temp: number; condition: string; icon: string; high: number; low: number;
    sunrise_label: string; sunset_label: string;
    air: null | { value: number; label: string };
    hourly: { label: string; temp: number; icon: string; precip: number }[];
  };
  moon: null | { name: string; illumination: number };
  games: { state: string; line1: string; line2: string; tv: string; colors: string[] }[];
  snow: { name: string; new: string; detail: string; temp: number | null }[];
  stocks: null | {
    open: boolean;
    items: { symbol: string; name: string; price: string; change: string; direction: string;
      points: number[]; baseline: number | null }[];
  };
  photos: { count: number };
}
