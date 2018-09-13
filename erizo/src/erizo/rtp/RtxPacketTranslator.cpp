#include "RtxPacketTranslator.h"
#include "RtpHeaders.h"

namespace erizo {
DEFINE_LOGGER(RtxPacketTranslator, "rtp.RtxPacketTranslator");

void RtxPacketTranslator::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
    auto *rtcp = reinterpret_cast<RtcpHeader *>(packet->data);
    if (!rtcp->isRtcp()) {
        auto *rtp = reinterpret_cast<RtpHeader *>(packet->data);
        auto ssrc = rtp->getSSRC();
        auto seq_num = rtp->getSeqNumber();
        auto pt = rtp->getPayloadType();
        auto apt_mapping = apt_translator.find(rtp->getPayloadType());
        if (apt_mapping == apt_translator.end()) {
            ctx->fireRead(std::move(packet));
            return;
        }
        auto fid_mapping = std::find_if(fid_map.begin(), fid_map.end(), [ssrc](std::pair<uint32_t, uint32_t> pair) {
            return pair.second == ssrc;
        });
        if (fid_mapping != fid_map.end()) {
            rtp->setSSRC(fid_mapping->first);
            // https://tools.ietf.org/html/rfc4588#section-4
            int header_len = 0;
            char *data = packet->data;
            data += sizeof(RtpHeader) - 8;
            header_len += sizeof(RtpHeader) - 8;
            if (rtp->getExtension()) {
                data += (rtp->getExtLength() + 1) * 4;
                header_len += (rtp->getExtLength() + 1) * 4;
            }
            header_len += 2;  // OSN SIZE
            uint16_t original_seqnum;
            memcpy(&original_seqnum, data, 2);
            original_seqnum = ntohs(original_seqnum);

            rtp->setSeqNumber(original_seqnum);
            rtp->setPayloadType(apt_mapping->second);

            memmove(data, data + 2, packet->length - header_len);
            packet->length -= 2;
            ELOG_DEBUG("%s Rewriting rtx packet from ssrc: %u to: %u, orig_seqnum: %u to: %u, orig_pt: %u to: %u",
                stream_->toLog(), ssrc, fid_mapping->first, seq_num, original_seqnum, pt, apt_mapping->second);
        }
    }
    ctx->fireRead(std::move(packet));
}

void RtxPacketTranslator::notifyUpdate() {
    if (initialized_) {
        return;
    }
    auto pipeline = getContext()->getPipelineShared();
    if (pipeline && !stream_) {
        stream_ = pipeline->getService<MediaStream>().get();
    }
    if (!stream_) {
        return;
    }
    for (const auto &params : stream_->getRemoteSdpInfo()->payload_parsed_map_) {
        for (const auto &param : params.second.format_parameters) {
            if (param.first == "apt") {
                apt_translator[params.first] = stoi(param.second);
                ELOG_DEBUG("%s Mapping apt %u to %u", stream_->toLog(), params.first, apt_translator[params.first]);
            }
        }
    }
    fid_map = stream_->getRemoteSdpInfo()->fid_ssrc_map[stream_->getLabel()];
    for (auto value : fid_map) {
        ELOG_DEBUG("%s Setting Rtx Mapping for ssrc %u: %u", stream_->toLog(), value.first, value.second);
    }
    initialized_ = true;
}
}  // namespace erizo
