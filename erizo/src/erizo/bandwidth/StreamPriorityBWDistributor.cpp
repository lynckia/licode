/*
 * StreamPriorityBWDistributor.cpp
 */

#include <algorithm>

#include "StreamPriorityBWDistributor.h"
#include "MediaStream.h"
#include "rtp/QualityManager.h"
#include "Transport.h"
#include "rtp/RtpUtils.h"

namespace erizo {
DEFINE_LOGGER(StreamPriorityBWDistributor, "bandwidth.StreamPriorityBWDistributor");
  StreamPriorityBWDistributor::StreamPriorityBWDistributor(StreamPriorityStrategy strategy,
  std::shared_ptr<Stats>stats): strategy_{strategy}, stats_{stats} {
  }

std::string StreamPriorityBWDistributor::getStrategyId() {
  return strategy_.getStrategyId();
}

void StreamPriorityBWDistributor::distribute(uint32_t remb, uint32_t ssrc,
                                  std::vector<std::shared_ptr<MediaStream>> streams, Transport *transport) {
  std::map<std::string, std::vector<MediaStreamPriorityInfo>> stream_infos{};
  std::map<std::string, uint64_t> bitrate_for_priority{};
  uint32_t remaining_bitrate = remb;
  for (auto stream : streams) {
    ELOG_DEBUG("Adding stream %s with priority %s", stream->getId().c_str(), stream->getPriority().c_str());
    stream_infos[stream->getPriority()].push_back({stream,
                                                   0});
  }
  ELOG_DEBUG("Starting distribution with bitrate %lu for strategy %s", remaining_bitrate, getStrategyId().c_str());
  strategy_.reset();
  while (strategy_.hasNextStep()) {
    StreamPriorityStep step = strategy_.getNextStep();
    std::string priority = step.priority;
    if (step.isLevelSlideshow()) {
      for (MediaStreamPriorityInfo& stream_info : stream_infos[priority]) {
        ELOG_DEBUG("Setting slideshow below spatial layer 0 for stream %s", stream_info.stream->getId());
        stream_info.stream->enableSlideShowBelowSpatialLayer(true, 0);
        if (stream_info.stream->isSlideShowModeEnabled()) {
          ELOG_DEBUG("stream %s has slideshow using bitrate %u", stream_info.stream->getId().c_str(),
              stream_info.stream->getVideoBitrate());
          remaining_bitrate -= stream_info.stream->getVideoBitrate();
        }
      }
      continue;
    }
    if (step.isLevelFallback()) {
      for (MediaStreamPriorityInfo& stream_info : stream_infos[priority]) {
        ELOG_DEBUG("Setting fallback below spatial layer 0 for stream %s", stream_info.stream->getId());
        stream_info.stream->enableFallbackBelowMinLayer(true);
      }
      continue;
    }
    if (remaining_bitrate == 0) {
      ELOG_DEBUG("No more bitrate to distribute");
      break;
    }

    bool is_max = step.isLevelMax();
    int layer = step.getSpatialLayer();
    ELOG_DEBUG("Step with priority %s, layer %d, is_max %u remaining %lu, bitrate assigned to priority %lu",
        priority.c_str(), layer, is_max, remaining_bitrate, bitrate_for_priority[priority]);
    // bitrate_for_priority is automatically initialized to 0 with the first [] call to the map
    remaining_bitrate += bitrate_for_priority[priority];
    bitrate_for_priority[priority] = 0;
    uint64_t remaining_avg_bitrate = remaining_bitrate;
    if (stream_infos[priority].size() > 0) {
      remaining_avg_bitrate = remaining_bitrate / stream_infos[priority].size();
    }
    for (MediaStreamPriorityInfo& stream_info : stream_infos[priority]) {
      uint64_t needed_bitrate_for_stream = 0;
      if (is_max) {
        stream_info.stream->setTargetIsMaxVideoBW(true);
        needed_bitrate_for_stream = stream_info.stream->getMaxVideoBW();
      } else if (!stream_info.stream->isSimulcast()) {
        ELOG_DEBUG("Stream %s is not simulcast", stream_info.stream->getId());
        int number_of_layers = strategy_.getHighestLayerForPriority(priority) + 1;
        if (number_of_layers > 0) {
          needed_bitrate_for_stream = (stream_info.stream->getMaxVideoBW()/number_of_layers) * (layer + 1);
        }
        ELOG_DEBUG("Non-simulcast stream, number of layers %d, needed_bitrate %lu",
            number_of_layers, needed_bitrate_for_stream);
      } else {
        uint64_t bitrate_for_higher_temporal_in_spatial =
          stream_info.stream->getBitrateForHigherTemporalInSpatialLayer(layer);
        uint64_t max_bitrate_that_meets_constraints = stream_info.stream->getBitrateFromMaxQualityLayer();

        needed_bitrate_for_stream =
          bitrate_for_higher_temporal_in_spatial == 0 ? max_bitrate_that_meets_constraints :
          std::min(bitrate_for_higher_temporal_in_spatial, max_bitrate_that_meets_constraints);
        needed_bitrate_for_stream = needed_bitrate_for_stream * (1 + QualityManager::kIncreaseLayerBitrateThreshold);
      }
      uint64_t bitrate = std::min(needed_bitrate_for_stream, remaining_avg_bitrate);
      uint64_t remb = std::min(static_cast<uint64_t>(stream_info.stream->getMaxVideoBW()), bitrate);
      stream_info.assigned_bitrate = remb;
      bitrate_for_priority[priority] += remb;
      remaining_bitrate -= remb;
      ELOG_DEBUG("needed_bitrate %lu, max_bitrate %u, bitrate %u, streams_in_priority %d",
          needed_bitrate_for_stream,
          stream_info.stream->getMaxVideoBW(),
          bitrate, stream_infos[priority].size());
      ELOG_DEBUG("Assigning bitrate %lu to stream %s, remaining %lu",
          remb, stream_info.stream->getId().c_str(), remaining_bitrate);
    }
  }
  stats_->getNode()["total"].insertStat("unnasignedBitrate", CumulativeStat{remaining_bitrate});
  for (const auto& kv_pair : stream_infos) {
    for (MediaStreamPriorityInfo& stream_info : stream_infos[kv_pair.first]) {
      auto generated_remb = RtpUtils::createREMB(ssrc,
          {stream_info.stream->getVideoSinkSSRC()}, stream_info.assigned_bitrate);
      ELOG_DEBUG("Sending remb with %lu", stream_info.assigned_bitrate);
      stream_info.stream->onTransportData(generated_remb, transport);
    }
  }
}



}  // namespace erizo
