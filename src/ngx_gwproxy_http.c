
#include "ngx_gwproxy_module.h"

extern ngx_module_t  ngx_http_gwproxy_module;

ngx_int_t ngx_http_gwproxy_handler(ngx_http_request_t *r)
{
		return NGX_DECLINED;
}

