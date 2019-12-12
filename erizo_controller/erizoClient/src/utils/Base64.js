/* global unescape */

const Base64 = (() => {
  let base64Str;
  let base64Count;

  const END_OF_INPUT = -1;
  const base64Chars = [
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/',
  ];

  const reverseBase64Chars = [];

  for (let i = 0; i < base64Chars.length; i += 1) {
    reverseBase64Chars[base64Chars[i]] = i;
  }

  const setBase64Str = (str) => {
    base64Str = str;
    base64Count = 0;
  };

  const readBase64 = () => {
    if (!base64Str) {
      return END_OF_INPUT;
    }
    if (base64Count >= base64Str.length) {
      return END_OF_INPUT;
    }
    const c = base64Str.charCodeAt(base64Count) & 0xff; // eslint-disable-line no-bitwise
    base64Count += 1;
    return c;
  };

  const encodeBase64 = (str) => {
    let result;
    let lineCount;
    let done;
    setBase64Str(str);
    result = '';
    const inBuffer = new Array(3);
    lineCount = 0;
    done = false;
    while (!done && (inBuffer[0] = readBase64()) !== END_OF_INPUT) {
      inBuffer[1] = readBase64();
      inBuffer[2] = readBase64();
      // eslint-disable-next-line no-bitwise
      result += (base64Chars[inBuffer[0] >> 2]);
      if (inBuffer[1] !== END_OF_INPUT) {
        // eslint-disable-next-line no-bitwise
        result += (base64Chars[((inBuffer[0] << 4) & 0x30) | (inBuffer[1] >> 4)]);
        if (inBuffer[2] !== END_OF_INPUT) {
          // eslint-disable-next-line no-bitwise
          result += (base64Chars[((inBuffer[1] << 2) & 0x3c) | (inBuffer[2] >> 6)]);
          // eslint-disable-next-line no-bitwise
          result += (base64Chars[inBuffer[2] & 0x3F]);
        } else {
          // eslint-disable-next-line no-bitwise
          result += (base64Chars[((inBuffer[1] << 2) & 0x3c)]);
          result = `${result}=`;
          done = true;
        }
      } else {
        // eslint-disable-next-line no-bitwise
        result += (base64Chars[((inBuffer[0] << 4) & 0x30)]);
        result = `${result}=`;
        result = `${result}=`;
        done = true;
      }
      lineCount += 4;
      if (lineCount >= 76) {
        result = `${result}\n`;
        lineCount = 0;
      }
    }
    return result;
  };

  const readReverseBase64 = () => {
    if (!base64Str) {
      return END_OF_INPUT;
    }
    // eslint-disable-next-line no-constant-condition
    while (true) {
      if (base64Count >= base64Str.length) {
        return END_OF_INPUT;
      }
      const nextCharacter = base64Str.charAt(base64Count);
      base64Count += 1;
      if (reverseBase64Chars[nextCharacter]) {
        return reverseBase64Chars[nextCharacter];
      }
      if (nextCharacter === 'A') {
        return 0;
      }
    }
  };

  const ntos = (value) => {
    let n = value.toString(16);
    if (n.length === 1) {
      n = `0${n}`;
    }
    n = `%${n}`;
    return unescape(n);
  };

  const decodeBase64 = (str) => {
    let result;
    let done;
    setBase64Str(str);
    result = '';
    const inBuffer = new Array(4);
    done = false;
    while (!done &&
              (inBuffer[0] = readReverseBase64()) !== END_OF_INPUT &&
              (inBuffer[1] = readReverseBase64()) !== END_OF_INPUT) {
      inBuffer[2] = readReverseBase64();
      inBuffer[3] = readReverseBase64();
      // eslint-disable-next-line no-bitwise,no-mixed-operators
      result += ntos((((inBuffer[0] << 2) & 0xff) | inBuffer[1] >> 4));
      if (inBuffer[2] !== END_OF_INPUT) {
        // eslint-disable-next-line no-bitwise,no-mixed-operators
        result += ntos((((inBuffer[1] << 4) & 0xff) | inBuffer[2] >> 2));
        if (inBuffer[3] !== END_OF_INPUT) {
          // eslint-disable-next-line no-bitwise
          result += ntos((((inBuffer[2] << 6) & 0xff) | inBuffer[3]));
        } else {
          done = true;
        }
      } else {
        done = true;
      }
    }
    return result;
  };

  return {
    encodeBase64,
    decodeBase64,
  };
})();

export default Base64;
