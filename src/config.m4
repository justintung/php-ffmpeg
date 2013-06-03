dnl config.m4 to create configure file with phpize

PHP_ARG_WITH(ffmpeg,for php ffmpeg support,[  --with-ffmpeg[=DIR] include ffmpeg support (version >= 0.10.2).])
PHP_ARG_ENABLE(debug,for debug enable, [  --enable-debug enable ffmpeg debug support])

PHP_ADD_INCLUDE(/usr/include/)
PHP_ADD_INCLUDE(/usr/inlcude/)

PHP_ADD_LIBRARY_WITH_PATH(avformat, /usr/lib64/, LDFLAGS)
PHP_ADD_LIBRARY_WITH_PATH(avcodec, /usr/lib64/, LDFLAGS)

PHP_NEW_EXTENSION(ffmpeg, php_ffmpeg.c ffmepg_media.c, $ext_shared,,)
PHP_SUBST(LDFLAGS)
