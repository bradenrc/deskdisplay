"""Game cards for the teams you follow: next game, live score, final score.

Scores come from ESPN's public site feeds. They need no account but are unofficial, so they
could change without notice; every failure here leaves the rest of the dashboard untouched.
"""

from __future__ import annotations

import logging
from datetime import UTC, datetime

import requests

log = logging.getLogger("deskpal.sports")

BASE = "https://site.api.espn.com/apis/site/v2/sports/football"
HEADERS = {"User-Agent": "desk-companion/1.0 (personal dashboard)"}
LIVE_WINDOW = (
    6 * 3600
)  # a game that has started is treated as possibly live for this long


def _score(value) -> int | None:
    if isinstance(value, dict):
        value = value.get("displayValue", value.get("value"))
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return None


def parse_game(event_id: str, date_iso: str, competition: dict) -> dict:
    """One ESPN competition -> the few fields a card needs."""
    status = competition.get("status", {})
    kind = status.get("type", {})
    sides = {}
    for side in competition.get("competitors", []):
        team = side.get("team", {})
        sides[side.get("homeAway", "home")] = {
            "abbr": team.get("abbreviation", ""),
            "name": team.get("shortDisplayName") or team.get("displayName") or "",
            "score": _score(side.get("score")),
        }
    channels = [
        (b.get("media") or {}).get("shortName")
        for b in competition.get("broadcasts", [])
    ]
    detail = kind.get("shortDetail", "")
    return {
        "id": str(event_id),
        "start": datetime.fromisoformat(date_iso.replace("Z", "+00:00")).timestamp(),
        "time_set": detail != "TBD" and competition.get("timeValid", True) is not False,
        "state": kind.get("state", "pre"),  # pre | in | post
        "detail": detail,  # "8:42 - 3rd", "Halftime", "Final"
        "home": sides.get("home", {"abbr": "", "name": "", "score": None}),
        "away": sides.get("away", {"abbr": "", "name": "", "score": None}),
        "tv": next((c for c in channels if c), ""),
    }


def fetch_schedule(league: str, team: str) -> list[dict]:
    resp = requests.get(
        f"{BASE}/{league}/teams/{team}/schedule", headers=HEADERS, timeout=20
    )
    resp.raise_for_status()
    body = resp.json()
    abbr = body.get("team", {}).get("abbreviation", "")
    games = []
    for event in body.get("events", []):
        game = parse_game(event["id"], event["date"], event["competitions"][0])
        game["mine"] = "home" if game["home"]["abbr"] == abbr else "away"
        games.append(game)
    return games


def fetch_live(league: str, game: dict) -> dict:
    """Current score and clock for one game."""
    resp = requests.get(
        f"{BASE}/{league}/summary",
        params={"event": game["id"]},
        headers=HEADERS,
        timeout=20,
    )
    resp.raise_for_status()
    competition = resp.json()["header"]["competitions"][0]
    fresh = parse_game(
        game["id"], competition.get("date") or _iso(game["start"]), competition
    )
    for side in ("home", "away"):  # the summary feed omits the short names
        fresh[side]["name"] = fresh[side]["name"] or game[side]["name"]
    fresh["tv"] = fresh["tv"] or game["tv"]
    fresh["mine"] = game["mine"]
    return fresh


def _iso(ts: float) -> str:
    return datetime.fromtimestamp(ts, UTC).strftime("%Y-%m-%dT%H:%MZ")


def pick(
    games: list[dict], now: float, preview_days: float, final_hours: float
) -> dict | None:
    """The one game worth showing for a team right now, or None in a quiet week."""
    live = [g for g in games if g["state"] == "in"]
    if live:
        return live[0]
    finals = [
        g
        for g in games
        if g["state"] == "post" and now - g["start"] < final_hours * 3600
    ]
    if finals:
        return max(finals, key=lambda g: g["start"])
    ahead = [
        g
        for g in games
        if g["state"] == "pre"
        and -LIVE_WINDOW < g["start"] - now < preview_days * 86400
    ]
    return min(ahead, key=lambda g: g["start"]) if ahead else None


def may_be_live(game: dict | None, now: float) -> bool:
    if not game:
        return False
    if game["state"] == "in":
        return True
    return (
        game["state"] == "pre"
        and game["time_set"]
        and -LIVE_WINDOW < game["start"] - now < 600
    )


class Tracker:
    """Keeps each team's schedule fresh, and polls only games that are actually on."""

    def __init__(
        self,
        teams: list[dict],
        schedule_minutes: float,
        preview_days: float,
        final_hours: float,
    ):
        self.teams = teams
        self.schedule_seconds = schedule_minutes * 60
        self.preview_days = preview_days
        self.final_hours = final_hours
        self.schedules: dict[int, list[dict]] = {}
        self.fetched_at: dict[int, float] = {}

    def refresh(self, now: float) -> int:
        """Returns how many teams could not be read."""
        failures = 0
        for index, team in enumerate(self.teams):
            try:
                if now - self.fetched_at.get(index, 0) >= self.schedule_seconds:
                    self.schedules[index] = fetch_schedule(
                        team["league"], str(team["team"])
                    )
                    self.fetched_at[index] = now
                game = pick(
                    self.schedules.get(index, []),
                    now,
                    self.preview_days,
                    self.final_hours,
                )
                if may_be_live(game, now):
                    fresh = fetch_live(team["league"], game)
                    self.schedules[index] = [
                        fresh if g["id"] == fresh["id"] else g
                        for g in self.schedules[index]
                    ]
            except Exception as exc:
                failures += 1
                log.warning(
                    "%s: %s", team.get("name", team.get("team")), type(exc).__name__
                )
        return failures

    def games(self, now: float) -> list[tuple[dict, dict]]:
        """(team, game) for each team with something to show, live games first."""
        out = []
        for index, team in enumerate(self.teams):
            game = pick(
                self.schedules.get(index, []), now, self.preview_days, self.final_hours
            )
            if game:
                out.append((team, game))
        out.sort(key=lambda pair: (pair[1]["state"] != "in", pair[1]["start"]))
        return out


DAYS = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]


def _when(start: datetime, now: datetime, time_set: bool) -> str:
    """Kick-off in a few characters: "Today 1:30 pm", "Sat 1:30 pm", "Sat · TBD"."""
    day = "Today" if start.date() == now.date() else DAYS[start.weekday()]
    if not time_set:
        return f"{day} · TBD"
    half = "am" if start.hour < 12 else "pm"
    return f"{day} {start.hour % 12 or 12}:{start.minute:02d} {half}"


def _clock(detail: str) -> str:
    """ESPN's "8:42 - 3rd" reads better as "3rd 8:42" in a narrow space."""
    left, sep, right = detail.partition(" - ")
    return f"{right} {left}" if sep else detail


def chip(game: dict, now: datetime) -> dict:
    """A two-line summary small enough to sit beside the clock.

    line1 is always "AWAY @ HOME"; line2 is the kick-off time, or the score in the same
    away-home order followed by the game clock or "Final".
    """
    home, away = game["home"], game["away"]
    if game["state"] == "pre":
        start = datetime.fromtimestamp(game["start"], now.tzinfo)
        line2 = _when(start, now, game["time_set"])
    else:
        status = (
            _clock(game["detail"])
            if game["state"] == "in"
            else game["detail"] or "Final"
        )
        line2 = f"{away['score'] or 0}–{home['score'] or 0} · {status}"
    return {
        "state": game["state"],  # pre | in | post
        "line1": f"{away['abbr']} @ {home['abbr']}",
        "line2": line2,
        "tv": game["tv"],
    }


def chips(tracker: Tracker | None, now: datetime) -> list[dict]:
    if not tracker:
        return []
    return [chip(game, now) for _team, game in tracker.games(now.timestamp())]
