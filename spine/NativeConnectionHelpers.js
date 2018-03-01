/* global */
const logger = require('./logger').logger;

const log = logger.getLogger('NativeConnectionHelpers');

exports.getBrowser = () => 'fake';

exports.GetUserMedia = (opt, callback) => {
  log.info('Fake getUserMedia to use with files', opt);
  // if (that.peerConnection && opt.video.file){
  //     that.peerConnection.prepareVideo(opt.video.file);
  // }
  callback('');
};
