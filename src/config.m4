dnl config.m4 to create configure file with phpize

PHP_ARG_WITH(ffmpeg,for php ffmpeg support,[  --with-ffmpeg[=DIR] include ffmpeg support (version >= 0.10.2).])

PHP_ADD_INCLUDE(/usr/include/)
PHP_ADD_INCLUDE(/usr/inlcude/)

PHP_ADD_LIBRARY_WITH_PATH(avformat, /usr/lib64/, FFMPEG_MEDIA_SHARE_LIB)
PHP_ADD_LIBRARY_WITH_PATH(avcodec, /usr/lib64/, FFMPEG_MEDIA_SHARE_LIB)

PHP_NEW_EXTENSION(ffmpeg, php_ffmpeg.c php_ffmpeg_utility.c, $ext_shared,,)
PHP_SUBST(FFMPEG_MEDIA_SHARE_LIB)
