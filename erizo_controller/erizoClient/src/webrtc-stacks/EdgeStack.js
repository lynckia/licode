import Logger from '../utils/Logger';
import BaseStack from './BaseStack';

const EdgeStack = (specInput) => {
  Logger.info('Starting Edge stack');
  const spec = specInput;

  // Setting bundle policy
  spec.bundlePolicy = 'max-bundle';

  const that = BaseStack(spec);
  return that;
};

export default EdgeStack;
