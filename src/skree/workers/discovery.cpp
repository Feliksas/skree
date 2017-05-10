#include "discovery.hpp"
#include "../meta.hpp"

#include <algorithm>
#include <errno.h>

namespace Skree {
    namespace Workers {
        void Discovery::run() {
            while(true) {
                auto& peers_to_discover = server.peers_to_discover;
                peers_to_discover.lock();
                auto peers_to_discover_copy = peers_to_discover;
                peers_to_discover.unlock();

                for(auto& it : peers_to_discover_copy) {
                    auto peer_to_discover = it.second;

                    std::shared_ptr<sockaddr_in> addr;
                    socklen_t addr_len;
                    int fh;

                    if(!do_connect(
                        peer_to_discover->host->data,
                        peer_to_discover->port,
                        addr,
                        addr_len,
                    fh)) {
                        continue;
                    }

                    auto conn_name = Utils::get_host_from_sockaddr_in(addr);
                    uint32_t conn_port = Utils::get_port_from_sockaddr_in(addr);
                    auto conn_id = Utils::make_peer_id(conn_name->len, conn_name->data, conn_port);

                    // free(conn_name);
                    bool found = false;

                    {
                        auto& known_peers_by_conn_id = server.known_peers_by_conn_id;
                        auto known_peers_by_conn_id_end = known_peers_by_conn_id.lock();
                        auto it = known_peers_by_conn_id.find(conn_id);
                        known_peers_by_conn_id.unlock();

                        found = (it != known_peers_by_conn_id_end);

                        // if(!found)
                        //     Utils::cluck(2, "no %s in known_peers_by_conn_id", conn_id);
                    }

                    if(!found) {
                        auto& known_peers = server.known_peers;
                        auto known_peers_end = known_peers.lock();
                        auto it = known_peers.find(conn_id);
                        known_peers.unlock();

                        found = (it != known_peers_end);

                        // if(!found)
                        //     Utils::cluck(2, "no %s in known_peers", conn_id);
                    }

                    if(!found) {
                        auto& me = server.me;
                        auto me_end = me.lock();
                        auto it = me.find(conn_id);
                        me.unlock();

                        found = (it != me_end);

                        // if(!found)
                        //     Utils::cluck(2, "no %s in me", conn_id);
                    }

                    // free(conn_id);

                    if(found) {
                        shutdown(fh, SHUT_RDWR);
                        close(fh);
                        // free(addr);
                        continue;
                    }

                    std::shared_ptr<new_client_t> new_client;
                    new_client.reset(new new_client_t {
                        .fh = fh,
                        .cb = [this](Skree::Client& client) {
                            on_new_client(client);
                            cb1(client);
                        },
                        .s_in = addr
                    });

                    server.push_new_client(new_client);
                }

                sleep(5);
            }
        }

        bool Discovery::do_connect(
            const char* host,
            uint32_t peer_port,
            std::shared_ptr<sockaddr_in>& addr,
            socklen_t& addr_len,
            int& fh
        ) {
            addrinfo hints;
            addrinfo* service_info;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_NUMERICSERV;

            char port[6];
            sprintf(port, "%d", peer_port);
            int rv;

            if((rv = getaddrinfo(host, port, &hints, &service_info)) != 0) {
                Utils::cluck(4, "getaddrinfo(%s, %u): %s\n", host, peer_port, gai_strerror(rv));
                return false;
            }

            addr.reset(new sockaddr_in);
            bool connected = false;

            for(addrinfo* ai_it = service_info; ai_it != nullptr; ai_it = ai_it->ai_next) {
                if((fh = socket(ai_it->ai_family, ai_it->ai_socktype, ai_it->ai_protocol)) == -1) {
                    perror("socket");
                    continue;
                }

                // TODO: discovery should be async too
                if(!Utils::SetupSocket(fh, server.discovery_timeout_milliseconds)) {
                    close(fh);
                    continue;
                }

                if(connect(fh, ai_it->ai_addr, ai_it->ai_addrlen) == -1) {
                    if(errno != EINPROGRESS) {
                        std::string str ("connect(");
                        str += host;
                        str += ":";
                        str += port;
                        str += ")";

                        perror(str.c_str());

                        close(fh);
                        continue;
                    }

                    fcntl(fh, F_SETFL, fcntl(fh, F_GETFL, 0) | O_NONBLOCK);
                }

                *addr = *(sockaddr_in*)(ai_it->ai_addr);
                addr_len = ai_it->ai_addrlen;
                connected = true;

                break;
            }

            freeaddrinfo(service_info);

            if(connected && (getpeername(fh, (sockaddr*)addr.get(), &addr_len) == -1)) {
                perror("getpeername");
                close(fh);
                // free(addr);
                connected = false;
            }

            return connected;
        }

        void Discovery::cb1(Skree::Client& client) {
            auto _cb = [this](
                Skree::Client& client,
                const Skree::Base::PendingRead::QueueItem& item,
                std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args
            ) {
                return cb2(client, item, args);
            };

            auto w_req = Skree::Actions::W::out_init();

            w_req->set_cb(std::make_shared<Skree::Base::PendingRead::QueueItem>(
                std::shared_ptr<void>(),
                std::make_shared<Skree::PendingReads::Callbacks::Discovery<decltype(_cb)>>(server, _cb)
            ));

            w_req->finish();

            client.push_write_queue(w_req);
        }

        std::shared_ptr<Skree::Base::PendingWrite::QueueItem> Discovery::cb6(
            Skree::Client& client,
            const Skree::Base::PendingRead::QueueItem& item,
            std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args
        ) {
            // for(int i = 0; i < item.len; ++i)
            //     fprintf(stderr, "[discovery::cb6] read from %s [%d]: 0x%.2X\n", client.get_peer_id(),i,args->data[i]);

            if(args->opcode == SKREE_META_OPCODE_K) {
                uint64_t in_pos = 0;

                uint32_t cnt;
                memcpy(&cnt, (args->data + in_pos), sizeof(cnt));
                in_pos += sizeof(cnt);
                cnt = ntohl(cnt);

                bool got_new_peers = false;
                auto& peers_to_discover = server.peers_to_discover;

                while(cnt > 0) {
                    --cnt;

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

                    auto _peer_id = Utils::make_peer_id(host_len, host, port);

                    auto peers_to_discover_end = peers_to_discover.lock();
                    auto prev_item = peers_to_discover.find(_peer_id);

                    if(prev_item == peers_to_discover_end) {
                        // Utils::cluck("[discovery] fill peers_to_discover: %s:%u\n", host, port);
                        std::shared_ptr<Utils::muh_str_t> _host(
                            new Utils::muh_str_t(strndup(host, host_len), host_len, true)
                        );

                        peers_to_discover[_peer_id].reset(new peer_to_discover_t {
                            .host = _host,
                            .port = port
                        });

                        got_new_peers = true;

                    // } else {
                    //     free(_peer_id);
                        // free(host);
                    }

                    peers_to_discover.unlock();
                }

                if(got_new_peers)
                    server.save_peers_to_discover();
            }

            return std::shared_ptr<Skree::Base::PendingWrite::QueueItem>();
        }

        std::shared_ptr<Skree::Base::PendingWrite::QueueItem> Discovery::cb5(
            Skree::Client& client,
            const Skree::Base::PendingRead::QueueItem& item,
            std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args
        ) {
            // Utils::cluck(2, "DISCOVERY CB5 OPCODE: %c\n", args->opcode);
            if(args->opcode == SKREE_META_OPCODE_K) {
                auto& known_peers = server.known_peers;
                auto& known_peers_by_conn_id = server.known_peers_by_conn_id;
                auto known_peers_end = known_peers.lock();
                known_peers_by_conn_id.lock();

                // peer_id is guaranteed to be set here
                const auto& peer_id = client.get_peer_id();
                const auto& conn_id = client.get_conn_id();
                auto known_peer = known_peers.find(peer_id);

                if(known_peer != known_peers_end) {
                    auto& list = known_peer->second;

                    for(const auto& peer : list) {
                        if(strcmp(conn_id->data, peer->get_conn_id()->data) == 0) {
                            args->stop = true;
                            break;
                        }
                    }
                }

                if(!args->stop) {
                    known_peers[peer_id].push_back(&client);
                    known_peers_by_conn_id[conn_id].push_back(&client);
                }

                known_peers_by_conn_id.unlock();
                known_peers.unlock();

                if(!args->stop) {
                    int max_parallel_connections(client.get_max_parallel_connections() - 1);
                    const auto& peer_name = client.get_peer_name();
                    const auto& peer_port = client.get_peer_port();

                    for(int i = 0; i < max_parallel_connections; ++i) {
                        std::shared_ptr<sockaddr_in> addr;
                        socklen_t addr_len;
                        int fh;

                        if(!do_connect(
                            peer_name->data,
                            peer_port,
                            addr,
                            addr_len,
                            fh
                        )) {
                            break;
                        }

                        std::shared_ptr<new_client_t> new_client;
                        new_client.reset(new new_client_t {
                            .fh = fh,
                            .cb = [
                                this,
                                peer_name,
                                peer_port,
                                peer_id
                            ](Skree::Client& client) {
                                on_new_client(client);
                                client.set_peer_name(peer_name);
                                client.set_peer_port(peer_port);
                                client.set_peer_id(peer_id);

                                auto _cb = [this, peer_id](
                                    Skree::Client& client,
                                    const Skree::Base::PendingRead::QueueItem& item,
                                    std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args
                                ) {
                                    if(args->opcode != SKREE_META_OPCODE_K)
                                        return std::shared_ptr<Skree::Base::PendingWrite::QueueItem>();

                                    auto& known_peers = server.known_peers;
                                    auto& known_peers_by_conn_id = server.known_peers_by_conn_id;

                                    auto known_peers_end = known_peers.lock();
                                    known_peers_by_conn_id.lock();

                                    auto conn_id = client.get_conn_id();

                                    known_peers[peer_id].push_back(&client);
                                    known_peers_by_conn_id[conn_id].push_back(&client);

                                    known_peers_by_conn_id.unlock();
                                    known_peers.unlock();

                                    return std::shared_ptr<Skree::Base::PendingWrite::QueueItem>();
                                };

                                auto h_req = Skree::Actions::H::out_init(server);

                                h_req->set_cb(std::make_shared<Skree::Base::PendingRead::QueueItem>(
                                    std::shared_ptr<void>(),
                                    std::make_shared<Skree::PendingReads::Callbacks::Discovery<decltype(_cb)>>(server, _cb)
                                ));

                                h_req->finish();

                                client.push_write_queue(h_req);
                            },
                            .s_in = addr
                        });

                        server.push_new_client(new_client);
                    }

                    auto _cb = [this](
                        Skree::Client& client,
                        const Skree::Base::PendingRead::QueueItem& item,
                        std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args
                    ) {
                        return cb6(client, item, args);
                    };

                    auto l_req = Skree::Actions::L::out_init();

                    l_req->set_cb(std::make_shared<Skree::Base::PendingRead::QueueItem>(
                        std::shared_ptr<void>(),
                        std::make_shared<Skree::PendingReads::Callbacks::Discovery<decltype(_cb)>>(server, _cb)
                    ));

                    l_req->finish();

                    return l_req;
                }

            } else {
                args->stop = true;
            }

            return std::shared_ptr<Skree::Base::PendingWrite::QueueItem>();
        }

        std::shared_ptr<Skree::Base::PendingWrite::QueueItem> Discovery::cb2(
            Skree::Client& client,
            const Skree::Base::PendingRead::QueueItem& item,
            std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args
        ) {
            // Utils::cluck(2, "DISCOVERY CB2 OPCODE: %c\n", args->opcode);
            if(args->opcode == SKREE_META_OPCODE_K) {
                uint64_t in_pos = 0;

                uint32_t len;
                memcpy(&len, args->data + in_pos, sizeof(uint32_t));
                in_pos += sizeof(len);
                len = ntohl(len);

                const char* peer_name = args->data + in_pos;
                in_pos += len + 1;

                uint32_t _tmp;
                memcpy(&_tmp, args->data + in_pos, sizeof(uint32_t));
                in_pos += sizeof(_tmp);
                _tmp = ntohl(_tmp);

                client.set_max_parallel_connections(std::min(
                    server.get_max_parallel_connections(),
                    _tmp
                ));

                _tmp = client.get_conn_port();
                auto _peer_id = Utils::make_peer_id(len, peer_name, _tmp);
                bool accepted = false;

                auto& known_peers = server.known_peers;
                auto known_peers_end = known_peers.lock();
                auto known_peer = known_peers.find(_peer_id);
                known_peers.unlock();

                if(known_peer == known_peers_end) {
                    if(strcmp(_peer_id->data, server.my_peer_id->data) == 0) {
                        auto& me = server.me;
                        auto me_end = me.lock();
                        auto it = me.find(_peer_id);

                        if(it == me_end) {
                            server.me[_peer_id] = true;
                            server.me[client.get_conn_id()] = true;

                        // } else {
                        //     free(_peer_id);
                        }

                        me.unlock();

                        // free(peer_name);

                    } else {
                        std::shared_ptr<Utils::muh_str_t> _peer_name(
                            new Utils::muh_str_t(strndup(peer_name, len), len, true)
                        );

                        client.set_peer_name(_peer_name);
                        client.set_peer_port(_tmp);
                        client.set_peer_id(_peer_id);

                        accepted = true;
                    }

                // } else {
                //     free(_peer_id);
                    // free(peer_name);
                }

                if(accepted) {
                    auto _cb = [this](
                        Skree::Client& client,
                        const Skree::Base::PendingRead::QueueItem& item,
                        std::shared_ptr<Skree::Base::PendingRead::Callback::Args> args
                    ) {
                        return cb5(client, item, args);
                    };

                    auto h_req = Skree::Actions::H::out_init(server);

                    h_req->set_cb(std::make_shared<Skree::Base::PendingRead::QueueItem>(
                        std::shared_ptr<void>(),
                        std::make_shared<Skree::PendingReads::Callbacks::Discovery<decltype(_cb)>>(server, _cb)
                    ));

                    h_req->finish();

                    return h_req;

                } else {
                    args->stop = true;
                }

            } else {
                args->stop = true;
            }

            return std::shared_ptr<Skree::Base::PendingWrite::QueueItem>();
        }

        void Discovery::on_new_client(Skree::Client& client) { // TODO: also in Server::socket_cb
            client.push_write_queue(Skree::Actions::N::out_init(), true);
        }
    }
}
