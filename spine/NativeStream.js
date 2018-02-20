const Erizo = require('./erizofc');

exports.Stream = (altConnectionHelpers, specInput) => {
  const spec = specInput;
  spec.videoInfo = spec.video;
  spec.audioInfo = spec.audio;
  spec.dataInfo = spec.data;
  const that = Erizo.Stream(altConnectionHelpers, spec);

  that.hasVideo = () => spec.videoInfo;
  that.hasAudio = () => spec.audioInfo;
  that.hasData = () => spec.dataInfo;

  return that;
};
