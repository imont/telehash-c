#include <stdio.h>
#include "telehash.h"
#include "net_tcp4.h"

chan_t stream_send(chan_t chan, uint8_t * data, size_t size);

static char * reply_prefix = "REPLY: ";

void imont_stream_chan_handler(chan_t chan, void *arg) {
    lob_t packet;
    if(!chan) return;

    while((packet = chan_receiving(chan)))
    {
        printf("Received new packet with sequence: %d, body length is: %zd\n", lob_get_int(packet, "seq"), packet->body_len);
        if (packet->body_len > 0) {
            //size_t reply_length = packet->body_len + strlen(reply_prefix) + 1;
            //char * reply = malloc(reply_length);
            //memset(reply, 0, reply_length);
            //memcpy(reply, reply_prefix, strlen(reply_prefix) + 1);
            //reply = strncat(reply, (const char *) packet->body, packet->body_len);
            //printf("Replying with: %s\n", reply);
            //stream_send(chan, (uint8_t *) reply, strlen(reply));
            stream_send(chan, (uint8_t *) reply_prefix, strlen(reply_prefix));
            //free(reply);
            //free(received);
        }
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
    //util_sys_logging(1);
    printf("Test App\n");
    mesh_t mesh = mesh_new(11);
    lob_t secrets, json;
    if (argc > 1) {
        if (strcmp(argv[1], "static") == 0) {
            mesh = load_static();
        }
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
        printf("Mesh secrets: \n %s \n", lob_json(secrets));
    }

    mesh_on_open(mesh, "stream", imont_stream_on_open);

    lob_t tcp_options = lob_new();
    lob_set_int(tcp_options, "port", 4567);
    net_tcp4_t net = net_tcp4_new(mesh, tcp_options);
    printf("Listening on port: %d\n", net->port);
    while(net_tcp4_loop(net));
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
