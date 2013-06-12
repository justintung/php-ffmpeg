/*
 * description : 为视频的某个时间段截图, 原型 prototype
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>

int main(int argc, char **argv)
{
    int res = 0;
    int stream_index= 0;
    int video_stream_index = -1;
    int stream_count = 0;
    char input_media_file[] = "/home/xinlinrong/Videos/RobotWallet.rmvb";
    char output_media_file[] = "/home/xinlinrong/Videos/GinTama.jpg";
    struct AVFormatContext *pinAVFmtCtx = NULL, *poutAVFmtCtx = NULL;
    struct AVStream *pinAVStream = NULL, *poutAVStream = NULL;
    struct AVCodecContext *pinAVCodecCtx = NULL, *poutAVCodecCtx = NULL;
    struct AVCodec *pinAVCodec = NULL, *poutAVCodec = NULL;
    struct AVOutputFormat *poutFmt = NULL;

    enum CodecID cid = CODEC_ID_NONE;

    av_register_all();
    avcodec_register_all();

    // 获取存储视频上下文的存储地址
    pinAVFmtCtx = avformat_alloc_context();
    if (pinAVFmtCtx == NULL) {
        fprintf(stdout, "can not malloc memory for input media file.\n");
        return res;
    }

    // 打开存储上下文的文件
    res = avformat_open_input(&pinAVFmtCtx, input_media_file, NULL, NULL);
    if (AVERROR(res) < 0) {
        fprintf(stdout, "can not open media context for input media file.\n");
        return res;
    }

    // 查找流的信息
    res = avformat_find_stream_info(pinAVFmtCtx, NULL);
    if (res < 0) {
        fprintf(stdout, "can not find streams information for input media file.\n");
        return res;
    }

    // 获取 stream_count, 并逐一打开 decoder
    stream_count = pinAVFmtCtx->nb_streams;
    if (stream_count <= 0) {
        fprintf(stdout, "stream count is less than 0.\n");
        return -1;
    }

    // 循环 streams, 找出 codec_id, decoder, 并且逐一打开 decoder
    for (stream_index = 0; stream_index < stream_count; ++ stream_index) {
        pinAVStream = pinAVFmtCtx->streams[stream_index];
        pinAVCodecCtx = pinAVStream->codec;
        cid = pinAVCodecCtx->codec_id;

        if (pinAVCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = stream_index;
        }

        if (cid != CODEC_ID_NONE) {
            struct AVCodec *avCodec = avcodec_find_decoder(cid);
            avcodec_open2(pinAVCodecCtx, avCodec, NULL);
        }
    }

    // 以上几步是初始化 源的 streams, 后面的几步就是初始化输出的步骤
    poutFmt = av_guess_format(NULL, output_media_file, NULL);
    if (poutFmt == NULL) {
        fprintf(stdout, "can not find output format by specified file name.\n");
        return -1;
    }

    // 猜测 codec_id 编码代号
    cid = av_guess_codec(poutFmt, NULL, output_media_file, NULL, AVMEDIA_TYPE_VIDEO);
    if (cid == CODEC_ID_NONE) {
        fprintf(stdout, "can not get codec id by specified output format and media type.\n");
        return -1;
    }

    // 找到 encoder 编码子
    poutAVCodec = avcodec_find_encoder(cid);
    if (poutAVCodec == NULL) {
        fprintf(stdout, "can not fin/fd encoder by specified codec id.\n");
        return -1;
    }

    // 请求 malloc 输出文件的容器结构体
    res= avformat_alloc_output_context2(&poutAVFmtCtx, poutFmt, NULL, output_media_file);
    if (AVERROR(res) < 0) {
        fprintf(stdout, "can not malloc memory for output context.\n");
        return -1;
    }

    // 打开输出文件的 avio 句柄
    res = avio_open(&(poutAVFmtCtx->pb), output_media_file, AVIO_FLAG_WRITE);
    if (AVERROR(res) < 0) {
        fprintf(stdout, "can not open output media file.\n");
        return res;
    }

    // 请求  malloc 容器所存在的流的结构提
    // 填写输出的 AVCodecContext 的内容
    // 保持原来的平均码率
    // 保持原来的宽高
    poutAVStream = avformat_new_stream(poutAVFmtCtx, poutAVCodec);
    if (poutAVStream == NULL) {
        fprintf(stdout, "can not malloc a new stream from avformat context.\n");
        return -1;
    }

    pinAVCodecCtx = pinAVFmtCtx->streams[video_stream_index]->codec;
    poutAVCodecCtx = poutAVStream->codec;
    poutAVCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;
    poutAVCodecCtx->time_base.num = pinAVCodecCtx->time_base.num;
    poutAVCodecCtx->time_base.den = pinAVCodecCtx->time_base.den;
    poutAVCodecCtx->width = pinAVCodecCtx->width;
    poutAVCodecCtx->height = pinAVCodecCtx->height;

    // 打开编码子的容器
    res = avcodec_open2(poutAVFmtCtx->streams[0]->codec, poutAVCodec, NULL);
    if (AVERROR(res) < 0) {
        fprintf(stdout, "can not open codec.\n");
        return res;
    }

    // 写输出 AVFormatContext 的文件头
    res = avformat_write_header(poutAVFmtCtx, NULL);
    if (AVERROR(res) < 0) {
        fprintf(stdout, "can not write output file header");
        return res;
    }

    // 读取一帧数据, 最好是视频的数据
    int find_first = 0;
    int get_picture = 0;
    int read_frames = 0;
    int bit_buffer_size = 1024 * 256; // 256kB
    uint8_t *bit_buffer = NULL;

    struct AVFrame av_decode_frame;
    struct AVPacket av_decode_pkt;

    while (1) {
        struct AVPacket avpkt;
        av_init_packet(&avpkt);
        res = av_read_frame(pinAVFmtCtx, &avpkt);
        if (AVERROR(res) == EAGAIN) {
            av_free_packet(&avpkt);
            continue;
        } else if (AVERROR(res) < 0 && AVERROR(res) != EAGAIN){
            av_free_packet(&avpkt);
            fprintf(stdout, "can not read packet from input media file.\n");
            return res;
        } else if (AVERROR(res) == 0) {
            read_frames ++;
            pinAVStream = pinAVFmtCtx->streams[avpkt.stream_index];
            pinAVCodecCtx = pinAVStream->codec;
            if (pinAVCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
                res = avcodec_decode_video2(pinAVCodecCtx, &av_decode_frame, &get_picture, &avpkt);
                if (res > 0 && get_picture > 0 && avpkt.pts > 11000) {
                    av_free_packet(&avpkt);
                    break;
                }
            } else {
                av_free_packet(&avpkt);
            }
        }
    }

    bit_buffer = av_malloc(bit_buffer_size); // 在写入的时候会被自动地释放掉
    // 如果执行成功, 就会返回编码数据的 bytes 数量
    res = avcodec_encode_video(poutAVCodecCtx, bit_buffer, bit_buffer_size, &av_decode_frame);

    av_init_packet(&av_decode_pkt);
    av_decode_pkt.stream_index = 0;
    av_decode_pkt.data = bit_buffer;
    av_decode_pkt.size = res;

    if (av_decode_frame.key_frame) {
        av_decode_pkt.flags |= AV_PKT_FLAG_KEY;
    }

    if (av_decode_frame.pts != AV_NOPTS_VALUE) {
        av_decode_pkt.pts = av_rescale_q(av_decode_frame.pts, poutAVCodecCtx->time_base, poutAVStream->time_base);
    }

    res = av_interleaved_write_frame(poutAVFmtCtx, &av_decode_pkt);
    if (AVERROR(res) < 0) {
        fprintf(stdout, "can not write frame to output media file.\n");
        return res;
    }

    // 写输出 AVFormatContext 的文件末尾
    res = av_write_trailer(poutAVFmtCtx);
    if (AVERROR(res) < 0) {
        fprintf(stdout, "can not write output file trailer");
        return res;
    }

    avio_flush(poutAVFmtCtx->pb);
    // 关闭输入 stream , codec 以及容器
    for (stream_index = 0; stream_index < pinAVFmtCtx->nb_streams; ++ stream_index) {
        pinAVStream = pinAVFmtCtx->streams[stream_index];
        pinAVCodecCtx = pinAVStream->codec;
        avcodec_close(pinAVCodecCtx);
    }

    avformat_close_input(&pinAVFmtCtx);

    // 关闭输出 stream, codec 以及容器
    avio_close(poutAVFmtCtx->pb);
    for (stream_index = 0; stream_index < poutAVFmtCtx->nb_streams; ++ stream_index) {
        poutAVStream = poutAVFmtCtx->streams[stream_index];
        poutAVCodecCtx = poutAVStream->codec;
        avcodec_close(poutAVCodecCtx);
    }

    avformat_free_context(poutAVFmtCtx);
    return 0;
}
