import Room from './Room';
import { LicodeEvent, RoomEvent, StreamEvent } from './Events';
import Stream from './Stream';

// Using script-loader to load global variables
require('../lib/socket.io.js');
require('../lib/adapter.js');
require('./utils/L.Logger.js');
require('./utils/L.Resizer.js');

const Erizo = { Room, LicodeEvent, RoomEvent, StreamEvent, Stream };

export default Erizo;
