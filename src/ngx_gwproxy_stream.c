
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;

static void ngx_stream_socks_gwproxy_upstream_handler(ngx_event_t *ev);

void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s)
{
    ngx_connection_t *c = s->connection;

	gwconn.src_conns[c->fd].link_type = NGX_STREAM_CONNECTION_LINK;
	gwconn.src_conns[c->fd].status = NGX_GWLINK_START;
    gwconn.src_conns[c->fd].conn = c;
	gwconn.src_conns[c->fd].rel_connection = NULL;

    c->read->handler = ngx_stream_socks_gwproxy_upstream_handler;

    ngx_post_event(c->read, &ngx_posted_events);
}


static void
ngx_stream_socks_gwproxy_upstream_handler(ngx_event_t *ev)
{
    ssize_t                n;
    ngx_connection_t      *c, **dc;
    ngx_stream_session_t  *s;

    if (ev->timedout) { 
        /* timeout expired */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): socks upstream timeout expired", __FUNCTION__);
    }

    c = ev->data;
    s = c->data;

	switch(gwconn.src_conns[c->fd].status) {
	case NGX_GWLINK_START:
		dc = ngx_gwproxy_get_gw_connection();
		if(!dc) {
			ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
				"(%s): socks upstream no find active gw connection", __FUNCTION__);
			ngx_stream_finalize_session(s, NGX_STREAM_SERVICE_UNAVAILABLE);
			goto release;
		}
		gwconn.src_conns[c->fd].rel_connection = dc;

		if(c->buffer == NULL) {
			c->buffer = ngx_calloc_buf(c->pool);
			if (c->buffer == NULL) {
				goto release;
			}

			c->buffer->start = ngx_palloc(c->pool, 8192);
			if (c->buffer->start == NULL) {
				goto release;
			}

			c->buffer->pos = c->buffer->start;
			c->buffer->last = c->buffer->start;
			c->buffer->end = c->buffer->last + 8192;
			c->buffer->temporary = 1;
		}

		if(dc && *dc) {
			c->buffer->pos[0] = (c->fd >> 24) & 0xFF;
			c->buffer->pos[1] = (c->fd >> 16) & 0xFF;
			c->buffer->pos[2] = (c->fd >> 8) & 0xFF;
			c->buffer->pos[3] = (c->fd) & 0xFF;

			ngx_str_t constr = ngx_string(NGX_GWPROXY_CONNECTION_NEW_PRE);
			c->buffer->last = ngx_copy(c->buffer->pos+4, constr.data, constr.len);

			(*dc)->send(*dc, c->buffer->pos, c->buffer->last - c->buffer->pos);

			gwconn.src_conns[c->fd].status = NGX_GWLINK_LISTEN;
		}
		break;

	case NGX_GWLINK_AUTH:
		break;

	case NGX_GWLINK_LISTEN:
		dc = gwconn.src_conns[c->fd].rel_connection;

		if(!dc || *dc == NULL) {
			dc = ngx_gwproxy_get_gw_connection();
			if(!dc || *dc == NULL) {
				ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
					"(%s): gw connection disable, socks upstream no find active", __FUNCTION__);
				goto release;
			}
			gwconn.src_conns[c->fd].rel_connection = dc;
		}

		while (ev->ready) {
			c->buffer->start[0] = (c->fd >> 24) & 0xFF;
			c->buffer->start[1] = (c->fd >> 16) & 0xFF;
			c->buffer->start[2] = (c->fd >> 8) & 0xFF;
			c->buffer->start[3] = (c->fd) & 0xFF;

			n = c->recv(c, c->buffer->start+4, c->buffer->end-c->buffer->start-4);

			if (n == NGX_AGAIN) {
				ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
					"(%s): socks upstream recv again", __FUNCTION__);
				break;
			}

			if (n == NGX_ERROR) { 
				/* error */ 
				ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
					"(%s): socks upstream recv error", __FUNCTION__);
				goto release;
			}

			if (n > 0) {
				if(dc && *dc) {
					(*dc)->send(*dc, c->buffer->start, n+4);
				}
				else {
					ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
						"(%s): socks upstream no src connection found", __FUNCTION__);
				}
			}
			else {
				if(dc && *dc) {
					ngx_str_t constr = ngx_string(NGX_GWPROXY_CONNECTION_NEW_SUF);
					c->buffer->last = ngx_copy(c->buffer->pos+4, constr.data, constr.len);
					(*dc)->send(*dc, c->buffer->pos, c->buffer->last - c->buffer->pos);
				}
				goto release;
			}
		}
		break;

	case NGX_GWLINK_RELEASE:
		break;
	}

    if (ngx_handle_read_event(ev, 0) != NGX_OK) { 
        /* error */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
            "(%s): socks upstream handle read event error", __FUNCTION__);
    }

	return;

release:
	gwconn.src_conns[c->fd].link_type = NGX_NONE_LINK;
	gwconn.src_conns[c->fd].conn = NULL;
	gwconn.src_conns[c->fd].rel_connection = NULL;
	gwconn.src_conns[c->fd].status = NGX_GWLINK_RELEASE;

	ngx_stream_finalize_session(s, NGX_STREAM_OK);
}

void
ngx_stream_socks_gwproxy_downstream_send(ngx_src_conn_t *sc, u_char *buf, size_t size)
{
	ngx_connection_t *c;
	if(!sc) {
		return;
	}

	c = sc->conn;
	if(c) {
    	c->send(c, buf, size);
	}
}

