
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;

static void ngx_stream_socks_gwproxy_upstream_handler(ngx_event_t *ev);

void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s)
{
    ngx_connection_t *c, **dc;

    c = s->connection;
    dc = ngx_gwproxy_get_gw_connection();
    if(!dc) {
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
            "(%s): socks upstream no find active gw connection", __FUNCTION__);
    }

	gwconn.src_conns[c->fd].link_type = NGX_STREAM_CONNECTION_LINK;
    gwconn.src_conns[c->fd].conn = c;
	gwconn.src_conns[c->fd].rel_connection = dc;

    c->read->handler = ngx_stream_socks_gwproxy_upstream_handler;

    ngx_post_event(c->read, &ngx_posted_events);
}


static void
ngx_stream_socks_gwproxy_upstream_handler(ngx_event_t *ev)
{
    ssize_t                n;
    ngx_connection_t      *c, **dc;
    ngx_stream_session_t  *s;
    u_char                buf[1024];

    if (ev->timedout) { 
        /* timeout expired */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): socks upstream timeout expired", __FUNCTION__);
    }

    c = ev->data;
    s = c->data;
    dc = gwconn.src_conns[c->fd].rel_connection;

    if(!dc || *dc == NULL) {
        dc = ngx_gwproxy_get_gw_connection();
        if(!dc || *dc == NULL) {
            ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
                "(%s): gw connection disable, socks upstream no find active", __FUNCTION__);
        }
        gwconn.src_conns[c->fd].rel_connection = dc;
    }

    while (ev->ready) {
		buf[0] = (c->fd >> 24) & 0xFF;
		buf[1] = (c->fd >> 16) & 0xFF;
		buf[2] = (c->fd >> 8) & 0xFF;
		buf[3] = (c->fd) & 0xFF;

        n = c->recv(c, buf+4, sizeof(buf)-4);

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
            if(dc && *dc) {
                (*dc)->send(*dc, buf, n+4);
            }
            else {
                ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                    "(%s): socks upstream no src connection found", __FUNCTION__);
            }
        }
        else {
            gwconn.src_conns[c->fd].link_type = NGX_NONE_LINK;
            gwconn.src_conns[c->fd].conn = NULL;
			gwconn.src_conns[c->fd].rel_connection = NULL;

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

