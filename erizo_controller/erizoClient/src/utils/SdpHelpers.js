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

SdpHelpers.setMaxBW = (sdp, spec) => {
  if (!spec.p2p) {
    return;
  }
  if (spec.video && spec.maxVideoBW) {
    const video = sdp.getMedia('video');
    if (video) {
      video.setBitrate(spec.maxVideoBW);
    }
  }

  if (spec.audio && spec.maxAudioBW) {
    const audio = sdp.getMedia('audio');
    if (audio) {
      audio.setBitrate(spec.maxAudioBW);
    }
  }
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

SdpHelpers.setParamForCodecs = (sdpInfo, mediaType, paramName, value) => {
  sdpInfo.medias.forEach((mediaInfo) => {
    if (mediaInfo.id === mediaType) {
      mediaInfo.codecs.forEach((codec) => {
        codec.setParam(paramName, value);
      });
    }
  });
};

export default SdpHelpers;
