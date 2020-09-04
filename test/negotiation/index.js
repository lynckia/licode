const expect = require('chai').expect;
const describeNegotiationTest = require('./utils/NegotiationTest');
const SdpChecker = require('./utils/SdpUtils');

describeNegotiationTest('SDP negotiations started by client', function(ctx) {
  ctx.publishToErizoStreamStep();
  ctx.subscribeToErizoStreamStep();
  ctx.unpublishStreamStep();
  ctx.publishToErizoStreamStep();
});

describeNegotiationTest('SDP negotiations started by Erizo', function(ctx) {
  ctx.subscribeToErizoStreamStep();
  ctx.publishToErizoStreamStep();
  ctx.unsubscribeStreamStep();
  ctx.subscribeToErizoStreamStep();
});

describeNegotiationTest('Conflicting SDP negotiation started by Erizo and Client', function(ctx) {
  ctx.publishAndSubscribeStreamsStep();
  ctx.publishAndSubscribeStreamsStep();
});
