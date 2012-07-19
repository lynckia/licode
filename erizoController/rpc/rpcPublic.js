var erizoController = require('./../erizoController');

exports.getUsersInRoom = function(id, callback) {

    erizoController.getUsersInRoom(id, function(users) {

    console.log('me piden usuarios para ', id, 'y son ', users);
        if(users == undefined) {
            callback('error');
        } else {
            callback(users);
        }
    });

}