#pragma once
namespace Skree {
    namespace Base {
        class Action;
        namespace PendingWrite {
            class QueueItem;
        }
    }
    class Server;
    class Client;
}

// #include "../server.hpp"
// #include "../client.hpp"

// #include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <cstdint>
#include <stdio.h>

namespace Skree {
    namespace Base {
        class Action {
        protected:
            Skree::Server& server;
            Skree::Client& client;
        public:
            Action(
                Skree::Server& _server,
                Skree::Client& _client
            ) : server(_server), client(_client) {}

            static const char opcode();

            virtual void in(
                const uint64_t& in_len, const char*& in_data,
                Skree::Base::PendingWrite::QueueItem*& out
            ) = 0;
        };
    }
}
