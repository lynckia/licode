const InstanceFactory = require('./utils/remote-spine').Factory;

const BASIC_TEST = {
  'basicExampleUrl': 'https://localhost/',
  'publishConfig' : {
    'video': { 'synthetic': {
             'audioBitrate': 30000,
             'minVideoBitrate': 10000,
             'maxVideoBitrate': 300000}},
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
  'stats': [
    'packetsLost',
    'bitrateCalculated'
  ]
};

describe('Licode Performance', function() {
  this.timeout(60 * 60 * 1000);
  let factory;

  getLicodeInstance = () => factory.instances[0];
  getSpineInstance = () => factory.instances[1];

  before(() => {
    let settings = {
      numberOfInstances: 2,
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

  beforeEach(() => {
    return getLicodeInstance().runLicode();
  });

  it('Test setup', (done) => {
    console.log('Starting test');
    console.log('Licode: ssh -i', process.env.PERF_PRIVATE_KEY, 'ec2-user@' + getLicodeInstance().host);
    console.log('Spine: ssh -i', process.env.PERF_PRIVATE_KEY, 'ec2-user@' + getSpineInstance().host);
    const config = Object.assign({}, BASIC_TEST);
    config.basicExampleUrl = 'https://' + getLicodeInstance().host + ':3004';
    getSpineInstance().runTest(config).then(() => {
      console.log(this.result);
      done();
    }).catch((reason) => {
      done(new Error(reason));
    });
  });

  afterEach(() => {
    return getLicodeInstance().stopLicode();
  });

  after(() => {
    return factory.terminate();
  });
});
