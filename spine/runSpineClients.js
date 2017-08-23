const Getopt = require('node-getopt'); // eslint-disable-line
const Spine = require('./Spine.js')

const getopt = new Getopt([
  ['s', 'stream-config=ARG', 'file containing the stream config JSON'],
  ['t', 'time=ARG', 'interval time to show stats (default 10 seconds)'],
  ['h', 'help', 'display this help'],
]);
const opt = getopt.parse(process.argv.slice(2));

let streamConfig;
let statsIntervalTime = 10000;
const optionKeys = Object.keys(opt.options);

optionKeys.forEach((key) => {
  const value = opt.options[key];
  switch (key) {
    case 'help':
      getopt.showHelp();
      process.exit(0);
      break;
    case 'stream-config':
      streamConfig = value;
      break;
    case 'time':
      statsIntervalTime = value * 1000;
      break;
    default:
      console.log('Default');
      break;
  }
});

if (!streamConfig) {
  streamConfig = 'spineClientsConfig.json';
}

console.log('Loading stream config file', streamConfig);
streamConfig = require(`./${streamConfig}`); // eslint-disable-line

const SpineClient = Spine.buildSpine(streamConfig);
SpineClient.run();
const statsInterval = setInterval(() => {
  SpineClient.getAllStats().then((result) => {
    console.log('getting stats', result);
  }).catch((reason) => {
    console.log('Reason', reason);
  });
}, statsIntervalTime);
