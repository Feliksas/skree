#include "h.hpp"
#include "../client.hpp"

namespace Skree {
    namespace Actions {
        void H::in(std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args) {
            uint64_t in_pos = 0;

            uint32_t host_len;
            memcpy(&host_len, (args->data + in_pos), sizeof(host_len));
            in_pos += sizeof(host_len);
            host_len = ntohl(host_len);

            const char* host = args->data + in_pos;
            in_pos += host_len + 1;

            uint32_t port;
            memcpy(&port, (args->data + in_pos), sizeof(port));
            in_pos += sizeof(port);
            port = ntohl(port);

            // TODO: (char*)(char[len])
            auto _peer_id = Utils::make_peer_id(host_len, host, port);
            const auto conn_id = client.get_conn_id();

            auto& known_peers = server.known_peers;
            auto& known_peers_by_conn_id = server.known_peers_by_conn_id;
            auto end = known_peers.lock();
            auto known_peer = known_peers.find(_peer_id);
            known_peers_by_conn_id.lock();

            if(known_peer == end) {
                args->out.reset(new Skree::Base::PendingWrite::QueueItem (SKREE_META_OPCODE_K));

            } else {
                bool found = false;
                const auto& list = known_peers[_peer_id];

                for(const auto& client : list) {
                    if(strcmp(conn_id->data, client->get_conn_id()->data) == 0) {
                        found = true;
                        break;
                    }
                }

                if(found) {
                    // free(_peer_id);
                    // free(host);
                    args->out.reset(new Skree::Base::PendingWrite::QueueItem (SKREE_META_OPCODE_F));

                } else {
                    args->out.reset(new Skree::Base::PendingWrite::QueueItem (SKREE_META_OPCODE_K));
                }
            }

            if(args->out->get_opcode() == SKREE_META_OPCODE_K) {
                std::shared_ptr<Utils::muh_str_t> _peer_name(
                    new Utils::muh_str_t(strndup(host, host_len), host_len, true)
                );

                client.set_peer_name(_peer_name);
                client.set_peer_port(port);
                client.set_peer_id(_peer_id);

                known_peers[_peer_id].push_back(&client);
                known_peers_by_conn_id[conn_id].push_back(&client);
            }

            known_peers_by_conn_id.unlock();
            known_peers.unlock();
        }

        std::shared_ptr<Skree::Base::PendingWrite::QueueItem> H::out_init(const Server& server) {
            auto out = std::make_shared<Skree::Base::PendingWrite::QueueItem>(opcode());

            uint32_t _hostname_len = htonl(server.my_hostname_len);
            out->copy_concat(sizeof(_hostname_len), &_hostname_len);

            out->concat(server.my_hostname_len + 1, server.my_hostname);

            uint32_t _my_port = htonl(server.my_port);
            out->copy_concat(sizeof(_my_port), &_my_port);

            return out;
        }
    }
}
