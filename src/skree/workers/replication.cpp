#include "replication.hpp"

namespace Skree {
    namespace Workers {
        void Replication::run() {
            known_peers_t::const_iterator _peer;
            Skree::Client* peer;
            uint64_t now;

            while(true) {
                now = std::time(nullptr);

                for(auto& _event : server.known_events) {
                    // fprintf(stderr, "replication: before read\n");
                    auto event = _event.second;
                    auto queue = event->r_queue;
                    auto item = queue->read();

                    if(item == NULL) {
                        // fprintf(stderr, "replication: empty queue\n");
                        continue;
                    }

                    size_t item_pos = 0;

                    uint32_t rin_len;
                    memcpy(&rin_len, item + item_pos, sizeof(rin_len));
                    item_pos += sizeof(rin_len);
                    rin_len = ntohl(rin_len);

                    // char rin [rin_len]; // TODO: malloc?
                    // memcpy(rin, item + item_pos, rin_len);
                    char* rin = item + item_pos;
                    item_pos += rin_len;

                    uint64_t rts;
                    memcpy(&rts, item + item_pos, sizeof(rts));
                    item_pos += sizeof(rts);
                    rts = ntohll(rts);

                    uint64_t rid_net;
                    memcpy(&rid_net, item + item_pos, sizeof(rid_net));
                    item_pos += sizeof(rid_net);

                    uint64_t rid = ntohll(rid_net);

                    // uint64_t wrinseq;
                    // memcpy(&wrinseq, item + item_pos, sizeof(wrinseq));
                    // item_pos += sizeof(wrinseq);
                    // wrinseq = ntohll(wrinseq);

                    uint32_t hostname_len;
                    memcpy(&hostname_len, item + item_pos, sizeof(hostname_len));
                    item_pos += sizeof(hostname_len);
                    hostname_len = ntohl(hostname_len);

                    char* hostname = item + item_pos;
                    item_pos += hostname_len;

                    uint32_t port;
                    memcpy(&port, item + item_pos, sizeof(port));
                    item_pos += sizeof(port);
                    port = htonl(port);

                    uint32_t peers_cnt;
                    memcpy(&peers_cnt, item + item_pos, sizeof(peers_cnt));
                    item_pos += sizeof(peers_cnt);
                    peers_cnt = ntohl(peers_cnt);

                    char* rpr = item + item_pos;

                    char* peer_id = Utils::make_peer_id(hostname_len, hostname, port);
                    uint32_t peer_id_len = strlen(peer_id);

                    // printf("repl thread: %s\n", event->id);

                    // TODO: overflow
                    if((rts + event->ttl) > now) {
                        fprintf(stderr, "skip repl: not now, rts: %llu, now: %llu\n", rts, now);
                        // free(rin);
                        // free(rpr);
                        free(item);
                        queue->sync_read_offset(false);
                        continue;
                    }

                    size_t suffix_len =
                        event->id_len
                        + 1 // :
                        + peer_id_len
                    ;

                    // TODO
                    char* failover_key = (char*)malloc(
                        suffix_len
                        + 1 // :
                        + 20 // wrinseq
                        + 1 // \0
                    );
                    sprintf(failover_key, "%s:%s", event->id, peer_id);
                    // printf("repl thread: %s\n", suffix);

                    failover_key[suffix_len] = ':';
                    ++suffix_len;

                    sprintf(failover_key + suffix_len, "%llu", rid);
                    // suffix_len += 20;

                    suffix_len = strlen(failover_key);

                    {
                        auto it = server.failover.find(failover_key);

                        if(it != server.failover.end()) {
                            // TODO: what should really happen here?
                            fprintf(stderr, "skip repl: failover flag is set\n");
                            // free(rin);
                            // free(rpr);
                            free(item);
                            // free(suffix);
                            free(failover_key);
                            queue->sync_read_offset(false);
                            continue;
                        }
                    }

                    {
                        auto it = server.no_failover.find(failover_key);

                        if(it != server.no_failover.end()) {
                            if((it->second + server.no_failover_time) > now) {
                                // TODO: what should really happen here?
                                fprintf(stderr, "skip repl: no_failover flag is set\n");
                                // free(rin);
                                // free(rpr);
                                free(item);
                                // free(suffix);
                                free(failover_key);
                                queue->sync_read_offset(false);
                                continue;

                            } else {
                                server.no_failover.erase(it);
                            }
                        }
                    }

                    // TODO: mark task as being processed before
                    //       sync_read_offset() call so it won't be lost
                    queue->sync_read_offset();
                    fprintf(stderr, "replication: after sync_read_offset(), rid: %llu\n", rid);

                    server.failover[failover_key] = 0;

                    pthread_mutex_lock(&(server.known_peers_mutex));

                    _peer = server.known_peers.find(peer_id);

                    if(_peer == server.known_peers.cend()) peer = NULL;
                    else peer = _peer->second;

                    pthread_mutex_unlock(&(server.known_peers_mutex));

                    fprintf(stderr, "Seems like I need to failover task %llu\n", rid);

                    if(peer == NULL) {
                        size_t offset = 0;
                        bool have_rpr = false;

                        uint32_t* count_replicas = (uint32_t*)malloc(sizeof(
                            *count_replicas));
                        uint32_t* acceptances = (uint32_t*)malloc(sizeof(
                            *acceptances));
                        uint32_t* pending = (uint32_t*)malloc(sizeof(
                            *pending));

                        *count_replicas = 0;
                        *acceptances = 0;
                        *pending = 0;

                        pthread_mutex_t* mutex = (pthread_mutex_t*)malloc(sizeof(*mutex));
                        pthread_mutex_init(mutex, NULL);

                        auto data_str = new Utils::muh_str_t {
                            .len = rin_len,
                            .data = rin
                        };

                        auto __peer_id = new Utils::muh_str_t {
                            .len = peer_id_len,
                            .data = peer_id
                        };

                        if(peers_cnt > 0) {
                            *count_replicas = peers_cnt;

                            auto i_req = Skree::Actions::I::out_init(__peer_id, event, rid_net);

                            have_rpr = true;
                            size_t peer_id_len;
                            char* peer_id;
                            auto _peers_cnt = peers_cnt; // TODO

                            while(peers_cnt > 0) {
                                peer_id_len = strlen(rpr + offset); // TODO: get rid of this shit
                                peer_id = rpr + offset;
                                offset += peer_id_len + 1;

                                auto it = server.known_peers.find(peer_id);

                                if(it == server.known_peers.end()) {
                                    pthread_mutex_lock(mutex);
                                    ++(*acceptances);
                                    pthread_mutex_unlock(mutex);

                                } else {
                                    pthread_mutex_lock(mutex);
                                    ++(*pending);
                                    pthread_mutex_unlock(mutex);

                                    auto ctx = new out_packet_i_ctx {
                                        .count_replicas = count_replicas,
                                        .pending = pending,
                                        .acceptances = acceptances,
                                        .mutex = mutex,
                                        .event = event,
                                        .data = data_str,
                                        .peer_id = __peer_id,
                                        .failover_key = failover_key,
                                        .failover_key_len = suffix_len,
                                        .rpr = rpr,
                                        .peers_cnt = _peers_cnt, // TODO
                                        .rid = rid
                                    };

                                    const auto cb = new Skree::PendingReads::Callbacks::ReplicationProposeSelf(server);
                                    const auto item = new Skree::Base::PendingRead::QueueItem {
                                        .len = 1,
                                        .cb = cb,
                                        .ctx = (void*)ctx,
                                        .opcode = true,
                                        .noop = false
                                    };

                                    auto witem = new Skree::Base::PendingWrite::QueueItem {
                                        .len = i_req->len,
                                        .data = i_req->data,
                                        .pos = 0,
                                        .cb = item
                                    };

                                    it->second->push_write_queue(witem);
                                }

                                --peers_cnt;
                            }
                        }

                        if(!have_rpr) {
                            auto ctx = new out_packet_i_ctx {
                                .count_replicas = count_replicas,
                                .pending = pending,
                                .acceptances = acceptances,
                                .mutex = mutex,
                                .event = event,
                                .data = data_str,
                                .peer_id = __peer_id,
                                .failover_key = failover_key,
                                .failover_key_len = suffix_len,
                                .rpr = rpr, // TODO: why is it not NULL here?
                                .peers_cnt = 0,
                                .rid = rid
                            };

                            server.replication_exec(ctx);
                        }

                    } else {
                        // TODO: rin_str's type
                        auto rin_str = new Utils::muh_str_t {
                            .len = rin_len,
                            .data = rin
                        };

                        Utils::muh_str_t* rpr_str = NULL;

                        if(peers_cnt > 0) {
                            rpr_str = new Utils::muh_str_t {
                                .len = (uint32_t)strlen(rpr), // TODO: it is incorrect
                                .data = rpr
                            };
                        }

                        auto ctx = new out_data_c_ctx {
                            .event = event,
                            .rin = rin_str,
                            .rpr = rpr_str, // TODO
                            .rid = rid,
                            .failover_key = failover_key,
                            .failover_key_len = suffix_len
                        };

                        const auto cb = new Skree::PendingReads::Callbacks::ReplicationPingTask(server);
                        const auto item = new Skree::Base::PendingRead::QueueItem {
                            .len = 1,
                            .cb = cb,
                            .ctx = (void*)ctx,
                            .opcode = true,
                            .noop = false
                        };

                        auto c_req = Skree::Actions::C::out_init(event, rid_net, rin_len, rin);

                        auto witem = new Skree::Base::PendingWrite::QueueItem {
                            .len = c_req->len,
                            .data = c_req->data,
                            .pos = 0,
                            .cb = item
                        };

                        peer->push_write_queue(witem);
                    }
                }
            }
        }
    }
}
