import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent, ConnectionEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';

// eslint-disable-next-line
require('expose-loader?adapter!../lib/adapter.js');

const Erizo = {
  Room: Room.bind(null, undefined, undefined, undefined),
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  ConnectionEvent,
  Stream: Stream.bind(null, undefined),
  Logger,
};

export default Erizo;
