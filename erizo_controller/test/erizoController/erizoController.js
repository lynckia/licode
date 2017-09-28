/*global require, describe, it, beforeEach, afterEach*/
'use strict';
var mocks = require('../utils');
var sinon = require('sinon');
var expect  = require('chai').expect;
var Permission = require('../../erizoController/permission');

describe('Erizo Controller / Erizo Controller', function() {
  var amqperMock,
      licodeConfigMock,
      roomControllerMock,
      ecchMock,
      httpMock,
      cryptoMock,
      socketIoMock,
      signatureMock,
      controller,
      rpcPublic,
      arbitraryErizoControllerId = 'ErizoControllerId';

  beforeEach(function() {
    global.config = {logger: {configFile: true}};
    global.config.erizoController = {};
    global.config.erizoController.report = {
      'session_events': true
    };
    signatureMock = mocks.signature;
    licodeConfigMock = mocks.start(mocks.licodeConfig);
    amqperMock = mocks.start(mocks.amqper);
    cryptoMock = mocks.start(mocks.crypto);
    roomControllerMock = mocks.start(mocks.roomController);
    ecchMock = mocks.start(mocks.ecch);
    httpMock = mocks.start(mocks.http);
    socketIoMock = mocks.start(mocks.socketIo);
    controller = require('../../erizoController/erizoController');
    rpcPublic = require('../../erizoController/rpc/rpcPublic');
  });

  afterEach(function() {
    mocks.stop(roomControllerMock);
    mocks.stop(socketIoMock);
    mocks.stop(httpMock);
    mocks.stop(ecchMock);
    mocks.stop(cryptoMock);
    mocks.stop(amqperMock);
    mocks.stop(licodeConfigMock);
    mocks.deleteRequireCache();
    mocks.reset();
    global.config = {};
  });

  it('should have a known API', function() {
    expect(controller.getUsersInRoom).not.to.be.undefined;  // jshint ignore:line
    expect(controller.deleteUser).not.to.be.undefined;  // jshint ignore:line
    expect(controller.deleteRoom).not.to.be.undefined;  // jshint ignore:line
  });

  it('should be added to Nuve', function() {
    expect(amqperMock.callRpc.withArgs('nuve', 'addNewErizoController').callCount).to.equal(1);
    expect(amqperMock.setPublicRPC.callCount).to.equal(1);
  });

  it('should bind to AMQP once connected', function(done) {
    var callback = amqperMock.callRpc.withArgs('nuve', 'addNewErizoController').args[0][3].callback;

    callback({id: arbitraryErizoControllerId});
    setTimeout(() => {
      expect(amqperMock.bind.callCount).to.equal(1);
      done();
    }, 0);
  });

  it('should listen to socket connection once bound', function(done) {
    var callback = amqperMock.callRpc.withArgs('nuve', 'addNewErizoController').args[0][3].callback;
    amqperMock.bind.withArgs('erizoController_' + arbitraryErizoControllerId).callsArg(1);

    callback({id: arbitraryErizoControllerId});

    setTimeout(() => {
      expect(mocks.socketIoInstance.sockets.on.withArgs('connection').callCount).to.equal(1);
      done();
    }, 0);
  });

  describe('Sockets', function() {
    var onToken,
        onReconnect,
        onSendDataStream,
        onSignalingMessage,
        onUpdateStreamAttributes,
        onPublish,
        onSubscribe,
        onStartRecorder,
        onStopRecorder,
        onUnpublish,
        onUnsubscribe,
        onDisconnect;

    beforeEach(function(done) {
      var callback = amqperMock.callRpc.withArgs('nuve', 'addNewErizoController')
            .args[0][3].callback;
      amqperMock.bind.withArgs('erizoController_' + arbitraryErizoControllerId).callsArg(1);
      mocks.socketIoInstance.sockets.on.withArgs('connection')
                                       .callsArgWith(1, mocks.socketInstance);
      callback({id: arbitraryErizoControllerId});

      setTimeout(() => {
        onToken = mocks.socketInstance.on.withArgs('token').args[0][1];
        onReconnect = mocks.socketInstance.on.withArgs('reconnected').args[0][1];
        onDisconnect = mocks.socketInstance.on.withArgs('disconnect').args[0][1];
        done();
      }, 0);
    });

    it('should listen to events', function() {
      expect(mocks.socketInstance.on.withArgs('token').callCount).to.equal(1);
    });

    describe('on Token', function() {
      var arbitrarySignature = 'c2lnbmF0dXJl',  // signature
          arbitraryGoodToken = {
              tokenId: 'tokenId',
              host: 'host',
              signature: arbitrarySignature
            },
          arbitraryBadToken = {
              tokenId: 'tokenId',
              host: 'host',
              signature: 'badSignature'
            };

      beforeEach(function() {
        signatureMock.update.returns(signatureMock);
        signatureMock.digest.returns('signature');
      });

      it('should fail if signature is wrong', function() {
        var callback = sinon.stub();

        onToken(arbitraryBadToken, callback);

        expect(callback.withArgs('error', 'Authentication error').callCount).to.equal(1);
      });

      it('should call Nuve if signature is ok', function() {
        var callback = sinon.stub();

        onToken(arbitraryGoodToken, callback);

        expect(amqperMock.callRpc
                .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId).callCount).to.equal(1);
      });

      describe('when deleted', function() {
        var onTokenCallback,
            callback;

        beforeEach(function() {
          onTokenCallback = sinon.stub();
          onToken(arbitraryGoodToken, onTokenCallback);

          callback = amqperMock.callRpc
                                   .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId)
                                   .args[0][3].callback;
        });

        it('should disconnect socket on Nuve error', function(done) {
          callback('error');
          setTimeout(() => {
            expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('error', 'Token does not exist').callCount).to.equal(1);
            done();
          }, 0);
        });

        it('should disconnect socket on Nuve timeout', function(done) {
          callback('timeout');
          setTimeout(() => {
            expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('error', 'Nuve does not respond').callCount)
                .to.equal(1);
            done();
          }, 0);
        });

        it('should disconnect if Token has an invalid host', function(done) {
          callback({host: 'invalidHost', room: 'roomId'});
          setTimeout(() => {
            expect(onTokenCallback.withArgs('error', 'Invalid host').callCount).to.equal(1);
            done();
          }, 0);
        });

        it('should create Room Controller if it does not exist', function(done) {
          callback({host: 'host', room: 'roomId'});
          setTimeout(() => {
            expect(roomControllerMock.RoomController.callCount).to.equal(1);
            expect(mocks.roomControllerInstance.addEventListener.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('success').callCount).to.equal(1);
            done();
          }, 0);
        });

        it('should not create Room Controller if it does exist', function(done) {
          callback({host: 'host', room: 'roomId'});
          setTimeout(() => {
            expect(roomControllerMock.RoomController.callCount).to.equal(1);
            expect(mocks.roomControllerInstance.addEventListener.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('success').callCount).to.equal(1);
            onToken(arbitraryGoodToken, onTokenCallback);
            let callback2 = amqperMock.callRpc
                                     .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId)
                                     .args[1][3].callback;
            callback2({host: 'host', room: 'roomId'});
            setTimeout(() => {
              expect(roomControllerMock.RoomController.callCount).to.equal(1);
              expect(mocks.roomControllerInstance.addEventListener.callCount).to.equal(1);
              expect(onTokenCallback.withArgs('success').callCount).to.equal(2);
              done();
            }, 0);
        }, 0);
        });

        describe('Public API', function() {
          beforeEach(function(done) {
            callback({host: 'host', room: 'roomId', userName: 'user'});

            setTimeout(() => {
              onSendDataStream = mocks.socketInstance.on.withArgs('sendDataStream').args[0][1];
              onSignalingMessage = mocks.socketInstance.on.withArgs('signaling_message').args[0][1];
              onUpdateStreamAttributes = mocks.socketInstance.on
                                                    .withArgs('updateStreamAttributes').args[0][1];
              onPublish = mocks.socketInstance.on.withArgs('publish').args[0][1];
              onSubscribe = mocks.socketInstance.on.withArgs('subscribe').args[0][1];
              onStartRecorder = mocks.socketInstance.on.withArgs('startRecorder').args[0][1];
              onStopRecorder = mocks.socketInstance.on.withArgs('stopRecorder').args[0][1];
              onUnpublish = mocks.socketInstance.on.withArgs('unpublish').args[0][1];
              onUnsubscribe = mocks.socketInstance.on.withArgs('unsubscribe').args[0][1];
              done();
            }, 0);
          });

          it('should listen to messages', function() {
            expect(mocks.socketInstance.on.withArgs('sendDataStream').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('signaling_message').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('updateStreamAttributes').callCount)
                  .to.equal(1);
            expect(mocks.socketInstance.on.withArgs('publish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('subscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('startRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('stopRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unpublish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unsubscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('disconnect').callCount).to.equal(1);
          });

          it('should return users when calling getUsersInRoom', function() {
            var callback = sinon.stub();

            rpcPublic.getUsersInRoom('roomId', callback);

            expect(callback.callCount).to.equal(1);
            expect(callback.args[0][1].length).to.equal(1);
          });

          it('should return no users when calling getUsersInRoom with an unknown room', function() {
            var callback = sinon.stub();

            rpcPublic.getUsersInRoom('unknownRoom', callback);

            expect(callback.callCount).to.equal(1);
            expect(callback.args[0][1].length).to.equal(0);
          });

          it('should disconnect users when calling deleteUsers', function() {
            var callback = sinon.stub();

            rpcPublic.deleteUser({user: 'user', roomId: 'roomId'}, callback);

            expect(callback.withArgs('callback', 'Success').callCount).to.equal(1);
            expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
          });

          it('should not disconnect users when calling deleteUsers with an unknown room',
              function() {
            var callback = sinon.stub();

            rpcPublic.deleteUser({user: 'user', roomId: 'unknownRoomId'}, callback);

            expect(callback.withArgs('callback', 'Success').callCount).to.equal(1);
            expect(mocks.socketInstance.disconnect.callCount).to.equal(0);
          });

          it('should not disconnect users when calling deleteUsers with an unknown user',
              function() {
            var callback = sinon.stub();

            rpcPublic.deleteUser({user: 'unknownUser', roomId: 'roomId'}, callback);

            expect(callback.withArgs('callback', 'User does not exist').callCount).to.equal(1);
            expect(mocks.socketInstance.disconnect.callCount).to.equal(0);
          });

          it('should remove suscriptions when calling deleteRoom', function() {
            var callback = sinon.stub();

            rpcPublic.deleteRoom('roomId', callback);

            expect(callback.withArgs('callback', 'Success').callCount).to.equal(1);
            expect(mocks.roomControllerInstance.removeSubscriptions.callCount).to.equal(1);
          });

          it('should not remove suscriptions when calling deleteRoom with an unknown room',
              function() {
            var callback = sinon.stub();

            rpcPublic.deleteRoom('unknownRoomId', callback);

            expect(callback.withArgs('callback', 'Success').callCount).to.equal(1);
            expect(mocks.roomControllerInstance.removeSubscriptions.callCount).to.equal(0);
          });
        });

        describe('Socket API', function() {
          let room;
          beforeEach(function(done) {
            const Rooms = require('../../erizoController/models/Room').Rooms;
            const Room = require('../../erizoController/models/Room').Room;
            room = new Room('roomId', false);
            sinon.stub(Rooms.prototype, 'getOrCreateRoom').returns(room);
            callback({host: 'host', room: 'roomId', userName: 'user'});

            setTimeout(() => {
              onSendDataStream = mocks.socketInstance.on.withArgs('sendDataStream').args[0][1];
              onSignalingMessage = mocks.socketInstance.on.withArgs('signaling_message').args[0][1];
              onUpdateStreamAttributes = mocks.socketInstance.on
                                                    .withArgs('updateStreamAttributes').args[0][1];
              onPublish = mocks.socketInstance.on.withArgs('publish').args[0][1];
              onSubscribe = mocks.socketInstance.on.withArgs('subscribe').args[0][1];
              onStartRecorder = mocks.socketInstance.on.withArgs('startRecorder').args[0][1];
              onStopRecorder = mocks.socketInstance.on.withArgs('stopRecorder').args[0][1];
              onUnpublish = mocks.socketInstance.on.withArgs('unpublish').args[0][1];
              onUnsubscribe = mocks.socketInstance.on.withArgs('unsubscribe').args[0][1];
              done();
            }, 0);
          });

          it('should listen to messages', function() {
            expect(mocks.socketInstance.on.withArgs('sendDataStream').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('signaling_message').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('updateStreamAttributes').callCount)
                    .to.equal(1);
            expect(mocks.socketInstance.on.withArgs('publish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('subscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('startRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('stopRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unpublish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unsubscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('disconnect').callCount).to.equal(1);
          });

          describe('on Publish', function() {
            let client;
            beforeEach(function() {
              for (let c of room.clients.values()) {
                client = c;
              }
              client.user.permissions[Permission.PUBLISH] = true;
            });

            it('should fail if user is undefined', function() {
              var options = {},
                  sdp = '',
                  callback = sinon.stub();

              client.user = undefined;

              onPublish(options, sdp, callback);

              expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should fail if user is not authorized', function() {
              var options = {},
                  sdp = '',
                  callback = sinon.stub();

              client.user.permissions[Permission.PUBLISH] = false;

              onPublish(options, sdp, callback);

              expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should fail if user is not authorized to publish video', function() {
              var options = {
                    video: true
                  },
                  sdp = '',
                  callback = sinon.stub();

              client.user.permissions[Permission.PUBLISH] = {video: false};

              onPublish(options, sdp, callback);

              expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should start an External Input if url is given', function() {
              var options = {state: 'url'},
                  sdp = '',
                  callback = sinon.stub();
              mocks.roomControllerInstance.addExternalInput.callsArgWith(2, 'success');

              onPublish(options, sdp, callback);

              expect(mocks.roomControllerInstance.addExternalInput.callCount).to.equal(1);
              expect(callback.callCount).to.equal(1);
            });

            it('should add a Stream', function() {
              var options = {},
                  sdp = '',
                  callback = sinon.stub();

              onPublish(options, sdp, callback);

              expect(mocks.socketInstance.emit.withArgs('onAddStream').callCount).to.equal(1);
            });

            describe('addPublisher with Erizo', function() {
              var options = {state: 'erizo'},
                  sdp = '',
                  callback = sinon.stub();

              it('should call addPublisher', function() {
                onPublish(options, sdp, callback);

                expect(mocks.roomControllerInstance.addPublisher.callCount).to.equal(1);
              });

              it('should send if when receiving initializing state', function() {
                var signMes = {type: 'initializing'};
                mocks.roomControllerInstance.addPublisher.callsArgWith(2, signMes);

                onPublish(options, sdp, callback);

                expect(callback.callCount).to.equal(1);
              });

              it('should emit connection_failed when receiving initializing state', function() {
                var signMes = {type: 'failed'};
                mocks.roomControllerInstance.addPublisher.callsArgWith(2, signMes);

                onPublish(options, sdp, callback);

                expect(mocks.socketInstance.emit.withArgs('connection_failed').callCount)
                        .to.equal(1);
              });

              it('should send onAddStream event to Room when receiving ready stat', function() {
                var signMes = {type: 'initializing'};
                mocks.roomControllerInstance.addPublisher.callsArgWith(2, signMes);

                onPublish(options, sdp, callback);

                var onEvent = mocks.roomControllerInstance.addPublisher.args[0][2];

                onEvent({type: 'ready'});

                expect(mocks.socketInstance.emit.withArgs('onAddStream').callCount).to.equal(1);
              });

              it('should emit ErizoJS unreachable', function() {
                var signMes = 'timeout-erizojs';
                mocks.roomControllerInstance.addPublisher.callsArgWith(2, signMes);

                onPublish(options, sdp, callback);

                expect(callback.withArgs(null, 'ErizoJS is not reachable').callCount).to.equal(1);
              });

              it('should emit ErizoAgent unreachable', function() {
                var signMes = 'timeout-agent';
                mocks.roomControllerInstance.addPublisher.callsArgWith(2, signMes);

                onPublish(options, sdp, callback);

                expect(callback.withArgs(null, 'ErizoAgent is not reachable').callCount)
                      .to.equal(1);
              });

              it('should emit ErizoJS or ErizoAgent unreachable', function() {
                var signMes = 'timeout';
                mocks.roomControllerInstance.addPublisher.callsArgWith(2, signMes);

                onPublish(options, sdp, callback);

                expect(callback.withArgs(null, 'ErizoAgent or ErizoJS is not reachable').callCount)
                                  .to.equal(1);
              });

              describe('on Subscribe', function() {
                var subscriberOptions, subscriberSdp, streamId;
                beforeEach(function() {
                  client.user.permissions = {};
                  client.user.permissions[Permission.SUBSCRIBE] = true;
                  client.user.permissions[Permission.PUBLISH] = true;

                  var options = {audio: true, video: true, screen:true, data: true},
                      sdp = '',
                      callback = sinon.stub();

                  onPublish(options, sdp, callback);

                  streamId = callback.args[0][0];

                  subscriberOptions= {
                       streamId: streamId
                     };
                  subscriberSdp = '';
                });

                it('should fail if user is undefined', function() {
                  var callback = sinon.stub();

                  client.user = undefined;

                  onSubscribe(subscriberOptions, subscriberSdp, callback);

                  expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                });

                it('should fail if user is not authorized', function() {
                  var callback = sinon.stub();

                  client.user.permissions[Permission.SUBSCRIBE] = false;

                  onSubscribe(subscriberOptions, subscriberSdp, callback);

                  expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                });

                it('should fail if user is not authorized to subscribe video', function() {
                  var callback = sinon.stub();

                  subscriberOptions.video = true;

                  client.user.permissions[Permission.SUBSCRIBE] = {video: false};

                  onSubscribe(subscriberOptions, subscriberSdp, callback);

                  expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                });

                it('should emit a publish_me event if the room is p2p', function() {
                  var callback = sinon.stub();
                  room.p2p = true;

                  onSubscribe(subscriberOptions, subscriberSdp, callback);

                  expect(mocks.socketInstance.emit.withArgs('publish_me').callCount).to.equal(1);
                });

                it('should return true if the stream has no media', function() {
                  var callback = sinon.stub();
                  var returnFalse = sinon.stub().returns(false);
                  client.room.getStreamById(streamId).hasVideo = returnFalse;
                  client.room.getStreamById(streamId).hasAudio = returnFalse;
                  client.room.getStreamById(streamId).hasScreen = returnFalse;

                  onSubscribe(subscriberOptions, subscriberSdp, callback);

                  expect(mocks.roomControllerInstance.addSubscriber.callCount).to.equal(0);
                  expect(callback.withArgs(true).callCount).to.equal(1);
                });

                describe('when receiving events from RoomController', function() {
                  var callback = sinon.stub();

                  it('should call addSubscriber', function() {
                    onSubscribe(subscriberOptions, subscriberSdp, callback);

                    expect(mocks.roomControllerInstance.addSubscriber.callCount).to.equal(1);
                  });

                  it('should send if when receiving initializing state', function() {
                    var signMes = {type: 'initializing'};
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, callback);

                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should emit connection_failed when receiving initializing state', function() {
                    var signMes = {type: 'failed'};
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, callback);

                    expect(mocks.socketInstance.emit.withArgs('connection_failed').callCount)
                          .to.equal(1);
                  });

                  it('should send signaling event to the socket when receiving ready stat',
                      function() {
                    var signMes = {type: 'ready'};
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, callback);

                    expect(mocks.socketInstance.emit.withArgs('signaling_message_erizo').callCount)
                                  .to.equal(1);
                  });

                  it('should send signaling event to the socket when receiving bandwidth alerts',
                      function() {
                    var signMes = {type: 'bandwidthAlert'};
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, callback);

                    expect(mocks.socketInstance.emit.withArgs('signaling_message_erizo')
                          .callCount).to.equal(1);
                  });


                  it('should emit ErizoJS unreachable', function() {
                    var signMes = 'timeout';
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, callback);

                    expect(callback.withArgs(null, 'ErizoJS is not reachable').callCount)
                          .to.equal(1);
                  });
                });

                describe('on Send Data Stream', function() {
                  let stream, streamId;
                  beforeEach(function() {
                    for (let st of room.streams.values()) {
                        stream = st;
                        streamId = stream.getID();
                    }
                    stream.getDataSubscribers = sinon.stub().returns([client.id]);
                  });

                  it('should succeed if stream exists', function() {
                    var msg = {id: streamId};

                    onSendDataStream(msg);

                    expect(mocks.socketInstance.emit.withArgs('onDataStream', msg).callCount)
                          .to.equal(1);
                  });

                  it('should fail if stream does not exist', function() {
                    var msg = {id: 'streamId2'};

                    onSendDataStream(msg);

                    expect(mocks.socketInstance.emit.withArgs('onDataStream', msg).callCount)
                          .to.equal(0);
                  });
                });

                describe('on Signaling Message', function() {
                  var arbitraryMessage = {msg: 'msg'};

                  let stream, streamId;
                  beforeEach(function() {
                    for (let st of room.streams.values()) {
                        stream = st;
                        streamId = stream.getID();
                    }
                    arbitraryMessage.streamId = streamId;
                    stream.getDataSubscribers = sinon.stub().returns([client.id]);
                  });

                  it('should send p2p signaling messages', function() {
                    room.p2p = true;
                    arbitraryMessage.peerSocket = client.id;

                    onSignalingMessage(arbitraryMessage);

                    expect(mocks.socketInstance.emit.withArgs('signaling_message_peer')
                          .callCount).to.equal(1);
                  });

                  it('should send other signaling messages', function() {
                    room.p2p = false;
                    room.controller = mocks.roomControllerInstance;

                    onSignalingMessage(arbitraryMessage);

                    expect(mocks.roomControllerInstance.processSignaling
                            .withArgs(arbitraryMessage.streamId).callCount).to.equal(1);
                  });
                });

                describe('on Update Stream Attributes', function() {
                  let stream, streamId;
                  beforeEach(function() {
                    for (let st of room.streams.values()) {
                        stream = st;
                        streamId = stream.getID();
                    }
                    stream.getDataSubscribers = sinon.stub().returns([client.id]);
                    stream.setAttributes = sinon.stub();
                  });

                  it('should succeed if stream exists', function() {
                    var msg = {id: streamId};

                    onUpdateStreamAttributes(msg);

                    expect(mocks.socketInstance.emit.withArgs('onUpdateAttributeStream', msg)
                          .callCount).to.equal(1);
                  });

                  it('should fail if stream does not exist', function() {
                    var msg = {id: 'streamId2'};

                    onUpdateStreamAttributes(msg);

                    expect(mocks.socketInstance.emit.withArgs('onUpdateAttributeStream', msg)
                          .callCount).to.equal(0);
                  });
                });

                describe('on Unsubscribe', function() {
                  let stream, streamId;
                  beforeEach(function() {
                    for (const st of room.streams.values()) {
                        stream = st;
                        streamId = stream.getID();
                    }
                    stream.getDataSubscribers = sinon.stub().returns([client.id]);
                    room.controller = mocks.roomControllerInstance;
                    client.user.permissions = {};
                    client.user.permissions[Permission.PUBLISH] = true;
                    client.user.permissions[Permission.SUBSCRIBE] = true;
                  });

                  it('should fail if user is undefined', function() {
                    var callback = sinon.stub();

                    client.user = undefined;

                    onUnsubscribe(streamId, callback);

                    expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should fail if user is not authorized', function() {
                    var callback = sinon.stub();

                    client.user.permissions[Permission.SUBSCRIBE] = false;

                    onUnsubscribe(streamId, callback);

                    expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should do nothing if stream does not exist', function() {
                    var callback = sinon.stub();
                    streamId = 'unknownStreamId';

                    onUnsubscribe(streamId, callback);

                    expect(callback.callCount).to.equal(0);
                  });

                  it('should call removeSubscriber when succeded', function() {
                    var callback = sinon.stub();

                    onUnsubscribe(streamId, callback);

                    expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should call removeSubscriber when succeded (audio only streams) ',
                      function() {
                    var callback = sinon.stub();
                    var returnFalse = sinon.stub().returns(false);
                    client.room.getStreamById(streamId).hasVideo = returnFalse;
                    client.room.getStreamById(streamId).hasScreen = returnFalse;

                    onUnsubscribe(streamId, callback);

                    expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should call removeSubscriber when succeded (video only streams) ',
                      function() {
                    var callback = sinon.stub();
                    var returnFalse = sinon.stub().returns(false);
                    client.room.getStreamById(streamId).hasAudio = returnFalse;
                    client.room.getStreamById(streamId).hasScreen = returnFalse;

                    onUnsubscribe(streamId, callback);

                    expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should call removeSubscriber when succeded (screen only streams) ',
                      function() {
                    var callback = sinon.stub();
                    var returnFalse = sinon.stub().returns(false);
                    client.room.getStreamById(streamId).hasAudio = returnFalse;
                    client.room.getStreamById(streamId).hasVideo = returnFalse;

                    onUnsubscribe(streamId, callback);

                    expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should not call removePublisher in p2p rooms', function() {
                    var callback = sinon.stub();

                    room.p2p = true;

                    onUnsubscribe(streamId, callback);

                    expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(0);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });
                });

                describe('on Unpublish', function() {
                  let stream, streamId;
                  beforeEach(function() {
                    for (const st of room.streams.values()) {
                        stream = st;
                        streamId = stream.getID();
                    }
                    stream.getDataSubscribers = sinon.stub().returns([client.id]);
                    room.controller = mocks.roomControllerInstance;
                    client.user.permissions = {};
                    client.user.permissions[Permission.PUBLISH] = true;
                    client.user.permissions[Permission.SUBSCRIBE] = true;
                  });

                  it('should fail if user is undefined', function() {
                    var callback = sinon.stub();

                    client.user = undefined;

                    onUnpublish(streamId, callback);

                    expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should fail if user is not authorized', function() {
                    var callback = sinon.stub();

                    client.user.permissions[Permission.PUBLISH] = false;

                    onUnpublish(streamId, callback);

                    expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should do nothing if stream does not exist', function() {
                    var callback = sinon.stub();
                    streamId = 'unknownStreamId';

                    onUnpublish(streamId, callback);

                    expect(callback.callCount).to.equal(0);
                  });

                  it('should send a onRemoveStream event when succeded', function() {
                    var callback = sinon.stub();

                    onUnpublish(streamId, callback);

                    expect(mocks.roomControllerInstance.removePublisher.withArgs(streamId)
                            .callCount).to.equal(1);
                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                          .to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should send a onRemoveStream event when succeded (audio only streams) ',
                      function() {
                    var callback = sinon.stub();
                    var returnFalse = sinon.stub().returns(false);
                    client.room.getStreamById(streamId).hasVideo = returnFalse;
                    client.room.getStreamById(streamId).hasScreen = returnFalse;

                    onUnpublish(streamId, callback);

                    expect(mocks.roomControllerInstance.removePublisher.withArgs(streamId)
                          .callCount).to.equal(1);
                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                          .to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should send a onRemoveStream event when succeded (video only streams) ',
                      function() {
                    var callback = sinon.stub();
                    var returnFalse = sinon.stub().returns(false);
                    client.room.getStreamById(streamId).hasAudio = returnFalse;
                    client.room.getStreamById(streamId).hasScreen = returnFalse;

                    onUnpublish(streamId, callback);

                    expect(mocks.roomControllerInstance.removePublisher.withArgs(streamId)
                          .callCount).to.equal(1);
                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                            .to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should send a onRemoveStream event when succeded (screen only streams) ',
                      function() {
                    var callback = sinon.stub();
                    var returnFalse = sinon.stub().returns(false);
                    client.room.getStreamById(streamId).hasAudio = returnFalse;
                    client.room.getStreamById(streamId).hasVideo = returnFalse;

                    onUnpublish(streamId, callback);

                    expect(mocks.roomControllerInstance.removePublisher.withArgs(streamId)
                          .callCount).to.equal(1);
                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                            .to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should not call removePublisher in p2p rooms', function() {
                    var callback = sinon.stub();

                    room.p2p = true;

                    onUnpublish(streamId, callback);

                    expect(mocks.roomControllerInstance.removePublisher
                            .withArgs(streamId).callCount).to.equal(0);
                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                          .to.equal(1);
                    expect(callback.withArgs(true).callCount).to.equal(1);
                  });
                });

                describe('on Disconnect', function() {
                  beforeEach(function() {
                  });

                  it('should send a onRemoveStream event', function() {
                    onDisconnect();

                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                          .to.equal(1);
                  });

                  it('should call removePublisher', function() {
                    onDisconnect();

                    expect(mocks.roomControllerInstance.removePublisher.callCount).to.equal(1);
                  });

                  it('should not call removePublisher if room is p2p', function() {
                    room.p2p = true;

                    onDisconnect();

                    expect(mocks.roomControllerInstance.removePublisher.callCount).to.equal(0);
                  });
                });
              });
            });
          });

          describe('on Start Recorder', function() {
            let subscriberOptions, streamId, client;

            beforeEach(function() {
              for (let c of room.clients.values()) {
                client = c;
              }
              client.user.permissions[Permission.PUBLISH] = true;
              client.user.permissions[Permission.RECORD] = true;

              var options = {audio: true, video: true, data: true},
                  sdp = '',
                  callback = sinon.stub();

              onPublish(options, sdp, callback);

              streamId = callback.args[0][0];

              subscriberOptions= {
                   to: streamId
                 };
            });

            it('should fail if user is undefined', function() {
              var callback = sinon.stub();
              client.user = undefined;

              onStartRecorder(subscriberOptions, callback);

              expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should fail if user is not authorized', function() {
              var callback = sinon.stub();
              client.user.permissions[Permission.RECORD] = false;

              onStartRecorder(subscriberOptions, callback);

              expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should return an error if the room is p2p', function() {
              var callback = sinon.stub();
              room.p2p = true;

              onStartRecorder(subscriberOptions, callback);

              expect(callback.withArgs(null, 'Stream can not be recorded').callCount).to.equal(1);
            });

            it('should return an error if the stream has no media', function() {
              var callback = sinon.stub();
              var returnFalse = sinon.stub().returns(false);
              client.room.getStreamById(streamId).hasVideo = returnFalse;
              client.room.getStreamById(streamId).hasAudio = returnFalse;
              client.room.getStreamById(streamId).hasScreen = returnFalse;

              onStartRecorder(subscriberOptions, callback);

              expect(callback.withArgs(null, 'Stream can not be recorded').callCount).to.equal(1);
            });

            describe('when receiving events from RoomController', function() {
              var callback = sinon.stub();

              it('should call addExternalOutput', function() {
                onStartRecorder(subscriberOptions, callback);

                expect(mocks.roomControllerInstance.addExternalOutput.callCount).to.equal(1);
              });

              it('should return ok if when receiving success state', function() {
                var signMes = 'success';
                mocks.roomControllerInstance.addExternalOutput.callsArgWith(2, signMes);

                onStartRecorder(subscriberOptions, callback);

                expect(callback.callCount).to.equal(1);
              });

              it('should return an error if when receiving a failed state', function() {
                var signMes = 'failed';
                mocks.roomControllerInstance.addExternalOutput.callsArgWith(2, signMes);

                onStartRecorder(subscriberOptions, callback);

                expect(callback.withArgs(null, 'Unable to subscribe to stream for recording, ' +
                               'publisher not present').callCount).to.equal(1);
              });
            });

            describe('on Stop Recorder', function() {
              var subscriberOptions = {};

              beforeEach(function() {
                room.controller = mocks.roomControllerInstance;
              });

              it('should fail if user is undefined', function() {
                var callback = sinon.stub();
                client.user = undefined;

                onStopRecorder(subscriberOptions, callback);

                expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
              });

              it('should fail if user is not authorized', function() {
                var callback = sinon.stub();
                client.user.permissions[Permission.RECORD] = false;

                onStopRecorder(subscriberOptions, callback);

                expect(callback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
              });

              it('should succeed if authorized', function() {
                var callback = sinon.stub();

                onStopRecorder(subscriberOptions, callback);

                expect(mocks.roomControllerInstance.removeExternalOutput.callCount).to.equal(1);
              });
            });
          });
        });
      });
    });
  });
});
