import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';

// Using script-loader to load global variables
require('../lib/adapter.js');
require('./utils/L.Resizer.js');

const Erizo = {
  Room: Room.bind(null, undefined, undefined),
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  Stream: Stream.bind(null, undefined),
  Logger,
};

export default Erizo;
