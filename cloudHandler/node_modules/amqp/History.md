...
==================
  * Change the default json content type to application/json
  * add a basic-return event for exchanges to catch simple
    basicReturn messages.
  * cleaner and more manageable handling of server channel closes
    from Glen Mailer.

0.1.0 / 2011-07-30
==================

  * BC BREAK: Changed the default exchange name from amq.topic to ''
    (the empty string).  This can be changed by using the new
    implementation options argument (second arg) to createConnection.
    Add { defaultExchangeName: 'amq.topic' } as second arg to
    createConnection for old behavior.  See docs.
  * Added url support to connection credentials. (squaremo)
  * Support for pristine messages and all AMQP properties.
  * Added this history file.
