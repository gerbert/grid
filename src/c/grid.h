#ifndef GRID_H
#define GRID_H

#include <pebble.h>

#define GRID_SETTINGS_VERSION 1

#define GRID_GLANCE_DEFAULT_DURATION_SEC 7
#define GRID_GLANCE_MIN_DURATION_SEC     3
#define GRID_GLANCE_MAX_DURATION_SEC     30

typedef struct App App;

typedef struct {
    uint8_t version;
    uint8_t glance_duration_sec;
} Settings;

typedef struct {
    Settings value;
} SettingsStore;

typedef struct {
    GRect time;
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
    Doppler       doppler;
    bool          mounted;
};

bool settings_init(SettingsStore *settings);
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

bool doppler_init(Doppler *doppler, Layer *root, Layer *target, GRect bounds);
void doppler_start(Doppler *doppler, uint8_t visible_duration_sec);
void doppler_deinit(Doppler *doppler);

bool app_mount(App *app);
void app_unmount(App *app);
void app_activate_glance(App *app);
App *app_from_active_window(void);

#endif // GRID_H
