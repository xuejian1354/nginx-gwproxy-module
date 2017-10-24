
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

typedef struct {
    ngx_chain_t *out;
} ngx_gwproxy_ctx_t;

void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s);
void ngx_stream_gw_proxy_handler(ngx_stream_session_t *s);
ngx_int_t ngx_http_gwproxy_handler(ngx_http_request_t *r);

#endif	//__NGX_GWPROXY_MODULE_H__

