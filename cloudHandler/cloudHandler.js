require('./rpc/rpc');

var INTERVAL_TIME_EC_READY = 100;
var INTERVAL_TIME_CHECK_KA = 5000;

var erizoControllers = {};
var ecQueue = [];
var idIndex = 0;

var checkKA = function() {

	for(var ec in erizoControllers) {
		erizoControllers[ec].keepAlive++;
		if (erizoControllers[ec].keepAlive > 10) {
			console.log('ErizoController', ec ,' in ', erizoControllers[ec].ip, 'does not respond. Deleting it.');
			delete erizoControllers[ec];
		};
		
	}
}

var checkKAInterval = setInterval(checkKA, INTERVAL_TIME_CHECK_KA);

var recalculatePriority = function() {

	//*******************************************************************
	// TODO: Algoritmo de ordenacion de ecQueue en función de los 'state' de erizoControllers
	// 		 En la cola están ordenados por prioridad los EC disponibles
	//       Si la cola se queda vacía se arranca un EC nuevo
	//
	//       Decidir qué estados hay
	//*******************************************************************

}

exports.addNewErizoController = function(ip, callback) {
	var id = ++idIndex;
	erizoControllers[id] = {ip: ip, state: 0, keepAlive: 0};
	console.log('New erizocontroller', id , 'in: ', erizoControllers[id].ip);
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
		console.log('KA: ', id);
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

exports.getTheBestEC = function(callback) {
	
	var id;

	var intervarId = setInterval(function() {

		id = ecQueue[0];

		if (id !== undefined) {
			var ip = erizoControllers[params.id].ip;
			callback(ip);

			clearInterval(intervarId);
		};


    }, INTERVAL_TIME_EC_READY);

}

















