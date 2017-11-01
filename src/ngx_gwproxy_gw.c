
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;

static void ngx_stream_gwproxy_downstream_handler(ngx_event_t *ev);

ngx_connection_t *ngx_gwproxy_get_gw_connection(ngx_connection_t *oc)
{
	ngx_uint_t i;
    ngx_connection_t **c;

    c = gwconn.connections;
	i = gwconn.cur_fd;
	while(i < gwconn.connection_n) {
		if(c[i] != NULL && gwconn.occupy_connections[i] == NULL) {
			gwconn.cur_fd = i+1;
			gwconn.occupy_connections[i] = oc;
			return c[i];
		}

		i++;
	}

	i = 0;
	while(i < gwconn.cur_fd) {
		if(c[i] != NULL && gwconn.occupy_connections[i] == NULL) {
			gwconn.cur_fd = i+1;
			gwconn.occupy_connections[i] = oc;
			return c[i];
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
	ngx_post_event(c->read, &ngx_posted_events);

	gwconn.connections[c->fd] = c;
}


static void
ngx_stream_gwproxy_downstream_handler(ngx_event_t *ev)
{
	ssize_t               n;
	ngx_stream_session_t  *s;
    ngx_connection_t      *c, *sc;
    u_char                buf[1024];

    if (ev->timedout) { 
		/* timeout expired */ 
		ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                   "(%s): gw downstream timeout expired", __FUNCTION__);
	}

    c = ev->data;
	s = c->data;
	sc = gwconn.occupy_connections[c->fd];

    while (ev->ready) {
        n = c->recv(c, buf, sizeof(buf));

        if (n == NGX_AGAIN) {
			ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                   "(%s): gw downstream recv again", __FUNCTION__);
            break;
        }

        if (n == NGX_ERROR) { 
			/* error */
			gwconn.connections[c->fd] = NULL;
			gwconn.occupy_connections[c->fd] = NULL;

			ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                   "(%s): gw downstream recv error", __FUNCTION__);
			return;
		}

        /* process buf */
		if (n > 0) {
			if(sc) {
				sc->send(sc, buf, n);
			}
			else {
				ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                   "(%s): gw downstream no src connection found", __FUNCTION__);
			}
		}
		else {
			gwconn.connections[c->fd] = NULL;
			gwconn.occupy_connections[c->fd] = NULL;
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

