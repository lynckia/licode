const SdpHelpers = {};

SdpHelpers.addSim = (spatialLayers) => {
  let line = 'a=ssrc-group:SIM';
  spatialLayers.forEach((spatialLayerId) => {
    line += ` ${spatialLayerId}`;
  });
  return `${line}\r\n`;
};

SdpHelpers.addGroup = (spatialLayerId, spatialLayerIdRtx) =>
  `a=ssrc-group:FID ${spatialLayerId} ${spatialLayerIdRtx}\r\n`;

SdpHelpers.addSpatialLayer = (cname, msid, mslabel,
  label, spatialLayerId, spatialLayerIdRtx) =>
  `a=ssrc:${spatialLayerId} cname:${cname}\r\n` +
  `a=ssrc:${spatialLayerId} msid:${msid}\r\n` +
  `a=ssrc:${spatialLayerId} mslabel:${mslabel}\r\n` +
  `a=ssrc:${spatialLayerId} label:${label}\r\n` +
  `a=ssrc:${spatialLayerIdRtx} cname:${cname}\r\n` +
  `a=ssrc:${spatialLayerIdRtx} msid:${msid}\r\n` +
  `a=ssrc:${spatialLayerIdRtx} mslabel:${mslabel}\r\n` +
  `a=ssrc:${spatialLayerIdRtx} label:${label}\r\n`;

SdpHelpers.setMaxBW = (sdpInput, spec) => {
  let r;
  let a;
  let sdp = sdpInput;
  if (spec.video && spec.maxVideoBW) {
    sdp = sdp.replace(/b=AS:.*\r\n/g, '');
    a = sdp.match(/m=video.*\r\n/);
    if (a == null) {
      a = sdp.match(/m=video.*\n/);
    }
    if (a && (a.length > 0)) {
      r = `${a[0]}b=AS:${spec.maxVideoBW}\r\n`;
      sdp = sdp.replace(a[0], r);
    }
  }

  if (spec.audio && spec.maxAudioBW) {
    a = sdp.match(/m=audio.*\r\n/);
    if (a == null) {
      a = sdp.match(/m=audio.*\n/);
    }
    if (a && (a.length > 0)) {
      r = `${a[0]}b=AS:${spec.maxAudioBW}\r\n`;
      sdp = sdp.replace(a[0], r);
    }
  }
  return sdp;
};

SdpHelpers.filterCodecFromMLine = (sdpInput, codecNum) => {
  return sdpInput
    .replace(new RegExp(`^(m=\\S+ \\S+ \\S+(?: \\d+)*) ${codecNum}`, 'm'), '$1');
};

SdpHelpers.filterCodec = (sdpInput, codec) => {
  const codecNumMatch = sdpInput.match(new RegExp(`a=rtpmap:(\\d+) ${codec}/`, 'i'));
  if (!codecNumMatch) {
    return sdpInput;
  }
  let codecNums = [codecNumMatch[1]];
  const rtxNumMatch = sdpInput.match(new RegExp(`a=fmtp:(\\d+) apt=${codecNums[0]}\\b`, 'i'));
  if (rtxNumMatch) {
    codecNums.push(rtxNumMatch[1]);
  }
  return codecNums.reduce(SdpHelpers.filterCodecFromMLine, sdpInput)
    .replace(new RegExp(`^a=[a-z-]+:(${codecNums.join('|')})\\b.*$`, 'gm'), '')
    .replace(/(\r?\n){2,}/g, '');
};

SdpHelpers.filterCodecs = (sdpInput, codecs) => {
  return codecs.reduce(SdpHelpers.filterCodec, sdpInput);
};

SdpHelpers.enableOpusNacks = (sdpInput) => {
  let sdp = sdpInput;
  const sdpMatch = sdp.match(/a=rtpmap:(.*)opus.*\r\n/);
  if (sdpMatch !== null) {
    const theLine = `${sdpMatch[0]}a=rtcp-fb:${sdpMatch[1]}nack\r\n`;
    sdp = sdp.replace(sdpMatch[0], theLine);
  }

  return sdp;
};

export default SdpHelpers;
