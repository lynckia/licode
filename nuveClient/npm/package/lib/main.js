var N = require('./nuve');

var printAll = function(result) {console.log(JSON.stringify(result,null,4));}

String.prototype.startsWith = function(prefix) {
    return this.indexOf(prefix) === 0;
}

var exitApp = function() {
    console.log('Usage: nuve-client [-s|--service] serviceID [-p|--secret] secret [-l|--url] nuve-url [-t|--token] token [-u|--username] username [-r|--role] role action [arguments]');
    console.log();
    console.log('ServiceID and Secret can be also stored in environment variables:');
    console.log('          NUVE_SERVICE : serviceID');
    console.log('          NUVE_SECRET  : secret');
    console.log();
    console.log('Available actions:');
    console.log('          room-list');
    console.log('          service-list');
    process.exit(1);
}

var serviceID, secret, nuveUrl, token, username, role, action, id, name;

serviceID = process.env.NUVE_SERVICE;
secret = process.env.NUVE_SECRET;
nuveUrl = process.env.NUVE_URL;

for (var i=2;i<process.argv.length;i++) {
  switch( process.argv[i]) {
    case '-h':
    case '--help':
        exitApp();
        break;
    case '-s':
    case '--service':
        i++;
        serviceID = process.argv[i];
        break;
    case '-p':
    case '--secret':
        i++;
        secret = process.argv[i];
        break;
    case '-l':
    case '--url':
        i++;
        nuveUrl = process.argv[i];
        break;
    case '-t':
    case '--token':
        i++;
        token = process.argv[i];
        break;
    case '-u':
    case '--username':
        i++;
        username = process.argv[i];
        break;
    case '-r':
    case '--role':
        i++;
        role = process.argv[i];
        break;
    default:
         if (process.argv[i].startsWith('-')) {
             exitApp();
         }
         action = process.argv[i];
         for (var j=i;j<process.argv.length;j++) {
             switch(process.argv[j]) {
                case '-i':
                case '--id':
                    j++;
                    id = process.argv[j];
                    break;
                case '-n':
                case '--name':
                    j++;
                    name = process.argv[j];
                    break;
             }
         }
         i = process.argv.length;
  }
}

if (serviceID === undefined || secret === undefined || nuveUrl === undefined) {
    exitApp();
}

if (action == undefined) {
        console.log('No action');
        process.exit(1);
}


var getKeys = function(obj){
   var keys = [];
   for(var key in obj){
      keys.push(key);
   }
   return keys;
}

var makeHeaders = function(structure) {
    var line1 = '+';
    var line2 = '|';
    for(var header in structure){
        var size = structure[header].length;
        line1 += Array(size).join('-') + '+';
        line2 += header + Array(size-header.length).join(' ') + '|';
    }
    var rtnHeader = line1 + '\n' + line2 + '\n' + line1 ;
    return rtnHeader; 
}

var makeRow = function(structure, data) {
    var line1 = '+';
    var line2 = '|';
    for(var header in structure){
        var field = structure[header].field;
        var size = structure[header].length;
        var cell = (data[field] == undefined) ? ' ' : data[field] + '';
        line1 += Array(size).join('-') + '+';
        line2 += cell + Array(size-cell.length).join(' ') + '|';
    }
    var rtnHeader = line2;
    return rtnHeader;
}

var makeTable = function(structure, data) {
    text = makeHeaders(structure) + '\n';
    for (var index in data) {
        var input = data[index];
        text += makeRow(structure,input) + '\n';    
    }
    return text;
}

String.prototype.splice = function( idx, s ) {
    return (this.slice(0,idx) + s + this.slice(idx + Math.abs(s.length)));
};


N.API.init(nuveUrl, serviceID, secret);

switch(action) {
    case 'room-list':

        var onroomlist = function(results) {
            results = JSON.parse(results);
            var structure = {
                "ID": {length:38, field:"_id"},
                "NAME": {length:38, field:"name"}
            };

            var table = makeTable(structure, results);
            console.log(table);
        };

        N.API.getRooms(onroomlist);
        break;
    case 'room-info':
        if (id === undefined) {
            console.log("No room id");
            process.exit(1);
        }
        var onroom = function(results) {
            results = JSON.parse(results);
            results = [results];
            var structure = {
                "ID": {length:38, field:"_id"},
                "NAME": {length:38, field:"name"}
            };

            var table = makeTable(structure, results);
            console.log(table);
        };

        N.API.getRoom(id, onroom);
        break;
    case 'room-add':
        if (name === undefined) {
            console.log("No room name");
            process.exit(1);
        }
        var oncreated = function(results) {
            console.log("Created. ", results);
        };

        N.API.createRoom(name, oncreated);
        break;
    case 'room-delete':
        if (id === undefined) {
            console.log("No room id");
            process.exit(1);
        }
        var ondeleted = function(results) {
            console.log("Deleted. ", results);
        };

        N.API.deleteRoom(id, ondeleted);
        break;
}