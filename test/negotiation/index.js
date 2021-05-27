const describeNegotiationTest = require('./utils/NegotiationTest');

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
  ctx.publishAndSubscribeStreamsStep([
    'client-add-stream',
                              'erizo-publish-stream',
                              'erizo-subscribe-stream',
                              'erizo-get-offer',
    'client-get-offer',
                              // Erizo is not polite 'erizo-process-offer',
    'client-process-offer',
    'client-get-answer',
                              'erizo-process-answer',
    'get-and-process-candidates',
    'client-get-offer',
                              'erizo-process-offer',
                              'erizo-get-answer',
    'client-process-answer',
    'wait-for-being-connected',
  ]);
  ctx.publishAndSubscribeStreamsStep([
    'client-add-stream',
                              'erizo-publish-stream',
    'client-get-offer',
                              // Erizo is not polite 'erizo-process-offer',
                              'erizo-subscribe-stream',
                              'erizo-get-offer',
    'client-process-offer',
    'client-get-answer',
                              'erizo-process-answer',
    'client-get-offer',
                              'erizo-process-offer',
                              'erizo-get-answer',
    'client-process-answer',
    'get-and-process-candidates',
    'wait-for-being-connected',
  ]);
});
