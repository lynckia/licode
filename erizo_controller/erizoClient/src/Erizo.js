import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';

// #if process.env.INCLUDE_ADAPTER === 'TRUE'
// eslint-disable-next-line
import adapter from 'webrtc-adapter';
// #endif

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
