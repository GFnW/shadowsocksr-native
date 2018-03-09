#if !defined(__tunnel_h__)
#define __tunnel_h__ 1

#include <uv.h>
#include <stdbool.h>
#include "sockaddr_universal.h"

struct tunnel_ctx;
struct server_env_t;
struct tunnel_cipher_ctx;
struct buffer_t;

enum socket_state {
    socket_stop,  /* Stopped. */
    socket_busy,  /* Busy; waiting for incoming data or for a write to complete. */
    socket_done,  /* Done; read incoming data or write finished. */
    socket_dead,
};

struct socket_ctx {
    enum socket_state rdstate;
    enum socket_state wrstate;
    unsigned int idle_timeout;
    struct tunnel_ctx *tunnel;  /* Backlink to owning tunnel context. */
    ssize_t result;
    union {
        uv_handle_t handle;
        uv_stream_t stream;
        uv_tcp_t tcp;
        uv_udp_t udp;
    } handle;
    uv_timer_t timer_handle;  /* For detecting timeouts. */
                              /* We only need one of these at a time so make them share memory. */
    union {
        uv_getaddrinfo_t addrinfo_req;
        uv_connect_t connect_req;
        uv_req_t req;
        union sockaddr_universal addr;
        uint8_t buf[SSR_BUFF_SIZE];  /* Scratch space. Used to read data into. */
    } t;
};

/* Session states. */
enum session_state {
    session_handshake,        /* Wait for client handshake. */
    session_handshake_auth,   /* Wait for client authentication data. */
    session_req_start,        /* Start waiting for request data. */
    session_req_parse,        /* Wait for request data. */
    session_req_udp_accoc,
    session_req_lookup,       /* Wait for upstream hostname DNS lookup to complete. */
    session_req_connect,      /* Wait for uv_tcp_connect() to complete. */
    session_ssr_auth_sent,
    session_proxy_start,      /* Connected. Start piping data. */
    session_proxy,            /* Connected. Pipe data back and forth. */
    session_kill,             /* Tear down session. */
    session_dead,             /* Dead. Safe to free now. */
};

struct tunnel_ctx {
    enum session_state state;
    struct server_env_t *env; // __weak_ptr
    struct tunnel_cipher_ctx *cipher;
    struct buffer_t *init_pkg;
    s5_ctx parser;  /* The SOCKS protocol parser. */
    uv_tcp_t *listener;  /* Backlink to owning listener context. */
    struct socket_ctx incoming;  /* Connection with the SOCKS client. */
    struct socket_ctx outgoing;  /* Connection with upstream. */
    int ref_count;

    void(*tunnel_dying)(struct tunnel_ctx *tunnel);
    void(*tunnel_connected_done)(struct tunnel_ctx *tunnel, struct socket_ctx *socket);
    void(*tunnel_read_done)(struct tunnel_ctx *tunnel, struct socket_ctx *socket);
    void(*tunnel_getaddrinfo_done)(struct tunnel_ctx *tunnel, struct socket_ctx *socket);
    void(*tunnel_write_done)(struct tunnel_ctx *tunnel, struct socket_ctx *socket);
};

void tunnel_initialize(uv_tcp_t *lx, void(*init_done_cb)(struct tunnel_ctx *tunnel, void *p), void *p);
void tunnel_shutdown(struct tunnel_ctx *tunnel);
int socket_connect(struct socket_ctx *c);
void socket_read(struct socket_ctx *c);
void socket_read_stop(struct socket_ctx *c);
void socket_getaddrinfo(struct socket_ctx *c, const char *hostname);
void socket_write(struct socket_ctx *c, const void *data, size_t len);

#endif // !defined(__tunnel_h__)