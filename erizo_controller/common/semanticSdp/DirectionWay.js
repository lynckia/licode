const Enum = require('./Enum');

const DirectionWay = Enum('SEND', 'RECV');

DirectionWay.byValue = direction => DirectionWay[direction.toUpperCase()];

DirectionWay.toString = (direction) => {
  switch (direction) {
    case DirectionWay.SEND:
      return 'send';
    case DirectionWay.RECV:
      return 'recv';
    default:
      return 'unknown';
  }
};

DirectionWay.reverse = (direction) => {
  switch (direction) {
    case DirectionWay.SEND:
      return DirectionWay.RECV;
    case DirectionWay.RECV:
      return DirectionWay.SEND;
    default:
      return DirectionWay.SEND;
  }
};

module.exports = DirectionWay;
