#include <iostream>

#define SDL2_HELPER_USE_SDL2_IMAGE

#include <sdl_helper/SDL2Helper.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
}

#define OUTPUT_YUV420P 0
#define USE_DSHOW 0

// Refresh Event
#define SFM_REFRESH_EVENT (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT (SDL_USEREVENT + 2)

using namespace std;

int thread_exit = 0;

int sfpRefreshThread(void *opaque) {
    thread_exit = 0;
    while (!thread_exit) {
        SDL_Event event;
        event.type = SFM_REFRESH_EVENT;
        SDL_PushEvent(&event);
        SDL_Delay(40);
    }
    thread_exit = 0;
    // Break
    SDL_Event event;
    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);
    return 0;
}

// Show Dshow Device
void showDShowDevice() {
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    AVDictionary *options = nullptr;
    av_dict_set(&options, "list_devices", "true", 0);
    AVInputFormat *iFormat = av_find_input_format("dshow");
    cout << "========Device Info=============" << endl;
}

int main() {

    AVFormatContext *pFormatContext;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;

    av_register_all();
    avformat_network_init();

    pFormatContext = avformat_alloc_context();
    avdevice_register_all();


    // Linux 打开摄像头设备的输入流
    AVInputFormat *avInputFormat = av_find_input_format("video4linux2");
    if (avformat_open_input(&pFormatContext, "/dev/video0", avInputFormat, nullptr) != 0) {
        cout << "Couldn't open input stream." << endl;
        return -1;
    }

    if (avformat_find_stream_info(pFormatContext, nullptr) < 0) {
        cout << "Couldn't find stream information." << endl;
        return -1;
    }

    int videoIndex = -1;
    cout << "pFormatContext->nb_streams: " << pFormatContext->nb_streams << endl;
    for (int i = 0; i < pFormatContext->nb_streams; i++) {
        if (pFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
            break;
        }
    }

    if (videoIndex == -1) {
        cout << "Couldn't find a video stream." << endl;
        return -1;
    }


    pCodecCtx = pFormatContext->streams[videoIndex]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == nullptr) {
        cout << "Codec not found." << endl;
        return -1;
    }

    if (avcodec_open2(pCodecCtx, pCodec, nullptr) < 0) {
        cout << "Could not open codec." << endl;
        return -1;
    }

    AVFrame *pFrame, *pFrameYUV;
    pFrame = avcodec_alloc_frame();
    pFrameYUV = avcodec_alloc_frame();
    int screenW = pCodecCtx->width;
    int screenH = pCodecCtx->height;
    unsigned char *outBuffer = nullptr;
    outBuffer = (unsigned char *) av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));

    avpicture_fill((AVPicture *) pFrameYUV, outBuffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

    /////////////////////////////////////////////////////////////
    //////// SDL
    /////////////////////////////////////////////////////////////
    SDL2Helper sdl2Helper(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
    sdl2Helper.createWindow("My Camera Capture test Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            screenW, screenH)
            ->createRenderer();

//    SDL_Create

    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    SwsContext *imageConvertCtx;
    imageConvertCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width,
                                     pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, nullptr,
                                     nullptr, nullptr);
    SDL_Texture *texture = SDL_CreateTexture(sdl2Helper.getRenderer(), SDL_PIXELFORMAT_YV12,
                                             SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = screenW;
    rect.h = screenH;

    SDL_Thread *videoTid = SDL_CreateThread(sfpRefreshThread, nullptr, nullptr);
    SDL_Event event;

    int got_picture;
    for (;;) {
        // Wait
        SDL_WaitEvent(&event);
        if (event.type == SFM_REFRESH_EVENT) {
            if (av_read_frame(pFormatContext, packet) >= 0) {
                if (packet->stream_index == videoIndex) {
                    if (avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet) < 0) {
                        cout << "decode error" << endl;
                        return -1;
                    }
                    if (got_picture) {
                        // Convert the image into YUV format that SDL uses
                        sws_scale(imageConvertCtx,
                                  (const unsigned char *const *) pFrame->data,
                                  pFrame->linesize,
                                  0,
                                  pCodecCtx->height,
                                  pFrameYUV->data,
                                  pFrameYUV->linesize);
                        SDL_UpdateYUVTexture(texture, &rect,
                                             pFrameYUV->data[0],
                                             pFrameYUV->linesize[0],
                                             pFrameYUV->data[1],
                                             pFrameYUV->linesize[1],
                                             pFrameYUV->data[2],
                                             pFrameYUV->linesize[2]);
                        sdl2Helper.renderClear()
                                ->renderCopy(texture, nullptr, &rect)
                                ->renderPresent()
                                ->delay(40);
                    }
                }
                av_free_packet(packet);
            } else {
                thread_exit = 1;
            }
        } else if (event.type == SDL_QUIT) {
            thread_exit = 1;
        } else if (event.type == SFM_BREAK_EVENT) {
            break;
        }
    }
    av_free(pFrameYUV);
    sws_freeContext(imageConvertCtx);
    sdl2Helper.quit();
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatContext);

    std::cout << "Hello, World!" << std::endl;
    return 0;
}