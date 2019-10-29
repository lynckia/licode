/* global require, describe, it, before */

// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('DataBase', () => {
  let db;

  before(() => {
    // eslint-disable-next-line global-require
    db = require('../../mdb/dataBase');
  });

  it('should contain a db', () => {
    expect(db).to.have.property('db');
  });

  it('should contain a superService', () => {
    expect(db).to.have.property('superService');
  });

  it('should contain a nuveKey', () => {
    expect(db).to.have.property('nuveKey');
  });

  it('should contain a testErizoController', () => {
    expect(db).to.have.property('testErizoController');
  });
});
