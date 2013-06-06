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
    ZEND_ME(ffmpeg, has_video, NULL, 0)
    ZEND_ME(ffmpeg, has_audio, NULL, 0)
    ZEND_ME(ffmpeg, get_media_video_bitrate, NULL, 0)
    ZEND_ME(ffmpeg, get_media_audio_bitrate, NULL, 0)
    ZEND_ME(ffmpeg, get_media_frame_height, NULL, 0)
    ZEND_ME(ffmpeg, get_media_frame_width, NULL, 0)
    ZEND_ME(ffmpeg, get_media_video_codec, NULL, 0)
    ZEND_ME(ffmpeg, get_media_audio_codec, NULL, 0)
    {NULL, NULL, NULL, 0, 0}
};

static int ffmpeg_media_handler;
static zend_class_entry ffmpeg_class_entry;
static zend_class_entry *pffmpeg_class_entry;

void _ffmpeg_media_register_class(int module_number);

static ffmpeg_media_context* _ffmpeg_media_context_malloc_mem();
static int _ffmpeg_media_open_context(struct ffmpeg_media_context *pfmctx, const char *uri_name);
static void _ffmpeg_media_free_context(struct ffmpeg_media_context *pfmctx);
static void _php_ffmpeg_media_free_context(zend_rsrc_list_entry *rsrc TSRMLS_DC);

static int _ffmpeg_media_fetch_resource(struct ffmpeg_media_context* pfmctx);
static AVStream* _ffmpeg_media_get_streams(struct AVFormatContext *av_fmt_ctx, int stream_index);
static uint64_t _ffmpeg_media_get_duration(struct AVFormatContext *av_fmt_ctx);
static bool _ffmpeg_media_has_media_type(struct AVFormatContext *av_fmt_ctx, enum AVMediaType media_type);
static double _ffmpeg_media_get_bitrate(struct AVFormatContext *av_fmt_ctx, enum AVMediaType media_type);
static long _ffmpeg_media_get_video_frame_shape(struct AVFormatContext *av_fmt_ctx, const char* shape_name);
static void _ffmpeg_media_get_codec_name(struct AVFormatContext *av_fmt_ctx, enum AVMediaType media_type, char *codec_name);

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
    int index = 0;
    int stream_count = 0;
    struct AVFormatContext *av_fmt_ctx = NULL;
    struct AVStream *av_stream = NULL;
    struct AVCodecContext *av_ctx = NULL;

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

     // get stream count and judge to next step
     av_fmt_ctx = pfmctx->av_fmt_ctx;
     stream_count = av_fmt_ctx->nb_streams;
     if (stream_count <= 0) {
         zend_error(E_ERROR, "stream count is invalid.\n");
         return -1;
     }

     // open and find stream info
     res = avformat_find_stream_info(av_fmt_ctx, NULL);
     if (res < 0) {
         zend_error(E_ERROR, "can not find media stream information.\n");
         avformat_close_input(&av_fmt_ctx);
         pfmctx->av_fmt_ctx = NULL;
         return -1;
     }

     for (index = 0; index < stream_count; index ++) {
         struct AVCodec *av_codec = NULL;
         av_stream = av_fmt_ctx->streams[index];
         av_ctx = av_stream->codec;

         if (av_ctx->codec_id == CODEC_ID_NONE) {
             zend_error(E_ERROR, "can not get media file decoder id.\n");
             return -1;
         } else {
             av_codec = avcodec_find_decoder(av_ctx->codec_id);
             if (av_codec == NULL) {
                 zend_error(E_ERROR, "can not find decoder by decoder id");
                 return -1;
             }

             res = avcodec_open2(av_ctx, av_codec, NULL);
             if (res < 0) {
                 zend_error(E_ERROR, "can not open decoder.\n");
                 return -1;
             }
         }
     }

    return 0;
}

// get stream information by stream index
static AVStream* _ffmpeg_media_get_streams(AVFormatContext *av_fmt_ctx, int stream_index)
{
    int total_streams = 0;

    // Check if av_fmt_ctx is NULL
    if (av_fmt_ctx == NULL) {
        return NULL;
    }

    // Check total streams
    total_streams  = av_fmt_ctx->nb_streams;
    if (total_streams == 0 || stream_index < total_streams) {
        return NULL;
    }

    return av_fmt_ctx->streams[stream_index];
}

// a destructor of free self define context
static void _ffmpeg_media_free_context(struct ffmpeg_media_context *pfmctx)
{
    int index = 0;
    int stream_count = 0;
    struct AVFormatContext *av_fmt_ctx = NULL;
    struct AVCodecContext *av_ctx = NULL;

    if (pfmctx && pfmctx->av_fmt_ctx) {
        av_fmt_ctx = pfmctx->av_fmt_ctx;
        stream_count = av_fmt_ctx->nb_streams;

        for (index = 0; index < stream_count; index++) {
            av_ctx = av_fmt_ctx->streams[index]->codec;
            avcodec_close(av_ctx);
        }

        avformat_close_input(&av_fmt_ctx);
        av_fmt_ctx = NULL;
        efree(pfmctx);
    }
}

// free media alloc memory
static void _php_ffmpeg_media_free_context(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    struct ffmpeg_media_context *pfmctx = NULL;
    pfmctx = (struct ffmpeg_media_context *) rsrc->ptr;
    _ffmpeg_media_free_context(pfmctx);
}

static uint64_t _ffmpeg_media_get_duration(AVFormatContext *av_fmt_ctx)
{
    // get if parameter av_fmt_ctx is null
    if (av_fmt_ctx == NULL) {
        return 0;
    } else {
        return av_fmt_ctx->duration / AV_TIME_BASE;
    }
}

// check if media file has video or audio
static bool _ffmpeg_media_has_media_type(struct AVFormatContext *av_fmt_ctx, enum AVMediaType media_type)
{
    bool has_media_type = false;
    int index = 0;
    int stream_count = 0;
    struct AVCodecContext *pstream_codec = NULL;

    if (av_fmt_ctx == NULL) {
        return false;
    } else {
        stream_count = av_fmt_ctx->nb_streams;
        for (index = 0; index < stream_count; index ++) {
            pstream_codec = av_fmt_ctx->streams[index]->codec;
            if (pstream_codec->codec_type == media_type) {
                has_media_type = true;
                break;
            }
        }
        return has_media_type;
    }
}

// get video and audio bitrate
static double _ffmpeg_media_get_bitrate(struct AVFormatContext *av_fmt_ctx, enum AVMediaType media_type)
{
    bool has_media_type = false;
    int index = 0;
    int stream_count = 0;
    double media_bitrate = 0.0;
    struct AVCodecContext *pstream_codec = NULL;

    if (av_fmt_ctx == NULL) {
        return 0.0;
    } else {
        stream_count = av_fmt_ctx->nb_streams;
        for (index = 0; index < stream_count; index ++) {
            pstream_codec = av_fmt_ctx->streams[index]->codec;
            if (pstream_codec->codec_type == media_type) {
                has_media_type == true;
                media_bitrate = (double) av_fmt_ctx->streams[index]->codec->bit_rate;
                break;
            }
        }
    }

    return media_bitrate;
}

// get video width and height
static long _ffmpeg_media_get_video_frame_shape(struct AVFormatContext *av_fmt_ctx, const char* shape_name)
{
    bool has_video = false;
    int index = 0;
    int stream_count = 0;
    int shape_value = 0;
    struct AVCodecContext *pstream_codec = NULL;

    if (av_fmt_ctx == NULL) {
        return 0;
    } else {
        stream_count = av_fmt_ctx->nb_streams;
        for (index = 0; index < stream_count; index ++) {
            pstream_codec = av_fmt_ctx->streams[index]->codec;
            if (pstream_codec->codec_type == AVMEDIA_TYPE_VIDEO) {
                has_video = true;
                break;
            }
        }

        if (has_video == false) {
            return 0;
        } else {
           if (strcmp(shape_name, "width")) {
               return av_fmt_ctx->streams[index]->codec->width;
           } else if (strcmp(shape_name, "height")) {
               return av_fmt_ctx->streams[index]->codec->height;
           } else {
               return 0;
           }
        }
    }
}

// get video codec name or audio codec name
static void _ffmpeg_media_get_codec_name(struct AVFormatContext *av_fmt_ctx, enum AVMediaType media_type, char *codec_name)
{
    int index = 0;
    int stream_count = 0;
    struct AVStream *av_stream = NULL;
    struct AVCodecContext *av_ctx = NULL;
    struct AVCodec *av_codec = NULL;

    if (codec_name == NULL) {
        return;
    }

    if (av_fmt_ctx == NULL 
                || media_type != AVMEDIA_TYPE_VIDEO 
                || media_type != AVMEDIA_TYPE_AUDIO) {
        strncpy(codec_name, "", strlen(""));
        return;
    }

    stream_count = av_fmt_ctx->nb_streams;
    for (index = 0; index < stream_count; ++ index) {
        av_stream = av_fmt_ctx->streams[index];
        av_ctx = av_stream->codec;
        if (av_ctx->codec_type == media_type) {
            av_codec = av_ctx->codec;
            strncpy(codec_name, av_codec->name, strlen(av_codec->name));
            return;
        }
    }

    return;
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
    if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "s", &uri_name, &uri_len) != SUCCESS) {
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
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_DOUBLE(_ffmpeg_media_get_duration(pfmctx->av_fmt_ctx));    
}

// function to check if media file has video
ZEND_METHOD(ffmpeg, has_video)
{
    int res = 0;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_BOOL(_ffmpeg_media_has_media_type(pfmctx->av_fmt_ctx, AVMEDIA_TYPE_VIDEO));
}

// function to check if media file has audio
ZEND_METHOD(ffmpeg, has_audio)
{
    int res = 0;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_BOOL(_ffmpeg_media_has_media_type(pfmctx->av_fmt_ctx, AVMEDIA_TYPE_AUDIO));
}

ZEND_METHOD(ffmpeg, get_media_video_bitrate)
{
    int res = 0;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_DOUBLE(_ffmpeg_media_get_bitrate(pfmctx->av_fmt_ctx, AVMEDIA_TYPE_VIDEO));
}

ZEND_METHOD(ffmpeg, get_media_audio_bitrate)
{
    int res = 0;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_DOUBLE(_ffmpeg_media_get_bitrate(pfmctx->av_fmt_ctx, AVMEDIA_TYPE_AUDIO));
}

ZEND_METHOD(ffmpeg, get_media_frame_height)
{
    int res = 0;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_LONG(_ffmpeg_media_get_video_frame_shape(pfmctx->av_fmt_ctx, "height"));
}

ZEND_METHOD(ffmpeg, get_media_frame_width)
{
    int res = 0;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_LONG(_ffmpeg_media_get_video_frame_shape(pfmctx->av_fmt_ctx, "width"));
}

ZEND_METHOD(ffmpeg, get_media_video_codec)
{
    int res = 0;
    zval **resource = NULL;
    char codec_name[32] = {0};
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    _ffmpeg_media_get_media_codec(pfmctx->av_fmt_ctx, AVMEDIA_TYPE_VIDEO, codec_name);
    RETURN_STRING(codec_name, 0);
}

ZEND_METHOD(ffmpeg, get_media_audio_codec)
{
    int res = 0;
    zval **resource = NULL;
    char codec_name[32] = {0};
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    _ffmpeg_media_get_media_codec(pfmctx->av_fmt_ctx, AVMEDIA_TYPE_AUDIO, codec_name);
    RETURN_STRING(codec_name, 0);
}
