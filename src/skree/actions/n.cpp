#include "n.hpp"

namespace Skree {
    namespace Actions {
        void N::in(
            const uint64_t in_len, const char* in_data,
            Skree::Base::PendingWrite::QueueItem*& out
        ) {
            client.set_protocol_version(ntohl(*(uint32_t*)in_data));
        }

        Skree::Base::PendingWrite::QueueItem* N::out_init() {
            const uint32_t protocol_version (htonl(PROTOCOL_VERSION));
            auto* out = new Skree::Base::PendingWrite::QueueItem(sizeof(protocol_version), opcode());

            out->push(sizeof(protocol_version), &protocol_version);
            out->finish();

            return out;
        }
    }
}