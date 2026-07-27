# Weather Icons font

`weathericons-regular-webfont.ttf` is the original, unmodified font from
Weather Icons 2.0.12.

- Project: https://erikflowers.github.io/weather-icons/
- Source: https://github.com/erikflowers/weather-icons
- Release asset: https://cdnjs.cloudflare.com/ajax/libs/weather-icons/2.0.12/font/weathericons-regular-webfont.ttf
- License: SIL Open Font License 1.1
- SHA-256: `176bda6661f213dde47c2114d76e476ec8ca9aae07dd54f9550d2d28fe02b4fd`

The source TTF is included directly so local and RePebble Cloud builds do not
require network access or a separate preparation step.

`package.json` selects only the 16 glyphs used by `//GRID` when the Pebble SDK
creates the platform-specific font resources.

See `licenses/weather-icons/` for the complete license and attribution notice.
