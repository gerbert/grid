/**
 * @file main.c
 * @brief Application composition and entry point for //GRID.
 */
#include "grid.h"

static bool do_not_disturb_active(const Settings *settings, time_t now)
{
    if (!settings || !settings->do_not_disturb.enabled)
        return false;

    struct tm *time_info = localtime(&now);

    if (!time_info)
        return false;

    uint16_t current_minute = (uint16_t)(time_info->tm_hour * 60 + time_info->tm_min);
    uint16_t start_minute   = settings->do_not_disturb.start_minute;
    uint16_t end_minute     = settings->do_not_disturb.end_minute;

    if (start_minute == end_minute)
        return false;

    if (start_minute < end_minute)
        return current_minute >= start_minute && current_minute < end_minute;

    return current_minute >= start_minute || current_minute < end_minute;
}

App *app_from_active_window(void)
{
    Window *window = window_stack_get_top_window();

    return window ? window_get_user_data(window) : NULL;
}

bool app_mount(App *app)
{
    if (!app || app->mounted)
        return app != NULL;

    Layer *root = window_get_root_layer(app->screen.window);

    if (!root || !app->screen.details_layer)
        return false;

    if (!clock_init(&app->clock, root, app->screen.details_layer, &app->screen.geometry))
        goto fail;

    if (!weather_init(&app->weather, app->screen.details_layer, &app->screen.geometry, &app->settings.value))
        goto fail;

    if (!health_init(&app->health, app->screen.details_layer, &app->screen.geometry))
        goto fail;

    if (!alarm_init(&app->alarm, root, app->screen.details_layer, &app->settings, &app->screen.geometry))
        goto fail;

    if (!doppler_init(&app->doppler, root, app->screen.details_layer, layer_get_bounds(root)))
        goto fail;

    app->mounted = true;

    time_t now = time(NULL);

    if (!alarm_handle_launch(&app->alarm, now))
        alarm_tick(&app->alarm, now);

    if (app->settings.value.weather.enabled)
        weather_tick(&app->weather, &app->settings.value, now);

    return true;

fail:
    app_unmount(app);
    return false;
}

void app_unmount(App *app)
{
    if (!app)
        return;

    // Block callbacks from treating partially destroyed modules as mounted.
    app->mounted = false;

    doppler_deinit(&app->doppler);
    alarm_deinit(&app->alarm);
    health_deinit(&app->health);
    weather_deinit(&app->weather);
    clock_deinit(&app->clock);
}

void app_activate_glance(App *app)
{
    if (!app || !app->mounted)
        return;

    if (do_not_disturb_active(&app->settings.value, time(NULL)))
        return;

    clock_refresh_details(&app->clock);
    doppler_start(&app->doppler, app->settings.value.glance_duration_sec);
}

static bool app_init(App *app)
{
    if (!settings_init(&app->settings, app))
        return false;

    if (!screen_init(&app->screen, app)) {
        settings_deinit(&app->settings);
        return false;
    }

    screen_show(&app->screen);
    return true;
}

static void app_deinit(App *app)
{
    // Destroying a loaded window invokes main_window_unload(), which owns app_unmount().
    screen_deinit(&app->screen);
    settings_deinit(&app->settings);
}

int main(void)
{
    App app = {0};

    if (!app_init(&app))
        return 1;

    app_event_loop();
    app_deinit(&app);

    return 0;
}
