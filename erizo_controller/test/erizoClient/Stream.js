/* global describe, beforeEach, afterEach, it, expect, Erizo, sinon, navigator */

/* eslint-disable no-unused-expressions */

describe('Stream.init', () => {
  beforeEach(() => {
    Erizo.Logger.setLogLevel(Erizo.Logger.NONE);
    sinon.spy(navigator.mediaDevices, 'getUserMedia');
    sinon.spy(navigator.mediaDevices, 'getDisplayMedia');
  });

  afterEach(() => {
    navigator.mediaDevices.getUserMedia.restore();
    navigator.mediaDevices.getDisplayMedia.restore();
  });

  it('should get access to user media when requesting access to video and audio', async () => {
    const localStream = Erizo.Stream({ audio: true, video: true, data: true });
    const promise = new Promise((resolve) => {
      localStream.on('access-accepted', () => {
        resolve();
      });
    });
    localStream.init();

    await promise;
    expect(localStream).to.have.property('stream');
    expect(navigator.mediaDevices.getUserMedia.called).to.be.true;
    const constraints = navigator.mediaDevices.getUserMedia.firstCall.firstArg;
    expect(constraints.video).not.equals.false;
    expect(constraints.audio).not.equals.false;
    expect(localStream.stream.getVideoTracks().length).to.equals(1);
    expect(localStream.stream.getAudioTracks().length).to.equals(1);
  });

  it('should get access to user media when requesting access to video', async () => {
    const localStream = Erizo.Stream({ audio: false, video: true, data: true });
    const promise = new Promise((resolve) => {
      localStream.on('access-accepted', () => {
        resolve();
      });
    });
    localStream.init();

    await promise;
    expect(localStream).to.have.property('stream');
    expect(navigator.mediaDevices.getUserMedia.called).to.be.true;
    const constraints = navigator.mediaDevices.getUserMedia.firstCall.firstArg;
    expect(constraints.video).not.equals.false;
    expect(constraints.audio).equals.false;
    expect(localStream.stream.getVideoTracks().length).to.equals(1);
    expect(localStream.stream.getAudioTracks().length).to.equals(0);
  });

  it('should get access to user media when requesting access to audio', async () => {
    const localStream = Erizo.Stream({ audio: true, video: false, data: true });
    const promise = new Promise((resolve) => {
      localStream.on('access-accepted', () => {
        resolve();
      });
    });
    localStream.init();

    await promise;
    expect(localStream).to.have.property('stream');
    expect(navigator.mediaDevices.getUserMedia.called).to.be.true;
    const constraints = navigator.mediaDevices.getUserMedia.firstCall.firstArg;
    expect(constraints.video).equals.false;
    expect(constraints.audio).not.equals.false;
    expect(localStream.stream.getVideoTracks().length).to.equals(0);
    expect(localStream.stream.getAudioTracks().length).to.equals(1);
  });

  it('should get access to user media when requesting access to screen sharing', async () => {
    const localStream = Erizo.Stream({ screen: true, data: true });
    const promise = new Promise((resolve) => {
      localStream.on('access-accepted', () => {
        resolve();
      });
    });
    localStream.init();
    await promise;
    expect(localStream).to.have.property('stream');
    expect(navigator.mediaDevices.getDisplayMedia.called).to.be.true;
    const constraints = navigator.mediaDevices.getDisplayMedia.firstCall.firstArg;
    expect(constraints.screen).equals.false;
    expect(constraints.video).equals.false;
    expect(constraints.audio).equals.false;
    expect(localStream.stream.getVideoTracks().length).to.equals(1);
    expect(localStream.stream.getAudioTracks().length).to.equals(0);
  });
});
