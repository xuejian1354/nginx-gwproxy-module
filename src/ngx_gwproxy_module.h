
#ifndef __NGX_GWPROXY_MODULE_H__
#define __NGX_GWPROXY_MODULE_H__

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_stream.h>
#include <ngx_http.h>

typedef struct {
    ngx_flag_t flag;
    ngx_flag_t gwflag;
} ngx_gwproxy_srv_conf_t;

typedef enum {
    NGX_NONE_LINK = 0,
    NGX_STREAM_CONNECTION_LINK,
    NGX_HTTP_REQUEST_LINK
} ngx_src_conn_link_e;

typedef struct {
    ngx_src_conn_link_e    link_type;
    void                   *src_link;
}ngx_src_conn_t;

typedef struct {
    ngx_connection_t       **connections;
    ngx_src_conn_t         *src_conns;
    ngx_uint_t             connection_n;
    ngx_uint_t             cur_fd;
}ngx_gwproxy_conn_t;

ngx_connection_t *ngx_gwproxy_get_gw_connection_with_set_srclink(ngx_src_conn_link_e type, void *link);

void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s);
void ngx_stream_gw_proxy_handler(ngx_stream_session_t *s);
ngx_int_t ngx_http_gwproxy_handler(ngx_http_request_t *r);

#endif	//__NGX_GWPROXY_MODULE_H__

