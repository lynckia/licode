import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';

// Using script-loader to load global variables
require('../lib/socket.io.js');
require('../lib/adapter.js');
require('./utils/L.Logger.js');
require('./utils/L.Resizer.js');

const boundRoom = Room.bind(null, undefined, undefined);
const boundStream = Stream.bind(null, undefined);
const Erizo = {
  Room: boundRoom,
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  Stream: boundStream,
};

export default Erizo;
