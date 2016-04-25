#ifndef _SKREE_WORKERS_DISCOVERY_H_
#define _SKREE_WORKERS_DISCOVERY_H_

#include "../base/worker.hpp"
#include "../client.hpp"
#include "../base/pending_read.hpp"
#include "../pending_reads/discovery.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace Skree {
    namespace Workers {
        class Discovery : public Skree::Base::Worker {
        public:
            Discovery(Skree::Server& _server, const void* _args = NULL)
                : Skree::Base::Worker(_server, _args) {}

            virtual void run() override;
            void cb1(Skree::Client& client);

            const Skree::Base::PendingRead::QueueItem* cb2(
                Skree::Client& client,
                const Skree::Base::PendingRead::QueueItem& item,
                const Skree::Base::PendingRead::Callback::Args& args
            );

            const Skree::Base::PendingRead::QueueItem* cb5(
                Skree::Client& client,
                const Skree::Base::PendingRead::QueueItem& item,
                const Skree::Base::PendingRead::Callback::Args& args
            );

            const Skree::Base::PendingRead::QueueItem* cb6(
                Skree::Client& client,
                const Skree::Base::PendingRead::QueueItem& item,
                const Skree::Base::PendingRead::Callback::Args& args
            );
        };
    }
}

#include "../server.hpp" // sorry

#endif