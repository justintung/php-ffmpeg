/* php_ffmpeg_utility.c is a function file
 * which include calling the ffmpeg api
 *
 *
 * create time : 2013-06-01
 * author : xinlinrong
 */

#include "php_ffmpeg.h"

zend_function_entry ffmpeg_class_functions_entry[] = {
    ZEND_ME(ffmpeg, __construct, NULL, 0)
    ZEND_ME(ffmpeg, get_media_duration, NULL, 0)
    {NULL, NULL, NULL, 0, 0}
};

static int ffmpeg_media_handler;
static zend_class_entry ffmpeg_class_entry;
static zend_class_entry *pffmpeg_class_entry;

static ffmpeg_media_context* _ffmpeg_media_context_malloc_mem();
static int _ffmpeg_media_open_context(struct ffmpeg_media_context *pfmctx, const char *uri_name);
static void _ffmpeg_media_free_context(struct ffmpeg_media_context *pfmctx);
static void _ffmpeg_media_free_context(struct ffmpeg_media_context *pfmctx);
static void _php_ffmpeg_media_free_context(zend_rsrc_list_entry *rsrc TSRMLS_DC);
void _ffmpeg_media_register_class(int module_number);

// alloc memory for ffmpeg AVFormatContext
static ffmpeg_media_context* _ffmpeg_media_context_malloc_mem()
{
    struct ffmpeg_media_context *pfmctx = NULL;
    pfmctx = (struct ffmpeg_media_context *) emalloc(sizeof(struct ffmpeg_media_context));
    if (pfmctx == NULL) {
        return NULL;
    } else {
        pfmctx->av_fmt_ctx = NULL;
        pfmctx->php_resource_id = -1;
        return pfmctx;
    }
}

// use ffmpeg api to open media context
static int _ffmpeg_media_open_context(struct ffmpeg_media_context *pfmctx, const char *uri_name)
{
    int res = 0;

    // check ffmpeg media context
    if (pfmctx == NULL) {
        zend_error(E_ERROR, "ffmpeg media context is NULL, can not open media context.\n");
        return -1;
    }

    // check uri name
    if (uri_name == NULL || strlen(uri_name) == 0) {
        zend_error(E_ERROR, "paramtere uri_name is invalid, can not open media context.\n");
        return -1;
    }

    // use avformat_alloc_context to alloc memory for context
    pfmctx->av_fmt_ctx = avformat_alloc_context();
    if (pfmctx->av_fmt_ctx == NULL) {
        zend_error(E_ERROR, "can not alloc memory for ffmpeg_media_context");
        return -1;
    }

    // open media context by given file name
    res = avformat_open_input(&(pfmctx->av_fmt_ctx), uri_name, NULL, NULL);
    if (res < 0) {
        zend_error(E_ERROR, "can not open media file and get context.\n");
        avfree(pfmctx->av_fmt_ctx);
        pfmctx->av_fmt_ctx = NULL;
        return -1;
    }

     // open and find stream info
     res = avformat_find_stream_info(pfmctx->av_fmt_ctx, NULL);
     if (res < 0) {
         zend_error(E_ERROR, "can not find media stream information.\n");
         avfree(pfmctx->av_fmt_ctx);
         pfmctx->av_fmt_ctx = NULL;
         return -1;
     }

    return 0;
}

// a destructor of free self define context
static void _ffmpeg_media_free_context(struct ffmpeg_media_context *pfmctx)
{
    if (pfmctx && pfmctx->av_fmt_ctx) {
        avformat_close_input(&(pfmctx->av_fmt_ctx));
        pfmctx->av_fmt_ctx = NULL;
        efree(pfmctx);
    }
}

static void _php_ffmpeg_media_free_context(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    struct ffmpeg_media_context *pfmctx = NULL;
    pfmctx = (struct ffmpeg_media_context *) rsrc->ptr;
    _ffmpeg_free_media_context(pfmctx);
}

void _ffmpeg_media_register_class(int module_number)
{
    // register class and method
    ffmpeg_media_handler = zend_register_list_destructors_ex(_php_ffmpeg_media_free_context, NULL, "ffmpeg initialize", module_number);
    INIT_CLASS_ENTRY(ffmpeg_class_entry, "ffmpeg_media", ffmpeg_class_functions_entry);
    pffmpeg_class_entry = zend_register_internal_class(&ffmpeg_class_entry TSRMLS_CC);
}

// construct function with ffmpeg media
ZEND_METHOD(ffmpeg, __construct)
{
    char *uri_name = NULL;
    int uri_len = 0;

    if (ZEND_NUM_ARGS() < 1) {
        zend_error(E_ERROR, "parameter count must be large zero.\n");
        RETURN_FALSE;
    }

    // parse parameters uri name
    if (zend_parse_parameteres_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "s", &uri_name, &uri_len) != SUCCESS) {
        zend_error(E_ERROR,"can not parse parameter.\n");
        RETURN_FALSE;
    }

    // allocate memory for struct pfmctx
    int res = 0;
    struct ffmpeg_media_context *pfmctx = NULL;
    pfmctx = _ffmpeg_media_context_malloc_mem();
    if (pfmctx == NULL) {
        zend_error(E_ERROR, "can not malloc memory for ffmpeg media context.\n");
        RETURN_FALSE;
    }

    // open ffmpeg media context
    res = _ffmpeg_media_open_context(pfmctx, uri_name);
    if (res < 0) {
        _ffmpeg_media_free_context(pfmctx);
        RETURN_FALSE;
    }

    // register resource from zend core
    pfmctx->php_resource_id = ZEND_REGISTER_RESOURCE(NULL, (struct ffmpeg_media_context *) pfmctx, ffmpeg_media_handler);
    object_init_ex(getThis(), pffmpeg_class_entry);
    add_property_resource(getThis(), FFMPEG_PHP_EXTNAME,pfmctx->php_resource_id);
}

// function to get duration
ZEND_METHOD(ffmpeg, get_media_duration)
{
    int res = 0;
    struct ffmpeg_media_context *pfmctx = NULL;

    //res = _ffmpeg_media_fetch_resource(pfmctx);
    if (res < 0) {
        RETURN_FALSE;
    } else {
        RETVAL_LONG(10);
    }
}
