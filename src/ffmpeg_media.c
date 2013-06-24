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
    ZEND_ME(ffmpeg, get_media_frame_rate, NULL, 0)
    ZEND_ME(ffmpeg, get_media_image, NULL, 0)
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
static bool _ffmpeg_media_get_image(struct ffmpeg_media_context *pfmctx, int timestamp, char* image_path, int height, int width);

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
    int audio_stream_count = 0;
    struct AVFormatContext *av_fmt_ctx = NULL;
    struct AVStream *av_stream = NULL;
    struct AVCodecContext *av_ctx = NULL;
    enum CodecID cid = CODEC_ID_NONE;

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

    // initialize ffmpeg_media_context data;
    pfmctx->has_video = false;
    pfmctx->has_audio = false;
    pfmctx->has_extra_audio = false;

    // initialize video part
    pfmctx->duration = 0;
    pfmctx->video_stream_index = -1;
    pfmctx->width = 0;
    pfmctx->height = 0;
    pfmctx->video_bitrate = 0;
    pfmctx->frame_rate = 0.0;
    bzero(pfmctx->video_codec, sizeof(pfmctx->video_codec));

    // initialize audio part
    pfmctx->audio_stream_index = -1;
    pfmctx->audio_channels = 0;
    pfmctx->audio_sample_rate = 0;
    pfmctx->audio_bitrate = 0;
    bzero(pfmctx->audio_codec, sizeof(pfmctx->audio_codec));

    // for some media file which got mulitple audio stream
    pfmctx->extra_audio_stream_index = -1;
    pfmctx->extra_audio_channels = 0;
    pfmctx->extra_audio_sample_rate = 0;
    pfmctx->extra_audio_bitrate = 0;
    bzero(pfmctx->extra_audio_codec, sizeof(pfmctx->extra_audio_codec));

    pfmctx->duration = av_fmt_ctx->duration / AV_TIME_BASE;
    for (index = 0; index < stream_count; index ++) {
        struct AVCodec *av_codec = NULL;
        av_stream = av_fmt_ctx->streams[index];
        av_ctx = av_stream->codec;
        cid = av_ctx->codec_id;
        if (cid == CODEC_ID_NONE) {
            continue;
        } else {
            av_codec = avcodec_find_decoder(cid);
            if (av_codec == NULL) {
                zend_error(E_ERROR, "can not find decoder by decoder id.\n");
                return -1;
            }

            res = avcodec_open2(av_ctx, av_codec, NULL);
            if (AVERROR(res) < 0) {
                zend_error(E_ERROR, "can not open decoder.\n");
                return -1;
            }

            if (av_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                pfmctx->has_video = true;
                pfmctx->video_stream_index = index;
                pfmctx->width = av_ctx->width;
                pfmctx->height = av_ctx->height;
                pfmctx->video_bitrate = av_ctx->bit_rate;
                snprintf(pfmctx->video_codec, 64, "%s", av_codec->name);

                // get frame rate
                if (av_stream->r_frame_rate.num && av_stream->r_frame_rate.den) {
                    pfmctx->frame_rate = av_q2d(av_stream->r_frame_rate);
                } else if (av_stream->time_base.num && av_stream->time_base.den) {
                    pfmctx->frame_rate = 1.0 / av_q2d(av_stream->time_base);
                } else if (av_stream->codec->time_base.num && av_stream->codec->time_base.den) {
                    pfmctx->frame_rate = 1.0 / av_q2d(av_stream->codec->time_base);
                }
            } else if (av_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_count = audio_stream_count + 1;
                if (audio_stream_count == 2) {
                    pfmctx->has_extra_audio = true;
                    pfmctx->extra_audio_stream_index = index;
                    pfmctx->extra_audio_sample_rate = av_ctx->sample_rate;
                    pfmctx->extra_audio_channels = av_ctx->channels;
                    pfmctx->extra_audio_bitrate = av_ctx->bit_rate;
                    snprintf(pfmctx->extra_audio_codec, 64, "%s", av_codec->name);
                } else {
                    pfmctx->has_audio = true;
                    pfmctx->audio_stream_index = index;
                    pfmctx->audio_sample_rate = av_ctx->sample_rate;
                    pfmctx->audio_channels = av_ctx->channels;
                    pfmctx->audio_bitrate = av_ctx->bit_rate;
                    snprintf(pfmctx->audio_codec, 64, "%s", av_codec->name);
                }
            }
        }
    }

    return 0;
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

static bool _ffmpeg_media_get_image (struct ffmpeg_media_context* pfmctx, int timestamp, char *image_path, int width, int height)
{
    int res = 0;
    bool result = true;
    bool has_video = false;
    struct AVFormatContext *av_fmt_ctx = NULL, *av_out_fmt_ctx = NULL;
    struct AVOutputFormat *av_out_fmt = NULL;
    struct AVStream *av_in_stream = NULL, *av_out_stream = NULL;
    struct AVCodecContext *av_in_codec_ctx = NULL, *av_out_codec_ctx = NULL;
    struct AVCodec *av_out_codec = NULL;
    enum CodecID cid = CODEC_ID_NONE;

    av_fmt_ctx = pfmctx->av_fmt_ctx;
    if (av_fmt_ctx == NULL) {
        zend_error(E_ERROR, "av format context can not be NULL.\n");
        return false;
    }

    // alloc av output format, free by av_freep
    av_out_fmt = av_guess_format(NULL, image_path, NULL);
    if (av_out_fmt == NULL) {
       zend_error(E_ERROR, "can not guess image output format"); 
       return false;
    }

    // alloc av codec
    cid = av_guess_codec(av_out_fmt, NULL, image_path, NULL, AVMEDIA_TYPE_VIDEO);
    if (cid == CODEC_ID_NONE) {
        zend_error(E_ERROR, "can not get outptu codec id");
        return false;
    }

    // alloc av encoder, free by av_freep
    av_out_codec = avcodec_find_encoder(cid);
    if (av_out_codec == NULL) {
        zend_error(E_ERROR, "can not find encoder by encode id");
        return false;
    }

    // alloc avformat context
    res = avformat_alloc_output_context2(&av_out_fmt_ctx, av_out_fmt, NULL, image_path);
    if (AVERROR(res) < 0) {
        zend_error(E_ERROR, "can not alloc output format context.\n");
        return false;
    }

    res = avio_open(&(av_out_fmt_ctx->pb), image_path, AVIO_FLAG_WRITE);
    if (AVERROR(res) < 0) {
        zend_error(E_ERROR, "can not open media io.\n");
        result = false;
        goto error;
    }

    // alloc stream
    av_out_stream = avformat_new_stream(av_out_fmt_ctx, av_out_codec);
    if (av_out_stream == NULL) {
        zend_error(E_ERROR, "can not alloc stream.\n");
        result = false;
        goto error;
    }

    // initialize codec context data
    av_in_stream = av_fmt_ctx->streams[pfmctx->video_stream_index];
    av_in_codec_ctx = av_in_stream->codec;
    av_out_codec_ctx = av_out_stream->codec;
    av_out_codec_ctx->time_base.num = av_in_codec_ctx->time_base.num;
    av_out_codec_ctx->time_base.den = av_in_codec_ctx->time_base.den;
    av_out_codec_ctx->width = av_in_codec_ctx->width;
    av_out_codec_ctx->height = av_in_codec_ctx->height;
    av_out_codec_ctx->pix_fmt = PIX_FMT_YUVJ420P;

    res = avcodec_open2(av_out_codec_ctx, av_out_codec, NULL);
    if (AVERROR(res) < 0) {
        zend_error(E_ERROR, "can not open encoder.\n");
        result = false;
        goto error;
    }

    res = avformat_write_header(av_out_fmt_ctx, NULL);
    if (AVERROR(res) < 0) {
        zend_error(E_ERROR, "can not write header.\n");
        result = false;
        goto error;
    }

    uint8_t *bit_buffer = NULL;
    int bit_buffer_size = 1024 * 1024;
    int video_stream_index = 0;
    int get_picture = 0;
    struct AVPacket outavpkt;
    struct AVFrame av_decode_frame;

    int64_t actual_timestamp = 0;
    actual_timestamp = av_rescale_q(timestamp, AV_TIME_BASE_Q, av_in_stream->time_base);
    res = av_seek_frame(av_fmt_ctx, pfmctx->video_stream_index, actual_timestamp, AVSEEK_FLAG_BACKWARD);
    if (AVERROR(res) < 0) {
        zend_error(E_ERROR, "can not seek frame by timestamp.\n");
        result = false;
        goto error;
    }

    while (true) {
        struct AVPacket avpkt;
        av_init_packet(&avpkt);

        res = av_read_frame(av_fmt_ctx, &avpkt);
        if (AVERROR(res) == EAGAIN) {
            continue;
        } else if (AVERROR(res) < 0){
            break;
        }

        av_in_stream = av_fmt_ctx->streams[avpkt.stream_index];
        av_in_codec_ctx = av_in_stream->codec;
        if (av_in_codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            res = avcodec_decode_video2(av_in_codec_ctx, &av_decode_frame, &get_picture, &avpkt);
            if (res >= 0 && get_picture > 0) {
                if (avpkt.pts >= actual_timestamp) {
                    av_free_packet(&avpkt);
                    break;
                }
            }
        } else {
            av_free_packet(&avpkt);
        }
    }

    bit_buffer = (uint8_t *) malloc(bit_buffer_size * sizeof(uint8_t));
    res = avcodec_encode_video(av_out_codec_ctx, bit_buffer, bit_buffer_size, &av_decode_frame);
    if (res < 0) {
        frpintf(stdout, "encoder frame error.\n");
        goto error;
    }

    av_init_packet(&outavpkt);
    outavpkt.stream_index = 0;
    outavpkt.data = bit_buffer;
    outavpkt.size = res;
 
    if (av_decode_frame.key_frame) {
        outavpkt.flags |= AV_PKT_FLAG_KEY;
    }
 
    if (av_decode_frame.pts != AV_NOPTS_VALUE) {
        outavpkt.pts = av_rescale_q(av_decode_frame.pts, av_out_codec_ctx->time_base, av_in_stream->time_base);
    }

    res = av_interleaved_write_frame(av_out_fmt_ctx, &outavpkt);
    if (AVERROR(res) < 0) {
        zend_error(E_ERROR, "can not write frame.\n");
        result = false;
        goto error;
    }

    res = av_write_trailer(av_out_fmt_ctx);
    if (AVERROR(res) < 0) {
        zend_error(E_ERROR, "can not write trailer.\n");
        result = false;
        goto error;
    }

error:
    if (bit_buffer) {
        free(bit_buffer);
    }

    if (av_out_fmt_ctx->pb) {
        avio_flush(av_out_fmt_ctx->pb);
        avio_close(av_out_fmt_ctx->pb);
    }

    if (avcodec_is_open(av_out_codec_ctx) < 0) {
        avcodec_close(av_out_codec_ctx);
    }

    if (av_out_fmt_ctx) {
        avformat_free_context(av_out_fmt_ctx);
    }

    return result;
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

    RETURN_DOUBLE(pfmctx->duration);    
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

    RETURN_BOOL(pfmctx->has_video);
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

    RETURN_BOOL(pfmctx->has_audio);
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

    RETURN_DOUBLE(pfmctx->video_bitrate);
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

    RETURN_DOUBLE(pfmctx->audio_bitrate);
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

    RETURN_LONG(pfmctx->height);
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

    RETURN_LONG(pfmctx->width);
}

ZEND_METHOD(ffmpeg, get_media_video_codec)
{
    int res = 0;
    zval **resource = NULL;
    char codec_name[32] = "";
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_STRINGL(pfmctx->video_codec, strlen(pfmctx->video_codec), true); // use RETURN_STRINGL is more safe and avoid the segement fault
}

ZEND_METHOD(ffmpeg, get_media_audio_codec)
{
    int res = 0;
    zval **resource = NULL;
    char codec_name[32] = "";
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_STRINGL(pfmctx->audio_codec, strlen(pfmctx->audio_codec), true); // use RETURN_STRINGL is more safe and avoid the segement fault
}

ZEND_METHOD(ffmpeg, get_media_frame_rate)
{
    int res = 0;
    double frame_rate = 0.0;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    RETURN_DOUBLE(pfmctx->frame_rate);
}

ZEND_METHOD(ffmpeg, get_media_image)
{
    bool res = 0;
    int timestamp = 0; 
    int path_len = 0;
    int width = 0, height = 0;
    char *image_path = NULL;
    zval **resource = NULL;
    struct ffmpeg_media_context *pfmctx = NULL;

    if (ZEND_NUM_ARGS() < 2) {
        zend_error(E_ERROR, "parameter count can not less than 2.\n");
        RETURN_FALSE;
    }

    if (zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "ls", &timestamp, &image_path, &path_len) != SUCCESS) {
        zend_error(E_ERROR, "can not parse parameter.\n");
        RETURN_FALSE;
    }

    if (timestamp < 0) {
        zend_error(E_ERROR, "timestamp can not be NULL.\n");
        RETURN_FALSE;
    }

    if (image_path == NULL || path_len == 0) {
        zend_error(E_ERROR, "image path is invalid.\n");
        RETURN_FALSE
    }

    res = zend_hash_find(Z_OBJPROP_P(getThis()), FFMPEG_PHP_EXTNAME, sizeof(FFMPEG_PHP_EXTNAME), (void **)&resource);
    if (res == FAILURE) {
        zend_error(E_ERROR, "can not get resource from hash table.\n");
        RETURN_FALSE;
    } else {
        ZEND_FETCH_RESOURCE(pfmctx, ffmpeg_media_context*, resource, -1, FFMPEG_PHP_EXTNAME, ffmpeg_media_handler);
    }

    if (timestamp > pfmctx->duration) {
        zend_error(E_ERROR, "snapshot time is larger than movie duration.\n");
        RETURN_FALSE;
    }

    if (pfmctx->has_video == false) {
        zend_error(E_ERROR, "media file has no video media type.\n");
        RETURN_FALSE;
    }

    res = _ffmpeg_media_get_image(pfmctx, timestamp * 1000000, image_path, 0, 0);
    if (res == false) {
        RETURN_FALSE;
    } else {
        RETURN_TRUE;
    }
}
