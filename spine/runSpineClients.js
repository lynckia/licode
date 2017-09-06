const fs = require('fs');
const Getopt = require('node-getopt'); // eslint-disable-line
const Spine = require('./Spine.js');

const getopt = new Getopt([
  ['s', 'stream-config=ARG', 'file containing the stream config JSON (default is spineClientsConfig.json)'],
  ['t', 'interval=ARG', 'interval time to show stats (default 10 seconds)'],
  ['i', 'total-intervals=ARG', 'total test intervals (default 10 intervals)'],
  ['o', 'output=ARG', 'output file for the stat digest (default is testResult.json)'],
  ['h', 'help', 'display this help'],
]);
const opt = getopt.parse(process.argv.slice(2));

let streamConfig = 'spineClientsConfig.json';
let statsIntervalTime = 10000;
let totalStatsIntervals = 10;
let statOutputFile = 'testResult.json';
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
    case 'interval':
      statsIntervalTime = value * 1000;
      break;
    case 'total-intervals':
      totalStatsIntervals = value;
      break;
    case 'output':
      statOutputFile = value;
      break;
    default:
      console.log('Default');
      break;
  }
});

console.log('Loading stream config file', streamConfig);
streamConfig = require(`./${streamConfig}`); // eslint-disable-line

const relevantStats = streamConfig.stats;
const SpineClient = Spine.buildSpine(streamConfig);

/*
JSON stats format
[
  [
    {'ssrc': {statkey:value ...} ...},
  ...
  ]
...
]
*/
const filterStats = (statsJson) => {
  const results = {};
  relevantStats.forEach((targetStat) => {
    results[targetStat] = [];
    statsJson.forEach((clientStatEntry) => {
      clientStatEntry.forEach((streamStatEntry) => {
        const extractedStats = [];
        const componentStatKeys = Object.keys(streamStatEntry);
        componentStatKeys.forEach((componentStatKey) => {
          const componentStats = streamStatEntry[componentStatKey];
          const statKeys = Object.keys(componentStats);
          statKeys.forEach((statKey) => {
            if (statKey === targetStat) {
              extractedStats.push(componentStats[targetStat]);
            }
          });
        });
        results[targetStat].push(...extractedStats);
      });
    });
  });
  return results;
};

const gatherStatuses = () => {
  const statuses = SpineClient.getAllStreamStatuses();
  const statusResult = {};
  statusResult.up = 0;
  statusResult.down = 0;
  statuses.forEach((clientStatus) => {
    clientStatus.forEach((streamStatus) => {
      if (streamStatus === 'connected') {
        statusResult.up += 1;
      } else {
        statusResult.down += 1;
      }
    });
  });
  return statusResult;
};

const writeJsonToFile = (result) => {
  fs.writeFileSync(statOutputFile, JSON.stringify(result));
};

SpineClient.run();

const statInterval = setInterval(() => {
  SpineClient.getAllStreamStats().then((result) => {
    const statsResult = filterStats(result);
    const statusResult = gatherStatuses();
    const allResults = {};
    allResults.stats = statsResult;
    allResults.alive = statusResult;
    totalStatsIntervals -= 1;
    console.log('Intervals to end:', totalStatsIntervals);
    console.log(allResults);
    writeJsonToFile(allResults);
    if (totalStatsIntervals === 0) {
      clearInterval(statInterval);
      writeJsonToFile(allResults);
      process.exit(0);
    }
  }).catch((reason) => {
    console.log('Stat gather failed:', reason);
  });
}, statsIntervalTime);
