#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "tcp4.h"


// this is just a minimal tcp4 transport backing for testing, it should only serve as an example for using a real socket event lib

// our unique id per mesh
#define MUID "net_tcp4"

// individual pipe local info
typedef struct pipe_tcp4_struct
{
  struct sockaddr_in sa;
  int client;
  pipe_t pipe;
  net_tcp4_t net;
} *pipe_tcp4_t;

// just make sure it's connected
pipe_tcp4_t tcp4_connected(pipe_tcp4_t to)
{
  struct sockaddr_in addr;
  if(!to) return NULL;
  if(to->client > 0) return to;
  
  // no socket yet, connect one
  if((to->client = socket(AF_INET, SOCK_STREAM, 0)) <= 0) return LOG("client socket faied %s, will retry next send",strerror(errno));

  // try to bind to our server port for some reflection
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(to->net->port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind(to->client, (struct sockaddr *)&addr, sizeof(struct sockaddr)); // optional, ignore fail

  if(connect(to->client, (struct sockaddr *)&(to->sa), sizeof(struct sockaddr)) < 0)
  {
    LOG("client socket connect faied to %s: %s, will retry next send",to->pipe->id,strerror(errno));
    close(to->client);
    to->client = 0;
    return NULL;
  }

  LOG("connected to %s",to->pipe->id);
  fcntl(to->client, F_SETFL, O_NONBLOCK);
  return to;
}

// do all chunk/socket stuff
pipe_tcp4_t tcp4_flush(pipe_tcp4_t to)
{
  to = tcp4_connected(to);
  if(!to) return NULL;

  LOG("TODO send and receive to %s",to->pipe->id);
  return to;
}

// chunkize a packet
void tcp4_send(pipe_t pipe, lob_t packet, link_t link)
{
  pipe_tcp4_t to = (pipe_tcp4_t)pipe->arg;

  if(!to || !packet || !link) return;
  LOG("tcp4 to %s",link->id->hashname);

  // TODO chunks

  tcp4_flush(to);
}

// internal, get or create a pipe
pipe_t tcp4_pipe(net_tcp4_t net, char *ip, int port)
{
  pipe_t pipe;
  pipe_tcp4_t to;
  char id[23];

  snprintf(id,23,"%s:%d",ip,port);
  pipe = xht_get(net->pipes,id);
  if(pipe) return pipe;

  LOG("new pipe to %s",id);

  // create new tcp4 pipe
  if(!(to = malloc(sizeof (struct pipe_tcp4_struct)))) return LOG("OOM");
  memset(to,0,sizeof (struct pipe_tcp4_struct));
  to->net = net;
  to->sa.sin_family = AF_INET;
  inet_aton(ip, &(to->sa.sin_addr));
  to->sa.sin_port = htons(port);

  // create the general pipe object to return and save reference to it
  if(!(pipe = pipe_new("tcp4")))
  {
    free(to);
    return LOG("OOM");
  }
  pipe->id = strdup(id);
  xht_set(net->pipes,pipe->id,pipe);

  pipe->arg = to;
  to->pipe = pipe;
  pipe->send = tcp4_send;

  return pipe;
}


pipe_t tcp4_path(link_t link, lob_t path)
{
  net_tcp4_t net;
  char *ip;
  int port;

  // just sanity check the path first
  if(!link || !path) return NULL;
  if(!(net = xht_get(link->mesh->index, MUID))) return NULL;
  if(util_cmp("tcp4",lob_get(path,"type"))) return NULL;
  if(!(ip = lob_get(path,"ip"))) return LOG("missing ip");
  if((port = lob_get_int(path,"port")) <= 0) return LOG("missing port");
  return tcp4_pipe(net, ip, port);
}

net_tcp4_t net_tcp4_new(mesh_t mesh, lob_t options)
{
  int port, sock, pipes, opt = 1;
  net_tcp4_t net;
  struct sockaddr_in sa;
  socklen_t size = sizeof(struct sockaddr_in);
  
  port = lob_get_int(options,"port");
  pipes = lob_get_int(options,"pipes");
  if(!pipes) pipes = 11; // hashtable for active pipes

  // create a udp socket
  if((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) ) < 0 ) return LOG("failed to create socket %s",strerror(errno));

  memset(&sa,0,sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = htonl(INADDR_ANY);
  if(bind(sock, (struct sockaddr*)&sa, size) < 0) return LOG("bind failed %s",strerror(errno));
  getsockname(sock, (struct sockaddr*)&sa, &size);
  if(listen(sock, 10) < 0) return LOG("listen failed %s",strerror(errno));
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *)&opt , sizeof(int));

  if(!(net = malloc(sizeof (struct net_tcp4_struct)))) return LOG("OOM");
  memset(net,0,sizeof (struct net_tcp4_struct));
  net->server = sock;
  net->port = ntohs(sa.sin_port);
  net->pipes = xht_new(pipes);

  // connect us to this mesh
  net->mesh = mesh;
  xht_set(mesh->index, MUID, net);
  mesh_on_path(mesh, MUID, tcp4_path);
  
  // convenience
  net->path = lob_new();
  lob_set(net->path,"type","tcp4");
  lob_set(net->path,"ip","127.0.0.1");
  lob_set_int(net->path,"port",net->port);

  return net;
}

void net_tcp4_free(net_tcp4_t net)
{
  if(!net) return;
  close(net->server);
  xht_free(net->pipes);
  lob_free(net->path);
  free(net);
  return;
}

// process any new incoming connections
void net_tcp4_accept(net_tcp4_t net)
{
  struct sockaddr_in addr;
  int client;
  pipe_t pipe;
  pipe_tcp4_t to;
  socklen_t size = sizeof(struct sockaddr_in);

  while((client = accept(net->server, (struct sockaddr *)&addr,&size)) > 0)
  {
    fcntl(client, F_SETFL, O_NONBLOCK);
    if(!(pipe = tcp4_pipe(net, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port)))) continue;
    LOG("connected %s",pipe->id);
    to = (pipe_tcp4_t)pipe->arg;
    if(to->client) close(to->client);
    to->client = client;
  }

}

// check a single pipe's socket for any read/write activity
void _walkflush(xht_t h, const char *key, void *val, void *arg)
{
  tcp4_flush((pipe_tcp4_t)val);
}

net_tcp4_t net_tcp4_loop(net_tcp4_t net)
{
  net_tcp4_accept(net);
  xht_walk(net->pipes, _walkflush, NULL);
  return net;
}
