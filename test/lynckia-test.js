/*global require, module*/
var vows = require('vows'),
    assert = require('assert'),
    N = require('../extras/basic_example/nuve'),
    config = require('../licode_config');

vows.describe('licode').addBatch({
    "Licode service": {
        topic: function () {
            "use strict";
            return null;
        },
        "should have the correct methods defined": function () {
            "use strict";
            assert.isTrue(true);
        }
    }
}).export(module);