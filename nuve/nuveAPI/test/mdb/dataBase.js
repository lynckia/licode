/*global require, describe, it, before*/
'use strict';
var expect  = require('chai').expect;

describe('DataBase', function() {

  var db;

  before(function() {
    db = require('../../mdb/dataBase');
  });

  it('should contain a db', function() {
    expect(db).to.have.property('db');
  });

  it('should contain a superService', function() {
    expect(db).to.have.property('superService');
  });

  it('should contain a nuveKey', function() {
    expect(db).to.have.property('nuveKey');
  });

  it('should contain a testErizoController', function() {
    expect(db).to.have.property('testErizoController');
  });


});
