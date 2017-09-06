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
  'connectionCreationInterval': 2000,
  'duration': 4 * 60,
  'stats': [
    'packetsLost',
    'bitrateCalculated'
  ]
};

describe('Licode Performance', function() {
  this.timeout(60 * 60 * 1000);
  this.retries(4);

  let factory, test, testId = 0;

  getLicodeInstance = () => factory.instances[0];
  getSpineInstanceFromClass = (classId) => factory.instances[(classId % TOTAL_SPINE_SERVERS) + 1];

  before(() => {
    log('Starting instances');
    let settings = {
      numberOfInstances: 1 + TOTAL_SPINE_SERVERS,
      imageId: 'ami-ebfce892',
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

  const runTestWith = (numRooms, spinesPerRoom, numPubs, numSubs) => {
    const promises = [];
    const getNumParticipants = (spineNum, participants) => {
      const numParticipantsPerEachSpine = Math.floor(participants / spinesPerRoom);
      const numSpinesWithAdditionalParticipant = participants % spinesPerRoom;
      let numParticipantsInSpine = numParticipantsPerEachSpine;
      if (spineNum < numSpinesWithAdditionalParticipant) {
        numParticipantsInSpine += 1;
      }
      return numParticipantsInSpine;
    }

    for (let classId = 0; classId < numRooms; classId += 1) {
      for (let spineId = 0; spineId < spinesPerRoom; spineId += 1) {
        let numPubsInSpine = getNumParticipants(spineId, numPubs);
        let numSubsInSpine = getNumParticipants(spineId, numSubs);
        const totalStreamsPerSpine = numPubsInSpine + numPubs * numSubsInSpine;
        const totalStats = totalStreamsPerSpine * 3;

        let custom_test = Object.assign({}, test);
        custom_test.id = classId;
        custom_test.testId += '_' + spineId;
        custom_test.basicExampleUrl += '?room=' + custom_test.id;
        custom_test.numPublishers = numPubsInSpine;
        custom_test.numSubscribers = numSubsInSpine;

        promises.push(getSpineInstanceFromClass(classId).runTest(custom_test).then((result) => {
          should.exist(result);
          should.exist(result.stats);
          should.exist(result.alive);
          should.exist(result.stats.bitrateCalculated);
          should.exist(result.alive.up);

          log(result.alive);

          result.alive.up.should.be.above(Math.floor(totalStreamsPerSpine * 0.9));
          result.alive.down.should.be.below(Math.ceil(totalStreamsPerSpine * 0.1));
          result.stats.bitrateCalculated.length.should.be.above(Math.floor(totalStats * 0.9));

          for (let index = 2; index < result.stats.bitrateCalculated.length; index += 3) {
            result.stats.bitrateCalculated[index].should.be.above(MAX_VIDEO_BITRATE * 0.9);
          }
        }));
      }
    }

    return Promise.all(promises);
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

    it(' 5 rooms with 20 publishers and  19 subscribers', () => runTestWith( 5,  1, 20,  19));
    it(' 1 rooms with  1 publishers and 300 subscribers', () => runTestWith( 1,  2,  1, 300));
    //it(' 5 rooms with 40 publishers and  4 subscribers', () => getLicodeInstance().wait(20 * 60 * 1000));

    afterEach(() => {
      return getLicodeInstance().stopLicode();
    });
  });

  after(() => {
    return factory.terminate();
  });
});
