/* global require, exports, Buffer */


const crypto = require('crypto');

/*
 * Parses a string header to a json with the fields of the authentication header.
 */
exports.parseHeader = (header) => {
  const params = {};
  let array = [];
  const p = header.split(',');
  let i;
  let j;
  let val;

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
exports.makeHeader = (params) => {
  if (params.realm === undefined) {
    return undefined;
  }

  let header = `MAuth realm=${params.realm}`;
  Object.keys(params).forEach((key) => {
    if (key !== 'realm') {
      header += `,mauth_${key}=${params[key]}`;
    }
  });
  return header;
};

/*
 * Given a json with the header params and the key calculates the signature.
 */
exports.calculateClientSignature = (params, key) => {
  let toSign = `${params.timestamp},${params.cnonce}`;

  if (params.username !== undefined && params.role !== undefined) {
    toSign += `,${params.username},${params.role}`;
  }

  const signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

  return (new Buffer(signed)).toString('base64');
};

/*
 * Given a json with the header params and the key calculates the signature.
 */
exports.calculateServerSignature = (params, key) => {
  const toSign = params.timestamp;
  const signed = crypto.createHmac('sha1', key).update(toSign).digest('hex');

  return (new Buffer(signed)).toString('base64');
};
