import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';

// eslint-disable-next-line 
require('expose-loader?adapter!../lib/adapter.js');
// eslint-disable-next-line
require('script-loader!./utils/L.Resizer.js');


const Erizo = {
  Room: Room.bind(null, undefined, undefined, undefined),
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  Stream: Stream.bind(null, undefined),
  Logger,
};

export default Erizo;
