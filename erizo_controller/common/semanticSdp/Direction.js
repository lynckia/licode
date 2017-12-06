const Enum = require('./Enum');

const Direction = Enum('SENDRECV', 'SENDONLY', 'RECVONLY', 'INACTIVE');

Direction.byValue = direction => Direction[direction.toUpperCase()];

/**
* Get Direction name
* @memberOf Direction
* @param {Direction} direction
* @returns {String}
*/
Direction.toString = (direction) => {
  switch (direction) {
    case Direction.SENDRECV:
      return 'sendrecv';
    case Direction.SENDONLY:
      return 'sendonly';
    case Direction.RECVONLY:
      return 'recvonly';
    case Direction.INACTIVE:
      return 'inactive';
    default:
      return 'unknown';
  }
};

Direction.reverse = (direction) => {
  switch (direction) {
    case Direction.SENDRECV:
      return Direction.SENDRECV;
    case Direction.SENDONLY:
      return Direction.RECVONLY;
    case Direction.RECVONLY:
      return Direction.SENDONLY;
    case Direction.INACTIVE:
      return Direction.INACTIVE;
    default:
      return Direction.SENDRECV;
  }
};

module.exports = Direction;
