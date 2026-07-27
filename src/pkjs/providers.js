var OPEN_METEO_ID = 0;

var PROVIDERS = [
    {
        label: 'Open-Meteo',
        request: requestOpenMeteo
    }
];

function finiteNumber(value) {
    return typeof value === 'number' && isFinite(value);
}

function integerNumber(value) {
    return finiteNumber(value) && Math.floor(value) === value;
}

function validCoordinate(value, minimum, maximum) {
    return finiteNumber(value) && value >= minimum && value <= maximum;
}

function requestOpenMeteo(position, slotCount, timeoutMs, callback) {
    if (!position || !position.coords) {
        callback('location coordinates are missing');
        return;
    }

    var latitude = position.coords.latitude;
    var longitude = position.coords.longitude;

    if (!validCoordinate(latitude, -90, 90) ||
        !validCoordinate(longitude, -180, 180)) {
        callback('invalid location coordinates');
        return;
    }

    var url = 'https://api.open-meteo.com/v1/forecast?' +
        'latitude=' + encodeURIComponent(latitude) +
        '&longitude=' + encodeURIComponent(longitude) +
        '&hourly=temperature_2m,weather_code' +
        '&forecast_hours=' + slotCount +
        '&timeformat=unixtime';

    var completed = false;
    var xhr = new XMLHttpRequest();

    function finish(error, responseText) {
        if (completed) {
            return;
        }

        completed = true;
        callback(error, responseText);
    }

    xhr.onload = function() {
        if (xhr.status < 200 || xhr.status >= 300) {
            finish('Open-Meteo HTTP ' + xhr.status);
            return;
        }

        finish(null, xhr.responseText);
    };

    xhr.onerror = function() {
        finish('Open-Meteo network error');
    };

    xhr.ontimeout = function() {
        finish('Open-Meteo timeout');
    };

    xhr.onabort = function() {
        finish('Open-Meteo request aborted');
    };

    try {
        xhr.timeout = timeoutMs;
        xhr.open('GET', url, true);
        xhr.send();
    } catch (error) {
        finish('unable to start Open-Meteo request');
    }
}

function isValidId(providerId) {
    return integerNumber(providerId) && providerId >= 0 && providerId < PROVIDERS.length;
}

function getConfigOptions() {
    var options = [];
    var i;

    for (i = 0; i < PROVIDERS.length; i += 1) {
        options.push({
            label: PROVIDERS[i].label,
            value: i
        });
    }

    return options;
}

function request(providerId, position, slotCount, timeoutMs, callback) {
    if (!isValidId(providerId)) {
        callback('unsupported weather provider');
        return;
    }

    PROVIDERS[providerId].request(position, slotCount, timeoutMs, callback);
}

module.exports = {
    defaultId: OPEN_METEO_ID,
    openMeteoId: OPEN_METEO_ID,
    getConfigOptions: getConfigOptions,
    isValidId: isValidId,
    request: request
};
