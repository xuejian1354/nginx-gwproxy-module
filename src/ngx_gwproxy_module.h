
#ifndef __NGX_GWPROXY_MODULE_H__
#define __NGX_GWPROXY_MODULE_H__

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>
#include <ngx_http.h>

#define NGX_GWPROXY_CONNECTION_NEW_PRE	"JFLS#%^Fs&XK*HJGTT&$#@!S$L:ZXRLC"
#define NGX_GWPROXY_CONNECTION_NEW_SUF	"NBID*^4>BC{j&t#5PK!FLSi7^9HCBO%U"


typedef struct {
    ngx_flag_t flag;
    ngx_flag_t gwflag;
	ngx_flag_t auth;
	ngx_str_t user;
	ngx_str_t pass;
} ngx_gwproxy_srv_conf_t;

typedef enum {
    NGX_NONE_LINK = 0,
    NGX_GW_STREAM_LINK,
    NGX_STREAM_CONNECTION_LINK,
    NGX_HTTP_REQUEST_LINK
} ngx_src_conn_link_e;

typedef enum {
    NGX_GWLINK_AUTH_SOCKS5 = 0,
    NGX_GWLINK_AUTH_PASS,
    NGX_GWLINK_START,
    NGX_GWLINK_LISTEN,
    NGX_GWLINK_RELEASE
} ngx_gwlink_status_e;

typedef struct {
    ngx_src_conn_link_e    link_type;
    ngx_gwlink_status_e    status;
    void                   *conn;	//connection or request
    ngx_connection_t       **rel_connection;
}ngx_src_conn_t;

typedef struct {
    ngx_src_conn_t         *src_conns;
    ngx_uint_t             connection_n;
    ngx_uint_t             cur_fd;
}ngx_gwproxy_conn_t;

ngx_connection_t **ngx_gwproxy_get_gw_connection();


void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s);
void ngx_stream_gw_proxy_handler(ngx_stream_session_t *s);
void ngx_stream_socks_gwproxy_downstream_send(ngx_src_conn_t *sc, u_char *buf, size_t size);

ngx_int_t ngx_http_gwproxy_handler(ngx_http_request_t *r);
void ngx_http_gwproxy_downstream_callback(ngx_src_conn_t *sc, u_char *buf, size_t size);


#endif	//__NGX_GWPROXY_MODULE_H__

