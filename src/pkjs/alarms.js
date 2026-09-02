var MAX_ALARMS = 5;
var DAYS_ALL = 0x7f;
var DAYS_WEEKDAYS = 0x3e;
var DAYS_WEEKENDS = 0x41;

var REPEAT_ONCE = 0;
var REPEAT_EVERY_DAY = 1;
var REPEAT_WEEKDAYS = 2;
var REPEAT_WEEKENDS = 3;
var REPEAT_SELECTED = 4;

var defaultState = JSON.stringify({ a: [] });

function integerNumber(value) {
    return typeof value === 'number' && isFinite(value) && Math.floor(value) === value;
}

function pad2(value) {
    return value < 10 ? '0' + value : String(value);
}

function localDateString(date) {
    return date.getFullYear() + '-' + pad2(date.getMonth() + 1) + '-' + pad2(date.getDate());
}

function validTime(value) {
    return typeof value === 'string' && /^([01][0-9]|2[0-3]):([0-5][0-9])$/.test(value);
}

function dateParts(value) {
    if (typeof value !== 'string') {
        return null;
    }

    var match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(value);
    if (!match) {
        return null;
    }

    var year = parseInt(match[1], 10);
    var month = parseInt(match[2], 10);
    var day = parseInt(match[3], 10);
    var date = new Date(year, month - 1, day, 0, 0, 0, 0);

    if (date.getFullYear() !== year || date.getMonth() !== month - 1 || date.getDate() !== day) {
        return null;
    }

    return { year: year, month: month, day: day };
}

function minuteFromTime(value) {
    var parts = value.split(':');
    return parseInt(parts[0], 10) * 60 + parseInt(parts[1], 10);
}

function nextOneTimeDate(timeValue, now) {
    var current = now instanceof Date ? now : new Date();
    var minute = validTime(timeValue) ? minuteFromTime(timeValue) : 7 * 60;
    var candidate = new Date(
        current.getFullYear(),
        current.getMonth(),
        current.getDate(),
        Math.floor(minute / 60),
        minute % 60,
        0,
        0
    );

    if (candidate.getTime() <= current.getTime()) {
        candidate.setDate(candidate.getDate() + 1);
    }

    return localDateString(candidate);
}

function repeatMode(value) {
    var mode = value;
    if (typeof mode === 'string' && /^\d+$/.test(mode)) {
        mode = parseInt(mode, 10);
    }
    if (!integerNumber(mode) || mode < REPEAT_ONCE || mode > REPEAT_SELECTED) {
        return REPEAT_ONCE;
    }
    return mode;
}

function maskForRepeat(mode, selectedMask) {
    if (mode === REPEAT_EVERY_DAY) {
        return DAYS_ALL;
    }
    if (mode === REPEAT_WEEKDAYS) {
        return DAYS_WEEKDAYS;
    }
    if (mode === REPEAT_WEEKENDS) {
        return DAYS_WEEKENDS;
    }
    if (mode === REPEAT_SELECTED) {
        return selectedMask & DAYS_ALL;
    }
    return 0;
}

function scheduledDate(alarm) {
    var parts = dateParts(alarm.d);
    if (!parts || !validTime(alarm.t)) {
        return null;
    }

    var minute = minuteFromTime(alarm.t);
    return new Date(parts.year, parts.month - 1, parts.day,
        Math.floor(minute / 60), minute % 60, 0, 0);
}

function onceHasExpired(alarm, now) {
    var when = scheduledDate(alarm);
    if (!when) {
        return true;
    }

    return when.getTime() <= now.getTime();
}

function normalizeAlarm(value, expirePast) {
    var source = value && typeof value === 'object' ? value : {};
    var timeValue = validTime(source.t) ? source.t : '07:00';
    var alarm = {
        e: 1,
        t: timeValue,
        r: repeatMode(source.r),
        d: dateParts(source.d) ? source.d : nextOneTimeDate(timeValue, new Date()),
        m: integerNumber(source.m) ? source.m & DAYS_ALL : DAYS_WEEKDAYS
    };

    if (alarm.r === REPEAT_SELECTED && alarm.m === 0) {
        alarm.m = 1 << 1;
    }

    return alarm;
}

function normalizeState(value, expirePast) {
    var source = value;
    if (typeof source === 'string') {
        try {
            source = JSON.parse(source);
        } catch (error) {
            source = null;
        }
    }
    if (!source || typeof source !== 'object') {
        source = {};
    }

    var state = {
        a: []
    };
    var alarms = source.a instanceof Array ? source.a : [];
    var i;

    for (i = 0; i < alarms.length && state.a.length < MAX_ALARMS; i += 1) {
        var alarm = normalizeAlarm(alarms[i], expirePast);

        if (!alarms[i] || alarms[i].e === 0) {
            continue;
        }
        if (expirePast && alarm.r === REPEAT_ONCE && onceHasExpired(alarm, new Date())) {
            continue;
        }

        state.a.push(alarm);
    }

    return state;
}

function encode(stateValue) {
    var state = normalizeState(stateValue, true);
    var data = [state.a.length];

    state.a.forEach(function(alarm) {
        var minute = minuteFromTime(alarm.t);
        var mode = alarm.r === REPEAT_ONCE ? 0 : 1;
        var mask = maskForRepeat(alarm.r, alarm.m);
        var enabled = 1;
        var parts = alarm.r === REPEAT_ONCE ? dateParts(alarm.d) : null;

        if ((mode === 1 && mask === 0) || (mode === 0 && !parts)) {
            enabled = 0;
        }

        var year = parts ? parts.year : 0;
        var month = parts ? parts.month : 0;
        var day = parts ? parts.day : 0;

        data.push(
            enabled,
            mode,
            minute & 0xff,
            (minute >> 8) & 0xff,
            mask,
            0, // Reserved.
            year & 0xff,
            (year >> 8) & 0xff,
            month,
            day
        );
    });

    return data;
}

module.exports = {
    defaultState: defaultState,
    normalizeState: normalizeState,
    encode: encode
};
