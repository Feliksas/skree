#pragma once
#define SAVE_EVENT_RESULT_F 0
#define SAVE_EVENT_RESULT_A 1
#define SAVE_EVENT_RESULT_K 2
#define SAVE_EVENT_RESULT_NULL 3

#define REPL_SAVE_RESULT_F 0
#define REPL_SAVE_RESULT_K 1

// #include "actions/c.hpp"
// #include "actions/e.hpp"
// #include "actions/h.hpp"
// #include "actions/i.hpp"
// #include "actions/l.hpp"
// #include "actions/r.hpp"
// #include "actions/w.hpp"
// #include "actions/x.hpp"

namespace Skree {
    class Server;
}

#include "utils/atomic_hash_map.hpp"
#include "utils/round_robin_vector.hpp"
#include "client.hpp"
#include "workers/client.hpp"
#include "actions/e.hpp"
#include "actions/r.hpp"
#include "workers/synchronization.hpp"
#include "workers/replication.hpp"
#include "workers/discovery.hpp"
#include "pending_reads/replication.hpp"
#include "queue_db.hpp"
#include "workers/processor.hpp"
#include "meta/states.hpp"
#include "workers/cleanup.hpp"

#include <stdexcept>
#include <functional>
#include <algorithm>
#include <atomic>
#include <utility>

namespace Skree {
    struct new_client_t {
        int fh;
        std::function<void(Client&)> cb;
        std::shared_ptr<sockaddr_in> s_in;
    };

    typedef Utils::AtomicHashMap<uint64_t, uint64_t> wip_t;
    typedef Utils::AtomicHashMap<
        std::shared_ptr<Utils::muh_str_t>,
        Utils::RoundRobinVector<Client*>,
        Utils::TMuhStrPointerHasher,
        Utils::TMuhStrPointerComparator
    > known_peers_t;

    typedef Utils::AtomicHashMap<
        std::shared_ptr<Utils::muh_str_t>,
        bool,
        Utils::TMuhStrPointerHasher,
        Utils::TMuhStrPointerComparator
    > me_t;

    struct peer_to_discover_t {
        std::shared_ptr<Utils::muh_str_t> host;
        uint32_t port;
    };

    typedef Utils::AtomicHashMap<
        std::shared_ptr<Utils::muh_str_t>,
        std::shared_ptr<peer_to_discover_t>,
        Utils::TMuhStrPointerHasher,
        Utils::TMuhStrPointerComparator
    > peers_to_discover_t;

    typedef std::pair<
        std::shared_ptr<Workers::Client::Args>,
        std::shared_ptr<Workers::Client>
    > ClientWorkerPair;

    template<>
    inline ClientWorkerPair Utils::RoundRobinVector<ClientWorkerPair>::next() {
        return next_impl();
    }

    class Server {
    private:
        Utils::RoundRobinVector<ClientWorkerPair> threads;
        const uint32_t max_client_threads;
        const uint32_t max_parallel_connections;
        void load_peers_to_discover();
        static void socket_cb(struct ev_loop* loop, ev_io* watcher, int events);
    public:
        const size_t read_size = 131072;
        const uint64_t no_failover_time = 10 * 60;
        const time_t discovery_timeout_milliseconds = 3000;
        const uint32_t max_replication_factor = 3;
        const uint64_t job_time = 10 * 60;

        std::atomic<uint_fast64_t> stat_num_inserts;
        std::atomic<uint_fast64_t> stat_num_replications;
        std::atomic<uint_fast64_t> stat_num_repl_it;
        std::atomic<uint_fast64_t> stat_num_requests_detailed [256];
        std::atomic<uint_fast64_t> stat_num_responses_detailed [256];

        char* my_hostname;
        uint32_t my_hostname_len;
        uint32_t my_port;
        std::shared_ptr<Utils::muh_str_t> my_peer_id;
        uint32_t my_peer_id_len_net;

        known_peers_t known_peers;
        known_peers_t known_peers_by_conn_id;
        wip_t wip;
        me_t me;
        peers_to_discover_t peers_to_discover;
        const Utils::known_events_t& known_events;

        Server(
            uint32_t _my_port,
            uint32_t _max_client_threads,
            uint32_t _max_parallel_connections,
            const Utils::known_events_t& _known_events
        );
        virtual ~Server();

        short save_event(
            in_packet_e_ctx& ctx,
            uint32_t replication_factor,
            Client* client,
            uint64_t* task_ids,
            QueueDb& queue
        );

        short repl_save(
            in_packet_r_ctx& ctx,
            Client& client,
            QueueDb& queue
        );

        void repl_clean(
            size_t failover_key_len,
            const char* failover_key,
            Utils::known_event_t& event
        );

        void begin_replication(std::shared_ptr<out_packet_r_ctx> r_ctx);
        void save_peers_to_discover();
        void replication_exec(out_packet_i_ctx& ctx);

        short get_event_state(
            uint64_t id,
            Utils::known_event_t& event,
            const uint64_t now
        );

        inline const uint32_t get_max_parallel_connections() const {
            return max_parallel_connections;
        }

        void push_new_client(std::shared_ptr<new_client_t> new_client);
    };
}
