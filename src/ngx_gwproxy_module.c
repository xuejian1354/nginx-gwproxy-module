
#include "ngx_gwproxy_module.h"

static void *ngx_gwproxy_create_srv_conf(ngx_conf_t *cf);
static ngx_int_t ngx_gwproxy_post_conf(ngx_conf_t *cf);
static char *ngx_stream_set_stream_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_stream_set_gw_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_set_gw_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


ngx_gwproxy_conn_t gwconn;

static ngx_command_t  ngx_stream_gwproxy_commands[] = {

    { ngx_string("stream_proxy"),
      NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_stream_set_stream_proxy,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("gw_proxy"),
      NGX_STREAM_SRV_CONF|NGX_CONF_FLAG,
      ngx_stream_set_gw_proxy,
      NGX_STREAM_SRV_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};


static ngx_stream_module_t  ngx_stream_gwproxy_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_gwproxy_post_conf,                 /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_gwproxy_create_srv_conf,           /* create server configuration */
    NULL                                   /* merge server configuration */
};


ngx_module_t  ngx_stream_gwproxy_module = {
    NGX_MODULE_V1,
    &ngx_stream_gwproxy_module_ctx,        /* module context */
    ngx_stream_gwproxy_commands,           /* module directives */
    NGX_STREAM_MODULE,                     /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_command_t  ngx_http_gwproxy_commands[] = {

    { ngx_string("http_proxy"),
      NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_http_set_gw_proxy,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    ngx_null_command
};

static ngx_http_module_t  ngx_http_gwproxy_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_gwproxy_create_srv_conf,           /* create location configuration */
    NULL                                   /* merge location configuration */
};

ngx_module_t  ngx_http_gwproxy_module = {
    NGX_MODULE_V1,
    &ngx_http_gwproxy_module_ctx,          /* module context */
    ngx_http_gwproxy_commands,             /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static void *
ngx_gwproxy_create_srv_conf(ngx_conf_t *cf)
{
    ngx_gwproxy_srv_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_gwproxy_srv_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->flag = NGX_CONF_UNSET;
    conf->gwflag = NGX_CONF_UNSET;

    return conf;
}


static ngx_int_t
ngx_gwproxy_post_conf(ngx_conf_t *cf)
{
    struct rlimit  rlmt;
    if (getrlimit(RLIMIT_NOFILE, &rlmt) == -1) {
        ngx_log_error(NGX_LOG_ALERT, cf->log, ngx_errno,
                        "getrlimit(RLIMIT_NOFILE) failed");
        return NGX_ERROR;
    }

    gwconn.connection_n = (ngx_uint_t) rlmt.rlim_cur;
    gwconn.connections = ngx_calloc(sizeof(ngx_connection_t *) * gwconn.connection_n,
                                        cf->log);
    if (gwconn.connections == NULL) {
        return NGX_ERROR;
    }

    gwconn.src_conns = ngx_calloc(sizeof(ngx_src_conn_t) * gwconn.connection_n,
                                    cf->log);
    if (gwconn.src_conns == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


static char *
ngx_stream_set_stream_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t        *value;
    ngx_stream_core_srv_conf_t *cscf;

    ngx_gwproxy_srv_conf_t  *gscf = conf;

    if (gscf && gscf->flag != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcasecmp(value[1].data, (u_char *) "on") == 0) {
        gscf->flag = 1;
        cscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_core_module);
        cscf->handler = ngx_stream_socks_proxy_handler;
    } else if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        gscf->flag = 0;
    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                        "invalid value \"%s\" in \"%s\" directive, "
                        "it must be \"on\" or \"off\"",
                        value[1].data, cmd->name.data);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_stream_set_gw_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t        *value;
    ngx_stream_core_srv_conf_t *cscf;

    ngx_gwproxy_srv_conf_t  *gscf = conf;

    if (gscf && gscf->flag != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcasecmp(value[1].data, (u_char *) "on") == 0) {
        gscf->gwflag = 1;
        cscf = ngx_stream_conf_get_module_srv_conf(cf, ngx_stream_core_module);
        cscf->handler = ngx_stream_gw_proxy_handler;
    } else if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        gscf->gwflag = 0;
    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "invalid value \"%s\" in \"%s\" directive, "
                    "it must be \"on\" or \"off\"",
                    value[1].data, cmd->name.data);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_set_gw_proxy(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t        *value;
    ngx_http_core_loc_conf_t   *clcf;

    ngx_gwproxy_srv_conf_t  *gscf = conf;

    if (gscf && gscf->flag != NGX_CONF_UNSET) {
        return "is duplicate";
    }

    value = cf->args->elts;

    if (ngx_strcasecmp(value[1].data, (u_char *) "on") == 0) {
        gscf->flag = 1;
        clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
        clcf->handler = ngx_http_gwproxy_handler;
    } else if (ngx_strcasecmp(value[1].data, (u_char *) "off") == 0) {
        gscf->flag = 0;
    } else {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "invalid value \"%s\" in \"%s\" directive, "
                    "it must be \"on\" or \"off\"",
                    value[1].data, cmd->name.data);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

