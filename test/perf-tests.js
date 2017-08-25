const InstanceFactory = require('./utils/remote-spine').Factory;

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
      dockerTag: process.env.PERF_DOCKER_TAG || 'staging',
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
    console.log('ssh -i', process.env.PERF_PRIVATE_KEY, 'ec2-user@' + getLicodeInstance().host);
    setTimeout(() => {
      console.log('Finished');
      done();
    }, 20 * 60 * 1000);
  });

  afterEach(() => {
    return getLicodeInstance().stopLicode();
  });

  after(() => {
    return factory.terminate();
  });
});
