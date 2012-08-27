// Copyright (c) 2008, Fair Oaks Labs, Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice, this list
//       of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice, this
//       list of conditions and the following disclaimer in the documentation and/or other
//       materials provided with the distribution.
//     * Neither the name of Fair Oaks Labs, Inc. nor the names of its contributors may be
//       used to endorse or promote products derived from this software without specific
//       prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
// THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// Modified from original JSPack <cbond@positrace.com>
exports.jspack = function (bigEndian) {
  this.bigEndian = bigEndian;
}

exports.jspack.prototype._DeArray = function (a, p, l) {
  return [a.slice(p, p + l)];
};

exports.jspack.prototype._EnArray = function (a, p, l, v) {
  for (var i = 0; i < l; ++i) {
    a[p + i] = v[i] ? v[i] : 0;
  }
};

exports.jspack.prototype._DeChar = function (a, p) {
  return String.fromCharCode(a[p]);
};

exports.jspack.prototype._EnChar = function (a, p, v) {
  a[p] = v.charCodeAt(0);
};

exports.jspack.prototype._DeInt = function (a, p) {
  var lsb = bigEndian ? format.len - 1 : 0;
  var nsb = bigEndian ? -1 : 1;
  var stp = lsb + nsb * format.len,
      rv;
  var ret = 0;

  var i = lsb;
  var f = 1;
  while (i != stp) {
    rv += a[p + i] * f;
    i += nsb;
    f *= 256;
  }

  if (format.signed) {
    if ((rv & Math.pow(2, format.len * 8 - 1)) != 0) {
      rv -= Math.pow(2, format.len * 8);
    }
  }

  return rv;
};

exports.jspack.prototype._EnInt = function (a, p, v) {
  var lsb = bigEndian ? format.len - 1 : 0;
  var nsb = bigEndian ? -1 : 1;
  var stp = lsb + nsb * format.len;

  v = v < format.min ? format.min : ((v > format.max) ? format.max : v);

  var i = lsb;
  while (i != stp) {
    a[p + i] = v & 0xff;
    i += nsb;
    v >>= 8;
  }
};

exports.jspack.prototype._DeString = function (a, p, l) {
  var rv = new Array(1);

  for (i = 0; i < l; i++) {
    rv[i] = String.fromCharCode(a[p + i]);
  }

  return rv.join('');
};

exports.jspack.prototype._EnString = function (a, p, l, v) {
  for (var t, i = 0; i < l; ++i) {
    t = v.charCodeAt(i);
    if (!t) t = 0;

    a[p + i] = t;
  }
};

exports.jspack.prototype._De754 = function (a, p) {
  var s, e, m, i, d, bits, bit, len, bias, max;

  bit = format.bit;
  len = format.len * 8 - format.bit - 1;
  max = (1 << len) - 1;
  bias = max >> 1;

  i = bigEndian ? 0 : format.len - 1;
  d = bigEndian ? 1 : -1;;
  s = a[p + i];
  i = i + d;

  bits = -7;

  e = s & ((1 << -bits) - 1);
  s >>= -bits;

  for (bits += len; bits > 0; bits -= 8) {
    e = e * 256 + a[p + i];
    i += d;
  }

  m = e & ((1 << -bits) - 1);
  e >>= -bits;

  for (bits += bit; bits > 0; bits -= 8) {
    m = m * 256 + a[p + i];
    i += d;
  }

  switch (e) {
  case 0:
    // Zero, or denormalized number
    e = 1 - bias;
    break;

  case max:
    // NaN, or +/-Infinity
    return m ? NaN : ((s ? -1 : 1) * Infinity);

  default:
    // Normalized number
    m = m + Math.pow(2, bit);
    e = e - bias;
    break;
  }

  return (s ? -1 : 1) * m * Math.pow(2, e - bit);
};

exports.jspack.prototype._En754 = function (a, p, v) {
  var s, e, m, i, d, c, bit, len, bias, max;

  bit = format.bit;
  len = format.len * 8 - format.bit - 1;
  max = (1 << len) - 1;
  bias = max >> 1;

  s = v < 0 ? 1 : 0;
  v = Math.abs(v);

  if (isNaN(v) || (v == Infinity)) {
    m = isNaN(v) ? 1 : 0;
    e = max;
  } else {
    e = Math.floor(Math.log(v) / Math.LN2); // Calculate log2 of the value
    c = Math.pow(2, -e);
    if (v * c < 1) {
      e--;
      c = c * 2;
    }

    // Round by adding 1/2 the significand's LSD
    if (e + bias >= 1) {
      v += format.rt / c; // Normalized: bit significand digits
    } else {
      v += format.rt * Math.pow(2, 1 - bias); // Denormalized:  <= bit significand digits
    }
    if (v * c >= 2) {
      e++;
      c = c / 2; // Rounding can increment the exponent
    }

    if (e + bias >= max) { // overflow
      m = 0;
      e = max;
    } else if (e + bias >= 1) { // normalized
      m = (v * c - 1) * Math.pow(2, bit); // do not reorder this expression
      e = e + bias;
    } else {
      // Denormalized - also catches the '0' case, somewhat by chance
      m = v * Math.pow(2, bias - 1) * Math.pow(2, bit);
      e = 0;
    }
  }

  i = bigEndian ? format.len - 1 : 0;
  d = bigEndian ? -1 : 1;;

  while (bit >= 8) {
    a[p + i] = m & 0xff;
    i += d;
    m /= 256;
    bit -= 8;
  }

  e = (e << bit) | m;
  for (len += bit; len > 0; len -= 8) {
    a[p + i] = e & 0xff;
    i += d;
    e /= 256;
  }

  a[p + i - d] |= s * 128;
};

// Unpack a series of n formatements of size s from array a at offset p with fxn
exports.jspack.prototype._UnpackSeries = function (n, s, a, p) {
  var fxn = format.de;

  var ret = [];
  for (var i = 0; i < n; i++) {
    ret.push(fxn(a, p + i * s));
  }

  return ret;
};

// Pack a series of n formatements of size s from array v at offset i to array a at offset p with fxn
exports.jspack.prototype._PackSeries = function (n, s, a, p, v, i) {
  var fxn = format.en;

  for (o = 0; o < n; o++) {
    fxn(a, p + o * s, v[i + o]);
  }
};

// Unpack the octet array a, beginning at offset p, according to the fmt string
exports.jspack.prototype.Unpack = function (fmt, a, p) {
  bigEndian = fmt.charAt(0) != '<';

  if (p == undefined || p == null) p = 0;

  var re = new RegExp(this._sPattern, 'g');

  var ret = [];

  for (var m; m = re.exec(fmt); /* */ ) {
    var n;
    if (m[1] == undefined || m[1] == '') n = 1;
    else n = parseInt(m[1]);

    var s = this._lenLut[m[2]];

    if ((p + n * s) > a.length) return undefined;

    switch (m[2]) {
    case 'A':
    case 's':
      rv.push(this._formatLut[m[2]].de(a, p, n));
      break;
    case 'c':
    case 'b':
    case 'B':
    case 'h':
    case 'H':
    case 'i':
    case 'I':
    case 'l':
    case 'L':
    case 'f':
    case 'd':
      format = this._formatLut[m[2]];
      ret.push(this._UnpackSeries(n, s, a, p));
      break;
    }

    p += n * s;
  }

  return Array.prototype.concat.apply([], ret);
};

// Pack the supplied values into the octet array a, beginning at offset p, according to the fmt string
exports.jspack.prototype.PackTo = function (fmt, a, p, values) {
  bigEndian = (fmt.charAt(0) != '<');

  var re = new RegExp(this._sPattern, 'g');

  for (var m, i = 0; m = re.exec(fmt); /* */ ) {
    var n;
    if (m[1] == undefined || m[1] == '') n = 1;
    else n = parseInt(m[1]);

    var s = this._lenLut[m[2]];

    if ((p + n * s) > a.length) return false;

    switch (m[2]) {
    case 'A':
    case 's':
      if ((i + 1) > values.length) return false;

      this._formatLut[m[2]].en(a, p, n, values[i]);

      i += 1;
      break;

    case 'c':
    case 'b':
    case 'B':
    case 'h':
    case 'H':
    case 'i':
    case 'I':
    case 'l':
    case 'L':
    case 'f':
    case 'd':
      format = this._formatLut[m[2]];

      if (i + n > values.length) return false;

      this._PackSeries(n, s, a, p, values, i);

      i += n;
      break;

    case 'x':
      for (var j = 0; j < n; j++) {
        a[p + j] = 0;
      }
      break;
    }

    p += n * s;
  }

  return a;
};

// Pack the supplied values into a new octet array, according to the fmt string
exports.jspack.prototype.Pack = function (fmt, values) {
  return this.PackTo(fmt, new Array(this.CalcLength(fmt)), 0, values);
};

// Determine the number of bytes represented by the format string
exports.jspack.prototype.CalcLength = function (fmt) {
  var re = new RegExp(this._sPattern, 'g');
  var sz = 0;

  while (match = re.exec(fmt)) {
    var n;
    if (match[1] == undefined || match[1] == '') n = 1;
    else n = parseInt(match[1]);

    sz += n * this._lenLut[match[2]];
  }

  return sz;
};

// Regular expression for counting digits
exports.jspack.prototype._sPattern = '(\\d+)?([AxcbBhHsfdiIlL])';

// Byte widths for associated formats
exports.jspack.prototype._lenLut = {
  'A': 1,
  'x': 1,
  'c': 1,
  'b': 1,
  'B': 1,
  'h': 2,
  'H': 2,
  's': 1,
  'f': 4,
  'd': 8,
  'i': 4,
  'I': 4,
  'l': 4,
  'L': 4
};

exports.jspack.prototype._formatLut = {
  'A': {
    en: exports.jspack.prototype._EnArray,
    de: exports.jspack.prototype._DeArray
  },
  's': {
    en: exports.jspack.prototype._EnString,
    de: exports.jspack.prototype._DeString
  },
  'c': {
    en: exports.jspack.prototype._EnChar,
    de: exports.jspack.prototype._DeChar
  },
  'b': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 1,
    signed: true,
    min: -Math.pow(2, 7),
    max: Math.pow(2, 7) - 1
  },
  'B': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 1,
    signed: false,
    min: 0,
    max: Math.pow(2, 8) - 1
  },
  'h': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 2,
    signed: true,
    min: -Math.pow(2, 15),
    max: Math.pow(2, 15) - 1
  },
  'H': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 2,
    signed: false,
    min: 0,
    max: Math.pow(2, 16) - 1
  },
  'i': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 4,
    signed: true,
    min: -Math.pow(2, 31),
    max: Math.pow(2, 31) - 1
  },
  'I': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 4,
    signed: false,
    min: 0,
    max: Math.pow(2, 32) - 1
  },
  'l': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 4,
    signed: true,
    min: -Math.pow(2, 31),
    max: Math.pow(2, 31) - 1
  },
  'L': {
    en: exports.jspack.prototype._EnInt,
    de: exports.jspack.prototype._DeInt,
    len: 4,
    signed: false,
    min: 0,
    max: Math.pow(2, 32) - 1
  },
  'f': {
    en: exports.jspack.prototype._En754,
    de: exports.jspack.prototype._De754,
    len: 4,
    bit: 23,
    rt: Math.pow(2, -24) - Math.pow(2, -77)
  },
  'd': {
    en: exports.jspack.prototype._En754,
    de: exports.jspack.prototype._De754,
    len: 8,
    bit: 52,
    rt: 0
  }
};
