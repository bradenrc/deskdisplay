// The data the dashboard shows, parsed from the server's /api/dashboard JSON.
#pragma once
#include <cstddef>
#include <string>
#include <vector>

struct Event {
    std::string title, start_label, start_ampm, status;  // status: past | now | soon | upcoming
    int minutes_until = -1;
    int minutes_left = -1;
    std::string leave;          // e.g. "Leave by 1:34 pm  ·  21 min drive"; empty if no drive
    std::string leave_urgency;  // later | soon | now
};

struct Hour {
    std::string label, icon;
    int temp = 0;
};

struct NewsItem {
    std::string title, source, age;
};

// A game, small enough to sit beside the clock: "IU @ WASH" over "Sat 1:30 pm" or "17–24 · 3rd 8:42".
struct Game {
    std::string state, line1, line2;  // state: pre | in | post
};

// Fresh snow at a ski area: 4" over "+2" by tomorrow".
struct Snow {
    std::string name, amount, detail, temp;  // temp like "24°", may be empty
};

struct Model {
    bool valid = false;
    long long epoch = 0;  // server time when this was fetched
    int utc_offset = 0;   // seconds; lets the device show local time without a timezone table

    bool has_agenda = false;  // false until a calendar link is added on the server
    std::vector<Event> events;
    std::vector<Event> tomorrow;
    std::vector<std::string> all_day;
    std::string free_after;  // e.g. "5:30 pm", empty when nothing is left today

    bool has_weather = false;
    bool has_today = false;  // high/low/sunrise/sunset present
    int temp = 0, high = 0, low = 0;
    std::string condition, icon, sunrise, sunset;
    std::vector<Hour> hourly;
    bool has_air = false;
    int aqi = 0, aqi_level = 0;  // level 0 (good) to 5 (hazardous)
    std::string aqi_label;

    float moon_phase = 0;  // 0 new, 0.5 full
    std::string moon_name;

    std::vector<NewsItem> news;
    std::vector<Game> games;
    std::vector<Snow> snow;  // empty unless there has been new snow in the last 48 hours
    int photo_count = 0;
    bool night = false;  // server says it is quiet hours
};

bool parse_model(const char* json, size_t len, Model& out);
