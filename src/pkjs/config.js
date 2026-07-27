var weatherProviders = require('./providers');

module.exports = [
    {
        type: 'heading',
        defaultValue: '//GRID Settings'
    },
    {
        type: 'section',
        items: [
            {
                type: 'heading',
                defaultValue: 'Display'
            },
            {
                type: 'slider',
                messageKey: 'GLANCE_DURATION_SEC',
                label: 'Glance duration',
                description: 'Seconds shown after the animation',
                defaultValue: 7,
                min: 3,
                max: 30,
                step: 1
            }
        ]
    },
    {
        type: 'section',
        items: [
            {
                type: 'heading',
                defaultValue: 'Weather'
            },
            {
                type: 'toggle',
                messageKey: 'WEATHER_ENABLE',
                label: 'Enable weather',
                defaultValue: false
            },
            {
                type: 'select',
                messageKey: 'WEATHER_PROVIDER_ID',
                label: 'Provider',
                defaultValue: weatherProviders.defaultId,
                options: weatherProviders.getConfigOptions(),
                group: 'weather-details'
            },
            {
                type: 'select',
                messageKey: 'WEATHER_DISPLAY_MODE',
                label: 'Condition display',
                defaultValue: 0,
                options: [
                    {
                        label: 'Text',
                        value: 0
                    },
                    {
                        label: 'Icon',
                        value: 1
                    },
                    {
                        label: 'Icon + text',
                        value: 2
                    },
                    {
                        label: 'Temperature only',
                        value: 3
                    }
                ],
                group: 'weather-details'
            },
            {
                type: 'slider',
                messageKey: 'WEATHER_UPDATE_INTERVAL_HOURS',
                label: 'Update interval, hours',
                defaultValue: 3,
                min: 1,
                max: 6,
                step: 1,
                group: 'weather-details'
            },
            {
                type: 'slider',
                messageKey: 'WEATHER_RETRY_INTERVAL_MINUTES',
                label: 'Retry interval, minutes',
                defaultValue: 30,
                min: 15,
                max: 60,
                step: 1,
                group: 'weather-details'
            }
        ]
    },
    {
        type: 'submit',
        defaultValue: 'Save Settings'
    }
];
