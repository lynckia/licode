/*global require, exports, Buffer*/
var crypto = require('crypto');

/*
 * Parses a string header to a json with the fields of the authentication header.
 */
exports.parseHeader = function (header) {
    "use strict";

    var params = {},

        array = [],
        p = header.split(','),
        i,
        j,
        val;

    for (i = 0; i < p.length; i += 1) {

        array = p[i].split('=');
        val = '';

        for (j = 1; j < array.length; j += 1) {
            if (array[j] === '') {
                val += '=';
            } else {
                val += array[j];
            }
        }

        params[array[0].slice(6)] = val;

    }
    return params;
};

/*
 * Makes a string header from a json with the fields of an authentication header.
 */
exports.makeHeader = function (params) {
    "use strict";

    if (params.realm === undefined) {
        return undefined;
    }

    var header = 'MAuth realm=\"' + params.realm + '\"',
        key;

    for (key in params) {
        if (params.hasOwnProperty(key)) {
            if (key !== 'realm') {
                header += ',mauth_' + key + '=\"' + params[key] + '\"';
            }
        }
    }
    return header;
};

/*
 * Given a json with the header params and the key calculates the signature.
 */
exports.calculateClientSignature = function (params, key) {
    "use strict";

    var toSign = params.timestamp + ',' + params.cnonce,
        signed;

    if (params.username !== undefined && params.role !== undefined) {
        toSign += ',' + params.username + ',' + params.role;
    }

    signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

    return (new Buffer(signed)).toString('base64');
};

/*
 * Given a json with the header params and the key calculates the signature.
 */
exports.calculateServerSignature = function (params, key) {
    "use strict";

    var toSign = params.timestamp,
        signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

    return (new Buffer(signed)).toString('base64');
};
