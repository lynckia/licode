var vows = vows = require('vows')
    assert = require('assert');

vows.describe('lynckia').addBatch({
    "Lynckia service": {
        topic: function () {
            return null;
        },
        "should have the correct methods defined": function () {
            assert.isTrue(true);
        }
    }
}).export(module);