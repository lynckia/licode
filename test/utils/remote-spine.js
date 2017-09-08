const SSH = require('node-ssh');
const AWS = require('aws-sdk');

const SPINE_DOCKER_PULL_COMMAND = 'sudo docker pull lynckia/licode:TAG';
const SPINE_RUN_COMMAND =
'sudo docker run --rm --name spine_TEST_PREFIX_TEST_ID --log-driver none --detach --network="host" \
-e TESTPREFIX=TEST_PREFIX -e TESTID=TEST_ID -e DURATION=TEST_DURATION \
-v $(pwd)/licode_default.js:/opt/licode/licode_config.js \
-v $(pwd)/runSpineTest.sh:/opt/licode/test/runSpineTest.sh \
-v $(pwd)/runSpineClients.js:/opt/licode/spine/runSpineClients.js \
-v $(pwd)/simpleNativeConnection.js:/opt/licode/spine/simpleNativeConnection.js \
-v $(pwd)/results/:/opt/licode/results/ --workdir "/opt/licode/" \
--entrypoint "test/runSpineTest.sh" lynckia/licode:TAG';

const SPINE_GET_RESULT_COMMAND = 'cat results/output_PREFIX_TEST_ID.json';

const LICODE_RUN_COMMAND =
'sudo docker run --name licode --log-driver none \
--network="host" \
-e "PUBLIC_IP=INSTANCE_PUBLIC_IP" \
-v $(pwd)/licode_default.js:/opt/licode/scripts/licode_default.js \
--rm --detach lynckia/licode:TAG';

const LICODE_STOP_COMMAND= 'sudo docker stop licode';

const EC2_API_VERSION = '2016-11-15';
const SSH_RETRY_TIMEOUT = 5000;
const SSH_MAX_RETRIES = 20;

const EC2_INSTANCE_LIFETIME = 50; // minutes

const VERBOSE = process.env.TEST_VERBOSE || false;
const log = (...args) => VERBOSE && console.log.apply(null, args);

class RemoteInstance {
  constructor(id, host, ip, username, privateKey, dockerTag) {
    this.id = id;
    this.ssh = new SSH();
    this.host = host;
    this.ip = ip;
    this.username = username;
    this.privateKey = privateKey;
    this.dockerTag = dockerTag;
    this.retries = 0;
  }

  _connect(success = () => {}, fail = () => {}) {
    this.ssh.connect({
      host: this.host,
      username: this.username,
      privateKey: this.privateKey,
    }).then(() => {
      success();
    }).catch(() => {
      this.retries += 1;
      if (this.retries > SSH_MAX_RETRIES) {
        fail('unable to connect');
        return;
      }
      setTimeout(() => {
        this._connect(success, fail);
      }, SSH_RETRY_TIMEOUT);

    });
  }

  connect() {
    return new Promise((resolve, reject) => {
      this._connect(resolve, reject);
    });
  }

  _execCommand(...args) {
    log(this.id, '- Exec Command:', ...args);
    return this.ssh.execCommand(...args)
      .then(data => {
        if (data /* && data.code === 0 */) {
          return data.stdout;
        } else {
          log(this.id, '- Error:', data.stdout, data.stderr);
          throw Error(data.stderr);
        }
      });
  }

  wait(duration) {
    return new Promise((resolve, reject) => {
      setTimeout(() => {
        resolve();
      }, duration);
    });
  }

  setup() {
    log('Setting up instance', this.id);
    return this._execCommand('sudo yum update -y')
      .then(() => this._execCommand('sudo sh -c \'echo "root hard nofile 65536" >> /etc/security/limits.conf\''))
      .then(() => this._execCommand('sudo sh -c \'echo "root soft nofile 65536" >> /etc/security/limits.conf\''))
      .then(() => this._execCommand('sudo sh -c \'echo "* hard nofile 65536" >> /etc/security/limits.conf\''))
      .then(() => this._execCommand('sudo sh -c \'echo "* soft nofile 65536" >> /etc/security/limits.conf\''))
      .then(() => this.disconnect())
      .then(() => this.connect())
      .then(() => this._execCommand('ulimit -n'))
      .then(data => { log(data); return 'ok'; })
      .then(() => this._execCommand('sudo yum install -y docker'))
      .then(() => this._execCommand('sudo service docker start'))
      .then(() => this._execCommand(SPINE_DOCKER_PULL_COMMAND.replace(/TAG/, this.dockerTag)))
      .then(() => this.ssh.mkdir('results'))
      .then(() => this.ssh.putFiles([{local:  'licode_default.js',
                                      remote: 'licode_default.js'},
                                     {local:  'rtp_media_config_default.js',
                                      remote: 'rtp_media_config_default.js'},
                                     {local:  'runSpineTest.sh',
                                      remote: 'runSpineTest.sh'},
                                     {local:  '../spine/runSpineClients.js',
                                      remote: 'runSpineClients.js'},
                                     {local:  '../spine/simpleNativeConnection.js',
                                      remote: 'simpleNativeConnection.js'}]))
      .then(() => this._execCommand('chmod +x runSpineTest.sh'));
  }

  runLicode() {
    log('Running licode');
    return this._execCommand(LICODE_RUN_COMMAND
        .replace(/INSTANCE_PUBLIC_IP/g, this.ip)
        .replace(/TAG/g, this.dockerTag))
      .then(() => this.wait(30 * 1000));
  }

  stopLicode() {
    return this._execCommand(LICODE_STOP_COMMAND);
  }

  runTest(settings) {
    const settingsText = JSON.stringify(settings);
    return this._execCommand('echo ' + JSON.stringify(settingsText) +
                             ' > results/config_' + settings.testId + '_' + settings.id + '.json')
      .then(() => this._execCommand(SPINE_RUN_COMMAND
        .replace(/TEST_ID/g, settings.id)
        .replace(/TEST_PREFIX/g, settings.testId)
        .replace(/TEST_DURATION/g, Math.floor(settings.duration / 10))
        .replace(/TAG/g, this.dockerTag)));
  }

  stopTest(settings) {
    return this._execCommand(SPINE_RUN_COMMAND
        .replace(/TEST_ID/g, settings.id)
        .replace(/TEST_PREFIX/g, settings.testId));
  }

  getResult(settings) {
    const promise = this._execCommand(SPINE_GET_RESULT_COMMAND
      .replace(/TEST_ID/g, settings.id)
      .replace(/PREFIX/g, settings.testId));
    return promise.then((data) => {
      return JSON.parse(data);
    });
  }

  downloadAllResults() {
    let dirname = 'results/';
    let filename = this.id + '_results.tgz';
    return this._execCommand('tar czvf ' + filename +  ' ' + dirname)
      .then(() => this.ssh.getFile(filename, filename));
  }

  disconnect() {
    this.ssh.dispose();
  }
}

// more info: http://docs.aws.amazon.com/AWSJavaScriptSDK/latest/AWS/EC2.html
class Factory {
  constructor(settings) {
    this.ec2 = new AWS.EC2({ apiVersion: EC2_API_VERSION, region: settings.region });
    this.numberOfInstances = settings.numberOfInstances;
    this.imageId = settings.imageId;
    this.instanceType = settings.instanceType;
    this.username = settings.username;
    this.privateKey = settings.privateKey;
    this.keyName = settings.keyName;
    this.dockerTag = settings.dockerTag;
    this.securityGroup = settings.securityGroup;
    this.instances = [];
  }

  run() {
    return new Promise((resolve, reject) => {
      this.params = {
        ImageId: this.imageId,
        MaxCount: this.numberOfInstances,
        MinCount: this.numberOfInstances,
        KeyName: this.keyName,
        Monitoring: {
          Enabled: false,
        },
        SecurityGroups: [ this.securityGroup, ],
        InstanceType: this.instanceType,
        InstanceInitiatedShutdownBehavior: 'terminate',
        UserData: new Buffer(`#!/bin/bash
          sudo shutdown -h +${EC2_INSTANCE_LIFETIME}`).toString('base64'), // automatically terminate instances
        TagSpecifications: [ {
          ResourceType: 'instance',
          Tags: [ {
              Key: 'Name',
              Value: 'Tests - licode-stress-test',
            }, ]
        }, ],
      };
      log('Running instances');
      this.ec2.runInstances(this.params, (err, data) => {
        if (err) {
          reject(err);
          return;
        }
        this.data = data;
        log('Waiting for instances to be running');
        this.waitForRunning()
        .then(() => {
          resolve();
        }).catch((reason) => {
          reject(reason);
        });
      });
    });
  }

  waitForRunning() {
    return new Promise((resolve, reject) => {
      const params = {
        Filters: [{
          Name: 'reservation-id',
          Values: [this.data.ReservationId],
        }],
      };
      this.ec2.waitFor('instanceRunning', params, (err, data) => {
        if (err) {
          reject(err);
          return;
        }
        let promises = [];
        data.Reservations[0].Instances.forEach((instance) => {
          const host = instance.PublicDnsName;
          const ip = instance.PublicIpAddress
          const spine = new RemoteInstance(instance.InstanceId, host, ip, this.username, this.privateKey, this.dockerTag);
          this.instances.push(spine);
          let promise = spine.connect()
                             .then(() => spine.setup());
          promises.push(promise);
        });
        Promise.all(promises)
          .then(() => {
            resolve();
          })
          .catch((reason) => {
            reject(reason);
          });
      });

    });
  }

  terminate() {
    return new Promise((resolve, reject) => {
      const params = {
        InstanceIds: [],
      };
      let downloadJobs = [];
      this.instances.forEach((instance) => {
        downloadJobs.push(instance.downloadAllResults());
        params.InstanceIds.push(instance.id);
      });
      let stopInstances = () => {
        this.ec2.terminateInstances(params, (err, data) => {
          if (err) {
            reject(err);
            return;
          }
          resolve();
        });
      };
      Promise.all(downloadJobs).then(stopInstances).catch(stopInstances);

    });
  }
}

exports.Factory = Factory;
