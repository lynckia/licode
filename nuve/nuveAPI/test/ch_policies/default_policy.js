/* global require, describe, it */


const policy = require('../../ch_policies/default_policy');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

const kArbitraryErizoController1 = 'erizoController1';
const kArbitraryErizoController2 = 'erizoController1';

describe('Default Policy', () => {
  it('should return the first Erizo Controller in the queue', () => {
    const result = policy.getErizoController(
      {}, // room
      [kArbitraryErizoController2, kArbitraryErizoController1]);
    expect(result).to.deep.equal(kArbitraryErizoController2);
  });
});
