/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;
const ErizoList = require('../../erizoAgent/erizoList').ErizoList;

describe('Erizo List', () => {
  let erizoList;

  beforeEach(() => {
  });

  afterEach(() => {
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {};
  });

  const pit = (prerun, max) => {
    it(`should not exceed prerun values, prerun: ${prerun}, max: ${max}`, () => {
      erizoList = new ErizoList(prerun, max);
      erizoList.fill();
      expect(erizoList.idle.length).to.equal(Math.min(prerun, max));
      expect(erizoList.running.length).to.equal(Math.min(prerun, max));
    });

    it(`should not exceed prerun or max values, prerun: ${prerun}, max: ${max}`, () => {
      erizoList = new ErizoList(prerun, max);
      erizoList.fill();
      expect(erizoList.idle.length).to.equal(Math.min(prerun, max));
      expect(erizoList.running.length).to.equal(Math.min(prerun, max));
    });

    it(`should emit event when launching erizo, prerun: ${prerun}, max: ${max}`, () => {
      const callback = sinon.stub();
      erizoList = new ErizoList(prerun, max);
      erizoList.on('launch-erizo', callback);
      erizoList.fill();
      expect(callback.callCount).to.equal(Math.min(prerun, max));
    });

    it(`should not exceed max erizos, prerun: ${prerun}, max: ${max}`, () => {
      erizoList = new ErizoList(prerun, max);
      erizoList.fill();
      for (let index = 0; index < max; index += 1) {
        erizoList.getErizo();
      }

      expect(erizoList.idle.length).to.equal(0);
      expect(erizoList.running.length).to.equal(max);
    });

    it(`should assign erizos in round robin, prerun: ${prerun}, max: ${max}`, () => {
      erizoList = new ErizoList(prerun, max);
      erizoList.fill();
      for (let index = 0; index < max; index += 1) {
        const erizo = erizoList.getErizo();
        expect(erizo.position).to.equal(index);
      }

      expect(erizoList.idle.length).to.equal(0);
      expect(erizoList.running.length).to.equal(max);
    });

    it(`should reassign erizos in round robin, prerun: ${prerun}, max: ${max}`, () => {
      erizoList = new ErizoList(prerun, max);
      erizoList.fill();
      const erizoIds = [];
      for (let index = 0; index < max; index += 1) {
        erizoIds.push(erizoList.getErizo().id);
      }

      for (let index = 0; index < max * 10; index += 1) {
        expect(erizoList.getErizo().id).to.equal(erizoIds[index % max]);
      }
    });

    it(`should delete erizos, prerun: ${prerun}, max: ${max}`, () => {
      erizoList = new ErizoList(prerun, max);
      erizoList.fill();
      const erizoIds = [];
      for (let index = 0; index < max; index += 1) {
        erizoIds.push(erizoList.getErizo().id);
      }

      erizoList.delete(erizoIds[0]);

      expect(erizoList.idle.length).to.equal(0);
      expect(erizoList.running.length).to.equal(max - 1);
    });

    it(`should relaunch new erizos, prerun: ${prerun}, max: ${max}`, () => {
      erizoList = new ErizoList(prerun, max);
      erizoList.fill();
      const erizoIds = [];
      for (let index = 0; index < max; index += 1) {
        erizoIds.push(erizoList.getErizo().id);
      }

      const oldErizoId = erizoIds[0];
      erizoList.delete(oldErizoId);

      const callback = sinon.stub();
      erizoList.on('launch-erizo', callback);
      const newErizoId = erizoList.getErizo().id;

      expect(erizoList.idle.length).to.equal(0);
      expect(erizoList.running.length).to.equal(max);
      expect(callback.callCount).to.equal(1);
      expect(oldErizoId).to.not.equal(newErizoId);
    });
  };

  pit(4, 2);
  pit(20, 40);
  pit(0, 4);
  pit(4, 4);
  pit(40, 40);
  pit(1, 1);
  pit(2, 4);
});
