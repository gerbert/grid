var weatherProviders = require('./providers');
var alarms = require('./alarms');

var repeatOptions = [
    { label: 'One shot', value: 0 },
    { label: 'Every day', value: 1 },
    { label: 'Weekdays', value: 2 },
    { label: 'Weekends', value: 3 },
    { label: 'Selected days', value: 4 }
];

var alarmItems = [
    {
        type: 'heading',
        defaultValue: '<table width="100%" cellpadding="0" cellspacing="0"><tr><td>Alarms</td><td id="alarm-add-slot" align="right"></td></tr></table>'
    },
    {
        type: 'input',
        messageKey: 'ALARM_STATE',
        defaultValue: alarms.defaultState,
        attributes: {
            type: 'hidden'
        }
    },
];

for (var alarmIndex = 0; alarmIndex < 5; alarmIndex += 1) {
    alarmItems.push(
        {
            type: 'select',
            id: 'ALARM_REPEAT_' + alarmIndex,
            defaultValue: 0,
            options: repeatOptions
        },
        {
            type: 'input',
            id: 'ALARM_TIME_' + alarmIndex,
            defaultValue: '07:00',
            attributes: {
                type: 'time',
                value: '07:00',
                'aria-label': 'Alarm ' + (alarmIndex + 1) + ' time'
            }
        },
        {
            type: 'button',
            id: 'ALARM_DELETE_' + alarmIndex,
            defaultValue: ' - ',
            primary: true
        },
        {
            type: 'checkboxgroup',
            id: 'ALARM_DAYS_' + alarmIndex,
            label: 'Days',
            defaultValue: [true, true, true, true, true, false, false],
            options: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
        }
    );
}

alarmItems.push({
    type: 'button',
    id: 'ALARM_ADD_BUTTON',
    defaultValue: '+',
    primary: true
});

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
            },
            {
                type: 'toggle',
                messageKey: 'DND_ENABLE',
                label: 'Do not disturb',
                description: 'Prevent glance activation during a scheduled time window',
                defaultValue: false
            },
            {
                type: 'input',
                messageKey: 'DND_START_TIME',
                label: 'Start',
                defaultValue: '22:00',
                attributes: {
                    type: 'time'
                }
            },
            {
                type: 'input',
                messageKey: 'DND_END_TIME',
                label: 'End',
                defaultValue: '07:00',
                attributes: {
                    type: 'time'
                }
            }
        ]
    },
    {
        type: 'section',
        items: alarmItems
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
                    { label: 'Text', value: 0 },
                    { label: 'Icon', value: 1 },
                    { label: 'Icon + text', value: 2 },
                    { label: 'Temperature only', value: 3 }
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
                min: 1,
                max: 30,
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
