/* ffmpeg_php.h is a header file for the extension of php
 * thought the author is foolish guy and a fresh bird in 
 * build extension in php.
 * 
 * It will take him quite a long time to learn the extension
 * in build php. But he will decide to do it which looked
 * like something stupid. Event it means that he will copy
 * some code from php-ffmpeh-0.6.0
 *
 * create time : 2013-05-26
 * author : xinlinrong
 */

#ifndef _FFMPEG_PHP_H_

#define _FFMPEG_PHP_H_

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "php.h"
#include "zend_ini.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// define ffmpeg version and extension name
#define FFMPEG_PHP_VERSION "0.1"
#define FFMPEG_PHP_EXTNAME "ffmpeg_media"

// define resource structure
typedef struct ffmpeg_media_context {
AVFormatContext *av_fmt_ctx;
long php_resource_id;
} ffmpeg_media_context, *pffmpeg_media_context;

// define ffmpeg function
ZEND_MINFO_FUNCTION(ffmpeg);
ZEND_MINIT_FUNCTION(ffmpeg);

ZEND_MSHUTDOWN_FUNCTION(ffmpeg);
ZEND_METHOD(ffmpeg, __construct);
ZEND_METHOD(ffmpeg, get_media_duration);

#endif
