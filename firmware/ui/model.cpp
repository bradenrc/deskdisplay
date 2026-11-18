#include "model.h"

#include <ArduinoJson.h>

static Event read_event(JsonObjectConst e) {
    Event ev;
    ev.title = e["title"] | "";
    ev.start_label = e["start_label"] | "";
    ev.start_ampm = e["start_ampm"] | "";
    ev.status = e["status"] | "upcoming";
    ev.minutes_until = e["minutes_until"] | -1;
    ev.minutes_left = e["minutes_left"] | -1;
    return ev;
}

bool parse_model(const char* json, size_t len, Model& out) {
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;

    Model m;
    m.epoch = doc["time"]["epoch"] | 0LL;
    m.utc_offset = doc["time"]["utc_offset_seconds"] | 0;
    if (m.epoch == 0) return false;

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
        if (!weather["sunrise_label"].isNull()) {
            m.has_today = true;
            m.high = weather["high"] | 0;
            m.low = weather["low"] | 0;
            m.sunrise = weather["sunrise_label"] | "";
            m.sunset = weather["sunset_label"] | "";
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
    m.photo_count = doc["photos"]["count"] | 0;

    m.valid = true;
    out = m;
    return true;
}
