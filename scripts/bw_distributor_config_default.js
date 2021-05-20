const bwDistributorConfig = {};
bwDistributorConfig.defaultType = 'TargetVideoBW'; // BwDistributor to use when no strategy is defined for a client. Options: 'MaxVideoBW', 'TargetVideoBW'
bwDistributorConfig.strategyDefinitions = {
  high: {
    strategyDescription: 'High Layers available',
    priorities: {
      default: ['SL0', 'SL1', 'SL2'],
      0: ['SL0', 'SL1', 'SL2'],
      10: ['SL0', 'SL1', 'SL2'],
      20: ['slideshow', 'SL0', 'SL1', 'SL2'],
      30: ['SL0', 'SL1', 'SL2'],
      40: ['SL0', 'SL1', 'SL2'],
    },
    strategy: ['40_0', '30_0', '20_0', '10_0', '0_0', 'default_0', // Base levels
      '40_1', '40_2',
      '30_1', '30_2',
      '20_1', '20_2', '20_3',
      '10_1', '10_2',
      '0_1', '0_2', 'default_1'],
  },
  low: {
    strategyDescription: 'Save bandwidth by limiting the amount of layers',
    priorities: {
      default: ['fallback', 'SL0'],
      0: ['fallback', 'SL0'],
      10: ['fallback', 'SL0'],
      20: ['slideshow', 'SL0'],
      30: ['fallback', 'SL0'],
      40: ['fallback', 'SL0', 'SL1', 'SL2'],
    },
    strategy: ['40_0', '30_0', '20_0', '10_0', '0_0', 'default_0', // Base levels
      '40_1',
      '30_1',
      '20_1',
      '10_1',
      '40_2', '40_3',
      '0_1', 'default_1'],
  },
};

var module = module || {};
module.exports = bwDistributorConfig;
