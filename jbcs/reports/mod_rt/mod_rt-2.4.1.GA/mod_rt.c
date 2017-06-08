/* This module implements End-user response time for Apache 2.0.  The concept
 * is simple.  The user must add an SSI tag to their web page, <!-- CAM -->,
 * which this module detects and replaces with a tag:
 *
 * <img src=CAM_blank.gif?t=(start_time)&v=(URL)
 *
 * When the browser then makes a request for CAM_blank.gif, this module 
 * intercepts that request and serves it as a static file (a transparant
 * single pixel file.)  This module also logs the response time for 
 * CAM_blank.gif so that we know how long the request took from the first
 * request until the request for the blank image.
 */
#include "mod_log_config.h"
#include "util_filter.h"
#include "mod_include.h"
#include "apr_strings.h"
#include "apr_buckets.h"
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_request.h"

static const char GIF_IMG[] = {'G', 'I', 'F', '8', '9', 'a', 0x01, 0x00, 0x01,
                               0x00, 0x80, 0x00, 0x00, 0xdb, 0xdf, 0xef, 0x00,
                               0x00, 0x00, 0x21, 0xf9, 0x04, 0x01, 0x00, 0x00,
                               0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x01,
                               0x00, 0x01, 0x00, 0x00, 0x02, 0x02, 0x44, 0x01,
                               0x00, 0x3b};

module AP_MODULE_DECLARE_DATA rt2_module;
static APR_OPTIONAL_FN_TYPE(ap_ssi_get_tag_and_value) *get_tag_value;
static APR_OPTIONAL_FN_TYPE(ap_ssi_parse_string) *parse_string;

/* XXX:  This is a hack.  We are including the structures from mod_log_config
 * in our module.  This means 2 things.  1)  We can find the log file name
 * and the log format easily.  2)  If mod_log_config changes it's structures,
 * our module may need to change.
 */
typedef struct {
    const char *default_format_string;
    apr_array_header_t *default_format;
    apr_array_header_t *config_logs;
    apr_array_header_t *server_config_logs;
    apr_table_t *formats;
} multi_log_state;

typedef struct {
    ap_log_handler_fn_t *func;
    char *arg;
    int condition_sense;
    int want_orig;
    apr_array_header_t *conditions;
} log_format_item;

typedef struct {
    const char *fname;
    const char *format_string;
    apr_array_header_t *format;
    void *log_writer;
    char *condition_var;
} config_log_state;
/* XXX End hack */


static int check_find_log_access(request_rec *r)
{
    const char *remotehost;
    int remotehost_is_ip;

    if (strcmp(r->uri, "/CAM_log_file")) {
        return DECLINED;
    }

    remotehost = ap_get_remote_host(r->connection, r->per_dir_config,
                                    REMOTE_NOLOOKUP, &remotehost_is_ip);
    if (!strcmp(remotehost, "127.0.0.1")) {
        return OK;
    }
    return HTTP_FORBIDDEN;
}

/* If the page that was requested is /CAM_blank.gif, then we need to log the
 * end-user response time.
 */
static const char *log_handler_S(request_rec *r, char *a)
{
    apr_int64_t orig_time;
    const char *qstr;
    char *tstr = apr_pcalloc(r->pool, 256);
    const char *timestr;    
    char *vhoststr;
    char *endstr;
    request_rec *first = r;
    request_rec *final = r;
    
    while (first->prev) {
        first = first->prev;
    }
    
    /* We have to divide by 1000 here, because the RT log is expected to be
     * in milliseconds, but gettimeofday is giving us nanoseconds.  So, we
     * do the conversion here, so that RT doesn't get dates in 35393.
     */
    apr_snprintf(tstr, 256, "%s %"APR_INT64_T_FMT" %"APR_INT64_T_FMT" %d %s",  
                 apr_uri_unparse(r->pool, &first->parsed_uri, 0),
                 r->request_time / 1000, apr_time_now() - r->request_time, 
#if AP_MODULE_MAGIC_AT_LEAST(20111130,0)
                 r->status, r->connection->client_ip);
#else
                 r->status, r->connection->remote_ip);
#endif

    return tstr;
}

/* If the page that was requested is /CAM_blank.gif, then we need to log the
 * end-user response time.
 */
static const char *log_handler_R(request_rec *r, char *a)
{
    apr_int64_t orig_time;
    const char *qstr;
    char *tstr = apr_pcalloc(r->pool, 256);
    const char *timestr;    
    char *vhoststr;
    char *endstr;

    if (strcmp(r->uri, "/CAM_blank.gif")) {
        return NULL;
    }

    if (!r->args) {
        return NULL;
    }

    qstr = r->args;
    timestr = ap_strchr_c(qstr, 't') + 2;
    vhoststr = ap_strchr_c(timestr, '&');

    orig_time = apr_strtoi64(timestr, &endstr, 10);

    apr_snprintf(tstr, 256, "%s %"APR_INT64_T_FMT" %"APR_INT64_T_FMT" %d %s",  
                 vhoststr + 3, apr_time_as_msec(orig_time), 
                 r->request_time - orig_time, r->status,
#if AP_MODULE_MAGIC_AT_LEAST(20111130,0)
                 r->connection->client_ip);
#else
                 r->connection->remote_ip);
#endif

    return tstr;
}

static int rt_handler(request_rec *r)
{
    if (strcmp(r->uri, "/CAM_blank.gif")) {
        return DECLINED;
    }

    ap_rwrite(GIF_IMG, sizeof(GIF_IMG), r);
    return OK;
}

static apr_status_t handle_cam(include_ctx_t *ctx,
                      ap_filter_t *f, apr_bucket_brigade *head_ptr)
{
    apr_bucket *tmp_bucket;
    request_rec *r = f->r;
    char *tstr = apr_palloc(r->pool, 256);

    /* We have to use the first request in the chain.  This ensures that the
     * URI we log and the URI that is logged by %r (in the access log) are
     * identical.
     */
    request_rec *first = r;

    while (first->main) {
        first = first->main;
    }
    while (first->prev) {
        first = first->prev;
    }
    
    tmp_bucket = apr_bucket_immortal_create("<img src=\"/CAM_blank.gif?t=",
                                         strlen("<img src=\"/CAM_blank.gif?t="),
                                         r->connection->bucket_alloc);
    APR_BRIGADE_INSERT_HEAD(head_ptr, tmp_bucket);

    apr_snprintf(tstr, 256, "%"APR_INT64_T_FMT"&v=%s\"/>", r->request_time,
                 apr_uri_unparse(r->pool, &first->parsed_uri, 0));
    tmp_bucket = apr_bucket_pool_create(tstr, strlen(tstr), r->pool,
                                        r->connection->bucket_alloc);
    APR_BRIGADE_INSERT_HEAD(head_ptr, tmp_bucket);
    
    return 0;
}

static int rt_post_config(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp,
                          server_rec *s)
{
    APR_OPTIONAL_FN_TYPE(ap_register_include_handler) *reg_with_ssi;

    reg_with_ssi = APR_RETRIEVE_OPTIONAL_FN(ap_register_include_handler);
    get_tag_value = APR_RETRIEVE_OPTIONAL_FN(ap_ssi_get_tag_and_value);
    parse_string = APR_RETRIEVE_OPTIONAL_FN(ap_ssi_parse_string);

    if ((reg_with_ssi) && (get_tag_value) && (parse_string)) {
        /* Required by mod_include filter. This is how mod_cgi registers
         *   with mod_include to provide processing of the exec directive.
         */
        reg_with_ssi("cam", handle_cam);
    }

    return OK;
}

static void register_hooks(apr_pool_t *p)
{
    static const char * const pre[] = {"mod_include.c", NULL};

    /* Everything from here to the next comment is for logging End User
     * response time.
     */
    APR_OPTIONAL_FN_TYPE(ap_register_log_handler) *log_pfn_register;
    log_pfn_register = APR_RETRIEVE_OPTIONAL_FN(ap_register_log_handler);

    if (log_pfn_register) {
        log_pfn_register(p, "R", log_handler_R, 0);
        log_pfn_register(p, "S", log_handler_S, 0);
    }

    ap_hook_post_config(rt_post_config, pre, NULL, APR_HOOK_MIDDLE);
    ap_hook_handler(rt_handler, NULL, NULL, APR_HOOK_MIDDLE);

    /* These functions are required for finding the current log file and the
     * log format.
     */
    ap_hook_access_checker(check_find_log_access, NULL, NULL, APR_HOOK_MIDDLE);
}

module AP_MODULE_DECLARE_DATA rt_module =
{
    STANDARD20_MODULE_STUFF,
    NULL,                       /* create per-dir config */
    NULL,                       /* merge per-dir config */
    NULL,                       /* server config */
    NULL,                       /* merge server config */
    NULL,                       /* command table */
    register_hooks              /* register hooks */
};
