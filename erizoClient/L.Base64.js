var L = L ||{};

L.Base64 = function(L) {
    
    var END_OF_INPUT = -1;
    
    var base64Chars = new Array(
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X',
        'Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n',
        'o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3',
        '4','5','6','7','8','9','+','/'
    );

    var reverseBase64Chars = [];

    var base64Str;
    var base64Count;

    for (var i=0; i < base64Chars.length; i++){
        reverseBase64Chars[base64Chars[i]] = i;
    }

    var setBase64Str = function(str){
        base64Str = str;
        base64Count = 0;
    };

    var readBase64 = function(){
        if (!base64Str) return END_OF_INPUT;
        if (base64Count >= base64Str.length) return END_OF_INPUT;
        var c = base64Str.charCodeAt(base64Count) & 0xff;
        base64Count++;
        return c;
    };

    var encodeBase64 = function(str){
        setBase64Str(str);
        var result = '';
        var inBuffer = new Array(3);
        var lineCount = 0;
        var done = false;
        while (!done && (inBuffer[0] = readBase64()) != END_OF_INPUT){
            inBuffer[1] = readBase64();
            inBuffer[2] = readBase64();
            result += (base64Chars[ inBuffer[0] >> 2 ]);
            if (inBuffer[1] != END_OF_INPUT){
                result += (base64Chars [(( inBuffer[0] << 4 ) & 0x30) | (inBuffer[1] >> 4) ]);
                if (inBuffer[2] != END_OF_INPUT){
                    result += (base64Chars [((inBuffer[1] << 2) & 0x3c) | (inBuffer[2] >> 6) ]);
                    result += (base64Chars [inBuffer[2] & 0x3F]);
                } else {
                    result += (base64Chars [((inBuffer[1] << 2) & 0x3c)]);
                    result += ('=');
                    done = true;
                }
            } else {
                result += (base64Chars [(( inBuffer[0] << 4 ) & 0x30)]);
                result += ('=');
                result += ('=');
                done = true;
            }
            lineCount += 4;
            if (lineCount >= 76){
                result += ('\n');
                lineCount = 0;
            }
        }
        return result;
    };

    var readReverseBase64 = function(){
        if (!base64Str) return END_OF_INPUT;
        while (true){
            if (base64Count >= base64Str.length) return END_OF_INPUT;
            var nextCharacter = base64Str.charAt(base64Count);
            base64Count++;
            if (reverseBase64Chars[nextCharacter]){
                return reverseBase64Chars[nextCharacter];
            }
            if (nextCharacter == 'A') return 0;
        }
        return END_OF_INPUT;
    };
     
    var ntos = function(n){
        n=n.toString(16);
        if (n.length == 1) n="0"+n;
        n="%"+n;
        return unescape(n);
    };
     
    var decodeBase64 = function(str){
        setBase64Str(str);
        var result = "";
        var inBuffer = new Array(4);
        var done = false;
        while (!done && (inBuffer[0] = readReverseBase64()) != END_OF_INPUT && (inBuffer[1] = readReverseBase64()) != END_OF_INPUT){
            inBuffer[2] = readReverseBase64();
            inBuffer[3] = readReverseBase64();
            result += ntos((((inBuffer[0] << 2) & 0xff)| inBuffer[1] >> 4));
            if (inBuffer[2] != END_OF_INPUT){
                result +=  ntos((((inBuffer[1] << 4) & 0xff)| inBuffer[2] >> 2));
                if (inBuffer[3] != END_OF_INPUT){
                    result +=  ntos((((inBuffer[2] << 6)  & 0xff) | inBuffer[3]));
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
        encodeBase64: encodeBase64,
        decodeBase64: decodeBase64
    };
}(L);