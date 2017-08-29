const InstanceFactory = require('./utils/remote-spine').Factory;
const chai = require('chai');
const should = chai.should();

const TOTAL_SPINE_SERVERS = 1;

const AUDIO_BITRATE = 30000;
const MIN_VIDEO_BITRATE = 10000;
const MAX_VIDEO_BITRATE = 300000;

const VERBOSE = process.env.TEST_VERBOSE || false;
const log = (...args) => VERBOSE && console.log.apply(null, args);

const BASIC_TEST = {
  'basicExampleUrl': 'https://localhost/',
  'publishConfig' : {
    'video': { 'synthetic': {
             'audioBitrate': AUDIO_BITRATE,
             'minVideoBitrate': MIN_VIDEO_BITRATE,
             'maxVideoBitrate': MAX_VIDEO_BITRATE}},
    'audio':true,
    'data':true
  },
  'subscribeConfig' : {
    'video': true,
    'audio': true,
    'data': true
  },
  'numPublishers': 1,
  'numSubscribers': 1,
  'publishersAreSubscribers': false,
  'connectionCreationInterval': 500,
  'duration': 60,
  'stats': [
    'packetsLost',
    'bitrateCalculated'
  ]
};

describe('Licode Performance', function() {
  this.timeout(60 * 60 * 1000);
  let factory, test, testId = 0;

  getLicodeInstance = () => factory.instances[0];
  getSpineInstanceFromClass = (classId) => factory.instances[(classId % TOTAL_SPINE_SERVERS) + 1];

  before(() => {
    let settings = {
      numberOfInstances: 1 + TOTAL_SPINE_SERVERS,
      imageId: 'ami-2ff3e756',
      username: 'ec2-user',
      instanceType: process.env.PERF_INSTANCE_TYPE || 't1.micro',
      keyName: process.env.PERF_KEY_NAME || 'staging',
      privateKey: process.env.PERF_PRIVATE_KEY || '~/.ssh/id_rsa',
      dockerTag: process.env.PERF_DOCKER_TAG || 'develop',
      region: process.env.PERF_REGION || 'us-west-2',
      securityGroup: process.env.PERF_SECURITY_GROUP || 'Licode'
    };
    factory = new InstanceFactory(settings);
    return factory.run();
  });

  before(() => {
    log('Starting tests');
    log('Licode: ssh -i', process.env.PERF_PRIVATE_KEY,
                'ec2-user@' + getLicodeInstance().host);
    for (let index = 0; index < TOTAL_SPINE_SERVERS; index += 1) {
      log('Spine' + index + ': ssh -i',  process.env.PERF_PRIVATE_KEY,
                  'ec2-user@' + getSpineInstanceFromClass(index).host);
    }
  });

  const runTestWith = (numClasses, numPubs, numSubs) => {
    const promises = [];
    const totalStreams = numPubs * (numPubs - 1 + numSubs);
    const totalStats = totalStreams * 3;

    for (let classId = 0; classId < numClasses; classId += 1) {
      let custom_test = Object.assign({}, test);
      custom_test.id = classId;
      custom_test.basicExampleUrl += '?room=' + custom_test.id;
      custom_test.numPublishers = numPubs;
      custom_test.numSubscribers = numSubs;

      promises.push(getSpineInstanceFromClass(classId).runTest(custom_test));
    }

    return Promise.all(promises).then((results) => {
      results.forEach((result) => {
        log(result);

        should.exist(result);
        should.exist(result.stats);
        should.exist(result.stats.alive);
        should.exist(result.stats.bitrateCalculated);

        result.stats.alive.up.should.equal(numPubs + numSubs);
        result.stats.bitrateCalculated.should.have.lengthOf(totalStats);

        for (let index = 2; index < totalStats; index += 3) {
          result.stats.bitrateCalculated[index].should.be.above(MAX_VIDEO_BITRATE * 0.9);
        }
      });
    });
  }

  beforeEach(function() {
    log('Starting:', this.currentTest.title);
  });

  afterEach(function() {
    log('Finished:', this.currentTest.title, '[' + this.currentTest.state + ']');
    testId += 1;
  });

  describe('Default Licode Configuration', function() {
    beforeEach(function() {
      test = Object.assign({}, BASIC_TEST);
      test.basicExampleUrl = 'https://' + getLicodeInstance().host + ':3004';
      test.testId = testId;
      return getLicodeInstance().runLicode();
    });

    it('1 class   with 1 participants, 1 viewers', () => runTestWith(1, 1, 1));
    it('2 classes with 1 participants, 2 viewers', () => runTestWith(2, 1, 2));
    it('wait', (done) => {
      setTimeout(done, 10 * 60 * 1000);
    });

    afterEach(() => {
      return getLicodeInstance().stopLicode();
    });
  });

  after(() => {
    return factory.terminate();
  });
});
