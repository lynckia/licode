const bwDistributorConfig = {};
bwDistributorConfig.defaultType = 'TargetVideoBW'; // Options: 'StreamPriority', 'MaxVideoBW', 'TargetVideoBW'
bwDistributorConfig.strategyDefinitions = {
  high: {
    strategyDescription: 'Strict Stream Prioritization with Budgeted Base Layer',
    priorities: {
      default: ['fallback', 'SL0', 'SL1', 'SL2'],
      0: ['fallback', 'SL0', 'SL1', 'SL2'],
      10: ['fallback', 'SL0', 'SL1', 'SL2'],
      20: ['slideshow', 'SL0', 'SL1', 'SL2'],
      30: ['fallback', 'SL0', 'SL1', 'SL2'],
      40: ['fallback', 'SL0', 'SL1', 'SL2'],
    },
    strategy: ['40_0', '30_0', '20_0', '10_0', '0_0', 'default_0', // Base levels
      '40_1', '40_2',
      '30_1', '30_2',
      '20_1', '20_2',
      '10_1', '10_2',
      '0_1', '0_2', '0_3', 'default_1'],
  },
  low: {
    strategyDescription: 'Strict Stream Prioritization with Budgeted Base Layer',
    priorities: {
      default: ['fallback', 'SL0'],
      0: ['fallback', 'SL0'],
      10: ['fallback', 'SL0', 'SL1', 'SL2'],
      20: ['slideshow', 'SL0', 'SL1', 'SL2'],
      30: ['fallback', 'SL0', 'SL1', 'SL2'],
      40: ['fallback', 'SL0', 'SL1', 'SL2'],
    },
    strategy: ['40_0', '30_0', '20_0', '10_0', '0_0', 'default_0', // Base levels
      '40_1',
      '30_1',
      '20_1',
      '10_1',
      '0_1', 'default_1'],
  },
};

var module = module || {};
module.exports = bwDistributorConfig;
