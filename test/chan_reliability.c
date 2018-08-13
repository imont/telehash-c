//
// Copyright (C) 2018 IMONT Technologies Limited
//

#include "e3x.h"
#include "util_sys.h"
#include "util.h"
#include "unit_test.h"

static void test_random_delivery(chan_t chan);
static void test_process_acks(chan_t chan);

static long rand_in_range(const int start, const int end) {
    return start + util_sys_random() / (RAND_MAX / (end - start + 1) + 1);
}

int main(int argc, char **argv)
{
    util_sys_logging(1);
    lob_t pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 1);
    lob_set(pkt, "type", "stream");
    chan_t chan = chan_new(pkt);
    fail_unless(chan);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 1);


    // next in sequence packet
    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 2);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 1);
    fail_unless(chan->in->next->id == 2);

    // deliver out-of-sequence packet
    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 4);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 1);
    fail_unless(chan->in->next->id == 2);
    fail_unless(chan->in->next->next->id == 4);

    // deliver missing packet now
    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 3);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 1);
    fail_unless(chan->in->next->id == 2);
    fail_unless(chan->in->next->next->id == 3);
    fail_unless(chan->in->next->next->next->id == 4);

    while ((pkt = chan_receiving(chan))) {
        lob_free(pkt);
    }
    fail_unless(!chan->in);

    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 6);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 6);
    fail_unless(!chan_receiving(chan)); // nothing to receive yet, don't have 5

    // deliver 5
    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 5);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 5);
    fail_unless(chan->in->next->id == 6);
    fail_unless((pkt = chan_receiving(chan))->id == 5);
    lob_free(pkt);
    fail_unless((pkt = chan_receiving(chan))->id == 6);
    lob_free(pkt);

    // ensure we prepend to beginning of c->in
    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 20);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 20);
    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 21);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 20);
    fail_unless(chan->in->next->id == 21);
    pkt = lob_new();
    lob_set_int(pkt, "c", 1);
    lob_set_int(pkt, "seq", 23);
    chan_receive(chan, pkt);
    fail_unless(chan->in->id == 20);
    fail_unless(chan->in->next->id == 21);
    fail_unless(chan->in->next->next->id == 23);
    fail_unless(!chan_receiving(chan));

    test_random_delivery(chan);
    test_process_acks(chan);

    chan_free(chan);
}

static void test_random_delivery(chan_t chan) {
    lob_t pkt, curr;
    util_sys_random_init();
    for (int x = 0; x < 100; x++) {
        int pktid = (int) rand_in_range(1, 100);
        pkt = lob_new();
        lob_set_int(pkt, "c", 1);
        lob_set_int(pkt, "seq", pktid);
        chan_receive(chan, pkt);
    }

    curr = chan->in;
    do {
        int prev_id = curr->prev ? curr->prev->id : 0;
        int next_id = curr->next ? curr->next->id : INT32_MAX;
        fail_unless(curr->id > prev_id);
        fail_unless(curr->id < next_id);
    } while((curr = lob_next(curr)));
}

static void test_process_acks(chan_t chan) {
    // generate a bunch of sent packets
    lob_t pkt, curr;
    util_sys_random_init();
    for (int x = 0; x < 100; x++) {
        pkt = lob_new();
        pkt->id = x;
        lob_set_int(pkt, "c", 1);
        lob_set_int(pkt, "seq", x);
        chan->sent = lob_push(chan->sent, pkt);
    }
    fail_unless(chan->sent);

    for (int x = 0; x < 100; x++) {
        pkt = lob_new();
        lob_set_int(pkt, "c", 1);
        lob_set_int(pkt, "ack", x);
        chan_receive(chan, pkt);
    }
    fail_unless(!chan->sent);
}