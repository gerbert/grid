#ifndef GRID_H
#define GRID_H

#include <pebble.h>

#define GRID_SETTINGS_VERSION 5

#define GRID_GLANCE_DEFAULT_DURATION_SEC 7
#define GRID_GLANCE_MIN_DURATION_SEC     3
#define GRID_GLANCE_MAX_DURATION_SEC     30

#define GRID_DND_DEFAULT_ENABLED      0
#define GRID_DND_DEFAULT_START_MINUTE (22 * 60)
#define GRID_DND_DEFAULT_END_MINUTE   (7 * 60)
#define GRID_MINUTES_PER_DAY          (24 * 60)

#define GRID_WEATHER_DEFAULT_ENABLED                0
#define GRID_WEATHER_DEFAULT_PROVIDER_ID            0
#define GRID_WEATHER_DEFAULT_UPDATE_INTERVAL_HOURS  3
#define GRID_WEATHER_MIN_UPDATE_INTERVAL_HOURS      1
#define GRID_WEATHER_MAX_UPDATE_INTERVAL_HOURS      6
#define GRID_WEATHER_DEFAULT_RETRY_INTERVAL_MINUTES 30
#define GRID_WEATHER_MIN_RETRY_INTERVAL_MINUTES     1
#define GRID_WEATHER_MAX_RETRY_INTERVAL_MINUTES     30
#define GRID_WEATHER_DEFAULT_DISPLAY_MODE           WEATHER_DISPLAY_TEXT

#define WEATHER_SLOT_COUNT           12
#define WEATHER_FORECAST_HEADER_SIZE 5
#define WEATHER_FORECAST_SLOT_SIZE   2

typedef struct App App;

typedef enum {
    WEATHER_DISPLAY_TEXT        = 0,
    WEATHER_DISPLAY_ICON        = 1,
    WEATHER_DISPLAY_ICON_TEXT   = 2,
    WEATHER_DISPLAY_TEMPERATURE = 3,
} WeatherDisplayMode;

typedef enum {
    WEATHER_CONDITION_UNKNOWN          = 0,
    WEATHER_CONDITION_CLEAR            = 1,
    WEATHER_CONDITION_MOSTLY_CLEAR     = 2,
    WEATHER_CONDITION_PARTLY_CLOUDY    = 3,
    WEATHER_CONDITION_OVERCAST         = 4,
    WEATHER_CONDITION_FOG              = 5,
    WEATHER_CONDITION_DRIZZLE          = 6,
    WEATHER_CONDITION_FREEZING_DRIZZLE = 7,
    WEATHER_CONDITION_RAIN             = 8,
    WEATHER_CONDITION_FREEZING_RAIN    = 9,
    WEATHER_CONDITION_SNOW             = 10,
    WEATHER_CONDITION_SNOW_GRAINS      = 11,
    WEATHER_CONDITION_SHOWERS          = 12,
    WEATHER_CONDITION_SNOW_SHOWERS     = 13,
    WEATHER_CONDITION_THUNDERSTORM     = 14,
    WEATHER_CONDITION_HAIL_STORM       = 15,
    WEATHER_CONDITION_COUNT            = 16,
} WeatherCondition;

typedef struct {
    uint8_t  enabled;
    uint16_t start_minute;
    uint16_t end_minute;
} DoNotDisturbSettings;

typedef struct {
    uint8_t enabled;
    uint8_t provider_id;
    uint8_t refresh_hrs;
    uint8_t retry_min;
    uint8_t display_mode;
} WeatherSettings;

typedef struct {
    uint8_t              version;
    uint8_t              glance_duration_sec;
    DoNotDisturbSettings do_not_disturb;
    WeatherSettings      weather;
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
    int8_t  temperature_c;
    uint8_t condition_id;
    // True when this slot was populated by the last accepted forecast.
    bool valid;
} WeatherSlot;

typedef struct {
    // Layer used to render the forecast slot matching the current time.
    Layer *layer;

    // Current application settings used only to choose the presentation mode.
    const Settings *settings;

    // Custom icon font, loaded only when icon mode is drawn.
    GFont icon_font;

    // Forecast slots addressed by the Unix hour modulo WEATHER_SLOT_COUNT.
    WeatherSlot slots[WEATHER_SLOT_COUNT];

    // Unix timestamp of the last successfully applied forecast.
    time_t last_update_at;

    // Unix timestamp of the next scheduled weather request.
    time_t next_update_at;
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

bool weather_init(Weather *weather, Layer *details, const ScreenGeometry *geometry, const Settings *settings);
void weather_refresh_display(Weather *weather);
void weather_schedule_refresh(Weather *weather);
void weather_tick(Weather *weather, const Settings *settings, time_t now);
void weather_handle_message(Weather *weather, const Settings *settings, DictionaryIterator *iterator);
void weather_deinit(Weather *weather);

bool doppler_init(Doppler *doppler, Layer *root, Layer *target, GRect bounds);
void doppler_start(Doppler *doppler, uint8_t visible_duration_sec);
void doppler_deinit(Doppler *doppler);

bool app_mount(App *app);
void app_unmount(App *app);
void app_activate_glance(App *app);
App *app_from_active_window(void);

#endif // GRID_H
