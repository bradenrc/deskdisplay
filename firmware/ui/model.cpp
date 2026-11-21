#include "model.h"

#include <ArduinoJson.h>

#include <cstdlib>

static Event read_event(JsonObjectConst e) {
    Event ev;
    ev.title = e["title"] | "";
    ev.start_label = e["start_label"] | "";
    ev.start_ampm = e["start_ampm"] | "";
    ev.status = e["status"] | "upcoming";
    ev.minutes_until = e["minutes_until"] | -1;
    ev.minutes_left = e["minutes_left"] | -1;
    JsonObjectConst travel = e["travel"];
    if (!travel.isNull()) {
        ev.leave_urgency = travel["urgency"] | "later";
        std::string drive = std::to_string(travel["drive_minutes"] | 0) + " min drive";
        if (ev.leave_urgency == "now") {
            ev.leave = "Leave now  ·  " + drive;
        } else {
            ev.leave = std::string("Leave by ") + (travel["leave_label"] | "") + " " +
                       (travel["leave_ampm"] | "") + "  ·  " + drive;
        }
    }
    return ev;
}

bool parse_model(const char* json, size_t len, Model& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;

    Model m;
    m.epoch = doc["time"]["epoch"] | 0LL;
    m.utc_offset = doc["time"]["utc_offset_seconds"] | 0;
    if (m.epoch == 0) return false;

    m.agenda_hidden = !(doc["desk"]["agenda"] | true);
    JsonObjectConst agenda = doc["agenda"];
    if (!agenda.isNull()) {
        m.has_agenda = true;
        for (JsonObjectConst e : agenda["events"].as<JsonArrayConst>()) m.events.push_back(read_event(e));
        for (JsonObjectConst e : agenda["tomorrow"].as<JsonArrayConst>()) m.tomorrow.push_back(read_event(e));
        for (JsonVariantConst t : agenda["all_day"].as<JsonArrayConst>()) m.all_day.push_back(t | "");
        JsonObjectConst free_after = agenda["free_after"];
        if (!free_after.isNull()) {
            m.free_after = std::string(free_after["label"] | "") + " " + (free_after["ampm"] | "");
        }
    }

    JsonObjectConst weather = doc["weather"];
    if (!weather.isNull()) {
        m.has_weather = true;
        m.temp = weather["temp"] | 0;
        m.condition = weather["condition"] | "";
        m.icon = weather["icon"] | "cloudy";
        m.weather_code = weather["code"] | -1;
        if (!weather["sunrise_label"].isNull()) {
            m.has_today = true;
            m.high = weather["high"] | 0;
            m.low = weather["low"] | 0;
            m.sunrise = weather["sunrise_label"] | "";
            // read as numbers of any kind: a whole value may still arrive as 1794839880.0
            long long rise = (long long)(weather["sunrise"] | 0.0), set = (long long)(weather["sunset"] | 0.0);
            if (rise > 0 && set > rise) {
                m.sunrise_min = (int)(((rise + m.utc_offset) % 86400 + 86400) % 86400 / 60);
                m.sunset_min = (int)(((set + m.utc_offset) % 86400 + 86400) % 86400 / 60);
            }
            m.sunset = weather["sunset_label"] | "";
        }
        JsonObjectConst air = weather["air"];
        if (!air.isNull()) {
            m.has_air = true;
            m.aqi = air["value"] | 0;
            m.aqi_level = air["level"] | 0;
            m.aqi_label = air["label"] | "";
        }
        for (JsonObjectConst h : weather["hourly"].as<JsonArrayConst>()) {
            Hour hour;
            hour.label = h["label"] | "";
            hour.icon = h["icon"] | "cloudy";
            hour.temp = h["temp"] | 0;
            m.hourly.push_back(hour);
        }
    }

    m.moon_phase = doc["moon"]["phase"] | 0.0f;
    m.moon_name = doc["moon"]["name"] | "";

    for (JsonObjectConst n : doc["news"].as<JsonArrayConst>()) {
        NewsItem item;
        item.title = n["title"] | "";
        item.source = n["source"] | "";
        item.age = n["age"] | "";
        if (!item.title.empty()) m.news.push_back(item);
    }
    for (JsonObjectConst g : doc["games"].as<JsonArrayConst>()) {
        Game game;
        game.state = g["state"] | "pre";
        game.line1 = g["line1"] | "";
        game.line2 = g["line2"] | "";
        for (JsonVariantConst c : g["colors"].as<JsonArrayConst>()) {
            const char* hex = c | "";
            if (hex[0] == '#' && game.color_count < 2) {
                game.colors[game.color_count++] = (uint32_t)strtoul(hex + 1, nullptr, 16);
            }
        }
        m.games.push_back(game);
    }
    m.market_open = doc["stocks"]["open"] | false;
    for (JsonObjectConst q : doc["stocks"]["items"].as<JsonArrayConst>()) {
        Stock stock;
        stock.name = q["name"] | "";
        stock.price = q["price"] | "";
        stock.change = q["change"] | "";
        stock.direction = q["direction"] | "flat";
        stock.baseline = q["baseline"] | -1;
        for (JsonVariantConst p : q["points"].as<JsonArrayConst>()) stock.points.push_back((uint8_t)(p | 0));
        m.stocks.push_back(stock);
    }
    JsonObjectConst tasks = doc["tasks"];
    if (!tasks.isNull()) {
        m.has_tasks = true;
        m.tasks_total = tasks["total"] | 0;
        for (JsonObjectConst list : tasks["lists"].as<JsonArrayConst>()) {
            if (tasks["lists"].size() > 1) {  // one list needs no heading
                TaskRow name;
                name.header = true;
                name.title = list["title"] | "";
                m.tasks.push_back(name);
            }
            for (JsonObjectConst t : list["items"].as<JsonArrayConst>()) {
                TaskRow row;
                row.title = t["title"] | "";
                row.due = t["due"] | "";
                row.tone = t["tone"] | "";
                row.indent = t["indent"] | 0;
                m.tasks.push_back(row);
            }
        }
    }
    m.clock_mode = doc["display"]["clock"] | "sun";
    for (JsonObjectConst s : doc["snow"].as<JsonArrayConst>()) {
        Snow spot;
        spot.name = s["name"] | "";
        spot.amount = s["new"] | "";
        spot.detail = s["detail"] | "";
        if (!s["temp"].isNull()) spot.temp = std::to_string(s["temp"] | 0) + "°";
        m.snow.push_back(spot);
    }
    m.photo_count = doc["photos"]["count"] | 0;
    m.night = doc["night"] | false;

    m.valid = true;
    out = m;
    return true;
}
