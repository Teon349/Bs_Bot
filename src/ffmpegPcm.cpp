#include <thread>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <chrono>
#include "../include/ffmpegPcm.h"
#include "../include/global.h"
#include <opus/opus.h>

#if 0
#include <mpg123.h>
#endif

#if 0
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}
#endif

void monitor_voice_client(dpp::snowflake guild) 
{
    dpp::discord_voice_client *vc = g_BotManager->voiceInfo[guild].c_voice->voiceclient;
    while (true) 
    {
#ifdef USE_LAVALINK
        if(vc && g_BotManager->voiceInfo[guild].c_voice)
        {
            if(!g_BotManager->voiceInfo[guild].is_trackPlaying)
                break;
        }
        else
            break;
#else
        if(vc && g_BotManager->voiceInfo[guild].c_voice)
        {
            if(!vc->is_playing())
                break;
        }
        else
            break;
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BotLogInfo << "[Monitoring] Pcm Buff Write Done";
    if(vc && g_BotManager->voiceInfo[guild].c_voice)
    {
        if (g_BotManager->is_connected(guild))
        {
            sendSignal(guild, SIG_NEXT_SONG);
        }
    }
}

void send_pcm_to_discord(dpp::snowflake guild, const std::string& url) 
{
    dpp::voiceconn* vc = g_BotManager->voiceInfo[guild].c_voice;

#ifdef USE_LAVALINK
    g_lavalink->play_audio_track(std::to_string(guild), url);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

#else
    BotLogDebug << "Stream Url : \n " << url;

    av_log_set_level(AV_LOG_ERROR);
    avformat_network_init();
    AVFormatContext* format_context = avformat_alloc_context();
    if (!format_context) {
        BotLogError << "Failed to allocate format context.";
        sendSignal(guild, SIG_FFMPEG_INIT_ERROR);
        return;
    }
    format_context->probesize = 50 * 1024 * 1024; // 50MB
    format_context->max_analyze_duration = 5 * AV_TIME_BASE; // 5초 동안 분석
    if (avformat_open_input(&format_context, url.c_str(), nullptr, nullptr) < 0) {
        BotLogError << "Could not open input file.";
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        sendSignal(guild, SIG_FFMPEG_INIT_ERROR);
        return;
    }
    if (avformat_find_stream_info(format_context, nullptr) < 0) {
        BotLogError << "Could not find stream info.";
        avformat_close_input(&format_context);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        sendSignal(guild, SIG_FFMPEG_INIT_ERROR);
        return;
    }

    int audio_stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index < 0) {
        BotLogError << "Could not find audio stream.";
        avformat_close_input(&format_context);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        sendSignal(guild, SIG_FFMPEG_INIT_ERROR);
        return;
    }
    AVStream* audio_stream = format_context->streams[audio_stream_index];
    if (!audio_stream) {
        BotLogError << "Could not get audio stream.";
        avformat_close_input(&format_context);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        sendSignal(guild, SIG_FFMPEG_INIT_ERROR);
        return;
    }

    AVCodec* codec = avcodec_find_decoder(format_context->streams[audio_stream_index]->codecpar->codec_id);
    AVCodecContext* codec_context = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_context, format_context->streams[audio_stream_index]->codecpar);

    codec_context->pkt_timebase = audio_stream->time_base;
    codec_context->thread_count = 4; // 사용할 쓰레드 수
    codec_context->thread_type = FF_THREAD_FRAME; // 프레임 기반 쓰레딩

    if (avcodec_open2(codec_context, codec, nullptr) < 0) {
        BotLogError << "Could not open codec.";
        avcodec_free_context(&codec_context);
        avformat_close_input(&format_context);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        sendSignal(guild, SIG_FFMPEG_INIT_ERROR);
        return;
    }

    SwrContext* swr_ctx = swr_alloc_set_opts(
        nullptr, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 48000,
        codec_context->channel_layout, codec_context->sample_fmt, codec_context->sample_rate, 0, nullptr);
    swr_init(swr_ctx);

    AVPacket packet;
    AVFrame* frame = av_frame_alloc();
    uint8_t* buffer = nullptr;
    av_samples_alloc(&buffer, nullptr, 2, 960, AV_SAMPLE_FMT_S16, 0);

    // Opus 인코더 초기화
    OpusEncoder* opus_encoder = nullptr;
    int opus_error = 0;
    opus_encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &opus_error);
    opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(128000));
    opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(10));
    opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

    while (av_read_frame(format_context, &packet) >= 0 && g_BotManager->voiceInfo[guild].is_connected && vc && vc->voiceclient && vc->is_active()) 
    {
        if (packet.stream_index == audio_stream_index) 
        {
            if (avcodec_send_packet(codec_context, &packet) >= 0) 
            {
                while (avcodec_receive_frame(codec_context, frame) >= 0 && g_BotManager->voiceInfo[guild].is_connected) 
                {
                    swr_convert(swr_ctx, &buffer, 960, (const uint8_t**)frame->data, frame->nb_samples);

                    unsigned char opus_packet[4000];
                    int opus_packet_size = opus_encode(opus_encoder, reinterpret_cast<int16_t*>(buffer), 960, opus_packet, 4000);
                    if (opus_packet_size > 0) 
                    {
                        int64_t current_time = av_gettime();
                        int64_t pts_time = av_rescale_q(packet.pts, audio_stream->time_base, {1, 1000000}); // PTS 변환
                        
                        // BotLogDebug << "PTS Time: " << pts_time 
                        // << " Current Time: " << current_time 
                        // << " Difference: " << (pts_time - current_time) << " us" << std::endl;

                        if (pts_time > current_time) 
                        {
                            av_usleep(pts_time - current_time);// 전송타임 싱크
                        }
                        try {
                            if(g_BotManager->voiceInfo[guild].is_connected && vc->is_active())
                                vc->voiceclient->send_audio_opus(opus_packet, opus_packet_size);
                            else
                                break;
                        } catch (const dpp::voice_exception& e) {
                            BotLogError << "Voice exception: " << e.what();
                            continue; 
                        }
                    }
                    else 
                    {
                        BotLogError << "Opus encoding failed: " << opus_strerror(opus_packet_size);
                    }
                }
            }
        }
        av_packet_unref(&packet);
    }

    BotLogDebug << "IS End";
    if(opus_encoder)
        opus_encoder_destroy(opus_encoder);
    if(buffer)
        av_freep(&buffer);
    if(swr_ctx)
        swr_free(&swr_ctx);
    if(frame)
        av_frame_free(&frame);
    if(codec_context)
        avcodec_free_context(&codec_context);
    if(format_context)
        avformat_close_input(&format_context);

    avformat_network_deinit();

#endif
    if(g_BotManager->is_connected(guild))
        std::thread(monitor_voice_client, guild).detach();

}


void send_pcm_to_discord_emoji(dpp::snowflake guild, const std::string& url) 
{
#if 0
    std::vector<uint8_t> pcmdata;
    
    std::string path = url;

    dpp::voiceconn* vc = g_BotManager->voiceInfo[guild].c_voice;

    mpg123_init();
 
    int err = 0;
    unsigned char* buffer;
    size_t buffer_size, done;
    int channels, encoding;
    long rate;
 
    mpg123_handle *mh = mpg123_new(NULL, &err);
    mpg123_param(mh, MPG123_FORCE_RATE, 48000, 48000.0);
 
    buffer_size = mpg123_outblock(mh);
    buffer = new unsigned char[buffer_size];
 
    mpg123_open(mh, MP3_DIR);
    mpg123_getformat(mh, &rate, &channels, &encoding);
 
    unsigned int counter = 0;
    for (int totalBytes = 0; mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK; ) 
    {
        for (size_t i = 0; i < buffer_size; i++) 
        {
            pcmdata.push_back(buffer[i]);
        }
        counter += buffer_size;
        totalBytes += done;
    }
    delete[] buffer;
    mpg123_close(mh);
    mpg123_delete(mh);

    vc->voiceclient->send_audio_raw((uint16_t*)pcmdata.data(), pcmdata.size());
#endif
    if(g_BotManager->is_connected(guild))
        std::thread(monitor_voice_client, guild).detach();

}
