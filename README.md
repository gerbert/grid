# //GRID

//GRID is a small, minimalist watchface for Pebble Time 2 and Pebble 2 Duo.
It takes visual cues from TRON: a black background, cyan geometry, a simple
screen grid, and large LCD-style digits.

The idea is simple. Most of the time, the watchface shows only the current
time. Flick your wrist and the screen plays a short Doppler animation, then
opens the full view with the weather, date, battery status, health data, and
sleep time.

## Highlights

- Clean idle screen with time only
- Doppler animation triggered by a wrist flick
- Configurable glance duration from 3 to 30 seconds
- Optional 12-hour weather forecast with a selectable provider
- Text or icon display for the current weather condition
- Configurable weather update interval from 1 to 6 hours
- Configurable failed-update retry interval from 15 to 60 minutes
- 12-hour and 24-hour time formats
- Date shown in bracket notation
- Segmented battery bar with percentage
- Steps, calories, distance, sleep, and heart rate where supported
- Separate layouts for Pebble Time 2 and Pebble 2 Duo
- Persistent settings stored on the watch
- Weather values stored only in watch RAM

## Supported watches

### Pebble Time 2

Platform: `emery`

The full view shows:

- Weather forecast for the current hour when enabled
- Steps (`STP`)
- Heart rate (`BPM`)
- Active calories (`CAL`)
- Distance in kilometers (`KM`)
- Sleep duration
- Date
- Battery level

Pebble Time 2 uses the full color layout, including the lower grid section and
color-coded battery warnings.

### Pebble 2 Duo

Platform: `flint`

The full view shows:

- Weather forecast for the current hour when enabled
- Steps (`STP`)
- Active calories (`CAL`)
- Distance in kilometers (`KM`)
- Sleep duration
- Date
- Battery level

Heart rate is not shown because Pebble 2 Duo does not have an optical heart-rate
sensor. Its smaller display uses a more compact monochrome layout and leaves out
the lower grid section.

## How it works

### Idle screen

During normal use, only the current time is visible. The watchface wakes once per
minute to update it and to compare the weather update timestamp with the current
time.

### Glance screen

A wrist flick starts the Doppler animation. When the animation ends, the full
view stays on screen for the configured number of seconds.

While the animation or glance screen is already active, extra flick events are
ignored. This avoids restarting the animation, extending the glance timer, or
repeatedly requesting the backlight.

The backlight is enabled only when it is currently off.

## Weather

Weather is disabled by default. The provider is selected in the settings.
Open-Meteo is currently available.

When enabled, PebbleKit JS obtains the phone location and asks the selected
provider for up to twelve forecast points. Provider-specific values, including
weather codes, are converted on the phone into a normalized sequence containing
a start time, slot interval, temperature, and condition ID. The complete result
is sent to the watch as one compact byte array.

The watch does not interpret provider-specific weather codes. It stores one
normalized forecast in RAM and selects the current slot using the received start
time, interval, and element count. A matching C enum is used only to select the
display text or icon. Icon mode uses selected glyphs from Weather Icons 2.0.12.
The original TTF is used without modification, while the Pebble resource
configuration limits conversion to the 16 glyphs required by the watchface.
The selected glyphs are rasterized at platform-specific sizes for Pebble Time 2
and Pebble 2 Duo.

No weather values are written to persistent watch storage. Restarting the
watchface clears the forecast until the next successful synchronization.

After weather is enabled, the existing minute tick is the only watch-side
scheduler. It starts an update when `next_update_at` is due. Failed or timed-out
updates leave the previous forecast unchanged and schedule the next attempt
using the configured retry interval.

The wrist gesture never starts a weather request. The weather layer only reads
the current normalized in-memory slot.

## Health data

Health values are kept in a small in-memory cache.

The cache is filled when the watchface starts and updated when Pebble reports a
movement, heart-rate, sleep, or significant health event. Opening the glance
screen only draws the cached values; it does not read every health metric again.

If a value is not available, the watchface shows `--`.

## Battery display

The battery bar has ten segments.

Above 10 percent, the segments show the approximate charge in 10-percent steps.
At 10 percent or below, each segment represents one percent, which makes the
last part of the battery easier to read.

On Pebble Time 2, the color changes with the remaining charge:

- Above 30 percent: cyan
- 21 to 30 percent: green
- 16 to 20 percent: yellow
- 11 to 15 percent: orange
- 10 percent or below: red

Pebble 2 Duo uses the monochrome system palette.

## Settings

The configuration page is built with Rebble Clay.

Available options:

- `Glance duration`: 3 to 30 seconds, default 7 seconds
- `Enable weather`: disabled by default
- `Provider`: Open-Meteo by default
- `Condition display`: text by default, with an optional icon mode
- `Weather update interval`: 1 to 6 hours, default 3 hours
- `Weather retry interval`: 15 to 60 minutes, default 30 minutes

The watch stores only the numeric provider index. Provider selection and API
handling are implemented in PebbleKit JS.

## Power use

The watchface tries to do as little work as possible while idle:

- The time is updated once per minute.
- The same minute tick performs one timestamp comparison for weather.
- No additional weather timer is created.
- Weather requests occur only when enabled and due.
- Twelve forecast slots allow the displayed value to advance without another
  phone request.
- Date and battery values are refreshed only before a glance starts.
- Health values are updated through HealthService events.
- Hidden health layers are not redrawn after every health event.
- Doppler timers exist only while the animation or glance is active.
- Repeated flicks are ignored until the current glance finishes.
- The backlight is requested only when it is off.
- PebbleKit JS has no polling or background interval timer.

## Building

For current SDK installation, setup, build, emulator, and device installation
instructions, see the official Pebble SDK documentation:

[Installing the Pebble SDK](https://developer.repebble.com/sdk/)

For a practical C watchface example, see:

[C Watchface Tutorial](https://github.com/coredevices/c-watchface-tutorial)

The project targets `emery` (Pebble Time 2) and `flint` (Pebble 2 Duo).

### Weather icon font

Icon mode uses the original, unmodified Weather Icons 2.0.12 TTF by Erik
Flowers. The original v1.0 icon designs are credited to Lukas Bischoff; icon art
from v1.1 onward and maintenance of Weather Icons are credited to Erik Flowers.

Project and source:

- https://erikflowers.github.io/weather-icons/
- https://github.com/erikflowers/weather-icons
- https://cdnjs.cloudflare.com/ajax/libs/weather-icons/2.0.12/font/weathericons-regular-webfont.ttf

The font is licensed under the SIL Open Font License 1.1. The complete license
text and third-party notice are stored in:

- `licenses/weather-icons/OFL-1.1.txt`
- `licenses/weather-icons/NOTICE.md`

The original TTF is stored in the project at:

```text
resources/fonts/weathericons-regular-webfont.ttf
```

The bundled file is the unmodified Weather Icons 2.0.12 release asset. Its
SHA-256 digest is:

```text
176bda6661f213dde47c2114d76e476ec8ca9aae07dd54f9550d2d28fe02b4fd
```

The full upstream TTF is about 97 KiB in the source tree. It is not copied whole
into the installed watch application: `package.json` lists exactly the 16
private-use Unicode glyphs required by `//GRID`, and the Pebble SDK converts only
those glyphs into the platform-specific font resource at 24 px for `emery` and
18 px for `flint`.

## License

Except for the third-party components listed in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), `//GRID` is licensed
under the GNU General Public License v2.0 only (`GPL-2.0-only`).

See [`LICENSE`](LICENSE) for the complete license text.
