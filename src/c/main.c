/**
 * @file main.c
 * @brief Application composition and entry point for //GRID.
 */
#include "grid.h"

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

    if (!health_init(&app->health, app->screen.details_layer, &app->screen.geometry))
        goto fail;

    if (!doppler_init(&app->doppler, root, app->screen.details_layer, layer_get_bounds(root)))
        goto fail;

    app->mounted = true;
    return true;

fail:
    app_unmount(app);
    return false;
}

void app_unmount(App *app)
{
    if (!app)
        return;

    doppler_deinit(&app->doppler);
    health_deinit(&app->health);
    clock_deinit(&app->clock);
    app->mounted = false;
}

void app_activate_glance(App *app)
{
    if (!app || !app->mounted)
        return;

    clock_refresh_details(&app->clock);
    doppler_start(&app->doppler, app->settings.value.glance_duration_sec);
}

static bool app_init(App *app)
{
    if (!settings_init(&app->settings))
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
    app_unmount(app);
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
