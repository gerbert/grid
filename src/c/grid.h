#ifndef GRID_H
#define GRID_H

#include <pebble.h>

#define GRID_SETTINGS_VERSION 3

#define GRID_GLANCE_DEFAULT_DURATION_SEC 7
#define GRID_GLANCE_MIN_DURATION_SEC     3
#define GRID_GLANCE_MAX_DURATION_SEC     30

#define GRID_WEATHER_DEFAULT_ENABLED                0
#define GRID_WEATHER_DEFAULT_PROVIDER_ID            0
#define GRID_WEATHER_DEFAULT_UPDATE_INTERVAL_HOURS  3
#define GRID_WEATHER_MIN_UPDATE_INTERVAL_HOURS      1
#define GRID_WEATHER_MAX_UPDATE_INTERVAL_HOURS      6
#define GRID_WEATHER_DEFAULT_RETRY_INTERVAL_MINUTES 30
#define GRID_WEATHER_MIN_RETRY_INTERVAL_MINUTES     15
#define GRID_WEATHER_MAX_RETRY_INTERVAL_MINUTES     60
#define GRID_WEATHER_REQUEST_TIMEOUT_MINUTES        2

#define WEATHER_SLOT_COUNT           12
#define WEATHER_CONDITION_LENGTH     16
#define WEATHER_FORECAST_HEADER_SIZE 7
#define WEATHER_FORECAST_SLOT_SIZE   (1 + WEATHER_CONDITION_LENGTH)

typedef struct App App;

typedef struct {
    uint8_t enabled;
    uint8_t provider_id;
    uint8_t refresh_hrs;
    uint8_t retry_min;
} WeatherSettings;

typedef struct {
    uint8_t         version;
    uint8_t         glance_duration_sec;
    WeatherSettings weather;
} Settings;

typedef struct {
    Settings value;
    App     *app;
    bool     callbacks_registered;
} SettingsStore;

typedef struct {
    GRect time;
    GRect weather;
    GRect date;
    GRect battery_bar;
    GRect battery_label;
    GRect health;
    GRect sleep;
    int   bottom_grid_y;
} ScreenGeometry;

typedef struct {
    Window        *window;
    Layer         *details_layer;
    Layer         *decor_layer;
    ScreenGeometry geometry;
} Screen;

typedef struct {
    TextLayer            *time_layer;
    Layer                *details_layer;
    const ScreenGeometry *geometry;
    char                  time_buf[12];
    char                  date_buf[20];
    int                   battery_pct;
    bool                  tick_subscribed;
} Clock;

typedef struct {
    Layer                *layer;
    const ScreenGeometry *geometry;
    int32_t               steps;
    int32_t               heart_rate;
    int32_t               kcal;
    int32_t               dist_m;
    int32_t               sleep_seconds;
    bool                  service_subscribed;
} Health;

typedef struct {
    int8_t temperature_c;
    char   condition[WEATHER_CONDITION_LENGTH];
} WeatherSlot;

typedef struct {
    // Layer used to render the forecast slot matching the current time.
    Layer *layer;

    // Forecast slots stored in chronological order.
    WeatherSlot slots[WEATHER_SLOT_COUNT];

    // Timestamp assigned to the first forecast slot.
    time_t start_at;

    // Next request time while idle, or the active request deadline while waiting for a response.
    time_t next_update_at;

    // Time between adjacent forecast slots.
    uint16_t interval_minutes;

    // Number of populated forecast slots; zero means that no forecast is available.
    uint8_t count;

    // True while waiting for a weather response from PebbleKit JS.
    bool in_progress;

    // Settings changed during an active request, so another refresh must follow its completion.
    bool refresh_pending;
} Weather;

typedef struct {
    Layer    *layer;
    Layer    *target_layer;
    AppTimer *frame_timer;
    AppTimer *visible_timer;
    int       frame;
    uint8_t   visible_duration_sec;
} Doppler;

struct App {
    SettingsStore settings;
    Screen        screen;
    Clock         clock;
    Health        health;
    Weather       weather;
    Doppler       doppler;
    bool          mounted;
};

bool settings_init(SettingsStore *settings, App *app);
void settings_deinit(SettingsStore *settings);

bool screen_init(Screen *screen, App *app);
void screen_show(Screen *screen);
void screen_deinit(Screen *screen);

bool clock_init(Clock *clock, Layer *root, Layer *details, const ScreenGeometry *geometry);
void clock_refresh_details(Clock *clock);
void clock_deinit(Clock *clock);

bool health_init(Health *health, Layer *details, const ScreenGeometry *geometry);
void health_refresh(Health *health);
void health_deinit(Health *health);

bool weather_init(Weather *weather, Layer *details, const ScreenGeometry *geometry);
void weather_schedule_refresh(Weather *weather);
void weather_tick(Weather *weather, const Settings *settings, time_t now);
void weather_handle_message(Weather *weather, const Settings *settings, DictionaryIterator *iterator);
void weather_update_failed(Weather *weather, const Settings *settings);
void weather_deinit(Weather *weather);

bool doppler_init(Doppler *doppler, Layer *root, Layer *target, GRect bounds);
void doppler_start(Doppler *doppler, uint8_t visible_duration_sec);
void doppler_deinit(Doppler *doppler);

bool app_mount(App *app);
void app_unmount(App *app);
void app_activate_glance(App *app);
App *app_from_active_window(void);

#endif // GRID_H
