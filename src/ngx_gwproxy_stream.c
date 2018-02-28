
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;
extern ngx_gwproxy_srv_conf_t  *gwcf;

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
	case NGX_GWLINK_INIT:
	case NGX_GWLINK_AUTH:
	case NGX_GWLINK_START:
		dc = ngx_gwproxy_get_gw_connection();
		if(!dc) {
			ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
				"(%s): socks upstream no find active gw connection", __FUNCTION__);
			goto release;
		}

		gwconn.src_conns[c->fd].rel_connection = dc;
		if(dc && *dc)
		{
			gwconn.src_conns[(*dc)->fd].rel_connection = 
				(ngx_connection_t **)&(gwconn.src_conns[c->fd].conn);
		}

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
			gwconn.src_conns[c->fd].status = NGX_GWLINK_LISTEN;
		}

	case NGX_GWLINK_LISTEN:
		dc = gwconn.src_conns[c->fd].rel_connection;

		/*if(!dc || *dc == NULL) {
			dc = ngx_gwproxy_get_gw_connection();
			if(!dc || *dc == NULL) {
				ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
					"(%s): gw connection disable, socks upstream no find active", __FUNCTION__);
				goto release;
			}

			gwconn.src_conns[c->fd].rel_connection = dc;
			if(dc && *dc)
			{
				gwconn.src_conns[(*dc)->fd].rel_connection = 
					(ngx_connection_t **)&(gwconn.src_conns[c->fd].conn);
			}
		}*/

		while (ev->ready) {
			n = c->recv(c, c->buffer->start, c->buffer->end - c->buffer->start);
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
					(*dc)->send(*dc, c->buffer->start, n);
				}
				else {
					//ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
						//"(%s): socks upstream no src connection found", __FUNCTION__);
					goto release;
				}
			}
			else {
				//ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
					//	"(%s): socks upstream no data transfer", __FUNCTION__);
				goto release;
			}
		}
		break;

	case NGX_GWLINK_RELEASE:
		goto release;
	}

    if (ngx_handle_read_event(ev, 0) != NGX_OK) { 
        /* error */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
            "(%s): socks upstream handle read event error", __FUNCTION__);
    }

	return;

release:
	dc = gwconn.src_conns[c->fd].rel_connection;
	if(dc && *dc)
	{
		gwconn.src_conns[(*dc)->fd].status = NGX_GWLINK_RELEASE;
	}

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

