var erizoController = require('./../erizoController');

/*
 * This function is called remotely from nuve to get a list of the users in a determined room.
 */
exports.setPublisher = function(id, callback) {

    erizoController.getUsersInRoom(id, function(users) {

        //console.log('Users for room ', id, ' are ', users);
        if(users == undefined) {
            callback('error');
        } else {
            callback(users);
        }
    });
}

exports.addSubscriber = function(roomId, callback) {
    erizoController.deleteRoom(roomId, function(result) {
        callback(result);
    });
}