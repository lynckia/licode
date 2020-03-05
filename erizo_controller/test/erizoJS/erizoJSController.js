/* global require, describe, it, beforeEach, afterEach */

/* eslint-disable no-unused-expressions */

// eslint-disable-next-line no-unused-expressions
const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;

describe('Erizo JS Controller', () => {
  let amqperMock;
  let licodeConfigMock;
  let erizoApiMock;
  let controller;
  const kActiveUptimeLimit = 1; // in days
  const kMaxTimeSinceLastOperation = 1; // in hours
  const kCheckUptimeInterval = 1; // in seconds
  const kArbitraryErizoJSId = 'erizoJSId1';

  beforeEach('Mock Process', () => {
    this.originalExit = process.exit;
    process.exit = sinon.stub();
  });

  afterEach('Unmock Process', () => {
    process.exit = this.originalExit;
  });


  beforeEach(() => {
    global.config = { logger: { configFile: true },
      erizo: {
        activeUptimeLimit: kActiveUptimeLimit,
        maxTimeSinceLastOperation: kMaxTimeSinceLastOperation,
        checkUptimeInterval: kCheckUptimeInterval,
      },
    };
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    erizoApiMock = mocks.start(mocks.erizoAPI);
    // eslint-disable-next-line global-require
    controller = require('../../erizoJS/erizoJSController').ErizoJSController(kArbitraryErizoJSId);
  });

  afterEach(() => {
    mocks.stop(erizoApiMock);
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {};
  });

  it('should provide the known API', () => {
    expect(controller.addExternalInput).not.to.be.undefined;
    expect(controller.addExternalOutput).not.to.be.undefined;
    expect(controller.removeExternalOutput).not.to.be.undefined;
    expect(controller.processConnectionMessage).not.to.be.undefined;
    expect(controller.processStreamMessage).not.to.be.undefined;
    expect(controller.addPublisher).not.to.be.undefined;
    expect(controller.addSubscriber).not.to.be.undefined;
    expect(controller.removePublisher).not.to.be.undefined;
    expect(controller.removeSubscriber).not.to.be.undefined;
    expect(controller.removeSubscriptions).not.to.be.undefined;
  });

  describe('Uptime cleanup', () => {
    let clock;
    const kActiveUptimeLimitMs = kActiveUptimeLimit * 3600 * 24 * 1000;
    const kMaxTimeSinceLastOperationMs = kMaxTimeSinceLastOperation * 3600 * 1000;
    const kCheckUptimeIntervalMs = kCheckUptimeInterval * 1000;
    let callback;
    const kArbitraryStreamId = 'pubStreamId1';
    const kArbitraryClientId = 'pubClientid1';
    const kArbitraryErizoControllerId = 'erizoControllerId1';

    beforeEach(() => {
      callback = sinon.stub();
      clock = sinon.useFakeTimers();
      global.config.erizo = {
        activeUptimeLimit: kActiveUptimeLimit,
        maxTimeSinceLastOperation: kMaxTimeSinceLastOperation,
        checkUptimeInterval: kCheckUptimeInterval,
      };
      global.config.erizoController = { report: {
        connection_events: true,
        rtcp_stats: true } };
    });

    afterEach(() => {
      clock.restore();
    });

    it('should not stop inactive processes', () => {
      clock.tick(kActiveUptimeLimitMs + kCheckUptimeIntervalMs);
      expect(process.exit.callCount).to.equal(0);
    });
    it('should exit when the max active time is up and there is no new activity', () => {
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      clock.tick(kActiveUptimeLimitMs + kCheckUptimeIntervalMs);
      expect(process.exit.callCount).to.equal(1);
    });
    it('should not exit when the max active time is up but there is new activity', () => {
      const kArbitraryTimeMargin = kMaxTimeSinceLastOperationMs - 1000;
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      clock.tick(kActiveUptimeLimitMs - kArbitraryTimeMargin);
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      clock.tick(kArbitraryTimeMargin + kCheckUptimeIntervalMs);
      expect(process.exit.callCount).to.equal(0);
    });
  });

  describe('Add External Input', () => {
    let callback;
    const kArbitraryErizoControllerId = 'erizoControllerId1';
    const kArbitraryStreamId = 'streamId1';
    const kArbitraryUrl = 'url1';
    const kArbitraryLabel = 'label1';

    beforeEach(() => {
      callback = sinon.stub();
    });

    it('should succeed creating OneToManyProcessor and ExternalInput', () => {
      mocks.ExternalInput.init.returns(1);
      controller.addExternalInput(kArbitraryErizoControllerId, kArbitraryStreamId,
        kArbitraryUrl, kArbitraryLabel, callback);

      expect(erizoApiMock.OneToManyProcessor.callCount).to.equal(1);
      expect(erizoApiMock.ExternalInput.args[0][0]).to.equal(kArbitraryUrl);
      expect(erizoApiMock.ExternalInput.callCount).to.equal(1);
      expect(mocks.ExternalInput.setAudioReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.ExternalInput.setVideoReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.OneToManyProcessor.setExternalPublisher.args[0][0]).to
        .equal(mocks.ExternalInput);
      expect(callback.callCount).to.equal(1);
      expect(callback.args[0]).to.deep.equal(['callback', 'success']);
    });

    it('should fail if ExternalInput is not intialized', () => {
      mocks.ExternalInput.init.returns(-1);
      controller.addExternalInput(kArbitraryErizoControllerId, kArbitraryStreamId,
        kArbitraryUrl, kArbitraryLabel, callback);

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0]).to.deep.equal(['callback', -1]);
    });

    it('should fail if it already exists', () => {
      const secondCallback = sinon.stub();
      controller.addExternalInput(kArbitraryErizoControllerId, kArbitraryStreamId,
        kArbitraryUrl, kArbitraryLabel, callback);
      controller.addExternalInput(kArbitraryErizoControllerId, kArbitraryStreamId,
        kArbitraryUrl, kArbitraryLabel, callback);

      expect(callback.callCount).to.equal(1);
      expect(secondCallback.callCount).to.equal(0);
    });
  });

  describe('Add External Output', () => {
    const kArbitraryEoUrl = 'eo_url1';
    const kArbitraryEoOptions = {};
    const kArbitraryEiId = 'ei_id1';
    const kArbitraryEiUrl = 'ei_url1';
    const kArbitraryEiLabel = 'ei_label1';
    const kArbitraryErizoControllerId = 'erizoControllerId1';

    beforeEach(() => {
      const eiCallback = () => {};
      controller.addExternalInput(kArbitraryErizoControllerId, kArbitraryEiId,
        kArbitraryEiUrl, kArbitraryEiLabel, eiCallback);
    });

    it('should succeed creating ExternalOutput', () => {
      controller.addExternalOutput(kArbitraryEiId, kArbitraryEoUrl, kArbitraryEoOptions);
      expect(erizoApiMock.ExternalOutput.args[0][1]).to.equal(kArbitraryEoUrl);
      expect(erizoApiMock.ExternalOutput.callCount).to.equal(1);
      expect(mocks.ExternalOutput.id).to.equal(`${kArbitraryEoUrl}_${kArbitraryEiId}`);
      expect(mocks.OneToManyProcessor.addExternalOutput.args[0]).to.deep
        .equal([mocks.ExternalOutput, kArbitraryEoUrl]);
    });

    it('should fail if Publisher does not exist', () => {
      controller.addExternalOutput(`${kArbitraryEiId}a`, kArbitraryEiUrl, kArbitraryEoOptions);

      expect(erizoApiMock.ExternalOutput.callCount).to.equal(0);
    });

    describe('Remove External Output', () => {
      beforeEach(() => {
        controller.addExternalOutput(kArbitraryEiId, kArbitraryEoUrl, kArbitraryEoOptions);
      });

      it('should succeed removing ExternalOutput', () => {
        controller.removeExternalOutput(kArbitraryEiId, kArbitraryEoUrl);
        expect(mocks.OneToManyProcessor.removeSubscriber.args[0][0]).to
          .equal(kArbitraryEoUrl);
      });

      it('should fail if Publisher does not exist', () => {
        controller.removeExternalOutput(`${kArbitraryEiId}a`, kArbitraryEiUrl);

        expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to
          .equal(0);
      });
    });
  });

  describe('Add Publisher', () => {
    let callback;
    const kArbitraryStreamId = 'pubStreamId1';
    const kArbitraryClientId = 'pubClientid1';
    const kArbitraryErizoControllerId = 'erizoControllerId1';

    beforeEach(() => {
      callback = sinon.stub();
      mocks.WebRtcConnection.setRemoteDescription.returns(Promise.resolve());
      global.config.erizo = {};
      global.config.erizoController = { report: {
        connection_events: true,
        rtcp_stats: true } };
    });

    it('should succeed creating OneToManyProcessor and WebRtcConnection', (done) => {
      mocks.WebRtcConnection.init.returns(1);
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      setTimeout(() => {
        expect(erizoApiMock.OneToManyProcessor.callCount).to.equal(1);
        expect(erizoApiMock.WebRtcConnection.args[0][2]).to.contain(kArbitraryClientId);
        expect(erizoApiMock.WebRtcConnection.callCount).to.equal(1);
        expect(mocks.MediaStream.setAudioReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
        expect(mocks.MediaStream.setVideoReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
        expect(mocks.OneToManyProcessor.setPublisher.args[0][0]).to
          .equal(mocks.MediaStream);
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0]).to.deep.equal(['callback',
          { type: 'initializing', connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        done();
      }, 0);
    });

    it('should fail if Publishers exists with no Subscriber', (done) => {
      mocks.WebRtcConnection.init.returns(1);
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        done();
      }, 0);
    });

    it('should fail if it already exists and it has subscribers', (done) => {
      const kArbitrarySubClientId = 'id2';
      const secondCallback = sinon.stub();
      const subCallback = sinon.stub();
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      controller.addSubscriber(kArbitraryErizoControllerId, kArbitrarySubClientId,
        kArbitraryClientId, {}, subCallback);

      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, secondCallback);
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        expect(secondCallback.callCount).to.equal(0);
        done();
      }, 0);
    });

    it('should succeed sending offer event', (done) => {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, { createOffer: { audio: true, video: true, bundle: true } }, callback);
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0]).to.deep.equal(['callback',
          { type: 'initializing', connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        expect(amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent').callCount).to.equal(1);
        const call = amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent');
        expect(call.args[0][2][3].type).to.equal('offer');
        done();
      }, 0);
    });

    it('should succeed sending offer event from SDP in Tricke ICE', (done) => {
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId, kArbitraryStreamId,
        { trickleIce: true, createOffer: { audio: true, video: true, bundle: true } }, callback);
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0]).to.deep.equal(['callback',
          { type: 'initializing', connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        expect(amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent').callCount).to.equal(1);
        const call = amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent');
        expect(call.args[0][2][3].type).to.equal('offer');
        done();
      }, 0);
    });

    it('should succeed sending answer event', (done) => {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, ''); // CONN_GATHERED
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitraryClientId,
        `${kArbitraryClientId}_${kArbitraryErizoJSId}_1`, { type: 'offer', sdp: '' });
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0]).to.deep.equal(['callback',
          { type: 'initializing', connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        expect(amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent').callCount).to.equal(1);
        const call = amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent');
        expect(call.args[0][2][3].type).to.equal('answer');
        done();
      }, 0);
    });

    it('should succeed sending answer event from SDP in Tricke ICE', (done) => {
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, { trickleIce: true }, callback);
      controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitraryClientId,
        `${kArbitraryClientId}_${kArbitraryErizoJSId}_1`, { type: 'offer', sdp: '' });
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0]).to.deep.equal(['callback', { type: 'initializing',
          connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        expect(amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent').callCount).to.equal(1);
        const call = amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent');
        expect(call.args[0][2][3].type).to.equal('answer');
        done();
      }, 0);
    });

    it('should succeed sending candidate event', (done) => {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 201, ''); // CONN_CANDIDATE
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0]).to.deep.equal(['callback',
          { type: 'initializing', connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        expect(amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent').callCount).to.equal(1);
        const call = amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent');
        expect(call.args[0][2][3].type).to.equal('candidate');
        done();
      }, 0);
    });

    it('should succeed sending failed event', (done) => {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 500, ''); // CONN_FAILED
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      setTimeout(() => {
        expect(callback.callCount).to.equal(1);
        expect(callback.args[0]).to.deep.equal(['callback',
          { type: 'initializing', connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        expect(amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent').callCount).to.equal(1);
        const call = amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
          'connectionStatusEvent');
        expect(call.args[0][2][3].type).to.equal('failed');
        done();
      }, 0);
    });

    it('should succeed sending ready event', () => {
      const stub = mocks.WebRtcConnection.init;
      stub.returns(1);
      let initCallback;

      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);

      return Promise.resolve().then(() => {
        initCallback = stub.getCall(0).args[0];
        initCallback(103, ''); // CONN_GATHERED
        initCallback(104, ''); // CONN_READY
      }).then(() =>
        controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitraryClientId,
          `${kArbitraryClientId}_${kArbitraryErizoJSId}_1`, { type: 'offer', sdp: '' }))
        .then(() => {
          expect(callback.callCount).to.equal(2);
          expect(callback.args[0]).to.deep.equal(['callback',
            { type: 'initializing', connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
          expect(amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
            'connectionStatusEvent').callCount).to.equal(2);
          const call = amqperMock.callRpc.withArgs(`erizoController_${kArbitraryErizoControllerId}`,
            'connectionStatusEvent');
          expect(call.args[0][2][3].type).to.equal('ready');
          expect(call.args[1][2][3].type).to.equal('answer');
        });
    });

    it('should succeed sending started event', () => {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 101); // CONN_INITIAL
      mocks.WebRtcConnection.addMediaStream.returns(Promise.resolve());
      controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
        kArbitraryStreamId, {}, callback);
      return Promise.resolve().then(() => {}).then(() => {
        expect(callback.callCount).to.equal(2);
        expect(callback.args[1]).to.deep.equal(['callback', { type: 'initializing',
          connectionId: `${kArbitraryClientId}_${kArbitraryErizoJSId}_1` }]);
        expect(callback.args[0]).to.deep.equal(['callback', { type: 'started' }]);
      });
    });

    describe('Process Signaling Message', () => {
      beforeEach((done) => {
        mocks.WebRtcConnection.init.onFirstCall().returns(1);
        controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
          kArbitraryStreamId, {}, callback);
        setTimeout(() => {
          done();
        }, 0);
      });

      it('should set remote sdp when received', () => {
        controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitraryClientId,
          `${kArbitraryClientId}_${kArbitraryErizoJSId}_1`,
          { type: 'offer', sdp: '', config: {} });

        expect(mocks.WebRtcConnection.setRemoteDescription.callCount).to.equal(1);
      });

      it('should set candidate when received', () => {
        controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitraryClientId,
          `${kArbitraryClientId}_${kArbitraryErizoJSId}_1`, {
            type: 'candidate',
            candidate: { msid: 1, candidate: 'a=candidate:0 1 UDP 2122194687 192.0.2.4 61665 typ host' } });

        expect(mocks.WebRtcConnection.addRemoteCandidate.callCount).to.equal(1);
      });

      it('should update sdp', () => {
        controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitraryClientId,
          `${kArbitraryClientId}_${kArbitraryErizoJSId}_1`, {
            type: 'updatestream',
            sdp: 'sdp' });

        expect(mocks.WebRtcConnection.setRemoteDescription.callCount).to.equal(1);
      });
    });

    describe('Add Subscriber', () => {
      let subCallback;
      const kArbitrarySubClientId = 'subClientId1';
      beforeEach(() => {
        subCallback = sinon.stub();
        mocks.WebRtcConnection.init.onFirstCall().returns(1);
        controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
          kArbitraryStreamId, {}, callback);
      });

      it('should succeed creating WebRtcConnection and adding sub to muxer', () => {
        mocks.WebRtcConnection.init.returns(1);

        controller.addSubscriber(kArbitraryErizoControllerId, kArbitrarySubClientId,
          kArbitraryStreamId, {}, subCallback);
        return Promise.resolve().then(() => {}).then(() => {
          expect(erizoApiMock.WebRtcConnection.callCount).to.equal(2);
          expect(erizoApiMock.WebRtcConnection.args[1][2]).to.contain(kArbitrarySubClientId);
          expect(mocks.OneToManyProcessor.addSubscriber.callCount).to.equal(1);
          expect(subCallback.callCount).to.equal(1);
        });
      });

      it('should fail when we subscribe to an unknown publisher', () => {
        const kArbitraryUnknownId = 'unknownId';
        mocks.WebRtcConnection.init.returns(1);

        controller.addSubscriber(kArbitraryErizoControllerId, kArbitrarySubClientId,
          kArbitraryUnknownId, {}, subCallback);
        return Promise.resolve().then(() => {
          expect(erizoApiMock.WebRtcConnection.callCount).to.equal(1);
          expect(mocks.OneToManyProcessor.addSubscriber.callCount).to.equal(0);
          expect(subCallback.callCount).to.equal(0);
        });
      });

      it('should set Slide Show Mode', () => {
        mocks.WebRtcConnection.init.onSecondCall().returns(1).callsArgWith(0, 104, '');
        controller.addSubscriber(kArbitraryErizoControllerId, kArbitrarySubClientId,
          kArbitraryStreamId, { slideShowMode: true }, subCallback);
        let initCallback;
        return Promise.resolve().then(() => {
          initCallback = mocks.WebRtcConnection.init.getCall(1).args[0];
          initCallback(103, ''); // CONN_GATHERED
        }).then(() => {
          initCallback(104, ''); // CONN_READY
        }).then(() => {
          expect(subCallback.callCount).to.equal(2);
          expect(subCallback.args[0]).to.deep.equal(['callback', { type: 'ready' }]);
          expect(subCallback.args[1]).to.deep.equal(['callback', { type: 'initializing',
            connectionId: `${kArbitrarySubClientId}_${kArbitraryErizoJSId}_1` }]);
          expect(mocks.MediaStream.setSlideShowMode.callCount).to.equal(1);
        });
      });

      describe('Process Signaling Message', () => {
        beforeEach(() => {
          mocks.WebRtcConnection.init.onSecondCall().returns(1);
          controller.addSubscriber(kArbitraryErizoControllerId, kArbitrarySubClientId,
            kArbitraryStreamId, {}, subCallback);
        });

        it('should set remote sdp when received', () => {
          controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitrarySubClientId,
            `${kArbitrarySubClientId}_${kArbitraryErizoJSId}_1`, {
              type: 'offer',
              sdp: '',
              config: {} });

          expect(mocks.WebRtcConnection.setRemoteDescription.callCount).to.equal(1);
        });

        it('should set candidate when received', () => {
          controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitrarySubClientId,
            `${kArbitrarySubClientId}_${kArbitraryErizoJSId}_1`, {
              type: 'candidate',
              candidate: { msid: 1, candidate: 'a=candidate:0 1 UDP 2122194687 192.0.2.4 61665 typ host' } });

          expect(mocks.WebRtcConnection.addRemoteCandidate.callCount).to.equal(1);
        });

        it('should update sdp', () => {
          controller.processConnectionMessage(kArbitraryErizoControllerId, kArbitrarySubClientId,
            `${kArbitrarySubClientId}_${kArbitraryErizoJSId}_1`, {
              type: 'updatestream',
              sdp: 'aaa' });

          expect(mocks.WebRtcConnection.setRemoteDescription.callCount).to.equal(1);
        });

        it('should mute and unmute subscriber stream', () => {
          controller.processStreamMessage(kArbitraryErizoControllerId, kArbitrarySubClientId,
            kArbitraryStreamId, {
              type: 'updatestream',
              config: {
                muteStream: {
                  audio: true,
                  video: false,
                },
              } });

          controller.processStreamMessage(kArbitraryErizoControllerId, kArbitrarySubClientId,
            kArbitraryStreamId, {
              type: 'updatestream',
              config: {
                muteStream: {
                  audio: false,
                  video: false,
                },
              } });

          expect(mocks.MediaStream.muteStream.callCount).to.equal(3);
          expect(mocks.MediaStream.muteStream.args[1]).to.deep.equal([false, true]);
          expect(mocks.MediaStream.muteStream.args[2]).to.deep.equal([false, false]);
        });

        it('should mute and unmute publisher stream', () => {
          controller.processStreamMessage(kArbitraryErizoControllerId, kArbitraryClientId,
            kArbitraryStreamId, {
              type: 'updatestream',
              config: {
                muteStream: {
                  audio: true,
                  video: false,
                },
              } });

          controller.processStreamMessage(kArbitraryErizoControllerId, kArbitraryClientId,
            kArbitraryStreamId, {
              type: 'updatestream',
              config: {
                muteStream: {
                  audio: false,
                  video: false,
                },
              } });

          expect(mocks.MediaStream.muteStream.callCount).to.equal(3);
          expect(mocks.MediaStream.muteStream.args[1]).to.deep.equal([false, true]);
          expect(mocks.MediaStream.muteStream.args[2]).to.deep.equal([false, false]);
        });

        it('should set slide show mode to true', () => {
          controller.processStreamMessage(kArbitraryErizoControllerId, kArbitrarySubClientId,
            kArbitraryStreamId, {
              type: 'updatestream',
              config: {
                slideShowMode: true,
              } });

          expect(mocks.MediaStream.setSlideShowMode.callCount).to.equal(1);
          expect(mocks.MediaStream.setSlideShowMode.args[0][0]).to.be.true;
        });

        it('should set slide show mode to false', () => {
          controller.processStreamMessage(kArbitraryErizoControllerId, kArbitrarySubClientId,
            kArbitraryStreamId, {
              type: 'updatestream',
              config: {
                slideShowMode: false,
              } });

          expect(mocks.MediaStream.setSlideShowMode.args[0][0]).to.be.false;
        });
      });

      describe('Remove Subscriber', () => {
        beforeEach(() => {
          mocks.WebRtcConnection.init.onSecondCall().returns(1);
          controller.addSubscriber(kArbitraryErizoControllerId, kArbitrarySubClientId,
            kArbitraryStreamId, {}, subCallback);
        });
        it('should succeed removing the mediaStream', () => {
          const testPromise = controller.removeSubscriber(kArbitrarySubClientId, kArbitraryStreamId)
            .then(() => {
              expect(mocks.WebRtcConnection.removeMediaStream.callCount).to.equal(1);
              expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(1);
            });
          return testPromise;
        });

        it('should fail removing unknown Subscriber', () => {
          const kArbitraryUnknownId = 'unknownId';

          controller.removeSubscriber(kArbitraryUnknownId, kArbitraryStreamId);

          expect(mocks.WebRtcConnection.close.callCount).to.equal(0);
          expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(0);
        });

        it('should fail removing Subscriber from unknown Publisher', () => {
          const kArbitraryUnknownId = 'unknownId';

          controller.removeSubscriber(kArbitrarySubClientId, kArbitraryUnknownId);

          expect(mocks.WebRtcConnection.close.callCount).to.equal(0);
          expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(0);
        });

        it('should remove subscriber only with its id', () => {
          controller.removeSubscriptions(kArbitrarySubClientId).then(() => {
            expect(mocks.WebRtcConnection.close.callCount).to.equal(1);
            expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(1);
          });
        });
      });
    });

    describe('Remove Publisher', () => {
      beforeEach(() => {
        mocks.OneToManyProcessor.close.callsArg(0);
        mocks.WebRtcConnection.init.onFirstCall().returns(1);
        controller.addPublisher(kArbitraryErizoControllerId, kArbitraryClientId,
          kArbitraryStreamId, {}, callback);
      });

      it('should succeed closing WebRtcConnection and OneToManyProcessor', () => {
        controller.removePublisher(kArbitraryClientId, kArbitraryStreamId).then(() => {
          expect(mocks.WebRtcConnection.close.callCount).to.equal(1);
          expect(mocks.OneToManyProcessor.close.callCount).to.equal(1);
        });
      });

      it('should fail closing an unknown Publisher', () => {
        const kArbitraryUnknownId = 'unknownId';

        controller.removePublisher(kArbitraryUnknownId, kArbitraryUnknownId).then(() => {
          expect(mocks.WebRtcConnection.close.callCount).to.equal(0);
          expect(mocks.OneToManyProcessor.close.callCount).to.equal(0);
        });
      });

      it('should succeed closing also Subscribers', () => {
        mocks.WebRtcConnection.init.returns(1);
        const kArbitrarySubClientId = 'subClientId2';
        const subCallback = sinon.stub();
        controller.addSubscriber(kArbitrarySubClientId, kArbitraryStreamId, {}, subCallback);

        controller.removePublisher(kArbitraryClientId, kArbitraryStreamId).then(() => {
          expect(mocks.WebRtcConnection.close.callCount).to.equal(1);
          expect(mocks.OneToManyProcessor.close.callCount).to.equal(1);
        });
      });
    });
  });
});
