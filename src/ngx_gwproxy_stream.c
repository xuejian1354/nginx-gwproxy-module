
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;

static void ngx_stream_socks_gwproxy_upstream_handler(ngx_event_t *ev);

void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s)
{
    ngx_connection_t *c, *dc;

    c = s->connection;
    dc = ngx_gwproxy_get_gw_connection_with_set_srclink(NGX_STREAM_CONNECTION_LINK, c);
    if(!dc) {
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
            "(%s): socks upstream no find active gw connection", __FUNCTION__);
    }
    s->connection = dc;
    c->read->handler = ngx_stream_socks_gwproxy_upstream_handler;

    ngx_post_event(c->read, &ngx_posted_events);
}


static void
ngx_stream_socks_gwproxy_upstream_handler(ngx_event_t *ev)
{
    ssize_t                n;
    ngx_connection_t      *c, *dc;
    ngx_stream_session_t  *s;
    u_char                buf[1024];

    if (ev->timedout) { 
        /* timeout expired */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): socks upstream timeout expired", __FUNCTION__);
    }

    c = ev->data;
    s = c->data;
    dc = s->connection;
    if(!dc) {
        dc = ngx_gwproxy_get_gw_connection_with_set_srclink(NGX_STREAM_CONNECTION_LINK, c);
        if(!dc) {
            ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
                "(%s): gw connection disable, socks upstream no find active", __FUNCTION__);
        }
        s->connection = dc;
    }

    while (ev->ready) {
        n = c->recv(c, buf, sizeof(buf));

        if (n == NGX_AGAIN) {
            ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): socks upstream recv again", __FUNCTION__);
            break;
        }

        if (n == NGX_ERROR) { 
            /* error */ 
            ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): socks upstream recv error", __FUNCTION__);
        }

        if (n > 0) {
            if(dc) {
                dc->send(dc, buf, n);
            }
            else {
                ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                    "(%s): socks upstream no src connection found", __FUNCTION__);
            }
        }
        else {
            if(dc) {
                gwconn.src_conns[dc->fd].link_type = NGX_NONE_LINK;
                gwconn.src_conns[dc->fd].src_link = NULL;
            }
            s->connection = c;
            ngx_stream_finalize_session(s, NGX_STREAM_OK);
            return;
        }
    }

    if (ngx_handle_read_event(ev, 0) != NGX_OK) { 
        /* error */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
            "(%s): socks upstream handle read event error", __FUNCTION__);
    }
}

