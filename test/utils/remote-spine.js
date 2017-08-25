const SSH = require('node-ssh');
const AWS = require('aws-sdk');

const SPINE_WRITE_CONFIG_FILE_COMMAND = 'echo DATA >> config.json';
const SPINE_DOCKER_PULL_COMMAND = 'sudo docker pull lynckia/licode:TAG';
const SPINE_RUN_COMMAND = 'sudo docker image ls';
const SPINE_GET_RESULT_COMMAND = 'ls';

const LICODE_RUN_COMMAND = 'sudo docker image ls';
const LICODE_STOP_COMMAND= 'sudo docker image ls';

const EC2_API_VERSION = '2016-11-15';
const SSH_RETRY_TIMEOUT = 5000;
const SSH_MAX_RETRIES = 10;

const EC2_INSTANCE_LIFETIME = 50; // minutes


class RemoteInstance {
  constructor(host, username, privateKey, dockerTag) {
    this.ssh = new SSH();
    this.host = host;
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
    return this.ssh.execCommand(...args)
      .then(data => {
        if (data.code === 0) {
          return data.stdout;
        } else {
          throw Error(data.stderr);
        }
      });
  }

  setup() {
    return this._execCommand('sudo yum update -y')
      .then(() => this._execCommand('sudo yum install -y docker'))
      .then(() => this._execCommand('sudo service docker start'))
      .then(() => this._execCommand(SPINE_DOCKER_PULL_COMMAND.replace(/TAG/, this.dockerTag)));
  }

  runLicode() {
    return this._execCommand(LICODE_RUN_COMMAND);
  }

  stopLicode() {
    return this._execCommand(LICODE_STOP_COMMAND)
  }

  runTest(settings) {
    const settingsText = JSON.stringify(settings);
    return this._execCommand(SPINE_WRITE_CONFIG_FILE_COMMAND.replace(/DATA/, `'${settingsText}'`))
      .then(() => this._execCommand(SPINE_RUN_COMMAND))
      .then(() => this.getResult());
  }

  getResult() {
    const promise = this._execCommand(SPINE_GET_RESULT_COMMAND);
    promise.then((data) => {
      this.result = JSON.parse(data);
    });
    return promise;
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
        InstanceType: this.InstanceType,
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
      this.ec2.runInstances(this.params, (err, data) => {
        if (err) {
          reject(err);
          return;
        }
        this.data = data;
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
          const spine = new RemoteInstance(host, this.username, this.privateKey, this.dockerTag);
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
      this.data.Instances.forEach((instance) => {
        params.InstanceIds.push(instance.InstanceId);
      });
      this.ec2.terminateInstances(params, (err, data) => {
        if (err) {
          reject(err);
          return;
        }
        resolve();
      });
    });
  }
}

exports.Factory = Factory;
