/* global require, describe, it, beforeEach, afterEach */


const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const request = require('supertest');
// eslint-disable-next-line import/no-extraneous-dependencies
const express = require('express');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;
// eslint-disable-next-line import/no-extraneous-dependencies
const async = require('async');

const kArbitraryServiceInfo = { key: '1' };

describe('Nuve Authenticator', () => {
  let app;
  let nextStep;
  let serviceRegistryMock;
  let nuveAuthenticator;

  beforeEach(() => {
    mocks.start(mocks.licodeConfig);
    // mauthParserMock = mocks.start(mocks.mauthParser);
    serviceRegistryMock = mocks.start(mocks.serviceRegistry);

    // eslint-disable-next-line global-require
    nuveAuthenticator = require('../../auth/nuveAuthenticator');

    const onRequest = (req, res) => {
      res.send('OK');
    };
    nextStep = sinon.spy(onRequest);

    app = express();
    app.all('*', nuveAuthenticator.authenticate);
    app.get('/arbitraryFunction', nextStep);

    serviceRegistryMock.getService = sinon.stub().callsArgWith(1, kArbitraryServiceInfo);
  });

  afterEach(() => {
    mocks.stop(mocks.licodeConfig);
    // mocks.stop(mauthParserMock);
    mocks.stop(serviceRegistryMock);
    mocks.deleteRequireCache();
  });

  it('should fail if authentication header is not present', (done) => {
    request(app)
      .get('/arbitraryFunction')
      .expect(401, { 'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"' })
      .end((err) => {
        if (err) throw err;
        done();
      });
  });

  it('should fail if service is not present', (done) => {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization', 'MAuth realm="",mauth_serviceid=1')
      .expect(401, { 'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"' })
      .end((err) => {
        if (err) throw err;
        done();
      });
  });

  it('should fail if signature_method is not present', (done) => {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization', 'MAuth realm="",mauth_serviceid=1')
      .expect(401, { 'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"' })
      .end((err) => {
        if (err) throw err;
        done();
      });
  });

  it('should fail if signature is wrong', (done) => {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization',
        'MAuth realm="",mauth_serviceid=1,' +
            'mauth_timestamp=1,mauth_signature_method=HMAC_SHA1,' +
            'mauth_signature=MzgwOTY5YWY2ZGNkYWM1YTMwM2M0MGM3ZTZmODhkYmEyOTEzNmVkZQ==')
      .expect(401, { 'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"' })
      .end((err) => {
        if (err) throw err;
        done();
      });
  });

  it('should succeed if signature is valid', (done) => {
    request(app)
      .get('/arbitraryFunction')
      .set('Authorization',
        'MAuth realm="",mauth_serviceid=1,' +
            'mauth_timestamp=1,mauth_signature_method=HMAC_SHA1,' +
            'mauth_signature=YmI1OTZlYWM1YzNjZjZhZTRmMTIyODQ1YTNiNWVhNWM0MzAxMmM5Yw==')
      .end((err) => {
        if (err) throw err;
        // eslint-disable-next-line
        expect(nextStep.called).to.be.true;
        done();
      });
  });

  it('should fail if same timestamp and cnonce are sent twice', (done) => {
    const header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=1,mauth_cnonce=1,` +
                        'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=1,mauth_cnonce=1,` +
                        'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .expect(401, { 'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"' })
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
    ], () => {
      // eslint-disable-next-line
      expect(nextStep.calledOnce).to.be.true;
      done();
    });
  });

  it('should fail if we send old timestamp', (done) => {
    const header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=2,mauth_cnonce=1,` +
                      'mauth_signature=OGM3NDQ1MTliNDI1OTUyMmQyY2YwYjQ1ZjJhNzdhYzBiNjBkNDM4Mg==')
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=1,mauth_cnonce=1,` +
                      'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .expect(401, { 'WWW-Authenticate': 'MAuth realm="http://marte3.dit.upm.es"' })
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
    ], () => {
      // eslint-disable-next-line
      expect(nextStep.calledOnce).to.be.true;
      done();
    });
  });

  it('should succeed if timestamp changes between messages', (done) => {
    const header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=1,mauth_cnonce=1,` +
                      'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=2,mauth_cnonce=1,` +
                      'mauth_signature=OGM3NDQ1MTliNDI1OTUyMmQyY2YwYjQ1ZjJhNzdhYzBiNjBkNDM4Mg==')
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
    ], () => {
      // eslint-disable-next-line
      expect(nextStep.calledTwice).to.be.true;
      done();
    });
  });

  it('should succeed if cnonce changes between messages', (done) => {
    const header = 'MAuth realm="",mauth_serviceid=1,mauth_signature_method=HMAC_SHA1,';
    async.series([
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=1,mauth_cnonce=1,` +
                      'mauth_signature=NDg2YTMzNjQ4OThhOTA1ZmY5MWM3OTcwZjY2MWVmZDdhN2RhZTI1Mw==')
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
      (next) => {
        request(app)
          .get('/arbitraryFunction')
          .set('Authorization', `${header},mauth_timestamp=2,mauth_cnonce=2,` +
                      'mauth_signature=ZjgwZDkyMTA4MjhhYWY1NmY5MGU0OTZkYTZhZTJhNWE1MWMwZjk1YQ==')
          .end((err) => {
            if (err) throw err;
            next();
          });
      },
    ], () => {
      // eslint-disable-next-line
      expect(nextStep.calledTwice).to.be.true;
      done();
    });
  });
});
