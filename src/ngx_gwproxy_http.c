
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_http_gwproxy_module;
extern ngx_gwproxy_conn_t gwconn;

static ngx_int_t ngx_http_gwproxy_create_request(ngx_http_request_t *r);

ngx_int_t ngx_http_gwproxy_handler(ngx_http_request_t *r)
{
    ngx_connection_t *dc;

    dc = ngx_gwproxy_get_gw_connection_with_set_srclink(NGX_HTTP_REQUEST_LINK, r);
    if(!dc) {
        ngx_log_debug1(NGX_LOG_DEBUG_STREAM, r->connection->log, 0, 
            "(%s): http gwproxy no find active gw connection", __FUNCTION__);
    }
    else if(ngx_http_gwproxy_create_request(r) == NGX_OK) {
        ngx_buf_t *request_buf = r->out->buf;
        dc->send(dc, request_buf->pos, request_buf->last-request_buf->pos);
        //gwconn.src_conns[dc->fd].link_type = NGX_NONE_LINK;
        //gwconn.src_conns[dc->fd].src_link = NULL;
    }

    //ngx_str_t retstr = ngx_string("HTTP/1.1 200 OK\r\nContent-Type: text/html;charset=ISO-8859-1\r\nContent-Length: 100\r\n\r\n<html><head><title>Ngx gwproxy module test</title></head><body><h1>Hello test from nginx!</h1></body></html>\r\n");
    //r->connection->send(r->connection, retstr.data, retstr.len);
    //ngx_http_finalize_request(scr, NGX_DONE);

    return NGX_DONE;
}


static ngx_int_t
ngx_http_gwproxy_create_request(ngx_http_request_t *r)
{
    ngx_uint_t                    i;
    ngx_str_t                     method;
    size_t                        len;
    ngx_buf_t                    *b;
    ngx_chain_t                  *cl;
    ngx_list_part_t              *part;
    ngx_table_elt_t              *header;
    //ngx_http_script_engine_t      e, le;

    char  ngx_http_gwproxy_version[] = " HTTP/1.1" CRLF;

    method = r->method_name;
    len = method.len + 1 + sizeof(ngx_http_gwproxy_version) - 1 + sizeof(CRLF) - 1;

    len += r->uri.len + sizeof("?") - 1 + r->args.len;


    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        len += header[i].key.len + sizeof(": ") - 1
                + header[i].value.len + sizeof(CRLF) - 1;
    }

    //ngx_memzero(&le, sizeof(ngx_http_script_engine_t));

    b = ngx_create_temp_buf(r->pool, len);
    if (b == NULL) {
        return NGX_ERROR;
    }

    cl = ngx_alloc_chain_link(r->pool);
    if (cl == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;

    /* the request line */
    b->last = ngx_copy(b->last, method.data, method.len);
    *b->last++ = ' ';

    b->last = ngx_copy(b->last, r->uri.data, r->uri.len);

    if (r->args.len > 0) {
        *b->last++ = '?';
        b->last = ngx_copy(b->last, r->args.data, r->args.len);
    }

    b->last = ngx_cpymem(b->last, ngx_http_gwproxy_version,
    sizeof(ngx_http_gwproxy_version) - 1);


    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        b->last = ngx_copy(b->last, header[i].key.data, header[i].key.len);
        *b->last++ = ':'; *b->last++ = ' ';
        b->last = ngx_copy(b->last, header[i].value.data, header[i].value.len);
        *b->last++ = CR; *b->last++ = LF;

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                        "http proxy header: \"%V: %V\"",
                        &header[i].key, &header[i].value);
    }

    /* add "\r\n" at the header end */
    *b->last++ = CR; *b->last++ = LF;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                    "http proxy header:%N\"%*s\"",
                    (size_t) (b->last - b->pos), b->pos);

    r->out = cl;
    b->flush = 1;
    cl->next = NULL;

     return NGX_OK;
}


