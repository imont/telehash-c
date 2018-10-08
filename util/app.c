/*
 * This is a simple test application to verify various aspects of telehash
 * It can be run in 3 modes:
 * [no arg] -> fires up a tcp3 transport and waits for connections
 * connect -> connects to ENV['PORT'] using ENV['CS1A_KEY'] on localhost
 * loopback -> establishes a loopback connection between two meshes and exchanges some data
 *
 * pass "static" to use a static ciphersuite (useful for testing)
 */

#include <stdio.h>
#include "telehash.h"
#include "net_tcp4.h"
#include "net_udp4.h"
#include "net_loopback.h"

chan_t stream_send(chan_t chan, uint8_t * data, size_t size);
void parse_args(int argc, char ** argv);
void test_loopback(mesh_t meshA, mesh_t meshB);

static char * reply_prefix = "REPLY: ";

static int loopback = 0;
static int static_keys = 0;
static int init_connection = 0;
static int listen_udp = 0;
static unsigned int packet_loss_percent = 0;

void imont_stream_chan_handler(chan_t chan, void *arg) {
    lob_t packet;
    if(!chan) return;

    chan->state = CHAN_OPEN;
    while((packet = chan_receiving(chan))) {
        //lob_free(lob_unlink(open));
        printf("Received new packet with sequence: %d, body length is: %zd\n", lob_get_int(packet, "seq"), packet->body_len);
        if (packet->body_len > 0) {
            size_t reply_length = packet->body_len + strlen(reply_prefix) + 1;
            char * reply = malloc(reply_length);
            memset(reply, 0, reply_length);
            memcpy(reply, reply_prefix, strlen(reply_prefix) + 1);
            reply = strncat(reply, (const char *) packet->body, packet->body_len);
            printf("Replying with: %s\n", reply);
            stream_send(chan, (uint8_t *) reply, strlen(reply));
            free(reply);
        } else {
            chan_process(chan, 0);
        }
        lob_free(packet);
    }

}

// new incoming stream channel, set up handler
lob_t imont_stream_on_open(link_t link, lob_t open) {
    chan_t chan;
    if(!link) return open;
    if(lob_get_cmp(open,"type","stream")) return open;

    LOG("incoming stream channel open");

    // create new channel, set it up, then receive this open
    chan = link_chan(link, open);
    chan_handle(chan,imont_stream_chan_handler,NULL);
    chan_receive(chan,open);
    chan_process(chan, 0);
    return NULL;
}

chan_t stream_new(link_t link) {
    lob_t open = lob_new();
    lob_set(open, "type", "stream");
    lob_set_int(open, "seq", 1);
    chan_t chan = link_chan(link, open);

    chan_send(chan, open);
    return chan;
}

chan_t stream_send(chan_t chan, uint8_t * data, size_t size) {
    lob_t pkt = chan_packet(chan);
    lob_body(pkt, data, size);
    if (!chan_send(chan, pkt)) {
        return NULL;
    }
    return chan;
}

mesh_t load_static();

int main(int argc, char ** argv) {
    if (getenv("DEBUG")) {
        util_sys_logging(1);
    } else {
        util_sys_logging(0);
    }
    setbuf(stdout, NULL);
    printf("Telehash-c Test App\n");
    parse_args(argc, argv);

    mesh_t mesh;
    lob_t secrets, json;
    if (static_keys) {
        mesh = load_static();
        secrets = NULL;
    } else {
        mesh = mesh_new(11);
        secrets = mesh_generate(mesh);
    }

    // accept anyone
    mesh_on_discover(mesh, "auto", mesh_add);

    json = mesh_json(mesh);
    char * json_str = lob_json(json);
    printf("Mesh keys: %s \n", json_str);
    if (secrets) {
        printf("Mesh secrets: %s \n", lob_json(secrets));
    }

    mesh_on_open(mesh, "stream", imont_stream_on_open);

    if (loopback) {
        mesh_t meshB = mesh_new(13);
        lob_t secretsB = mesh_generate(meshB);
        test_loopback(mesh, meshB);
        mesh_free(meshB);
        lob_free(secretsB);
    } else if (init_connection) {
        net_tcp4_t net = net_tcp4_new(mesh, NULL);
        printf("Listening on port: %d\n", net->port);
        int port = atoi(getenv("PORT"));
        char * key1a = getenv("CS1A_KEY");
        char * key20 = getenv("CS20_KEY");
        lob_t remote_key = lob_new();
        lob_set(remote_key, "1a", key1a);
        if (key20) {
            lob_set(remote_key, "20", key20);
        }

        lob_t path = lob_new();
        lob_set(path, "type", "tcp4");
        lob_set(path, "ip", "localhost");
        lob_set_int(path, "port", port);
        link_t remote_link = link_keys(mesh, remote_key);
        link_path(remote_link, path);
        link_sync(remote_link);
        while(net_tcp4_loop(net));
        net_tcp4_free(net);
    } else if (listen_udp) {
        printf("Configured packet loss: %d%%\n", packet_loss_percent);
        lob_t options = lob_new();
        lob_set_uint(options, "pkt_loss", packet_loss_percent);
        net_udp4_t net = net_udp4_new(mesh, options);
        printf("Listening on UDP port: %d\n", net->port);
        while(net_udp4_receive(net));
        net_udp4_free(net);
    } else {
        net_tcp4_t net = net_tcp4_new(mesh, NULL);
        printf("Listening on port: %d\n", net->port);
        while(net_tcp4_loop(net));
        net_tcp4_free(net);
    }

    mesh_free(mesh);
    lob_free(json);
    lob_free(secrets);
    printf("Exiting \n");
    return 0;
}

mesh_t load_static() {
    printf("Will use static keys\n");
    mesh_t mesh = mesh_new(11);
    lob_t keys = lob_new();
    lob_set(keys, "1a", "akznkdfv4yqrci5igrtvr3aj5vmoac7yfu");
    lob_t secrets = lob_new();
    lob_set(secrets, "1a", "aa6ltuce4k5v4a5x57hlljfhgqvccopp3e");
    mesh_load(mesh, secrets, keys);
    return mesh;
}

void parse_args(int argc, char** argv) {
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "static") == 0) {
            static_keys = 1;
        }
        if (strcmp(argv[i], "loopback") == 0) {
            loopback = 1;
        }
        if (strcmp(argv[i], "udp") == 0) {
            listen_udp = 1;
            if (argc > i) {
                packet_loss_percent = (unsigned int) atoi(argv[i + 1]);
            }
        }
        if (strcmp(argv[i], "connect") == 0) {
            init_connection = 1;
        }
    }
}

void test_loopback(mesh_t meshA, mesh_t meshB) {
    printf("Will test loopback \n");
    mesh_on_open(meshB, "stream", imont_stream_on_open);

    net_loopback_t loop = net_loopback_new(meshA, meshB);
    link_t linkBA = link_get(meshB, meshA->id);
    chan_t stream = stream_new(linkBA);
    for (uint8_t x = 0; x < 10; x++) {
        uint8_t msg[] = {72, 72, 69, 69, x};
        if (!stream_send(stream, msg, 5)) {
            printf("Sending failed! \n");
        }
        lob_t rcv;
        while ((rcv = chan_receiving(stream))) {
            printf("Received reply with id %d\n", rcv->id);
            lob_free(rcv);
        }
    }
    net_loopback_free(loop);
}
