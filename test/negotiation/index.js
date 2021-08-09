const describeNegotiationTest = require('./utils/NegotiationTest');
const describeStreamSwitchTest = require('./utils/StreamSwitchTest');

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

describeStreamSwitchTest('Basic Stream Switch Test', async function(ctx) {
  let publisherA, publisherB, subscriberA, subscriberB, subscriberC;
  before(async function() {
    publisherA = await ctx.createClientStream('streamA', undefined, 400);
    publisherB = await ctx.createClientStream('streamB', "#ff0000", 1000);
    publisherB = await ctx.createClientStream('streamC', "#0000ff", undefined);

    subscriberA = await ctx.createErizoStream('erizoStreamA', publisherA.label, true, false);
    subscriberB = await ctx.createErizoStream('erizoStreamB', publisherB.label, true, true);
    subscriberC = await ctx.createErizoStream('erizoStreamB', publisherB.label, false, true);
  });

  ctx.publishStream('pub', 'pub', 'streamA');
  ctx.publishStream('pub', 'pub', 'streamB');
  ctx.publishStream('pub', 'pub', 'streamC');
  ctx.subscribeToStream('sub', 'sub', 'erizoStreamA', 'pub');

  for (let i = 0; i < 20; i++) {
    // ctx.linkSubToPub('sub', 'pub', 'sub', 'streamA', 'erizoStreamA');
    // ctx.unlinkSubToPub('sub', 'pub', 'sub', 'streamA', 'erizoStreamA');
    ctx.linkSubToPub('sub', 'pub', 'sub', 'streamB', 'erizoStreamA');
    ctx.unlinkSubToPub('sub', 'pub', 'sub', 'streamB', 'erizoStreamA');
    ctx.linkSubToPub('sub', 'pub', 'sub', 'streamC', 'erizoStreamA');
    ctx.unlinkSubToPub('sub', 'pub', 'sub', 'streamC', 'erizoStreamA');
    ctx.linkSubToPub('sub', 'pub', 'sub', 'streamB', 'erizoStreamA');
    ctx.unlinkSubToPub('sub', 'pub', 'sub', 'streamB', 'erizoStreamA');
  }
}, true);