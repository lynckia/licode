const Enum = require('./Enum');

const Setup = Enum('ACTIVE', 'PASSIVE', 'ACTPASS', 'INACTIVE');

Setup.byValue = setup => Setup[setup.toUpperCase()];

Setup.toString = (setup) => {
  switch (setup) {
    case Setup.ACTIVE:
      return 'active';
    case Setup.PASSIVE:
      return 'passive';
    case Setup.ACTPASS:
      return 'actpass';
    case Setup.INACTIVE:
      return 'inactive';
    default:
      return null;
  }
};

Setup.reverse = (setup) => {
  switch (setup) {
    case Setup.ACTIVE:
      return Setup.PASSIVE;
    case Setup.PASSIVE:
      return Setup.ACTIVE;
    case Setup.ACTPASS:
      return Setup.PASSIVE;
    case Setup.INACTIVE:
      return Setup.INACTIVE;
    default:
      return Setup.ACTIVE;
  }
};

module.exports = Setup;
