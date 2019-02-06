
const erizoController = require('./../erizoController');
const ReplManager = require('./../../common/ROV/replManager').ReplManager;

let replManager = false;
/*
 * This function is called remotely from nuve to get a list of the users in a determined room.
 */
exports.getUsersInRoom = (id, callback) => {
  erizoController.getUsersInRoom(id, (users) => {
    if (users === undefined) {
      callback('callback', 'error');
    } else {
      callback('callback', users);
    }
  });
};

exports.deleteRoom = (roomId, callback) => {
  erizoController.deleteRoom(roomId, (result) => {
    callback('callback', result);
  });
};

exports.deleteUser = (args, callback) => {
  const user = args.user;
  const roomId = args.roomId;
  erizoController.deleteUser(user, roomId, (result) => {
    callback('callback', result);
  });
};

exports.rovMessage = (args, callback) => {
  if (!replManager) {
    replManager = new ReplManager(erizoController.getContext());
  }
  replManager.processRpcMessage(args, callback);
};
