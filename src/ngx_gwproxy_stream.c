
#include "ngx_gwproxy_module.h"

#define METHOD_NONE 0 //socks5(no auth)
#define METHOD_AUTH 2 //socks5(username/password)

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;
extern ngx_gwproxy_srv_conf_t  *gwcf;

static void ngx_stream_socks_gwproxy_upstream_handler(ngx_event_t *ev);

void ngx_stream_socks_proxy_handler(ngx_stream_session_t *s)
{
    ngx_connection_t *c = s->connection;

	gwconn.src_conns[c->fd].link_type = NGX_STREAM_CONNECTION_LINK;
	gwconn.src_conns[c->fd].status = NGX_GWLINK_INIT;
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
		if(1) {
			//selectSocks5Authentication
			u_char version = 0;
			u_char num_methods = 0;

			c->recv(c, &version, 1);
			if (version != 5) {
				break;
			}

			c->recv(c, &num_methods, 1);
			if(num_methods <= 0) {
				goto release;
			}

			u_char method_ids[num_methods];
			u_char response[2];
			response[0] = 5;  //SOCKS version
			response[1] = 0xFF;  //Not found

			size_t bread = 0;
			while(bread < num_methods) {
				bread += c->recv(c, method_ids+bread, num_methods-bread);
			}

			size_t i;
			ngx_flag_t found = 0;
			for(i=0; i<num_methods; i++) {
				if(method_ids[i] == METHOD_NONE || method_ids[i] == METHOD_AUTH) {
					found = 1;
					response[1] = method_ids[i];
					break;
				}
			}

			if(!found) {
				goto release;
			}

			c->send(c, response, 2);
			if(gwcf->auth) {
				gwconn.src_conns[c->fd].status = NGX_GWLINK_AUTH;
				break;
			}
			else {
				gwconn.src_conns[c->fd].status = NGX_GWLINK_START;
			}
		}

	case NGX_GWLINK_AUTH:
		if(gwcf->auth) {
			//doUserPasswordAuthentication
			u_char version = 0;
			u_char ulen, plen;
			u_char response[2];

			c->recv(c, &version, 1);
			if(version != 1) {
				goto release;
			}

			c->recv(c, &ulen, 1);
			if((char)ulen <= 0) {
				goto release;
			}

			u_char user[ulen];
			c->recv(c, user, ulen);

			c->recv(c, &plen, 1);
			if((char)plen <= 0) {
				goto release;
			}

			u_char password[plen];
			c->recv(c, password, plen);

			if(gwcf->user.len == ulen
				&& !ngx_strncmp(gwcf->user.data, user, ulen)
				&& gwcf->pass.len == plen
				&& !ngx_strncmp(gwcf->pass.data, password, plen)) {
				response[0] = 1;
				response[1] = 0;
				c->send(c, response, 2);
			}
			else {
				response[0] = 1;
				response[1] = 1;
				c->send(c, response, 2);
				goto release;
			}
		}
		gwconn.src_conns[c->fd].status = NGX_GWLINK_START;

	case NGX_GWLINK_START:
		dc = ngx_gwproxy_get_gw_connection();
		if(!dc) {
			ngx_log_debug1(NGX_LOG_DEBUG_STREAM, c->log, 0, 
				"(%s): socks upstream no find active gw connection", __FUNCTION__);
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
			ngx_str_t constr = ngx_string(NGX_GWPROXY_CONNECTION_NEW_PRE);
			c->buffer->last = ngx_copy(c->buffer->pos+8, constr.data, constr.len);
			int datalen = constr.len + 8;

			c->buffer->pos[0] = (datalen >> 24) & 0xFF;
			c->buffer->pos[1] = (datalen >> 16) & 0xFF;
			c->buffer->pos[2] = (datalen >> 8) & 0xFF;
			c->buffer->pos[3] = (datalen) & 0xFF;
			c->buffer->pos[4] = (c->fd >> 24) & 0xFF;
			c->buffer->pos[5] = (c->fd >> 16) & 0xFF;
			c->buffer->pos[6] = (c->fd >> 8) & 0xFF;
			c->buffer->pos[7] = (c->fd) & 0xFF;

			(*dc)->send(*dc, c->buffer->pos, datalen);
			gwconn.src_conns[c->fd].status = NGX_GWLINK_LISTEN;
		}
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
			n = c->recv(c, c->buffer->start+8, c->buffer->end - c->buffer->start - 8);

			int datalen = n + 8;
			c->buffer->start[0] = (datalen >> 24) & 0xFF;
			c->buffer->start[1] = (datalen >> 16) & 0xFF;
			c->buffer->start[2] = (datalen >> 8) & 0xFF;
			c->buffer->start[3] = (datalen) & 0xFF;
			c->buffer->start[4] = (c->fd >> 24) & 0xFF;
			c->buffer->start[5] = (c->fd >> 16) & 0xFF;
			c->buffer->start[6] = (c->fd >> 8) & 0xFF;
			c->buffer->start[7] = (c->fd) & 0xFF;

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
					(*dc)->send(*dc, c->buffer->start, datalen);
				}
				else {
					ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
						"(%s): socks upstream no src connection found", __FUNCTION__);
				}
			}
			else {
				if(dc && *dc) {
					ngx_str_t constr = ngx_string(NGX_GWPROXY_CONNECTION_NEW_SUF);
					int datalen = constr.len + 8;
					c->buffer->pos[0] = (datalen >> 24) & 0xFF;
					c->buffer->pos[1] = (datalen >> 16) & 0xFF;
					c->buffer->pos[2] = (datalen >> 8) & 0xFF;
					c->buffer->pos[3] = (datalen) & 0xFF;
					c->buffer->pos[4] = (c->fd >> 24) & 0xFF;
					c->buffer->pos[5] = (c->fd >> 16) & 0xFF;
					c->buffer->pos[6] = (c->fd >> 8) & 0xFF;
					c->buffer->pos[7] = (c->fd) & 0xFF;
					c->buffer->last = ngx_copy(c->buffer->pos+8, constr.data, constr.len);
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

