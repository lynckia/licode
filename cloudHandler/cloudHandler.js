var rpc = require('./rpc/rpc');

var INTERVAL_TIME_EC_READY = 100;
var INTERVAL_TIME_CHECK_KA = 1000;
var MAX_KA_COUNT = 5;

var erizoControllers = {};
var rooms = {};    	// roomId: erizoControllerId
var ecQueue = [];
var idIndex = 0;

var checkKA = function() {

	for(var ec in erizoControllers) {
		erizoControllers[ec].keepAlive++;
		if (erizoControllers[ec].keepAlive > MAX_KA_COUNT) {
			console.log('ErizoController', ec ,' in ', erizoControllers[ec].ip, 'does not respond. Deleting it.');
			delete erizoControllers[ec];
			for(var room in rooms) {
				if(rooms[room] == ec) {
					delete rooms[room];
				}
			}
			recalculatePriority();
		};
		
	}
}

var checkKAInterval = setInterval(checkKA, INTERVAL_TIME_CHECK_KA);

var recalculatePriority = function() {

	//*******************************************************************
	// Algoritmo de ordenacion de ecQueue en función de los 'state' de erizoControllers
	// 		 
	//		En la cola están ordenados por prioridad los EC disponibles
	//      Si la cola se queda vacía se arranca un EC nuevo
	//
	//		States: 
	//				0: Not available
	//				1: Warning
	//				2: Available 
	//*******************************************************************

	var newEcQueue = [];
	var available = 0;
	var warnings = 0;

	for(var ec in erizoControllers) {
		if (erizoControllers[ec].state == 2) {
			newEcQueue.push(ec);
			available++;
		}
	}

	for(var ec in erizoControllers) {
		if (erizoControllers[ec].state == 1) {
			newEcQueue.push(ec);
			warnings++;
		}
	}

	ecQueue = newEcQueue;

	if(ecQueue.length == 0 || (available == 0 && warnings < 2)) {
		//*****************************************
		// TODO: Arrancar un nuevo ErizoController.
		//*****************************************
	}

}

exports.addNewErizoController = function(ip, callback) {
	var id = ++idIndex;
	var rpcID = 'erizoController_' + id;
	erizoControllers[id] = {ip: ip, rpcID: rpcID, state: 2, keepAlive: 0};
	console.log('New erizocontroller (', id , ') in: ', erizoControllers[id].ip);
	recalculatePriority();
	callback(id);
}

exports.keepAlive = function(id, callback) {

	var result;

	if (erizoControllers[id] === undefined) {
		result = 'whoareyou';
		console.log('I received a keepAlive mess from a removed erizoController');
	} else {
		erizoControllers[id].keepAlive = 0;
		result = 'ok';
		//console.log('KA: ', id);
	}
	callback(result);
}

exports.setInfo = function(params) {
	
	console.log('Received info from ', params.id, '.Recalculating erizoControllers priority');
	erizoControllers[params.id].state = params.state;
	recalculatePriority();
}

exports.killMe = function(ip) {

	console.log('Killing erizoController in host ', ip);

	//************************************
	// TODO: Apagar la máquina de IP 'ip'.
	//*************************************

}

exports.getErizoControllerForRoom = function(roomId, callback) {

	if (rooms[roomId] !== undefined) {
		callback(erizoControllers[rooms[roomId]]);
		return;
	}

	var id;

	var intervarId = setInterval(function() {

		id = ecQueue[0];

		if (id !== undefined) {

			rooms[roomId] = id;
			callback(erizoControllers[id]);

			recalculatePriority();
			clearInterval(intervarId);
		};


    }, INTERVAL_TIME_EC_READY);

}

exports.getUsersInRoom = function(roomId, callback) {
	if (rooms[roomId] === undefined) {
		callback([]);
		return;
	}

	var rpcID = erizoControllers[rooms[roomId]].rpcID;
	rpc.callRpc(rpcID, 'getUsersInRoom', roomId, function(users) {
		if(users == 'timeout') users = '?';
		callback(users);
	});
}



