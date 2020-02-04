#include <aether/muxer/netcode.hh>
#include <aether/common/base_protocol.hh>
#include <aether/generic-netcode/generic_netcode.hh>
#include <aether/generic-netcode/trivial_marshalling.hh>
#include <protocol.hh>

using netcode = aether::netcode::generic_netcode<marshalling_factory>;

extern "C" {

void *new_netcode_context() {
    return new netcode();
}

void destroy_netcode_context(void *ctx) {
    delete static_cast<netcode*>(ctx);
}

void netcode_new_simulation_message(void *ctx, void *muxer, uint64_t worker_id, uint64_t tick, const void *data, size_t data_len) {
    auto nc = static_cast<netcode *>(ctx);
    nc->new_simulation_message(muxer, worker_id, tick, data, data_len);
}

void netcode_new_connection(void *ctx, void *muxer, void *connection, uint64_t id) {
    auto nc = static_cast<netcode *>(ctx);
    nc->new_connection(muxer, connection, id);
}

void netcode_drop_connection(void *ctx, void *muxer, uint64_t id) {
    auto nc = static_cast<netcode *>(ctx);
    nc->drop_connection(muxer, id);
}

void netcode_notify_alarm(void *ctx, void *muxer, uint64_t token) {
}

void netcode_notify_writable(void *ctx, void *muxer, uint64_t id) {
    auto nc = static_cast<netcode *>(ctx);
    nc->notify_writable(muxer, id);
}

}

extern "C" void muxer_main(ssize_t argc, const char *const *argv);
int main(int argc, const char *const *argv) {
    muxer_main(argc, argv);
}
