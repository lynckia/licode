/*global require, module*/
var vows = require('vows'),
    assert = require('assert'),
    N = require('../extras/basic_example/nuve'),
    config = require('../lynckia_config');

vows.describe('nuve').addBatch({
    "Nuve service": {
        topic: function () {
            "use strict";
            return null;
        },
        "should initialized": function () {
            "use strict";
            N.API.init(config.nuve.superserviceID, config.nuve.superserviceKey, 'http://localhost:3000/');
            N.API.getRooms(function (rooms) {
                assert.isTrue(true);
            });
            
        }
    }
}).export(module);