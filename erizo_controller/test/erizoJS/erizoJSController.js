/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;

describe('Erizo JS Controller', function() {
  var amqperMock,
      licodeConfigMock,
      erizoApiMock,
      controller;

  beforeEach('Mock Process', function() {
    this.originalExit = process.exit;
    process.exit = sinon.stub();
  });

  afterEach('Unmock Process', function() {
    process.exit = this.originalExit;
  });


  beforeEach(function() {
    global.config = {logger: {configFile: true}};
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    erizoApiMock = mocks.start(mocks.erizoAPI);
    controller = require('../../erizoJS/erizoJSController').ErizoJSController();
  });

  afterEach(function() {
    mocks.stop(erizoApiMock);
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {};
  });

  it('should provide the known API', function() {
    expect(controller.addExternalInput).not.to.be.undefined;  // jshint ignore:line
    expect(controller.addExternalOutput).not.to.be.undefined;  // jshint ignore:line
    expect(controller.removeExternalOutput).not.to.be.undefined;  // jshint ignore:line
    expect(controller.processSignaling).not.to.be.undefined;  // jshint ignore:line
    expect(controller.addPublisher).not.to.be.undefined;  // jshint ignore:line
    expect(controller.addSubscriber).not.to.be.undefined;  // jshint ignore:line
    expect(controller.removePublisher).not.to.be.undefined;  // jshint ignore:line
    expect(controller.removeSubscriber).not.to.be.undefined;  // jshint ignore:line
    expect(controller.removeSubscriptions).not.to.be.undefined;  // jshint ignore:line
  });

  describe('Add External Input', function() {
    var callback;
    var kArbitraryId = 'id1';
    var kArbitraryUrl = 'url1';

    beforeEach(function() {
      callback = sinon.stub();
    });

    it('should succeed creating OneToManyProcessor and ExternalInput', function() {
      mocks.ExternalInput.init.returns(1);
      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);

      expect(erizoApiMock.OneToManyProcessor.callCount).to.equal(1);
      expect(erizoApiMock.ExternalInput.args[0][0]).to.equal(kArbitraryUrl);
      expect(erizoApiMock.ExternalInput.callCount).to.equal(1);
      expect(mocks.ExternalInput.wrtcId).to.equal(kArbitraryId + '_' + kArbitraryUrl);
      expect(mocks.ExternalInput.setAudioReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.ExternalInput.setVideoReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.OneToManyProcessor.setExternalPublisher.args[0][0]).to.
                                                      equal(mocks.ExternalInput);
      expect(callback.callCount).to.equal(1);
      expect(callback.args[0]).to.deep.equal(['callback', 'success']);
    });

    it('should fail if ExternalInput is not intialized', function() {
      mocks.ExternalInput.init.returns(-1);
      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);

      expect(callback.callCount).to.equal(1);
      expect(callback.args[0]).to.deep.equal(['callback', -1]);
    });

    it('should fail if it already exists', function() {
      var secondCallback = sinon.stub();
      controller.addExternalInput(kArbitraryId, kArbitraryUrl, callback);
      controller.addExternalInput(kArbitraryId, kArbitraryUrl, secondCallback);

      expect(callback.callCount).to.equal(1);
      expect(secondCallback.callCount).to.equal(0);
    });
  });

  describe('Add External Output', function() {
    var kArbitraryEoUrl = 'eo_url1';
    var kArbitraryEiId = 'ei_id1';
    var kArbitraryEiUrl = 'ei_url1';

    beforeEach(function() {
      var eiCallback = function() {};
      controller.addExternalInput(kArbitraryEiId, kArbitraryEiUrl, eiCallback);
    });

    it('should succeed creating ExternalOutput', function() {
      controller.addExternalOutput(kArbitraryEiId, kArbitraryEoUrl);
      expect(erizoApiMock.ExternalOutput.args[0][0]).to.equal(kArbitraryEoUrl);
      expect(erizoApiMock.ExternalOutput.callCount).to.equal(1);
      expect(mocks.ExternalOutput.wrtcId).to.equal(kArbitraryEoUrl + '_' + kArbitraryEiId);
      expect(mocks.OneToManyProcessor.addExternalOutput.args[0]).to.deep.
                                                    equal([mocks.ExternalOutput, kArbitraryEoUrl]);
    });

    it('should fail if Publisher does not exist', function() {
      controller.addExternalOutput(kArbitraryEiId + 'a', kArbitraryEiUrl);

      expect(erizoApiMock.ExternalOutput.callCount).to.equal(0);
    });

    describe('Remove External Output', function() {

      beforeEach(function() {
        controller.addExternalOutput(kArbitraryEiId, kArbitraryEoUrl);
      });

      it('should succeed removing ExternalOutput', function() {
        controller.removeExternalOutput(kArbitraryEiId, kArbitraryEoUrl);
        expect(mocks.OneToManyProcessor.removeSubscriber.args[0][0]).to.
                                                      equal(kArbitraryEoUrl);
      });

      it('should fail if Publisher does not exist', function() {
        controller.removeExternalOutput(kArbitraryEiId + 'a', kArbitraryEiUrl);

        expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.
                                                      equal(0);

      });
    });
  });

  describe('Add Publisher', function() {
    var callback;
    var kArbitraryId = 'id1';
    var kArbitraryUrl = 'url1';

    beforeEach(function() {
      callback = sinon.stub();
      global.config.erizo = {};
      global.config.erizoController = {report: {
        'connection_events': true,
        'rtcp_stats': true}};
    });

    it('should succeed creating OneToManyProcessor and WebRtcConnection', function() {
      mocks.WebRtcConnection.init.returns(1);
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(erizoApiMock.OneToManyProcessor.callCount).to.equal(1);
      expect(erizoApiMock.WebRtcConnection.args[0][2]).to.equal(kArbitraryId);
      expect(erizoApiMock.WebRtcConnection.callCount).to.equal(1);
      expect(mocks.WebRtcConnection.wrtcId).to.equal(kArbitraryId);
      expect(mocks.WebRtcConnection.setAudioReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.WebRtcConnection.setVideoReceiver.args[0][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.OneToManyProcessor.setPublisher.args[0][0]).to.
                                                      equal(mocks.WebRtcConnection);
      expect(callback.callCount).to.equal(1);
      expect(callback.args[0]).to.deep.equal(['callback', {type: 'initializing'}]);
    });

    it('should succeed if Publishers exists with no Subscriber', function() {
      mocks.WebRtcConnection.init.returns(1);
      controller.addPublisher(kArbitraryId, {}, callback);
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(erizoApiMock.OneToManyProcessor.callCount).to.equal(1);
      expect(erizoApiMock.WebRtcConnection.args[0][2]).to.equal(kArbitraryId);
      expect(erizoApiMock.WebRtcConnection.callCount).to.equal(2);
      expect(mocks.WebRtcConnection.wrtcId).to.equal(kArbitraryId);
      expect(mocks.WebRtcConnection.setAudioReceiver.args[1][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.WebRtcConnection.setVideoReceiver.args[1][0]).to.equal(mocks.OneToManyProcessor);
      expect(mocks.OneToManyProcessor.setPublisher.args[1][0]).to.
                                                      equal(mocks.WebRtcConnection);
      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
    });

    it('should fail if it already exists and it has subscribers', function() {
      var kArbitraryId2 = 'id2';
      var secondCallback = sinon.stub();
      var subCallback = sinon.stub();
      controller.addPublisher(kArbitraryId, {}, callback);
      controller.addSubscriber(kArbitraryId2, kArbitraryId, {}, subCallback);

      controller.addPublisher(kArbitraryId, {}, secondCallback);

      expect(callback.callCount).to.equal(1);
      expect(secondCallback.callCount).to.equal(0);
    });

    it('should succeed sending offer event', function() {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, '');  // CONN_GATHERED
      controller.addPublisher(kArbitraryId, {createOffer:
        {audio: true, video: true, bundle: true}}, callback);

      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
      expect(callback.args[0]).to.deep.equal(['callback', {sdp: '', type: 'offer'}]);
    });

    it('should succeed sending answer event from SDP', function() {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 202, '');  // CONN_SDP
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
      expect(callback.args[0]).to.deep.equal(['callback', {sdp: '', type: 'answer'}]);
    });

    it('should succeed sending answer event', function() {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 103, '');  // CONN_GATHERED
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
      expect(callback.args[0]).to.deep.equal(['callback', {sdp: '', type: 'answer'}]);
    });

    it('should succeed sending candidate event', function() {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 201, '');  // CONN_CANDIDATE
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
      expect(callback.args[0]).to.deep.equal(['callback', {candidate: '', type: 'candidate'}]);
    });

    it('should succeed sending candidate event', function() {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 500, '');  // CONN_FAILED
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
      expect(callback.args[0]).to.deep.equal(['callback', {sdp: '', type: 'failed'}]);
    });

    it('should succeed sending ready event', function() {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 104, '');  // CONN_READY
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
      expect(callback.args[0]).to.deep.equal(['callback', {type: 'ready'}]);
    });

    it('should succeed sending started event', function() {
      mocks.WebRtcConnection.init.returns(1).callsArgWith(0, 101);  // CONN_INITIAL
      controller.addPublisher(kArbitraryId, {}, callback);

      expect(callback.callCount).to.equal(2);
      expect(callback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
      expect(callback.args[0]).to.deep.equal(['callback', {type: 'started'}]);
    });

    describe('Process Signaling Message', function() {
      beforeEach(function() {
        mocks.WebRtcConnection.init.onFirstCall().returns(1);
        controller.addPublisher(kArbitraryId, kArbitraryUrl, callback);
      });

      it('should set remote sdp when received', function() {
        controller.processSignaling(kArbitraryId, undefined, {type: 'offer'});

        expect(mocks.WebRtcConnection.setRemoteSdp.callCount).to.equal(1);
      });

      it('should set candidate when received', function() {
        controller.processSignaling(kArbitraryId, undefined, {
                    type: 'candidate',
                    candidate: {}});

        expect(mocks.WebRtcConnection.addRemoteCandidate.callCount).to.equal(1);
      });

      it('should update sdp', function() {
        controller.processSignaling(kArbitraryId, undefined, {
                    type: 'updatestream',
                    sdp: 'sdp'});

        expect(mocks.WebRtcConnection.setRemoteSdp.callCount).to.equal(1);
      });
    });

    describe('Add Subscriber', function() {
      var subCallback;
      var kArbitraryId2 = 'id2';
      beforeEach(function() {
        subCallback = sinon.stub();
        mocks.WebRtcConnection.init.onFirstCall().returns(1);
        controller.addPublisher(kArbitraryId, kArbitraryUrl, callback);
      });

      it('should succeed creating WebRtcConnection and adding sub to muxer', function() {
        mocks.WebRtcConnection.init.returns(1);

        controller.addSubscriber(kArbitraryId2, kArbitraryId, {}, subCallback);

        expect(erizoApiMock.WebRtcConnection.callCount).to.equal(2);
        expect(erizoApiMock.WebRtcConnection.args[1][2]).to.equal(kArbitraryId2 +
                                                            '_' + kArbitraryId);
        expect(mocks.WebRtcConnection.wrtcId).to.equal(kArbitraryId2 + '_' + kArbitraryId);
        expect(mocks.OneToManyProcessor.addSubscriber.callCount).to.equal(1);
        expect(subCallback.callCount).to.equal(1);
      });

      it('should fail when we subscribe to an unknown publisher', function() {
        var kArbitraryUnknownId = 'unknownId';
        mocks.WebRtcConnection.init.returns(1);

        controller.addSubscriber(kArbitraryId2, kArbitraryUnknownId, {}, subCallback);

        expect(erizoApiMock.WebRtcConnection.callCount).to.equal(1);
        expect(mocks.OneToManyProcessor.addSubscriber.callCount).to.equal(0);
        expect(subCallback.callCount).to.equal(0);
      });

      it('should set Slide Show Mode', function() {
        mocks.WebRtcConnection.init.onSecondCall().returns(1).callsArgWith(0, 104, '');
        controller.addSubscriber(kArbitraryId2, kArbitraryId, {slideShowMode: true}, subCallback);

        expect(subCallback.callCount).to.equal(2);
        expect(subCallback.args[1]).to.deep.equal(['callback', {type: 'initializing'}]);
        expect(subCallback.args[0]).to.deep.equal(['callback', {type: 'ready'}]);
        expect(mocks.WebRtcConnection.setSlideShowMode.callCount).to.equal(1);
      });

      describe('Process Signaling Message', function() {
        beforeEach(function() {
          mocks.WebRtcConnection.init.onSecondCall().returns(1);
          controller.addSubscriber(kArbitraryId2, kArbitraryId, {}, subCallback);
        });

        it('should set remote sdp when received', function() {
          controller.processSignaling(kArbitraryId, kArbitraryId2, {type: 'offer'});

          expect(mocks.WebRtcConnection.setRemoteSdp.callCount).to.equal(1);
        });

        it('should set candidate when received', function() {
          controller.processSignaling(kArbitraryId, kArbitraryId2, {
                      type: 'candidate',
                      candidate: {}});

          expect(mocks.WebRtcConnection.addRemoteCandidate.callCount).to.equal(1);
        });

        it('should update sdp', function() {
          controller.processSignaling(kArbitraryId, kArbitraryId2, {
                      type: 'updatestream',
                      sdp: 'sdp'});

          expect(mocks.WebRtcConnection.setRemoteSdp.callCount).to.equal(1);
        });

        it('should mute and unmute subscriber stream', function() {
          controller.processSignaling(kArbitraryId, kArbitraryId2, {
                      type: 'updatestream',
                      config: {
                        muteStream: {
                          audio: true,
                          video: false
                        }
                      }});

          controller.processSignaling(kArbitraryId, kArbitraryId2, {
                      type: 'updatestream',
                      config: {
                        muteStream: {
                          audio: false,
                          video: false
                        }
                      }});

          expect(mocks.WebRtcConnection.muteStream.callCount).to.equal(3);
          expect(mocks.WebRtcConnection.muteStream.args[1]).to.deep.equal([false, true]);
          expect(mocks.WebRtcConnection.muteStream.args[2]).to.deep.equal([false, false]);
        });

        it('should mute and unmute publisher stream', function() {
          controller.processSignaling(kArbitraryId, undefined, {
                      type: 'updatestream',
                      config: {
                        muteStream: {
                          audio: true,
                          video: false
                        }
                      }});

          controller.processSignaling(kArbitraryId, undefined, {
                      type: 'updatestream',
                      config: {
                        muteStream: {
                          audio: false,
                          video: false
                        }
                      }});

          expect(mocks.WebRtcConnection.muteStream.callCount).to.equal(3);
          expect(mocks.WebRtcConnection.muteStream.args[1]).to.deep.equal([false, true]);
          expect(mocks.WebRtcConnection.muteStream.args[2]).to.deep.equal([false, false]);
        });

        it('should set slide show mode to true', function() {
          controller.processSignaling(kArbitraryId, kArbitraryId2, {
                      type: 'updatestream',
                      config: {
                        slideShowMode: true
                      }});

          expect(mocks.WebRtcConnection.setSlideShowMode.callCount).to.equal(1);
          expect(mocks.WebRtcConnection.setSlideShowMode.args[0][0]).to.be.true;  // jshint ignore:line
        });

        it('should set slide show mode to false', function() {
          controller.processSignaling(kArbitraryId, kArbitraryId2, {
                      type: 'updatestream',
                      config: {
                        slideShowMode: false
                      }});

          expect(mocks.WebRtcConnection.setSlideShowMode.args[0][0]).to.be.false;  // jshint ignore:line
        });
      });

      describe('Remove Subscriber', function() {
        beforeEach(function() {
          mocks.WebRtcConnection.init.onSecondCall().returns(1);
          controller.addSubscriber(kArbitraryId2, kArbitraryId, {}, subCallback);
        });

        it('should succeed closing WebRtcConnection', function() {
          controller.removeSubscriber(kArbitraryId2, kArbitraryId);

          expect(mocks.WebRtcConnection.close.callCount).to.equal(1);
          expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(1);
        });

        it('should fail removing unknown Subscriber', function() {
          var kArbitraryUnknownId = 'unknownId';

          controller.removeSubscriber(kArbitraryUnknownId, kArbitraryId);

          expect(mocks.WebRtcConnection.close.callCount).to.equal(0);
          expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(0);
        });

        it('should fail removing Subscriber from unknown Publisher', function() {
          var kArbitraryUnknownId = 'unknownId';

          controller.removeSubscriber(kArbitraryId2, kArbitraryUnknownId);

          expect(mocks.WebRtcConnection.close.callCount).to.equal(0);
          expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(0);
        });

        it('should remove subscriber only with its id', function() {
          controller.removeSubscriptions(kArbitraryId2);

          expect(mocks.WebRtcConnection.close.callCount).to.equal(1);
          expect(mocks.OneToManyProcessor.removeSubscriber.callCount).to.equal(1);
        });
      });
    });

    describe('Remove Publisher', function() {
      beforeEach(function() {
        mocks.OneToManyProcessor.close.callsArg(0);
        mocks.WebRtcConnection.init.onFirstCall().returns(1);
        controller.addPublisher(kArbitraryId, kArbitraryUrl, callback);
      });

      it('should succeed closing WebRtcConnection and OneToManyProcessor', function() {
        controller.removePublisher(kArbitraryId);

        expect(mocks.WebRtcConnection.close.callCount).to.equal(1);
        expect(mocks.OneToManyProcessor.close.callCount).to.equal(1);
      });

      it('should fail closing an unknown Publisher', function() {
        var kArbitraryUnknownId = 'unknownId';

        controller.removePublisher(kArbitraryUnknownId);

        expect(mocks.WebRtcConnection.close.callCount).to.equal(0);
        expect(mocks.OneToManyProcessor.close.callCount).to.equal(0);
      });

      it('should succeed closing also Subscribers', function() {
        mocks.WebRtcConnection.init.returns(1);
        var kArbitraryId2 = 'id2';
        var subCallback = sinon.stub();
        controller.addSubscriber(kArbitraryId2, kArbitraryId, {}, subCallback);

        controller.removePublisher(kArbitraryId);

        expect(mocks.WebRtcConnection.close.callCount).to.equal(2);
        expect(mocks.OneToManyProcessor.close.callCount).to.equal(1);
      });
    });
  });
});
