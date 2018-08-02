/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var request = require('supertest');
var express = require('express');
var sinon = require('sinon');
var expect  = require('chai').expect;
var async = require('async');

var kArbitraryServiceInfo = {key: '1'};

describe('Nuve Authenticator', function() {
  var app,
      nextStep,
      serviceRegistryMock,
      nuveAuthenticator;

  beforeEach(function() {
    mocks.start(mocks.licodeConfig);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);

    nuveAuthenticator = require('../../auth/nuveAuthenticator');

    var onRequest = function(req, res) {
      res.send('OK');
    };
    nextStep = sinon.spy(onRequest);

    app = express();
    app.all('*', nuveAuthenticator.authenticate);
    app.get('/arbitraryFunction', nextStep);

    serviceRegistryMock.getService = sinon.stub().callsArgWith(1, kArbitraryServiceInfo);
  });

  afterEach(function() {
    mocks.stop(mocks.licodeConfig);
    mocks.stop(serviceRegistryMock);
    mocks.deleteRequireCache();
  });

  it('should pass if we send old timestamp when timestamp is not checked', function(done) {
    var header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      function(next) {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', header + ',mauth_timestamp=2,mauth_cnonce=1,' +
                      'mauth_signature=OGM3NDQ1MTliNDI1OTUyMmQyY2YwYjQ1ZjJhNzdhYzBiNjBkNDM4Mg==')
          .end(function(err) {
            if (err) throw err;
            next();
          });
      },
      function(next) {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', header + ',mauth_timestamp=1,mauth_cnonce=1,' +
                      'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .expect(200, 'OK')
          .end(function(err) {
            if (err) throw err;
            next();
          });
      }
    ], function() {
      expect(nextStep.calledTwice).to.be.true;  // jshint ignore:line
      done();
    });
  });
});
