/*
 * View class represents a HTML component
 * Every view is an EventDispatcher.
 */

import { EventDispatcher } from '../Events';

const View = () => {
  const that = EventDispatcher({});

  // Variables

  // URL where it will look for icons and assets
  that.url = '';
  return that;
};

export default View;
