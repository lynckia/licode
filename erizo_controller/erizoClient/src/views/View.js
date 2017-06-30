/* global Erizo*/
this.Erizo = this.Erizo || {};

/*
 * View class represents a HTML component
 * Every view is an EventDispatcher.
 */
Erizo.View = () => {
  const that = Erizo.EventDispatcher({});

  // Variables

  // URL where it will look for icons and assets
  that.url = '';
  return that;
};
