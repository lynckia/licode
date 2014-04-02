/*global require, logger. setInterval, clearInterval, Buffer, exports*/
var rpc = require('./../common/rpc');
var controller = require('./erizoJSController');
var logger = require('./../common/logger').logger;

var ejsController = controller.ErizoJSController();

ejsController.keepAlive = function(callback) {
    logger.info("KeepAlive from ErizoController");
    callback('callback', true);
};

rpc.connect(function () {
    "use strict";

    rpc.setPublicRPC(ejsController);

    var rpcID = process.argv[2];

    console.log("ID: ErizoJS_" + rpcID);

    rpc.bind("ErizoJS_" + rpcID, function() {
        console.log("ErizoJS started");
        
    });

});
/*
var sdp = '{"messageType":"OFFER","sdp":"v=0\\r\\no=- 6915254541026829363 2 IN IP4 127.0.0.1\\r\\ns=-\\r\\nt=0 0\\r\\na=group:BUNDLE audio video\\r\\na=msid-semantic: WMS UnArvhkmFB7DVXMqb7f4dcEdgmaRQ2vQbVEO\\r\\nm=audio 54748 RTP/SAVPF 111 103 104 0 8 106 105 13 126\\r\\nc=IN IP4 192.168.56.1\\r\\na=rtcp:54748 IN IP4 192.168.56.1\\r\\na=candidate:2999745851 1 udp 2113937151 192.168.56.1 54748 typ host generation 0\\r\\na=candidate:2999745851 2 udp 2113937151 192.168.56.1 54748 typ host generation 0\\r\\na=candidate:3633925102 1 udp 2113937151 138.4.4.168 54749 typ host generation 0\\r\\na=candidate:3633925102 2 udp 2113937151 138.4.4.168 54749 typ host generation 0\\r\\na=candidate:4233069003 1 tcp 1509957375 192.168.56.1 0 typ host generation 0\\r\\na=candidate:4233069003 2 tcp 1509957375 192.168.56.1 0 typ host generation 0\\r\\na=candidate:2518333214 1 tcp 1509957375 138.4.4.168 0 typ host generation 0\\r\\na=candidate:2518333214 2 tcp 1509957375 138.4.4.168 0 typ host generation 0\\r\\na=ice-ufrag:A8dCYvqNBi3AR8mA\\r\\na=ice-pwd:vFqVsVfVRRPSZLS+mRUGsJ6K\\r\\na=ice-options:google-ice\\r\\na=fingerprint:sha-256 72:D5:1E:A0:CD:71:C9:D8:65:99:BD:BB:82:E7:A5:AE:BE:DF:23:4C:0F:D8:8C:26:97:9E:5A:CA:56:00:57:47\\r\\na=setup:actpass\\r\\na=mid:audio\\r\\na=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\\r\\na=sendrecv\\r\\na=rtcp-mux\\r\\na=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:Ps33PIN5iNIn/5DCRR0KnQr/vvRMmMB/ziY/kyN/\\r\\na=rtpmap:111 opus/48000/2\\r\\na=fmtp:111 minptime=10\\r\\na=rtpmap:103 ISAC/16000\\r\\na=rtpmap:104 ISAC/32000\\r\\na=rtpmap:0 PCMU/8000\\r\\na=rtpmap:8 PCMA/8000\\r\\na=rtpmap:106 CN/32000\\r\\na=rtpmap:105 CN/16000\\r\\na=rtpmap:13 CN/8000\\r\\na=rtpmap:126 telephone-event/8000\\r\\na=maxptime:60\\r\\na=ssrc:3839633812 cname:8GSPxlTIqeSLyJtS\\r\\na=ssrc:3839633812 msid:UnArvhkmFB7DVXMqb7f4dcEdgmaRQ2vQbVEO 876be926-357a-4342-b3fb-03db9870cc93\\r\\na=ssrc:3839633812 mslabel:UnArvhkmFB7DVXMqb7f4dcEdgmaRQ2vQbVEO\\r\\na=ssrc:3839633812 label:876be926-357a-4342-b3fb-03db9870cc93\\r\\nm=video 54748 RTP/SAVPF 100 116 117\\r\\nc=IN IP4 192.168.56.1\\r\\na=rtcp:54748 IN IP4 192.168.56.1\\r\\na=candidate:2999745851 1 udp 2113937151 192.168.56.1 54748 typ host generation 0\\r\\na=candidate:2999745851 2 udp 2113937151 192.168.56.1 54748 typ host generation 0\\r\\na=candidate:3633925102 1 udp 2113937151 138.4.4.168 54749 typ host generation 0\\r\\na=candidate:3633925102 2 udp 2113937151 138.4.4.168 54749 typ host generation 0\\r\\na=candidate:4233069003 1 tcp 1509957375 192.168.56.1 0 typ host generation 0\\r\\na=candidate:4233069003 2 tcp 1509957375 192.168.56.1 0 typ host generation 0\\r\\na=candidate:2518333214 1 tcp 1509957375 138.4.4.168 0 typ host generation 0\\r\\na=candidate:2518333214 2 tcp 1509957375 138.4.4.168 0 typ host generation 0\\r\\na=ice-ufrag:A8dCYvqNBi3AR8mA\\r\\na=ice-pwd:vFqVsVfVRRPSZLS+mRUGsJ6K\\r\\na=ice-options:google-ice\\r\\na=fingerprint:sha-256 72:D5:1E:A0:CD:71:C9:D8:65:99:BD:BB:82:E7:A5:AE:BE:DF:23:4C:0F:D8:8C:26:97:9E:5A:CA:56:00:57:47\\r\\na=setup:actpass\\r\\na=mid:video\\r\\na=extmap:2 urn:ietf:params:rtp-hdrext:toffset\\r\\na=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\\r\\na=sendrecv\\r\\nb=AS:300\\r\\na=rtcp-mux\\r\\na=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:Ps33PIN5iNIn/5DCRR0KnQr/vvRMmMB/ziY/kyN/\\r\\na=rtpmap:100 VP8/90000\\r\\na=rtcp-fb:100 ccm fir\\r\\na=rtcp-fb:100 nack\\r\\na=rtcp-fb:100 goog-remb\\r\\na=rtpmap:116 red/90000\\r\\na=rtpmap:117 ulpfec/90000\\r\\na=ssrc:2014607583 cname:8GSPxlTIqeSLyJtS\\r\\na=ssrc:2014607583 msid:UnArvhkmFB7DVXMqb7f4dcEdgmaRQ2vQbVEO f396aae1-913c-4801-8e51-f194ff0e51fa\\r\\na=ssrc:2014607583 mslabel:UnArvhkmFB7DVXMqb7f4dcEdgmaRQ2vQbVEO\\r\\na=ssrc:2014607583 label:f396aae1-913c-4801-8e51-f194ff0e51fa\\r\\n","offererSessionId":104,"seq":1,"tiebreaker":120058340}';
controller.RoomController().addPublisher("1", sdp, function(type, answer) {
    console.log(answer);
});
*/