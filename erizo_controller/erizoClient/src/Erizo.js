import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';

// #if process.env.INCLUDE_ADAPTER === 'TRUE'
import adapter from 'webrtc-adapter'; // eslint-disable-line
// #endif

require('script-loader!./utils/L.Resizer.js');  // eslint-disable-line


const Erizo = {
  Room: Room.bind(null, undefined, undefined, undefined),
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  Stream: Stream.bind(null, undefined),
  Logger,
};

export default Erizo;
