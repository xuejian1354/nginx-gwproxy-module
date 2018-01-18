
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;

static void ngx_stream_gwproxy_downstream_handler(ngx_event_t *ev);

ngx_connection_t **ngx_gwproxy_get_gw_connection()
{
    ngx_uint_t i;
    ngx_src_conn_t *sc;

    sc = gwconn.src_conns;
    i = gwconn.cur_fd;
    while(i < gwconn.connection_n) {
        if(sc[i].link_type == NGX_GW_STREAM_LINK) {
            gwconn.cur_fd = i+1;
			if(gwconn.cur_fd >= gwconn.connection_n) {
				gwconn.cur_fd = 0;
			}
            return (ngx_connection_t **)&sc[i].conn;
        }

        i++;
    }

    i = 0;
    while(i < gwconn.cur_fd) {
        if(sc[i].link_type == NGX_GW_STREAM_LINK) {
            gwconn.cur_fd = i+1;
            return (ngx_connection_t **)&sc[i].conn;
        }

        i++;
    }

    return NULL;
}

void ngx_stream_gw_proxy_handler(ngx_stream_session_t *s)
{
    ngx_connection_t *c;

    c = s->connection;
    c->read->handler = ngx_stream_gwproxy_downstream_handler;

	gwconn.src_conns[c->fd].link_type = NGX_GW_STREAM_LINK;
    gwconn.src_conns[c->fd].conn = c;

    ngx_post_event(c->read, &ngx_posted_events);
}

static void
ngx_stream_gwproxy_downstream_handler(ngx_event_t *ev)
{
    ssize_t               n;
    ngx_stream_session_t  *s;
    ngx_connection_t      *c;
	int                   scfd;
    u_char                buf[1024];

    if (ev->timedout) { 
        /* timeout expired */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
            "(%s): gw downstream timeout expired", __FUNCTION__);
    }

    c = ev->data;
    s = c->data;

    while (ev->ready) {
        n = c->recv(c, buf, sizeof(buf));

        if (n == NGX_AGAIN) {
            ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): gw downstream recv again", __FUNCTION__);
            break;
        }

        if (n == NGX_ERROR) { 
            /* error */
            gwconn.src_conns[c->fd].link_type = NGX_NONE_LINK;
            gwconn.src_conns[c->fd].conn = NULL;

            ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): gw downstream recv error", __FUNCTION__);
            return;
        }

        /* process buf */
        if (n > 4) {
			scfd = (buf[0]<<24) + (buf[1]<<16) + (buf[2]<<8) + buf[3];
			ngx_src_conn_t *sc = &gwconn.src_conns[scfd];
            if(sc && sc->link_type == NGX_STREAM_CONNECTION_LINK) {
				ngx_stream_socks_gwproxy_downstream_send(sc, buf+4, n-4);
            }
            else if(sc && sc->link_type == NGX_HTTP_REQUEST_LINK) {
				ngx_http_gwproxy_downstream_callback(sc, buf+4, n-4);
            }
            else {
                ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                    "(%s): gw downstream no src connection found", __FUNCTION__);
            }
        }
        else {
            gwconn.src_conns[c->fd].link_type = NGX_NONE_LINK;
            gwconn.src_conns[c->fd].conn = NULL;

            ngx_stream_finalize_session(s, NGX_STREAM_OK);
            return;
        }
    }

    if (ngx_handle_read_event(ev, 0) != NGX_OK) { 
        /* error */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
            "(%s): gw downstream handle read event error", __FUNCTION__);
    }
}

