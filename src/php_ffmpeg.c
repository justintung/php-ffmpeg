/*
 * ffmpeg_php.c is a main c file of the ffmpeg extension
 *
 * situation now: now the author just create the framework
 * of the ffmpeg-php, it just be a developing plugins.
 *
 * create time : 2013-05-29
 * author : xinlinrong
 */

#include "php_ffmpeg.h"

// zend class functions entry

// zend module entry for ffmepg entry
zend_module_entry ffmpeg_module_entry = {
    STANDARD_MODULE_HEADER,
    "ffmpeg",
    NULL,
    ZEND_MINIT(ffmpeg),
    ZEND_MSHUTDOWN(ffmpeg),
    NULL,
    NULL,
    ZEND_MINFO(ffmpeg),
    FFMPEG_PHP_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_FFMPEG
ZEND_GET_MODULE(ffmpeg)
#endif

extern void _ffmpeg_media_register_class(int module_number);

// combine with ZEND_MINIT
ZEND_MINIT_FUNCTION(ffmpeg)
{
    //REGISTER_INI_ENTRIES();

    // register ffmpeg
    av_register_all();
    avcodec_register_all();
    _ffmpeg_media_register_class(module_number);
    return SUCCESS;
}

//  combine with ZEND_MSHUTDOWN
ZEND_MSHUTDOWN_FUNCTION(ffmpeg)
{
    //UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

// combine with ZEND_MINFO (phpinfo)
ZEND_MINFO_FUNCTION(ffmpeg)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "ffmpeg", "enable");
    php_info_print_table_row(2, "version", "0.1");
    php_info_print_table_end();
    return;
}
