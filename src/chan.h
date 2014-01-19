#ifndef chan_h
#define chan_h

typedef struct chan_struct
{
  unsigned char id[16];
  char hexid[33];
  struct switch_struct *s;
  struct hn_struct *to;
  char *type;
  int reliable;
  enum {STARTING, OPEN, ENDED} state;
  struct path_struct *last;
  struct chan_struct *next;
  struct packet_struct *in, *inend;
} *chan_t;

chan_t chan_new(struct switch_struct *s, struct hn_struct *to, char *type, int reliable);
void chan_free(chan_t c);

// returns existing or creates new and adds to from
chan_t chan_in(struct switch_struct *s, struct hn_struct *from, struct packet_struct *p);

// create a packet ready to be sent for this channel
struct packet_struct *chan_packet(chan_t c);

// pop a packet from this channel to be processed, caller must free
struct packet_struct *chan_pop(chan_t c);

// internal, receives/processes incoming packet
void chan_receive(chan_t c, struct packet_struct *p);

#endif