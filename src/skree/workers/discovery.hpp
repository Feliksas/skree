#pragma once

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
            Discovery(Skree::Server& _server, const void* _args = nullptr)
                : Skree::Base::Worker(_server, _args) {}

            virtual void run() override;
            void cb1(Skree::Client& client);

            Skree::Base::PendingWrite::QueueItem* cb2(
                Skree::Client& client,
                const Skree::Base::PendingRead::QueueItem& item,
                Skree::Base::PendingRead::Callback::Args& args
            );

            Skree::Base::PendingWrite::QueueItem* cb5(
                Skree::Client& client,
                const Skree::Base::PendingRead::QueueItem& item,
                Skree::Base::PendingRead::Callback::Args& args
            );

            Skree::Base::PendingWrite::QueueItem* cb6(
                Skree::Client& client,
                const Skree::Base::PendingRead::QueueItem& item,
                Skree::Base::PendingRead::Callback::Args& args
            );
        };
    }
}

#include "../server.hpp" // sorry

