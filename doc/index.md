<h1>Overview</h1>
<section id="architecture" class="row-fluid">
  <figure id="canvas" class="illustration">

    <a href="#server" class="server" data-toggle="tab">
      <span class="label">Server</span>
      <span class="icon"></span>
    </a>

    <div class="clients">
      <a href="#clients" class="client" data-toggle="tab">
        <span class="label">Client</span>
        <span class="icon"></span>
      </a>

      <a href="#clients" class="client" data-toggle="tab">
        <span class="label">Client</span>
        <span class="icon"></span>
      </a>

      <a href="#clients" class="client" data-toggle="tab">
        <span class="label">Client</span>
        <span class="icon"></span>
      </a>
    </div>

    <div class="routes">
      <div class="routing"></div>
      <div class="routing"></div>
      <div class="routing"></div>
      <div class="routing"></div>
      <div class="routing"></div>
      <div class="routing"></div>
    </div>

    <div class="erizo-controller">

      <a href="#room" class="room" data-toggle="tab">
        <span class="bg"></span>
        <span class="icon"></span>
      </a>
      <a href="#room" class="room" data-toggle="tab">
        <span class="bg"></span>
        <span class="icon"></span>
      </a>
      <a href="#erizo" class="back" data-toggle="tab"></a>

    </div>
    <a href="#nuve" class="nuve" data-toggle="tab"></a>
  </figure>
  <div class="description">
  <div class="tab-content alert alert-info">
    <div class="tab-pane active" id="room">
      <h3>Rooms</h3>
      <p>All users and clients in a room can share their streams through Licode.</p>
      <p>They can act like videoconferencing chats, instant messaging rooms, video streaming sessions, and any other kind of virtual space for real-time collaboration.</p>
      <p>A room is created by server apps through Nuve API, and the users will connect to these rooms through Erizo.</p>
      <p>Rooms are controlled by Erizo Controller, which manages Erizo through a JavaScript wrapper called Erizo API to control streams.</p>
    </div>
    <div class="tab-pane" id="server">
      <h3>Server</h3>
      <p>Developers will manage rooms (creation and deletion) from their server apps.</p>
      <p>Typical server apps can create a room, and request access for their users to Nuve, via a token-based authentication mechanism.</p>
      <p>This mechanism allows these servers to create access tokens, and they will provide these tokens to their clients.</p>
      <p>Server talks to Nuve in order to do these actions.</p>
    </div>
    <div class="tab-pane" id="erizo">
      <h3>Erizo</h3>
      <p>Clients will connect users to rooms through Erizo service. Developers can manage this service with a JavaScript library that runs in the browser.</p>
      <p>Erizo is a cloud-based scalable service that allows multiple users to connect to Licode rooms. These rooms are created by the server apps using Nuve API.</p>
      <p>Erizo Controllerwhich manages Erizo through a JavaScript wrapper called Erizo API to control streams.</p>
    </div>
    <div class="tab-pane" id="nuve">
      <h3>Nuve API</h3>
      <p>Developers can manage Licode rooms by sending requests to this API. These requests are typically sent from server apps, coded in python, node.js, Ruby on Rails, and so on. </p>
      <p>Licode provides different libraries, plug-ins and add-ons to facilitate the tasks of creating and removing rooms.</p>
      <p>Server apps can also ask for access tokens to this API. These tokens are needed for user connections, so developers should pass them to clients.</p>
      <p>Server talks to Nuve in order to do these actions.</p>
    </div>
    <div class="tab-pane" id="clients">
      <h3>Clients</h3>
      <p>Clients are JavaScript applications that run on browsers. Users will be able to access Licode rooms by using these clients.</p>
      <p>When connecting to a room a client needs the access token that is usually taken by server apps.</p>
      <p>But the use of these rooms depends on your imagination! You could implement videoconference rooms, chat, synchronization mechanisms, VoIP applications, and so on.</p>
      <p>Clients talk to Erizo Controller through Erizo client, which is part of this project.</p>
    </div>
  </div>
  </div>
</section>