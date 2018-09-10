#include "RtxPacketTranslator.h"
#include "MediaStream.h"
#include "RtpHeaders.h"
#include <iostream>

namespace erizo
{
DEFINE_LOGGER(RtxPacketTranslator, "rtp.RtxPacketTranslator");

void RtxPacketTranslator::read(Context *ctx, std::shared_ptr<DataPacket> packet)
{
    auto *rtcp = reinterpret_cast<RtcpHeader *>(packet->data);
    if (!rtcp->isRtcp())
    {
        auto *rtp = reinterpret_cast<RtpHeader *>(packet->data);
        auto ssrc = rtp->getSSRC();
        auto item = std::find_if(rtx_map.begin(), rtx_map.end(), [ssrc](std::pair<uint32_t, uint32_t> pair) {
            return pair.second == ssrc;
        });
        if (item != rtx_map.end())
        {
            rtp->setSSRC(item->first);

            // https://tools.ietf.org/html/rfc4588#section-4
            int header_len = 0;
            char *data = packet->data;
            data += sizeof(RtpHeader) - 4;
            header_len += sizeof(RtpHeader) - 4;
            if (rtp->getExtension())
            {
                data += (rtp->getExtLength() + 1) * 4;
                header_len += (rtp->getExtLength() + 1) * 4;
            }
            header_len += 2; // OSN SIZE
            uint16_t original_seqnum;
            memcpy(&original_seqnum, data, 2);
            original_seqnum = ntohs(original_seqnum);

            rtp->setSeqNumber(original_seqnum);
            rtp->setPayloadType(96);

            memmove(data, data + 2, packet->length - header_len);

            std::cout << "RECEIVED RTX -  ORIGINAL SSRC: " << ssrc << " TRANSLATED TO: " << rtp->getSSRC() << " PT: " << (int)rtp->getPayloadType() << "\n";
        } else {
            std::cout << "PT: " << (int)rtp->getPayloadType() << "\n";
        }
    }

    ctx->fireRead(std::move(packet));
}

void RtxPacketTranslator::setRtxMap(const std::map<uint32_t, uint32_t> &input_rtx_map)
{
    rtx_map = input_rtx_map;
    for (auto elemento : rtx_map)
    {
        std::cout << "SSRC: " << elemento.first << " RTX CHANNEL: " << elemento.second << std::endl;
    }
}
} // namespace erizo
