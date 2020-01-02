/* global require, describe, it, beforeEach, afterEach, before, after */

/* eslint-disable no-unused-expressions, global-require */

const mocks = require('../utils');
// eslint-disable-next-line import/no-extraneous-dependencies
const sinon = require('sinon');
// eslint-disable-next-line import/no-extraneous-dependencies
const expect = require('chai').expect;
const Permission = require('../../erizoController/permission');

describe('Erizo Controller / Erizo Controller', () => {
  let amqperMock;
  let licodeConfigMock;
  let roomControllerMock;
  let ecchMock;
  let httpMock;
  let cryptoMock;
  let socketIoMock;
  let signatureMock;
  let controller;
  let rpcPublic;
  let processArgsBackup;

  const arbitraryErizoControllerId = 'ErizoControllerId';

  // Allows passing arguments to mocha
  before(() => {
    processArgsBackup = [...process.argv];
    process.argv = [];
  });

  after(() => {
    process.argv = [...processArgsBackup];
  });

  beforeEach(() => {
    global.config = { logger: { configFile: true } };
    global.config.erizoController = {};
    global.config.erizoController.report = {
      session_events: true,
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

  afterEach(() => {
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

  it('should have a known API', () => {
    expect(controller.getUsersInRoom).not.to.be.undefined;
    expect(controller.deleteUser).not.to.be.undefined;
    expect(controller.deleteRoom).not.to.be.undefined;
  });

  it('should be added to Nuve', () => {
    expect(amqperMock.callRpc.withArgs('nuve', 'addNewErizoController').callCount).to.equal(1);
    expect(amqperMock.setPublicRPC.callCount).to.equal(1);
  });

  it('should bind to AMQP once connected', (done) => {
    const callback = amqperMock.callRpc.withArgs('nuve', 'addNewErizoController').args[0][3].callback;

    callback({ id: arbitraryErizoControllerId });
    setTimeout(() => {
      expect(amqperMock.bind.callCount).to.equal(1);
      done();
    }, 0);
  });

  it('should listen to socket connection once bound', (done) => {
    const callback = amqperMock.callRpc.withArgs('nuve', 'addNewErizoController').args[0][3].callback;
    amqperMock.bind.withArgs(`erizoController_${arbitraryErizoControllerId}`).callsArg(1);

    callback({ id: arbitraryErizoControllerId });

    setTimeout(() => {
      expect(mocks.socketIoInstance.sockets.on.withArgs('connection').callCount).to.equal(1);
      done();
    }, 0);
  });

  describe('Sockets', () => {
    let onToken;
    // eslint-disable-next-line no-unused-vars
    let onReconnect;
    let onSendDataStream;
    let onStreamMessageErizo;
    let onStreamMessageP2P;
    let onUpdateStreamAttributes;
    let onPublish;
    let onSubscribe;
    let onStartRecorder;
    let onStopRecorder;
    let onUnpublish;
    let onUnsubscribe;
    let onDisconnect;

    beforeEach((done) => {
      const callback = amqperMock.callRpc.withArgs('nuve', 'addNewErizoController')
        .args[0][3].callback;
      amqperMock.bind.withArgs(`erizoController_${arbitraryErizoControllerId}`).callsArg(1);
      mocks.socketIoInstance.sockets.on.withArgs('connection')
        .callsArgWith(1, mocks.socketInstance);
      callback({ id: arbitraryErizoControllerId });

      setTimeout(() => {
        onToken = mocks.socketInstance.on.withArgs('token').args[0][1];
        onReconnect = mocks.socketInstance.on.withArgs('reconnected').args[0][1];
        onDisconnect = mocks.socketInstance.on.withArgs('disconnect').args[0][1];
        done();
      }, 0);
    });

    it('should listen to events', () => {
      expect(mocks.socketInstance.on.withArgs('token').callCount).to.equal(1);
    });

    describe('on Token', () => {
      const arbitrarySignature = 'c2lnbmF0dXJl'; // signature
      const arbitraryGoodToken = {
        tokenId: 'tokenId',
        host: 'host',
        signature: arbitrarySignature,
      };
      const arbitraryBadToken = {
        tokenId: 'tokenId',
        host: 'host',
        signature: 'badSignature',
      };

      beforeEach(() => {
        signatureMock.update.returns(signatureMock);
        signatureMock.digest.returns('signature');
      });

      it('should fail if signature is wrong', () => {
        const callback = sinon.stub();

        onToken({ token: arbitraryBadToken }, callback);

        expect(callback.withArgs('error', 'Authentication error').callCount).to.equal(1);
      });

      it('should call Nuve if signature is ok', () => {
        const callback = sinon.stub();

        onToken({ token: arbitraryGoodToken }, callback);

        expect(amqperMock.callRpc
          .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId).callCount).to.equal(1);
      });

      describe('when deleted', () => {
        let onTokenCallback;
        let callback;

        beforeEach(() => {
          onTokenCallback = sinon.stub();
          onToken({ token: arbitraryGoodToken }, onTokenCallback);

          callback = amqperMock.callRpc
            .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId)
            .args[0][3].callback;
        });

        it('should disconnect socket on Nuve error', (done) => {
          callback('error');
          setTimeout(() => {
            expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('error', 'Token does not exist').callCount).to.equal(1);
            done();
          }, 0);
        });

        it('should disconnect socket on Nuve timeout', (done) => {
          callback('timeout');
          setTimeout(() => {
            expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('error', 'Nuve does not respond').callCount)
              .to.equal(1);
            done();
          }, 0);
        });

        it('should disconnect if Token has an invalid host', (done) => {
          callback({ host: 'invalidHost', room: 'roomId' });
          setTimeout(() => {
            expect(onTokenCallback.withArgs('error', 'Invalid host').callCount).to.equal(1);
            done();
          }, 0);
        });

        it('should create Room Controller if it does not exist', (done) => {
          callback({ host: 'host', room: 'roomId' });
          setTimeout(() => {
            expect(roomControllerMock.RoomController.callCount).to.equal(1);
            expect(mocks.roomControllerInstance.addEventListener.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('success').callCount).to.equal(1);
            done();
          }, 0);
        });

        it('should not create Room Controller if it does exist', (done) => {
          callback({ host: 'host', room: 'roomId' });
          setTimeout(() => {
            expect(roomControllerMock.RoomController.callCount).to.equal(1);
            expect(mocks.roomControllerInstance.addEventListener.callCount).to.equal(1);
            expect(onTokenCallback.withArgs('success').callCount).to.equal(1);
            onToken({ token: arbitraryGoodToken }, onTokenCallback);
            const callback2 = amqperMock.callRpc
              .withArgs('nuve', 'deleteToken', arbitraryGoodToken.tokenId)
              .args[1][3].callback;
            callback2({ host: 'host', room: 'roomId' });
            setTimeout(() => {
              expect(roomControllerMock.RoomController.callCount).to.equal(1);
              expect(mocks.roomControllerInstance.addEventListener.callCount).to.equal(1);
              expect(onTokenCallback.withArgs('success').callCount).to.equal(2);
              done();
            }, 0);
          }, 0);
        });

        describe('Public API', () => {
          beforeEach((done) => {
            callback({ host: 'host', room: 'roomId', userName: 'user' });

            setTimeout(() => {
              onSendDataStream = mocks.socketInstance.on.withArgs('sendDataStream').args[0][1];
              onStreamMessageErizo = mocks.socketInstance.on.withArgs('streamMessage').args[0][1];
              onStreamMessageP2P = mocks.socketInstance.on.withArgs('streamMessageP2P').args[0][1];
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

          it('should listen to messages', () => {
            expect(mocks.socketInstance.on.withArgs('sendDataStream').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('streamMessage').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('streamMessageP2P').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('connectionMessage').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('updateStreamAttributes').callCount)
              .to.equal(1);
            expect(mocks.socketInstance.on.withArgs('publish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('subscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('autoSubscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('startRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('stopRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unpublish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unsubscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('disconnect').callCount).to.equal(1);
          });

          it('should return users when calling getUsersInRoom', () => {
            const aCallback = sinon.stub();

            rpcPublic.getUsersInRoom('roomId', aCallback);

            expect(aCallback.callCount).to.equal(1);
            expect(aCallback.args[0][1].length).to.equal(1);
          });

          it('should return no users when calling getUsersInRoom with an unknown room', () => {
            const aCallback = sinon.stub();

            rpcPublic.getUsersInRoom('unknownRoom', aCallback);

            expect(aCallback.callCount).to.equal(1);
            expect(aCallback.args[0][1].length).to.equal(0);
          });

          it('should disconnect users when calling deleteUsers', () => {
            const aCallback = sinon.stub();

            rpcPublic.deleteUser({ user: 'user', roomId: 'roomId' }, aCallback);

            expect(aCallback.withArgs('callback', 'Success').callCount).to.equal(1);
            expect(mocks.socketInstance.disconnect.callCount).to.equal(1);
          });

          it('should not disconnect users when calling deleteUsers with an unknown room',
            () => {
              const aCallback = sinon.stub();

              rpcPublic.deleteUser({ user: 'user', roomId: 'unknownRoomId' }, aCallback);

              expect(aCallback.withArgs('callback', 'Success').callCount).to.equal(1);
              expect(mocks.socketInstance.disconnect.callCount).to.equal(0);
            });

          it('should not disconnect users when calling deleteUsers with an unknown user',
            () => {
              const aCallback = sinon.stub();

              rpcPublic.deleteUser({ user: 'unknownUser', roomId: 'roomId' }, aCallback);

              expect(aCallback.withArgs('callback', 'User does not exist').callCount).to.equal(1);
              expect(mocks.socketInstance.disconnect.callCount).to.equal(0);
            });
        });

        describe('Socket API', () => {
          let room;
          beforeEach((done) => {
            const Rooms = require('../../erizoController/models/Room').Rooms;
            const Room = require('../../erizoController/models/Room').Room;
            room = new Room('roomId', false);
            sinon.stub(Rooms.prototype, 'getOrCreateRoom').returns(room);
            callback({ host: 'host', room: 'roomId', userName: 'user' });

            setTimeout(() => {
              onSendDataStream = mocks.socketInstance.on.withArgs('sendDataStream').args[0][1];
              onStreamMessageErizo = mocks.socketInstance.on.withArgs('streamMessage').args[0][1];
              onStreamMessageP2P = mocks.socketInstance.on.withArgs('streamMessageP2P').args[0][1];
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

          it('should listen to messages', () => {
            expect(mocks.socketInstance.on.withArgs('sendDataStream').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('streamMessage').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('streamMessageP2P').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('connectionMessage').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('updateStreamAttributes').callCount)
              .to.equal(1);
            expect(mocks.socketInstance.on.withArgs('publish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('autoSubscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unsubscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('startRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('stopRecorder').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unpublish').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('unsubscribe').callCount).to.equal(1);
            expect(mocks.socketInstance.on.withArgs('disconnect').callCount).to.equal(1);
          });

          describe('on Publish', () => {
            let client;
            beforeEach(() => {
              room.forEachClient((aClient) => {
                client = aClient;
              });
              client.user.permissions[Permission.PUBLISH] = true;
            });

            it('should fail if user is undefined', () => {
              const options = {};
              const sdp = '';
              const aCallback = sinon.stub();

              client.user = undefined;

              onPublish(options, sdp, aCallback);

              expect(aCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should fail if user is not authorized', () => {
              const options = {};
              const sdp = '';
              const aCallback = sinon.stub();

              client.user.permissions[Permission.PUBLISH] = false;

              onPublish(options, sdp, aCallback);

              expect(aCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should fail if user is not authorized to publish video', () => {
              const options = {
                video: true,
              };
              const sdp = '';
              const aCallback = sinon.stub();

              client.user.permissions[Permission.PUBLISH] = { video: false };

              onPublish(options, sdp, aCallback);

              expect(aCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should start an External Input if url is given', () => {
              const options = { state: 'url' };
              const sdp = '';
              const aCallback = sinon.stub();
              mocks.roomControllerInstance.addExternalInput.callsArgWith(4, 'success');

              onPublish(options, sdp, aCallback);

              expect(mocks.roomControllerInstance.addExternalInput.callCount).to.equal(1);
              expect(aCallback.callCount).to.equal(1);
            });

            it('should add a Stream', () => {
              const options = {};
              const sdp = '';
              const aCallback = sinon.stub();

              onPublish(options, sdp, aCallback);

              expect(mocks.socketInstance.emit.withArgs('onAddStream').callCount).to.equal(1);
            });

            describe('addPublisher with Erizo', () => {
              const options = { state: 'erizo' };
              const sdp = '';
              const aCallback = sinon.stub();

              it('should call addPublisher', () => {
                onPublish(options, sdp, aCallback);

                expect(mocks.roomControllerInstance.addPublisher.callCount).to.equal(1);
              });

              it('should send if when receiving initializing state', () => {
                const signMes = { type: 'initializing' };
                mocks.roomControllerInstance.addPublisher.callsArgWith(3, signMes);

                onPublish(options, sdp, aCallback);

                expect(aCallback.callCount).to.equal(1);
              });

              it('should emit connection_failed when receiving initializing state', () => {
                const signMes = { type: 'failed' };
                mocks.roomControllerInstance.addPublisher.callsArgWith(3, signMes);

                onPublish(options, sdp, aCallback);

                expect(mocks.socketInstance.emit.withArgs('connection_failed').callCount)
                  .to.equal(1);
              });

              it('should send onAddStream event to Room when receiving ready stat', () => {
                const signMes = { type: 'initializing' };
                mocks.roomControllerInstance.addPublisher.callsArgWith(3, signMes);

                onPublish(options, sdp, aCallback);

                const onEvent = mocks.roomControllerInstance.addPublisher.args[0][3];

                onEvent({ type: 'ready' });

                expect(mocks.socketInstance.emit.withArgs('onAddStream').callCount).to.equal(1);
              });

              it('should emit ErizoJS unreachable', () => {
                const signMes = 'timeout-erizojs';
                mocks.roomControllerInstance.addPublisher.callsArgWith(3, signMes);

                onPublish(options, sdp, aCallback);

                expect(aCallback.withArgs(null, null, 'ErizoJS is not reachable').callCount)
                  .to.equal(1);
              });

              it('should emit ErizoAgent unreachable', () => {
                const signMes = 'timeout-agent';
                mocks.roomControllerInstance.addPublisher.callsArgWith(3, signMes);

                onPublish(options, sdp, aCallback);

                expect(aCallback.withArgs(null, null, 'ErizoAgent is not reachable').callCount)
                  .to.equal(1);
              });

              it('should emit ErizoJS or ErizoAgent unreachable', () => {
                const signMes = 'timeout';
                mocks.roomControllerInstance.addPublisher.callsArgWith(3, signMes);

                onPublish(options, sdp, aCallback);

                expect(aCallback.withArgs(null, null, 'ErizoAgent or ErizoJS is not reachable')
                  .callCount).to.equal(1);
              });

              describe('Subscriber Connection', () => {
                // eslint-disable-next-line no-unused-vars
                let onSubscriberReconnect;
                let onSubscriberAutoSubscribe;
                let onSubscriberTokenCallback;
                let onSubscriberToken;
                let subscriberClient;
                const arbitrarySubscriberSignature = 'c2lnbmF0dXJl'; // signature
                const arbitrarySubscriberGoodToken = {
                  tokenId: 'tokenId2',
                  host: 'host',
                  signature: arbitrarySubscriberSignature,
                };

                beforeEach((done) => {
                  const onConnection = mocks.socketIoInstance.sockets.on.withArgs('connection').args[0][1];
                  onConnection(mocks.socketInstance);
                  signatureMock.update.returns(signatureMock);
                  signatureMock.digest.returns('signature');
                  onSubscriberTokenCallback = sinon.stub();
                  onSubscriberToken = mocks.socketInstance.on.withArgs('token').args[1][1];
                  onSubscriberToken({ token: arbitrarySubscriberGoodToken },
                    onSubscriberTokenCallback);

                  callback = amqperMock.callRpc
                    .withArgs('nuve', 'deleteToken', arbitrarySubscriberGoodToken.tokenId)
                    .args[0][3].callback;

                  setTimeout(() => {
                    callback({ host: 'host', room: 'roomId', userName: 'user2' });
                    setTimeout(() => {
                      onSubscriberAutoSubscribe = mocks.socketInstance.on
                        .withArgs('autoSubscribe').args[1][1];
                      room.forEachClient((aClient) => {
                        if (aClient.token.userName === 'user2') {
                          subscriberClient = aClient;
                        }
                      });
                      subscriberClient.user.permissions[Permission.SUBSCRIBE] = true;
                      done();
                    }, 0);
                  }, 0);
                });

                describe('on AutoSubscription', () => {
                  let data;
                  const streams = [];
                  let streamId;

                  beforeEach(() => {
                    subscriberClient.user.permissions = {};
                    subscriberClient.user.permissions[Permission.SUBSCRIBE] = true;
                    subscriberClient.user.permissions[Permission.PUBLISH] = true;

                    const aOptions = {
                      audio: true,
                      video: true,
                      screen: true,
                      data: true,
                      attributes: { type: 'publisher' } };
                    const aSdp = '';
                    const publishCallback = sinon.stub();

                    onPublish(aOptions, aSdp, publishCallback);

                    streamId = publishCallback.args[0][0];

                    data = {
                      selectors: { '/attributes/type': 'publisher' },
                      negativeSelectors: {},
                      options: { audio: true, video: true },
                    };
                    streams.push(streamId);
                  });

                  it('should call callback', () => {
                    const subscribeCallback = sinon.stub();
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    expect(subscribeCallback.callCount).to.equal(1);
                  });

                  it('should fail if user is not authorized to subscribe', () => {
                    const subscribeCallback = sinon.stub();
                    subscriberClient.user.permissions[Permission.SUBSCRIBE] = false;
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    expect(subscribeCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should call RoomController if any stream meets selector', () => {
                    const subscribeCallback = sinon.stub();
                    data.selectors = { '/attributes/type': 'publisher' };
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    expect(mocks.roomControllerInstance.addMultipleSubscribers.callCount)
                      .to.equal(1);
                    expect(mocks.roomControllerInstance.removeMultipleSubscribers.callCount)
                      .to.equal(0);
                  });

                  it('should call RoomController any time a stream meets selector', () => {
                    const subscribeCallback = sinon.stub();
                    data.selectors = { '/attributes/type': 'publisher' };
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    expect(mocks.roomControllerInstance.addMultipleSubscribers.callCount)
                      .to.equal(3);
                    expect(mocks.roomControllerInstance.removeMultipleSubscribers.callCount)
                      .to.equal(0);
                  });

                  it('should not call RoomController if no stream meets a selector', () => {
                    const subscribeCallback = sinon.stub();
                    data.selectors = { '/attributes/type': 'subscriber' };
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    expect(mocks.roomControllerInstance.addMultipleSubscribers.callCount)
                      .to.equal(0);
                    expect(mocks.roomControllerInstance.removeMultipleSubscribers.callCount)
                      .to.equal(0);
                  });

                  it('should call RoomController to remove multiple subscribers', () => {
                    const subscribeCallback = sinon.stub();
                    data.selectors = { '/attributes/type': 'publisher' };
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    data.selectors = { '/attributes/type': 'subscriber' };
                    onSubscriberAutoSubscribe(data, subscribeCallback);
                    expect(mocks.roomControllerInstance.addMultipleSubscribers.callCount)
                      .to.equal(1);
                    expect(mocks.roomControllerInstance.removeMultipleSubscribers.callCount)
                      .to.equal(1);
                  });
                });
              });

              describe('on Subscribe', () => {
                let subscriberOptions;
                let subscriberSdp;
                let streamId;

                beforeEach(() => {
                  client.user.permissions = {};
                  client.user.permissions[Permission.SUBSCRIBE] = true;
                  client.user.permissions[Permission.PUBLISH] = true;

                  const aOptions = { audio: true, video: true, screen: true, data: true };
                  const aSdp = '';
                  const publishCallback = sinon.stub();

                  onPublish(aOptions, aSdp, publishCallback);

                  streamId = publishCallback.args[0][0];

                  subscriberOptions = {
                    streamId,
                  };
                  subscriberSdp = '';
                });

                it('should fail if user is undefined', () => {
                  const subscribeCallback = sinon.stub();

                  client.user = undefined;

                  onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                  expect(subscribeCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                });

                it('should fail if user is not authorized', () => {
                  const subscribeCallback = sinon.stub();

                  client.user.permissions[Permission.SUBSCRIBE] = false;

                  onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                  expect(subscribeCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                });

                it('should fail if user is not authorized to subscribe video', () => {
                  const subscribeCallback = sinon.stub();

                  subscriberOptions.video = true;

                  client.user.permissions[Permission.SUBSCRIBE] = { video: false };

                  onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                  expect(subscribeCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                });

                it('should emit a publish_me event if the room is p2p', () => {
                  const subscribeCallback = sinon.stub();
                  room.p2p = true;

                  onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                  expect(mocks.socketInstance.emit.withArgs('publish_me').callCount).to.equal(1);
                });

                it('should return true if the stream has no media', () => {
                  const subscribeCallback = sinon.stub();
                  const returnFalse = sinon.stub().returns(false);
                  client.room.streamManager.getPublishedStreamById(streamId).hasVideo = returnFalse;
                  client.room.streamManager.getPublishedStreamById(streamId).hasAudio = returnFalse;
                  client.room.streamManager.getPublishedStreamById(streamId)
                    .hasScreen = returnFalse;

                  onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                  expect(mocks.roomControllerInstance.addSubscriber.callCount).to.equal(0);
                  expect(subscribeCallback.withArgs(true).callCount).to.equal(1);
                });

                describe('when receiving events from RoomController', () => {
                  const subscribeCallback = sinon.stub();

                  it('should call addSubscriber', () => {
                    onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                    expect(mocks.roomControllerInstance.addSubscriber.callCount).to.equal(1);
                  });

                  it('should send if when receiving initializing state', () => {
                    const signMes = { type: 'initializing' };
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                    expect(subscribeCallback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should emit connection_failed when receiving initializing state', () => {
                    const signMes = { type: 'failed' };
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                    expect(mocks.socketInstance.emit.withArgs('connection_failed').callCount)
                      .to.equal(1);
                  });
                  it('should send signaling event to the socket when receiving bandwidth alerts',
                    () => {
                      const signMes = { type: 'bandwidthAlert' };
                      mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                      onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                      expect(mocks.socketInstance.emit.withArgs('stream_message_erizo')
                        .callCount).to.equal(1);
                    });


                  it('should emit ErizoJS unreachable', () => {
                    const signMes = 'timeout';
                    mocks.roomControllerInstance.addSubscriber.callsArgWith(3, signMes);

                    onSubscribe(subscriberOptions, subscriberSdp, subscribeCallback);

                    expect(subscribeCallback.withArgs(null, null, 'ErizoJS is not reachable').callCount)
                      .to.equal(1);
                  });
                });

                describe('on Send Data Stream', () => {
                  let stream;
                  let sendDataStreamId;
                  beforeEach(() => {
                    room.streamManager.forEachPublishedStream((aStream) => {
                      stream = aStream;
                      sendDataStreamId = stream.getID();
                    });
                    stream.dataSubscribers = [client.id];
                  });

                  it('should succeed if stream exists', () => {
                    const msg = { id: sendDataStreamId };

                    onSendDataStream(msg);

                    expect(mocks.socketInstance.emit.withArgs('onDataStream', msg).callCount)
                      .to.equal(1);
                  });

                  it('should fail if stream does not exist', () => {
                    const msg = { id: 'streamId2' };

                    onSendDataStream(msg);

                    expect(mocks.socketInstance.emit.withArgs('onDataStream', msg).callCount)
                      .to.equal(0);
                  });
                });

                describe('on Signaling Message', () => {
                  const arbitraryMessage = { msg: 'msg' };

                  let stream;
                  let signalingStreamId;
                  beforeEach(() => {
                    room.streamManager.forEachPublishedStream((aStream) => {
                      stream = aStream;
                      signalingStreamId = stream.getID();
                    });
                    arbitraryMessage.streamId = signalingStreamId;
                    stream.dataSubscribers = [client.id];
                  });

                  it('should send p2p signaling messages', () => {
                    room.p2p = true;
                    arbitraryMessage.peerSocket = client.id;

                    onStreamMessageP2P(arbitraryMessage);

                    expect(mocks.socketInstance.emit.withArgs('stream_message_p2p').callCount).to.equal(1);
                  });

                  it('should send other signaling messages', () => {
                    room.p2p = false;
                    room.controller = mocks.roomControllerInstance;

                    onStreamMessageErizo(arbitraryMessage);

                    expect(mocks.roomControllerInstance.processStreamMessageFromClient
                      .withArgs(arbitraryMessage.erizoId, client.id,
                        arbitraryMessage.streamId).callCount).to.equal(1);
                  });
                });

                describe('on Update Stream Attributes', () => {
                  let stream;
                  let updateStreamId;
                  beforeEach(() => {
                    room.streamManager.forEachPublishedStream((aStream) => {
                      stream = aStream;
                      updateStreamId = stream.getID();
                    });
                    stream.dataSubscribers = [client.id];
                    stream.setAttributes = sinon.stub();
                  });

                  it('should succeed if stream exists', () => {
                    const msg = { id: updateStreamId };

                    onUpdateStreamAttributes(msg);

                    expect(mocks.socketInstance.emit.withArgs('onUpdateAttributeStream', msg)
                      .callCount).to.equal(1);
                  });

                  it('should fail if stream does not exist', () => {
                    const msg = { id: 'streamId2' };

                    onUpdateStreamAttributes(msg);

                    expect(mocks.socketInstance.emit.withArgs('onUpdateAttributeStream', msg)
                      .callCount).to.equal(0);
                  });
                });

                describe('on Unsubscribe', () => {
                  let stream;
                  let unsubscribeStreamId;
                  beforeEach(() => {
                    room.streamManager.forEachPublishedStream((aStream) => {
                      stream = aStream;
                      unsubscribeStreamId = stream.getID();
                    });
                    stream.dataSubscribers = [client.id];
                    stream.addAvSubscriber(client.id);
                    room.controller = mocks.roomControllerInstance;
                    client.user.permissions = {};
                    client.user.permissions[Permission.PUBLISH] = true;
                    client.user.permissions[Permission.SUBSCRIBE] = true;
                  });

                  it('should fail if user is undefined', () => {
                    const unsubscribeCallback = sinon.stub();

                    client.user = undefined;

                    onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                    expect(unsubscribeCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should fail if user is not authorized', () => {
                    const unsubscribeCallback = sinon.stub();

                    client.user.permissions[Permission.SUBSCRIBE] = false;

                    onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                    expect(unsubscribeCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should do nothing if stream does not exist', () => {
                    const unsubscribeCallback = sinon.stub();
                    unsubscribeStreamId = 'unknownStreamId';

                    onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                    expect(unsubscribeCallback.callCount).to.equal(0);
                  });

                  it('should call removeSubscriber when succeded', () => {
                    const unsubscribeCallback = sinon.stub();

                    onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                    mocks.roomControllerInstance.removeSubscriber.firstCall.callArgWith(2, true);

                    expect(unsubscribeCallback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should call removeSubscriber when succeded (audio only streams) ',
                    () => {
                      const unsubscribeCallback = sinon.stub();
                      const returnFalse = sinon.stub().returns(false);
                      client.room.streamManager
                        .getPublishedStreamById(unsubscribeStreamId).hasVideo = returnFalse;
                      client.room.streamManager
                        .getPublishedStreamById(unsubscribeStreamId).hasScreen = returnFalse;

                      onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                      expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(1);
                      mocks.roomControllerInstance.removeSubscriber
                        .firstCall.callArgWith(2, true);
                      expect(unsubscribeCallback.withArgs(true).callCount).to.equal(1);
                    });

                  it('should call removeSubscriber when succeded (video only streams) ',
                    () => {
                      const unsubscribeCallback = sinon.stub();
                      const returnFalse = sinon.stub().returns(false);
                      client.room.streamManager
                        .getPublishedStreamById(unsubscribeStreamId).hasAudio = returnFalse;
                      client.room.streamManager
                        .getPublishedStreamById(unsubscribeStreamId).hasScreen = returnFalse;

                      onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                      expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(1);
                      mocks.roomControllerInstance.removeSubscriber
                        .firstCall.callArgWith(2, true);
                      expect(unsubscribeCallback.withArgs(true).callCount).to.equal(1);
                    });

                  it('should call removeSubscriber when succeded (screen only streams) ',
                    () => {
                      const unsubscribeCallback = sinon.stub();
                      const returnFalse = sinon.stub().returns(false);
                      client.room.streamManager
                        .getPublishedStreamById(unsubscribeStreamId).hasAudio = returnFalse;
                      client.room.streamManager
                        .getPublishedStreamById(unsubscribeStreamId).hasVideo = returnFalse;

                      onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                      expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(1);
                      mocks.roomControllerInstance.removeSubscriber
                        .firstCall.callArgWith(2, true);
                      expect(unsubscribeCallback.withArgs(true).callCount).to.equal(1);
                    });

                  it('should not call removePublisher in p2p rooms', () => {
                    const unsubscribeCallback = sinon.stub();

                    room.p2p = true;

                    onUnsubscribe(unsubscribeStreamId, unsubscribeCallback);

                    expect(mocks.roomControllerInstance.removeSubscriber.callCount).to.equal(0);
                    expect(unsubscribeCallback.withArgs(true).callCount).to.equal(1);
                  });
                });

                describe('on Unpublish', () => {
                  let stream;
                  let unpublishStreamId;
                  beforeEach(() => {
                    room.streamManager.forEachPublishedStream((aStream) => {
                      stream = aStream;
                      unpublishStreamId = stream.getID();
                    });
                    stream.dataSubscribers = [client.id];
                    room.controller = mocks.roomControllerInstance;
                    client.user.permissions = {};
                    client.user.permissions[Permission.PUBLISH] = true;
                    client.user.permissions[Permission.SUBSCRIBE] = true;
                  });

                  it('should fail if user is undefined', () => {
                    const unpublishCallback = sinon.stub();

                    client.user = undefined;

                    onUnpublish(unpublishStreamId, unpublishCallback);

                    expect(unpublishCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should fail if user is not authorized', () => {
                    const unpublishCallback = sinon.stub();

                    client.user.permissions[Permission.PUBLISH] = false;

                    onUnpublish(streamId, unpublishCallback);

                    expect(unpublishCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
                  });

                  it('should do nothing if stream does not exist', () => {
                    const unpublishCallback = sinon.stub();
                    streamId = 'unknownStreamId';

                    onUnpublish(streamId, unpublishCallback);

                    expect(unpublishCallback.callCount).to.equal(0);
                  });

                  it('should send a onRemoveStream event when succeded', () => {
                    const unpublishCallback = sinon.stub();

                    onUnpublish(streamId, unpublishCallback);

                    expect(mocks.roomControllerInstance.removePublisher
                      .withArgs(client.id, streamId)
                      .callCount).to.equal(1);
                    mocks.roomControllerInstance.removePublisher
                      .withArgs(client.id, streamId).callArgWith(2, true);
                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                      .to.equal(1);
                    expect(unpublishCallback.withArgs(true).callCount).to.equal(1);
                  });

                  it('should send a onRemoveStream event when succeded (audio only streams) ',
                    () => {
                      const unpublishCallback = sinon.stub();
                      const returnFalse = sinon.stub().returns(false);
                      client.room.streamManager
                        .getPublishedStreamById(streamId).hasVideo = returnFalse;
                      client.room.streamManager
                        .getPublishedStreamById(streamId).hasScreen = returnFalse;

                      onUnpublish(streamId, unpublishCallback);

                      expect(mocks.roomControllerInstance
                        .removePublisher.withArgs(client.id, streamId)
                        .callCount).to.equal(1);
                      mocks.roomControllerInstance.removePublisher
                        .withArgs(client.id, streamId).callArgWith(2, true);
                      expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                        .to.equal(1);
                      expect(unpublishCallback.withArgs(true).callCount).to.equal(1);
                    });

                  it('should send a onRemoveStream event when succeded (video only streams) ',
                    () => {
                      const unpublishCallback = sinon.stub();
                      const returnFalse = sinon.stub().returns(false);
                      client.room.streamManager
                        .getPublishedStreamById(streamId).hasAudio = returnFalse;
                      client.room.streamManager
                        .getPublishedStreamById(streamId).hasScreen = returnFalse;

                      onUnpublish(streamId, unpublishCallback);

                      expect(mocks.roomControllerInstance.removePublisher
                        .withArgs(client.id, streamId)
                        .callCount).to.equal(1);
                      mocks.roomControllerInstance.removePublisher
                        .withArgs(client.id, streamId).callArgWith(2, true);
                      expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                        .to.equal(1);
                      expect(unpublishCallback.withArgs(true).callCount).to.equal(1);
                    });

                  it('should send a onRemoveStream event when succeded (screen only streams) ',
                    () => {
                      const unpublishCallback = sinon.stub();
                      const returnFalse = sinon.stub().returns(false);
                      client.room.streamManager
                        .getPublishedStreamById(streamId).hasAudio = returnFalse;
                      client.room.streamManager
                        .getPublishedStreamById(streamId).hasVideo = returnFalse;

                      onUnpublish(streamId, unpublishCallback);

                      expect(mocks.roomControllerInstance.removePublisher
                        .withArgs(client.id, streamId)
                        .callCount).to.equal(1);
                      mocks.roomControllerInstance.removePublisher
                        .withArgs(client.id, streamId).callArgWith(2, true);
                      expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                        .to.equal(1);
                      expect(unpublishCallback.withArgs(true).callCount).to.equal(1);
                    });

                  it('should not call removePublisher in p2p rooms', () => {
                    const unpublishCallback = sinon.stub();

                    room.p2p = true;

                    onUnpublish(streamId, unpublishCallback);

                    expect(mocks.roomControllerInstance.removePublisher
                      .withArgs(streamId).callCount).to.equal(0);
                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                      .to.equal(1);
                    expect(unpublishCallback.withArgs(true).callCount).to.equal(1);
                  });
                });

                describe('on Disconnect', () => {
                  beforeEach(() => {
                  });

                  it('should send a onRemoveStream event', () => {
                    onDisconnect();

                    expect(mocks.socketInstance.emit.withArgs('onRemoveStream').callCount)
                      .to.equal(1);
                  });

                  it('should call removePublisher', () => {
                    onDisconnect();

                    expect(mocks.roomControllerInstance.removePublisher.callCount).to.equal(1);
                  });

                  it('should not call removePublisher if room is p2p', () => {
                    room.p2p = true;

                    onDisconnect();

                    expect(mocks.roomControllerInstance.removePublisher.callCount).to.equal(0);
                  });
                });
              });
            });
          });

          describe('on Start Recorder', () => {
            let subscriberOptions;
            let streamId;
            let client;

            beforeEach(() => {
              room.forEachClient((aClient) => {
                client = aClient;
              });
              client.user.permissions[Permission.PUBLISH] = true;
              client.user.permissions[Permission.RECORD] = true;

              const options = { audio: true, video: true, data: true };
              const sdp = '';
              const startRecorderCallback = sinon.stub();

              onPublish(options, sdp, startRecorderCallback);

              streamId = startRecorderCallback.args[0][0];

              subscriberOptions = {
                to: streamId,
              };
            });

            it('should fail if user is undefined', () => {
              const startRecorderCallback = sinon.stub();
              client.user = undefined;

              onStartRecorder(subscriberOptions, startRecorderCallback);

              expect(startRecorderCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should fail if user is not authorized', () => {
              const startRecorderCallback = sinon.stub();
              client.user.permissions[Permission.RECORD] = false;

              onStartRecorder(subscriberOptions, startRecorderCallback);

              expect(startRecorderCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
            });

            it('should return an error if the room is p2p', () => {
              const startRecorderCallback = sinon.stub();
              room.p2p = true;

              onStartRecorder(subscriberOptions, startRecorderCallback);

              expect(startRecorderCallback.withArgs(null, 'Stream can not be recorded').callCount).to.equal(1);
            });

            it('should return an error if the stream has no media', () => {
              const startRecorderCallback = sinon.stub();
              const returnFalse = sinon.stub().returns(false);
              client.room.streamManager.getPublishedStreamById(streamId).hasVideo = returnFalse;
              client.room.streamManager.getPublishedStreamById(streamId).hasAudio = returnFalse;
              client.room.streamManager.getPublishedStreamById(streamId).hasScreen = returnFalse;

              onStartRecorder(subscriberOptions, startRecorderCallback);

              expect(startRecorderCallback.withArgs(null, 'Stream can not be recorded').callCount).to.equal(1);
            });

            describe('when receiving events from RoomController', () => {
              const startRecorderCallback = sinon.stub();

              it('should call addExternalOutput', () => {
                onStartRecorder(subscriberOptions, startRecorderCallback);

                expect(mocks.roomControllerInstance.addExternalOutput.callCount).to.equal(1);
              });

              it('should return ok if can create external output', () => {
                mocks.roomControllerInstance.addExternalOutput.callsArgWith(3);

                onStartRecorder(subscriberOptions, startRecorderCallback);

                expect(startRecorderCallback.callCount).to.equal(1);
              });

              it('should return an error if published stream does not exist', () => {
                subscriberOptions.to = 'notAStream';
                mocks.roomControllerInstance.addExternalOutput.callsArgWith(3);

                onStartRecorder(subscriberOptions, startRecorderCallback);

                expect(startRecorderCallback.withArgs(null, 'Unable to subscribe to stream for recording, ' +
                               'publisher not present').callCount).to.equal(1);
              });
            });

            describe('on Stop Recorder', () => {
              const stopRecorderSubscriberOptions = {};
              let publisherId;

              beforeEach(() => {
                room.controller = mocks.roomControllerInstance;
                room.forEachClient((aClient) => {
                  client = aClient;
                });
                client.user.permissions[Permission.PUBLISH] = true;
                client.user.permissions[Permission.RECORD] = true;

                const options = { audio: true, video: true, data: true };
                const sdp = '';
                const startPublishCallback = sinon.stub();

                onPublish(options, sdp, startPublishCallback);

                streamId = startPublishCallback.args[0][0];

                subscriberOptions = {
                  to: streamId,
                };

                const startRecorderCallback = sinon.stub();
                mocks.roomControllerInstance.addExternalOutput.callsArgWith(3);
                onStartRecorder(subscriberOptions, startRecorderCallback);
                publisherId = startRecorderCallback.args[0];
              });

              it('should fail if user is undefined', () => {
                const stopRecorderCallback = sinon.stub();
                client.user = undefined;

                onStopRecorder(stopRecorderSubscriberOptions, stopRecorderCallback);

                expect(stopRecorderCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
              });

              it('should fail if user is not authorized', () => {
                const stopRecorderCallback = sinon.stub();
                client.user.permissions[Permission.RECORD] = false;

                onStopRecorder(stopRecorderSubscriberOptions, stopRecorderCallback);

                expect(stopRecorderCallback.withArgs(null, 'Unauthorized').callCount).to.equal(1);
              });

              it('should not call roomController if authorized and no publisher exists', () => {
                const stopRecorderCallback = sinon.stub();

                onStopRecorder(stopRecorderSubscriberOptions, stopRecorderCallback);

                expect(mocks.roomControllerInstance.removeExternalOutput.callCount).to.equal(0);
              });
              it('should call roomController if authorized and publisher exists', () => {
                const stopRecorderCallback = sinon.stub();

                stopRecorderSubscriberOptions.id = publisherId;
                onStopRecorder(stopRecorderSubscriberOptions, stopRecorderCallback);

                expect(mocks.roomControllerInstance.removeExternalOutput.callCount).to.equal(1);
              });
            });
          });
        });
      });
    });
  });
});
