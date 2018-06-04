import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';

// #if process.env.EXCLUDE_ADAPTER != 'TRUE'
import adapter from 'webrtc-adapter'; // eslint-disable-line
// #endif

require('script-loader!./utils/L.Resizer.js');  // eslint-disable-line


const Erizo = {
  // #if process.env.EXCLUDE_SOCKETIO != 'TRUE'
  Room: Room.bind(null, undefined, undefined, undefined),
  // #endif
  // #if process.env.EXCLUDE_SOCKETIO == 'TRUE'
  Room: Room, // eslint-disable-line
  // #endif
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  Stream: Stream.bind(null, undefined),
  Logger,
};

export default Erizo;
