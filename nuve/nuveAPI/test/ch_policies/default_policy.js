/*global require, describe, it*/
'use strict';
var policy = require('../../ch_policies/default_policy');
var expect  = require('chai').expect;

var kArbitraryErizoController1 = 'erizoController1';
var kArbitraryErizoController2 = 'erizoController1';

describe('Default Policy', function() {

  it('should return the first Erizo Controller in the queue', function() {
    var result = policy.getErizoController(
                        {},  // room
                        {},  // ec_list
                        [kArbitraryErizoController2, kArbitraryErizoController1]);
    expect(result).to.deep.equal(kArbitraryErizoController2);
  });
});
