
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;

void ngx_gwproxy_sockswrite_handler(ngx_event_t *ev);

void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s)
{
    ngx_str_t                      text = ngx_string("Socks Proxy");
    ngx_buf_t                     *b;
    ngx_connection_t              *c;
    ngx_gwproxy_ctx_t       *ctx;
    ngx_gwproxy_srv_conf_t  *rscf;

    c = s->connection;

    c->log->action = "returning text";

    rscf = ngx_stream_get_module_srv_conf(s, ngx_stream_gwproxy_module);
	(void) rscf;

    ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0,
                   "stream return text: \"%V\"", &text);

    if (text.len == 0) {
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    ctx = ngx_pcalloc(c->pool, sizeof(ngx_gwproxy_ctx_t));
    if (ctx == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_stream_set_ctx(s, ctx, ngx_stream_gwproxy_module);

    b = ngx_calloc_buf(c->pool);
    if (b == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    b->memory = 1;
    b->pos = text.data;
    b->last = text.data + text.len;
    b->last_buf = 1;

    ctx->out = ngx_alloc_chain_link(c->pool);
    if (ctx->out == NULL) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ctx->out->buf = b;
    ctx->out->next = NULL;

    c->write->handler = ngx_gwproxy_sockswrite_handler;

    ngx_gwproxy_sockswrite_handler(c->write);
}

void ngx_gwproxy_sockswrite_handler(ngx_event_t *ev)
{
    ngx_connection_t         *c;
    ngx_stream_session_t     *s;
    ngx_gwproxy_ctx_t  *ctx;

    c = ev->data;
    s = c->data;

    if (ev->timedout) {
        ngx_connection_error(c, NGX_ETIMEDOUT, "connection timed out");
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    ctx = ngx_stream_get_module_ctx(s, ngx_stream_gwproxy_module);

    if (ngx_stream_top_filter(s, ctx->out, 1) == NGX_ERROR) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ctx->out = NULL;

    if (!c->buffered) {
        ngx_log_debug0(NGX_LOG_DEBUG_STREAM, c->log, 0,
                       "stream return done sending");
        ngx_stream_finalize_session(s, NGX_STREAM_OK);
        return;
    }

    if (ngx_handle_write_event(ev, 0) != NGX_OK) {
        ngx_stream_finalize_session(s, NGX_STREAM_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_add_timer(ev, 5000);
}

