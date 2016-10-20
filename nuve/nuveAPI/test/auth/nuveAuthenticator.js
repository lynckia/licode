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
    // mauthParserMock = mocks.start(mocks.mauthParser);
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
    // mocks.stop(mauthParserMock);
    mocks.stop(serviceRegistryMock);
    mocks.deleteRequireCache();
  });

  it('should fail if authentication header is not present', function(done) {
    request(app)
      .get('/arbitraryFunction')
      .expect(401, {'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"'})
      .end(function(err) {
        if (err) throw err;
        done();
      });
  });

  it('should fail if service is not present', function(done) {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization', 'MAuth realm="",mauth_serviceid=1')
      .expect(401, {'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"'})
      .end(function(err) {
        if (err) throw err;
        done();
      });
  });

  it('should fail if signature_method is not present', function(done) {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization', 'MAuth realm="",mauth_serviceid=1')
      .expect(401, {'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"'})
      .end(function(err) {
        if (err) throw err;
        done();
      });
  });

  it('should fail if signature is wrong', function(done) {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization',
            'MAuth realm="",mauth_serviceid=1,'+
            'mauth_timestamp=1,mauth_signature_method=HMAC_SHA1,'+
            'mauth_signature=MzgwOTY5YWY2ZGNkYWM1YTMwM2M0MGM3ZTZmODhkYmEyOTEzNmVkZQ==')
      .expect(401, {'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"'})
      .end(function(err) {
        if (err) throw err;
        done();
      });
  });

  it('should succeed if signature is valid', function(done) {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization',
            'MAuth realm="",mauth_serviceid=1,'+
            'mauth_timestamp=1,mauth_signature_method=HMAC_SHA1,'+
            'mauth_signature=YmI1OTZlYWM1YzNjZjZhZTRmMTIyODQ1YTNiNWVhNWM0MzAxMmM5Yw==')
      .end(function(err) {
        if (err) throw err;
        expect(nextStep.called).to.be.true;  // jshint ignore:line
        done();
      });

  });

  it('should fail if same timestamp and cnonce are sent twice', function(done) {
    var header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      function(next) {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', header + ',mauth_timestamp=1,mauth_cnonce=1,' +
                        'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
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
          .expect(401, {'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"'})
          .end(function(err) {
            if (err) throw err;
            next();
          });
      }
    ], function() {
      expect(nextStep.calledOnce).to.be.true;  // jshint ignore:line
      done();
    });
  });

  it('should fail if we send old timestamp', function(done) {
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
          .expect(401, {'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"'})
          .end(function(err) {
            if (err) throw err;
            next();
          });
      }
    ], function() {
      expect(nextStep.calledOnce).to.be.true;  // jshint ignore:line
      done();
    });
  });

  it('should succeed if timestamp changes between messages', function(done) {
    var header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      function(next) {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', header + ',mauth_timestamp=1,mauth_cnonce=1,' +
                      'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .end(function(err) {
            if (err) throw err;
            next();
          });
      },
      function(next) {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', header + ',mauth_timestamp=2,mauth_cnonce=1,' +
                      'mauth_signature=OGM3NDQ1MTliNDI1OTUyMmQyY2YwYjQ1ZjJhNzdhYzBiNjBkNDM4Mg==')
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

  it('should succeed if cnonce changes between messages', function(done) {
    var header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      function(next) {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', header + ',mauth_timestamp=1,mauth_cnonce=1,' +
                      'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .end(function(err) {
            if (err) throw err;
            next();
          });
      },
      function(next) {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', header + ',mauth_timestamp=2,mauth_cnonce=2,' +
                      'mauth_signature=ZjgwZDkyMTA4MjhhYWY1NmY5MGU0OTZkYTZhZTJhNWE1MWMwZjk1YQ==')
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
