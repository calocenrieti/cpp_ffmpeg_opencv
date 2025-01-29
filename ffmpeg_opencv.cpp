#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include <opencv2/opencv.hpp>

void decode_and_encode_video(const char* output, const char* input);

void main() {
    const char* input = "test_sdr.mp4";
    const char* output = "output.mp4";
    decode_and_encode_video(output, input);
}


void decode_and_encode_video(const char* output, const char* input) {
    AVFormatContext* inputFmtContxt = NULL;
    AVFormatContext* outputFmtContxt = NULL;
    const AVCodec* encoder = NULL;
    const AVCodec* decoder = NULL;
    AVCodecContext* encoderContxt = NULL;
    AVCodecContext* decoderContxt = NULL;
    int ret = 0, video_stream_index = 0;
    ret = avformat_open_input(&inputFmtContxt, input, NULL, NULL);
    if (ret < 0) {
        std::cout << "Could not open input video" << std::endl;
    }
    ret = avformat_find_stream_info(inputFmtContxt, NULL);
    if (ret < 0) {
        std::cout << "Could not find the stream info" << std::endl;
    }
    const AVOutputFormat* outFmt = av_guess_format("mp4", NULL, NULL);
    avformat_alloc_output_context2(&outputFmtContxt, outFmt, NULL, NULL);
    //デコーダーとエンコーダーを設定
    for (int i = 0; i < (int)inputFmtContxt->nb_streams; ++i) {

        AVStream* in_stream = inputFmtContxt->streams[i];
        AVCodecParameters* in_par = in_stream->codecpar;
        AVStream* out_stream = avformat_new_stream(outputFmtContxt, NULL);
        if (in_par->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            //decoder = avcodec_find_decoder(in_par->codec_id);
            const AVCodec* decoder = avcodec_find_decoder_by_name("hevc_cuvid");
            if (!decoder) {
                fprintf(stderr, "Decoder not found\n");
                return;
            }
            decoderContxt = avcodec_alloc_context3(decoder);
            AVCodecContext* codec_ctx = avcodec_alloc_context3(NULL);
            avcodec_parameters_to_context(decoderContxt, in_par);
            decoderContxt->framerate = in_stream->r_frame_rate;
            decoderContxt->time_base = in_stream->time_base;
            ret=avcodec_open2(decoderContxt, decoder, NULL);
            if (ret < 0) {
                fprintf(stderr, "Could not open codec\n");
                return ;
            }
            //encoder = avcodec_find_encoder(in_par->codec_id);
            encoder = avcodec_find_encoder_by_name("hevc_nvenc");
            encoderContxt = avcodec_alloc_context3(encoder);
            encoderContxt->height = decoderContxt->height;
            encoderContxt->width = decoderContxt->width;
            //encoderContxt->pix_fmt = decoderContxt->pix_fmt;
            encoderContxt->pix_fmt = AV_PIX_FMT_YUV420P;
            //encoderContxt->qmax = 31;
            //encoderContxt->qmin = 2;
            //encoderContxt->qcompress = 0.6;
            //encoderContxt->max_qdiff = 4;
            //encoderContxt->gop_size = 250;
            //encoderContxt->keyint_min = 25;
            //encoderContxt->max_b_frames = 16;
            //encoderContxt->refs = 6;
            encoderContxt->framerate = in_stream->r_frame_rate;
            encoderContxt->time_base = in_stream->time_base;
            encoderContxt->bit_rate = decoderContxt->bit_rate;
            encoderContxt->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
            av_opt_set(encoderContxt->priv_data, "preset", "fast", 0);
            //av_opt_set(encoderContxt->priv_data, "tune", "zerolatency", 0);
            avcodec_open2(encoderContxt, encoder, NULL);
            out_stream->time_base = encoderContxt->time_base;
            avcodec_parameters_from_context(out_stream->codecpar, encoderContxt);
        }
        else {
            ret = avcodec_parameters_copy(out_stream->codecpar, in_par);
        }
    }
    //出力ファイルを準備
    av_dump_format(outputFmtContxt, 0, output, 1);
    avio_open(&outputFmtContxt->pb, output, AVIO_FLAG_WRITE);
    ret = avformat_write_header(outputFmtContxt, NULL);
    //YUV と RGB 間の変換用
    enum AVPixelFormat bgr_pix_fmt = AV_PIX_FMT_BGR24;
    int HEIGHT = decoderContxt->height;
    int WIDTH = decoderContxt->width;
    SwsContext* yuv2bgr = sws_getContext(WIDTH, HEIGHT, decoderContxt->pix_fmt,
        WIDTH, HEIGHT, bgr_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
    SwsContext* bgr2yuv = sws_getContext(WIDTH, HEIGHT, bgr_pix_fmt,
        WIDTH, HEIGHT, encoderContxt->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
    //パケットとフレームの準備
    int res = 0;
    AVPacket* packet = av_packet_alloc();
    AVPacket* out_packet = av_packet_alloc();
    out_packet->data = NULL;
    out_packet->size = 0;
    // デコーダーから受け取るフレーム
    AVFrame* frame = av_frame_alloc();
    // RGBへの変換先のフレーム
    AVFrame* bgrframe = av_frame_alloc();
    bgrframe->width = decoderContxt->width;
    bgrframe->height = decoderContxt->height;
    bgrframe->format = bgr_pix_fmt;
    ret = av_frame_get_buffer(bgrframe, 0);
    if (ret < 0) {
		fprintf(stderr, "Could not allocate the video frame data\n");
		return;
	}

    //uint8_t* buf = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGR24, decoderContxt->width, decoderContxt->height, 1));
    ////ret = av_image_fill_arrays(bgrframe->data, bgrframe->linesize, buf, AV_PIX_FMT_BGR24, decoderContxt->width, decoderContxt->height, 1);
    //ret = av_image_fill_arrays(bgrframe->data, bgrframe->linesize, NULL, bgr_pix_fmt, bgrframe->width, bgrframe->height, 1);
    //if (ret < 0) {
    //    fprintf(stderr, "Could not fill arrays for bgrframe\n");
    //    return;
    //}
    // エンコーダ―に渡すフレーム
    AVFrame* outframe = av_frame_alloc();
    outframe->width = decoderContxt->width;
    outframe->height = decoderContxt->height;
    //outframe->format = decoderContxt->pix_fmt;
    outframe->format = AV_PIX_FMT_YUV420P;
    ret = av_frame_get_buffer(outframe, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate the video frame data\n");
        return;
    }
    //uint8_t* outbuf = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, decoderContxt->width, decoderContxt->height, 1));
    //ret = av_image_fill_arrays(outframe->data, outframe->linesize, outbuf, AV_PIX_FMT_YUV420P, decoderContxt->width, decoderContxt->height, 1);
    //ret = av_image_fill_arrays(outframe->data, outframe->linesize, NULL, AV_PIX_FMT_YUV420P, outframe->width, outframe->height, 1);
    //if (ret < 0) {
    //    fprintf(stderr, "Could not fill arrays for outframe\n");
    //    return;
    //}
    //デコードとエンコードを開始
    while (true) {
        ret = av_read_frame(inputFmtContxt, packet);
        if (ret < 0) {
            break;
        }
        AVStream* input_stream = inputFmtContxt->streams[packet->stream_index];
        AVStream* output_stream = outputFmtContxt->streams[packet->stream_index];
        if (input_stream->codecpar->codec_type == video_stream_index) {
            res = avcodec_send_packet(decoderContxt, packet);
            while (res >= 0) {
                res = avcodec_receive_frame(decoderContxt, frame);
                if (res == AVERROR(EAGAIN) || res == AVERROR_EOF) {
                    break;
                }
                if (res >= 0) {
                    outframe->pict_type = frame->pict_type;
                    outframe->pts = frame->pts;
                    outframe->pkt_dts = frame->pkt_dts;
                    outframe->duration = frame->duration;
                    ret = av_frame_make_writable(outframe);
                    //YUVフレームをRGBフレームに変換
                    sws_scale(yuv2bgr, frame->data, frame->linesize, 0, frame->height, bgrframe->data, bgrframe->linesize);

                    ////ネガポジ反転
                    //int h = bgrframe->height;
                    //int l = bgrframe->linesize[0];
                    //for (int i = 0; i < h; ++i) {
                    //    for (int j = 0; j < l; ++j) {
                    //        bgrframe->data[0][i * l + j] = 255 - bgrframe->data[0][i * l + j];
                    //    }
                    //}

                    // ネガポジ反転の代わりに左右反転を行う部分
                    cv::Mat bgrMat(bgrframe->height, bgrframe->width, CV_8UC3, bgrframe->data[0], bgrframe->linesize[0]);
                    cv::Mat flippedMat;

                    cv::flip(bgrMat, flippedMat, 1); // 1は左右反転を意味します

                    // flippedMatをbgrframeにコピー
                    memcpy(bgrframe->data[0], flippedMat.data, flippedMat.total()* flippedMat.elemSize());


                    //RGBからYUVに戻す
                    sws_scale(bgr2yuv, bgrframe->data, bgrframe->linesize, 0, bgrframe->height, outframe->data, outframe->linesize);
                    res = avcodec_send_frame(encoderContxt, outframe);
                    //フレームの書き込み
                    while (res >= 0) {
                        res = avcodec_receive_packet(encoderContxt, out_packet);
                        if (res == AVERROR(EAGAIN) || res == AVERROR_EOF) {
                            break;
                        }
                        out_packet->pts = av_rescale_q_rnd(outframe->pts, input_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF);
                        out_packet->dts = outframe->pts;
                        out_packet->duration = av_rescale_q(packet->duration, input_stream->time_base, output_stream->time_base);
                        res = av_interleaved_write_frame(outputFmtContxt, out_packet);
                    }
                    av_packet_unref(out_packet);
                }
                av_frame_unref(frame);
            }
            av_packet_unref(packet);
        }
        else {
            //音声データはそのまま書き込み
            packet->pts = av_rescale_q_rnd(packet->pts, input_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF);
            packet->dts = av_rescale_q_rnd(packet->dts, input_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF);
            packet->duration = av_rescale_q(packet->duration, input_stream->time_base, output_stream->time_base);
            res = av_interleaved_write_frame(outputFmtContxt, packet);
            av_packet_unref(packet);
        }
    }
    //各メモリの解放
    av_packet_free(&packet);
    av_frame_free(&frame);
    avformat_free_context(inputFmtContxt);
    avcodec_free_context(&decoderContxt);
    av_packet_free(&out_packet);
    av_write_trailer(outputFmtContxt);
    avformat_free_context(outputFmtContxt);
    avcodec_free_context(&encoderContxt);
    //av_freep(&buf);
    //av_freep(&outbuf);
    sws_freeContext(yuv2bgr);
    sws_freeContext(bgr2yuv);
}

