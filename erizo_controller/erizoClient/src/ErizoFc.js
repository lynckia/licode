import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';
import Logger from './utils/Logger';
// Using script-loader to load global variables

const Erizo = {
  Room,
  LicodeEvent,
  RoomEvent,
  StreamEvent,
  Stream,
  Logger,
};

export default Erizo;
