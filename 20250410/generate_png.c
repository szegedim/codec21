// This document is Licensed under Creative Commons CC0.
// To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights
// to this document to the public domain worldwide.
// This document is distributed without any warranty.
// You should have received a copy of the CC0 Public Domain Dedication along with this document.
// If not, see https://creativecommons.org/publicdomain/zero/1.0/legalcode.

// Disclaimer: Patent rights reserved regardless of the license above

/*
gcc -o generate_png.out generate_png.c -lavcodec -lavformat -lavutil -lswscale -lpng -lcurl && ./generate_png.out
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <png.h>

#define VIDEO_URL "https://archive.org/download/sample-video-file-100-mb/Sample_Video_File_100MB.ia.mp4"
#define VIDEO_FILE "sample.mp4"
#define FILE_COUNT 200

size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

int download_video() {
    CURL *curl;
    FILE *fp;
    CURLcode res;

    fp = fopen(VIDEO_FILE, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to create file %s\n", VIDEO_FILE);
        return 0;
    }

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, VIDEO_URL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        printf("Downloading %s...\n", VIDEO_FILE);
        res = curl_easy_perform(curl);
        
        curl_easy_cleanup(curl);
        fclose(fp);

        if (res != CURLE_OK) {
            fprintf(stderr, "Download failed: %s\n", curl_easy_strerror(res));
            return 0;
        }
        printf("Download completed successfully\n");
        return 1;
    }

    fclose(fp);
    return 0;
}

void save_frame_as_png(AVFrame *frame, int64_t pts_us) {
    char filename[256];
    snprintf(filename, sizeof(filename), "%lld.png", pts_us);
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Could not open %s\n", filename);
        return;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);

    struct SwsContext *sws_ctx = sws_getContext(
        frame->width, frame->height, frame->format,
        frame->width, frame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    if (!sws_ctx) {
        fprintf(stderr, "Could not initialize SwsContext\n");
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    uint8_t *rgb_data = malloc(frame->width * frame->height * 3);
    uint8_t *rgb_rows[frame->height];
    
    for (int y = 0; y < frame->height; y++) {
        rgb_rows[y] = rgb_data + (y * frame->width * 3);
    }

    uint8_t *dst_data[4] = {rgb_data, NULL, NULL, NULL};
    int dst_linesize[4] = {frame->width * 3, 0, 0, 0};
    
    sws_scale(sws_ctx, (const uint8_t * const*)frame->data, frame->linesize,
              0, frame->height, dst_data, dst_linesize);

    png_set_IHDR(png, info, frame->width, frame->height, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    png_write_image(png, rgb_rows);
    png_write_end(png, NULL);

    sws_freeContext(sws_ctx);
    free(rgb_data);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    printf("Saved frame %lld.png\n", pts_us);
}

int main() {
    // Check if video file exists, if not download it
    if (access(VIDEO_FILE, F_OK) != 0) {
        if (!download_video()) {
            fprintf(stderr, "Failed to download video file\n");
            return 1;
        }
    }

    AVFormatContext *fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, VIDEO_FILE, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open input file\n");
        return 1;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return 1;
    }

    int video_stream_idx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        fprintf(stderr, "Could not find video stream\n");
        return 1;
    }

    AVStream *video_stream = fmt_ctx->streams[video_stream_idx];
    const AVCodec *codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    
    if (avcodec_parameters_to_context(codec_ctx, video_stream->codecpar) < 0) {
        fprintf(stderr, "Could not copy codec params to codec context\n");
        return 1;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return 1;
    }

    AVFrame *frame = av_frame_alloc();
    AVPacket *packet = av_packet_alloc();

    int frame_number = 0;
    int saved_files = 0;
    
    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_idx) {
            if (avcodec_send_packet(codec_ctx, packet) < 0) continue;

            while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
                frame_number++;
                
                if (frame_number >= 1000 && frame_number <= 1200) {
                    int64_t pts_us = frame->pts * av_q2d(video_stream->time_base) * 1000000;
                    save_frame_as_png(frame, pts_us);
                    saved_files++;
                    
                    // Exit if we've saved the specified number of files
                    if (saved_files >= FILE_COUNT) {
                        printf("Reached target of %d PNG files, exiting\n", FILE_COUNT);
                        goto cleanup;
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

cleanup:
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}