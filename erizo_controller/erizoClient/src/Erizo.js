import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';

//require('../lib/adapter.js');
require("expose-loader?adapter!../lib/adapter.js");
require("script-loader!./utils/L.Resizer.js");


const Erizo = {
  Room: Room.bind(null, undefined, undefined),
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  Stream: Stream.bind(null, undefined),
  Logger,
};

export default Erizo;
