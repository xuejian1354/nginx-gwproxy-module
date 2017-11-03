
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_stream_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;

static void ngx_stream_gwproxy_downstream_handler(ngx_event_t *ev);

ngx_connection_t *ngx_gwproxy_get_gw_connection_with_set_srclink(ngx_src_conn_link_e type, void *link)
{
    ngx_uint_t i;
    ngx_connection_t **c;

    c = gwconn.connections;
    i = gwconn.cur_fd;
    while(i < gwconn.connection_n) {
        if(c[i] != NULL && gwconn.src_conns[i].link_type == NGX_NONE_LINK) {
            gwconn.cur_fd = i+1;
            gwconn.src_conns[i].link_type = type;
            gwconn.src_conns[i].src_link = link;
            return c[i];
        }

        i++;
    }

    i = 0;
    while(i < gwconn.cur_fd) {
        if(c[i] != NULL && gwconn.src_conns[i].link_type == NGX_NONE_LINK) {
            gwconn.cur_fd = i+1;
            gwconn.src_conns[i].link_type = type;
            gwconn.src_conns[i].src_link = link;
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

    gwconn.connections[c->fd] = c;

    ngx_post_event(c->read, &ngx_posted_events);
}

static void
ngx_stream_gwproxy_downstream_handler(ngx_event_t *ev)
{
    ssize_t               n;
    ngx_stream_session_t  *s;
    ngx_connection_t      *c;
    ngx_src_conn_t        *sc;
    u_char                buf[1024];

    if (ev->timedout) { 
        /* timeout expired */ 
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
            "(%s): gw downstream timeout expired", __FUNCTION__);
    }

    c = ev->data;
    s = c->data;
    sc = &gwconn.src_conns[c->fd];

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
            gwconn.src_conns[c->fd].link_type = NGX_NONE_LINK;
            gwconn.src_conns[c->fd].src_link = NULL;

            ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                "(%s): gw downstream recv error", __FUNCTION__);
                return;
        }

        /* process buf */
        if (n > 0) {
            if(sc->link_type == NGX_STREAM_CONNECTION_LINK && sc->src_link) {
                ngx_connection_t *scl = sc->src_link;
                scl->send(scl, buf, n);
            }
            else if(sc->link_type == NGX_HTTP_REQUEST_LINK && sc->src_link) {
                ngx_http_request_t *scr = sc->src_link;
                if(scr) {
                    scr->connection->send(scr->connection, buf, n);
                    //ngx_str_t retstr = ngx_string("HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=ISO-8859-1\r\nContent-Length: 100\r\n\r\n<html><head><title>Ngx gwproxy module test</title></head><body><h1>Hello test from nginx!</h1></body></html>\r\n");
                    //scr->connection->send(scr->connection, retstr.data, retstr.len);
                    //ngx_http_finalize_request(scr, NGX_DONE);
                }

                sc->link_type = NGX_NONE_LINK;
                sc->src_link = NULL;
            }
            else {
                ngx_log_debug1(NGX_LOG_DEBUG_STREAM, ev->log, 0,
                    "(%s): gw downstream no src connection found", __FUNCTION__);
            }
        }
        else {
            gwconn.connections[c->fd] = NULL;
            gwconn.src_conns[c->fd].link_type = NGX_NONE_LINK;
            gwconn.src_conns[c->fd].src_link = NULL;
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

