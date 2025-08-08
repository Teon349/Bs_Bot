# DogBs Bot이 무엇인가

# 설명
❓ 보통 디스코드 봇 개발시 JS, JAVA, Python 중 하나로 개발하게 되는데 DPP라는 CPP 개발 Lib를 발견했다 

그래서 그냥 재밌어 보여서 DPP 라이브러리로 만들게 되었다

# 그래서 어떻게 동작하는데
 초반 봇 구조는 DPP + ytdlp 구조였다. 가장 흔한 방법이기도 하고 정보도 많이 있다

 하지만 ytdlp는 유튜브 영상 다운로드로 개발되었기에 스트리밍으로 돌리기엔 속도가 많이 느렸다

 그래서 스트리밍 목적으로 개발된 JAVA기반 Lavaplayer 라이브러리에서 사용하는 Lavalink를 사용하게 되었다

 사용 언어가 다르기 때문에 Lavalink를 스트리밍 서버로 구축하고 RestAPI를 사용하여 Socket으로 Bot <-> Lavalink가 통신하여
 Bot에서 요청을 보내고 Lavalink가 받은 요청에 따라 노래 스트리밍을 보내는 구조로 만들었다

 여기서 단점은 DPP에서 보이스 연결을 할 때 websocket을 디스코드 서버로 연결하게 되는데 이때 Lavalink도 음악 스트리밍을 위해

 디스코드 서버로 websocket 연결을 보내기 때문에 DPP의 wesocket 클라이언트가 비 정상적으로 끊기게 된다


**근데 그런대도 잘 된다 그래서 그냥 쓴다**


# 구조나 보여봐 

## main.cpp
프로그램이 실행되는 main이다 보통 봇의 상호작용과 커맨드 처리를 main에서 하고 botManager나 workthread에 넘겨 비동기로 처리된다

코드 내에서 발견할 수 있듯 ifdef로 ytdlp를 나중에 사용할 수 있도록 일단은 만들었다 
<details>
<summary>main.cpp </summary>

```cpp
#define _GNU_SOURC
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <thread>
#include <chrono>
#include <time.h>
#include <signal.h>
#include "../include/ffmpegPcm.h"
#include "../include/ytdlp.h"
#include "../include/gambling.h"

int interrupt_count = 0;

void handle_signal(int sig) 
{
    switch(sig) 
    {
        case SIGINT:
            interrupt_count++;
            BotLogError << "SIGINT received " << interrupt_count << " times.";
            if (interrupt_count >= 3) 
            {
                BotLogError << "Exiting program after 3 interrupts.";
                exit(0);
            }
        break;
        case SIGTERM:
            BotLogError << "SIGTERM (kill) received!";
            exit(1);
        break;
        case SIGSEGV:
            BotLogError << "SIGSEGV (segmentation fault) received!";
            exit(1);
        break;
        case SIGABRT:
            BotLogError << "SIGABRT (abort) received!";
            exit(1);
        break;
        case SIGFPE:
            BotLogError << "SIGFPE (floating point exception) received!";
            exit(1);
        break;
        default:
            BotLogError << "Unknown signal: " << sig;
            exit(1);
        break;
    }
}

void play_audio_command(std::string stream_url, dpp::snowflake guild) 
{
    if(!g_BotManager->is_connected(guild))
        return;

    if(stream_url.find(MP3_DIR) != std::string::npos)
    {
        try {
            std::thread([guild, stream_url]() {
                send_pcm_to_discord_emoji(guild, stream_url);
            }).detach();
        } catch (const std::exception& e) 
        {
            BotLogError << " play Audio: " << e.what();
        }
    }
    else
    {
        try {
            std::thread([guild, stream_url]() {
                send_pcm_to_discord(guild, stream_url);
            }).detach();
        } catch (const std::exception& e) 
        {
            BotLogError << " play Audio: " << e.what();
        }
    }
    return;
}

void initSignal()
{
    signal(SIGINT,  handle_signal);  // Ctrl+C
    signal(SIGTERM, handle_signal);  // kill
    signal(SIGSEGV, handle_signal);  // segmentation fault
    signal(SIGABRT, handle_signal);  // abort()
    signal(SIGFPE,  handle_signal);
}

void setupDogBs()
{
    initSignal();
    g_BotManager = botManager::getInstance();
    g_taskManager = TaskManager::getInstance();
    g_lavalink = lavalink::getInstance();
    g_gptManager = gptManager::getInstance();

    std::thread worker(workerThread);
    worker.detach();

    g_taskManager->start();

    g_BotManager->initChannelInfo();

    bot.on_log([&](const dpp::log_t& event) 
    {
        if (event.severity >= g_dpploglevel) 
        {
            DPPLog(event.severity) << event.message;
        } 
    });
}


int main() 
{
    std:: cout << R"(     _                                    )" << std::endl;
    std:: cout << R"(    | |                    ______  _____  )" << std::endl;
    std:: cout << R"(  __| |  ___    __ _       | ___ \/  ___| )" << std::endl;
    std:: cout << R"( / _` | / _ \  / _` |      | |_/ /\ `--.  )" << std::endl;
    std:: cout << R"(| (_| || (_) || (_| |      | ___ \ `--. \ )" << std::endl;
    std:: cout << R"( \__,_| \___/  \__, |      | |_/ //\__/ / )" << std::endl;
    std:: cout << R"(                __/ |      \____/ \____/  )" << std::endl;
    std:: cout << R"(               |___/                      )" << std::endl;

    BotLogDebug << "DIR PATH : " << PROJECT_DIR_PATH;

    setupDogBs();

    bot.on_slashcommand([&bot](const dpp::slashcommand_t& event) 
    {
        try {
            dpp::message msg;
            if (event.command.get_command_name() == "play") 
            {
                dpp::guild* g = dpp::find_guild(event.command.guild_id);
                if (!g->connect_member_voice(*event.owner, event.command.get_issuing_user().id)) 
                {
                    event.reply("들어와서 틀어임마");
                    return;
                }
                dpp::command_interaction cmd_data = event.command.get_command_interaction();
                std::string option = std::get<std::string>(cmd_data.options[0].value);


                if (!cmd_data.options[0].empty()) 
                {
                    g_BotManager->voiceInfo[event.command.guild_id].req_url = option;
                    g_BotManager->voiceInfo[event.command.guild_id].req_username = event.command.get_issuing_user().username;
                    g_BotManager->voiceInfo[event.command.guild_id].req_userprofile = event.command.get_issuing_user().get_avatar_url();


                    if(g_BotManager->is_connected(event.command.guild_id))
                    {
                        sendSignal(event.command.guild_id, SIG_GET_SONGINFO);
                    }
                    else
                    {
                        g_BotManager->voiceInfo[event.command.guild_id].is_afterPlay = true;
                        g_BotManager->voiceInfo[event.command.guild_id].msg_channel_Id = event.command.channel_id;
                    }
                    msg.set_content("어 그래 기다려 틀어줄게");
                    event.reply(dpp::ir_channel_message_with_source, msg);
                } 
                else 
                {
                    msg.set_content("❌ URL 없잖아 임마");
                    event.reply(dpp::ir_channel_message_with_source, msg);
                }
            } 
            else if (event.command.get_command_name() == "skip")
            {
                if(g_BotManager->is_connected(event.command.guild_id))
                {  
                    msg.set_content("이놈이 이 노래가 싫단다 넘긴다");
                    event.reply(dpp::ir_channel_message_with_source, msg);
#ifdef USE_LAVALINK
                    if(l_taskQueue.empty() && taskQueue.empty() && !g_BotManager->voiceInfo[event.command.guild_id].is_trackPlaying 
                        && !g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.empty())
    #else
                    if(l_taskQueue.empty() && taskQueue.empty() && !g_BotManager->voiceInfo[event.command.guild_id].c_voice->voiceclient->is_playing() 
                        && !g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.empty())
    #endif
                    {
                        sendSignal(event.command.guild_id, SIG_NEXT_SONG);
                    }
                    else
                    {
    #ifdef USE_LAVALINK
                        g_lavalink->stop_track(std::to_string(event.command.guild_id));
    #else
                        g_BotManager->voiceInfo[event.command.guild_id].c_voice->voiceclient->stop_audio();
    #endif
                    }
                }
                else
                    msg.set_content("? 봇이 아직 준비가 안 됐다는데");
                    event.reply(dpp::ir_channel_message_with_source, msg);
            }
            else if (event.command.get_command_name() == "stop") 
            {
                if(!g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.empty())
                    g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.clear();
                
                if (g_BotManager->is_connected(event.command.guild_id)) 
                {
                    g_taskManager->clear_guild_queue(event.command.guild_id);
                    g_BotManager->voiceInfo[event.command.guild_id].is_connected = false;
                    // if(vc->voiceclient->is_playing())
                    {

#ifdef USE_LAVALINK
                        g_lavalink->stop_track(std::to_string(event.command.guild_id));
#else
                        g_BotManager->voiceInfo[event.command.guild_id].c_voice->voiceclient->stop_audio();
#endif
                        // vc->voiceclient->send_audio_raw(nullptr, 0);
                    }

                    event.from()->disconnect_voice(event.command.guild_id);

                    msg.set_content("⏹️ 봇 중지맨");
                    event.reply(dpp::ir_channel_message_with_source, msg);
                } 
                else 
                {
                    msg.set_content("❌ 봇 임마 여기 없는데?");
                    event.reply(dpp::ir_channel_message_with_source, msg);
                }
            }
            else if (event.command.get_command_name() == "add")
            {
                g_BotManager->voiceInfo[event.command.guild_id].embed_channel_Id = event.command.channel_id;
                g_BotManager->add_or_update_guild_channel(std::to_string(event.command.guild_id), std::to_string(event.command.channel_id));
                g_BotManager->add_embed_Channel(event.command.guild_id, g_BotManager->voiceInfo[event.command.guild_id].embed_channel_Id);

                msg.set_content("채널 추가 완료");
                event.reply(dpp::ir_channel_message_with_source, msg);
            }
            else if (event.command.get_command_name() == "홀짝")
            {
                long money;
                long bat_money;
                std::string str;
                json user_data = load_json(GAMBLING_JSONPATH);
                std::string user_id = std::to_string(event.command.get_issuing_user().id);
                money = get_money(user_data, user_id);
                if (money <= 0)
                {
                    if (money == 0)
                        msg.set_content("이새끼ㅋㅋ 탕진했네 ㅋㅋㅋㅋ 병신 ㅋㅋㅋㅋ 엿이나 쳐먹어ㅋ");
                    else
                        msg.set_content("돈부터 받아 이자식아아");

                    event.reply(dpp::ir_channel_message_with_source, msg);
                }
                else 
                {
                    dpp::command_interaction cmd_data = event.command.get_command_interaction();
                    if (!cmd_data.options.empty())
                    {
                        bat_money = std::get<long>(cmd_data.options[0].value);
                        if(money < bat_money)
                        {
                            msg.set_content("니 그 정도 돈 없잖아");
                            event.reply(dpp::ir_channel_message_with_source, msg);
                        }
                        else
                        {
                            std::random_device rd;
                            std::mt19937 gen(rd());
                            std::uniform_int_distribution<> dist(0, 100); // 확률
                            std::uniform_int_distribution<> p_dist(0, 1); // 0 = 홀, 1 = 짝
                            dpp::embed e;

                            int chance = dist(gen);  // 먹튀 확률
                            int probability = p_dist(gen); // 홀/짝
                            std::string cmd = (probability == 1) ? "짝" : "홀";
                            if (chance == 69)
                            {
                                str = "씨!!!발 토사장이 먹튀 했습니다 병신ㅋ 니 돈 : 0";
                                e.set_title("씨!!!!!!발 토사장")
                                .set_color(0xED4245); // 초록 계열
                                e.add_field("토사장이 먹튀를 했습니다", "병신ㅋ", false);
                                e.add_field("니 돈 : ", "0원", false);
                                update_money(user_data, user_id, 0);
                                msg.set_content(str);
                                event.reply(dpp::ir_channel_message_with_source, msg);
                            }
                            else
                            {

                                if ((std::get<std::string>(cmd_data.options[1].value) == "홀" && probability == 0) || 
                                (std::get<std::string>(cmd_data.options[1].value) == "짝" && probability == 1))
                                {
                                    e.set_title("아 씨발 내 돈")
                                    .set_color(0x57F287); // 초록 계열
                                    e.add_field("홀/짝", cmd, false);
                                    e.add_field("니 돈 : ", std::to_string(money + bat_money) + "원", false);

                                    update_money(user_data, user_id, money + bat_money);
                                }
                                else
                                {
                                    e.set_title("병신ㅋ")
                                    .set_color(0xED4245); // 빨강 계열
                                    e.add_field("홀/짝", cmd, false);
                                    e.add_field("니 돈 : ", std::to_string(money - bat_money) + "원", false);

                                    update_money(user_data, user_id, money - bat_money);
                                }
                            }
                            event.reply(dpp::message().add_embed(e));
                            save_json(GAMBLING_JSONPATH, user_data);
                        }
                    }
                }
                

            }
            else if (event.command.get_command_name() == "돈가져와")
            {
                dpp::embed e;
                long money;
                json user_data = load_json(GAMBLING_JSONPATH);
                std::string user_id = std::to_string(event.command.get_issuing_user().id);
                money = get_money(user_data, user_id);
                if (money == -1)
                {
                    add_user(user_data, user_id, 100000);
                    money = 100000;
                    e.set_title("땅 파면 돈이 나오냐")
                    .set_color(0x3498DB); // 파랑
                    e.add_field("니 돈", "+" + std::to_string(money) + "원", false);
                }
                else
                {
                    if (claim_daily_reward(user_data, user_id))
                    {
                        money += 100000;
                        e.set_title("땅 파면 돈이 나오냐")
                        .set_color(0x3498DB); // 파랑
                        e.add_field("니 돈", "+" + std::to_string(money) + "원", false);
                        update_money(user_data, user_id, money);
                    }
                    else 
                    {
                        e.set_title("니 돈 쳐 받았잖아")
                        .set_color(0x3498DB); // 파랑
                        e.add_field("씨발 병신 좆같은 새끼", + " ㅗㅗ", false);
                    }
                }

                save_json(GAMBLING_JSONPATH, user_data);
                event.reply(dpp::message().add_embed(e));

            }
            else if (event.command.get_command_name() == "도와줘보성맨")
            {
                dpp::message msg;
                std::string req_msg;
                std::string resp_msg;
                dpp::command_interaction cmd_data = event.command.get_command_interaction();
                
                if (!cmd_data.options.empty()) 
                {
                    req_msg = std::get<std::string>(cmd_data.options[0].value);
                    
                    event.thinking();
                    msg.set_content(g_gptManager->req_gpt_message(req_msg));
                    event.edit_original_response(msg);
                } 
                else 
                {
                    msg.set_content("똑띠 적어라");
                    event.reply(dpp::ir_channel_message_with_source, msg);
                }
            }
        } catch (const std::exception& e) {
            bot.log(dpp::ll_error, std::string("오류 발생: ") + e.what());
            dpp::message msg;
            msg.set_content("❌ 어 쉬부랄 명령이 이상하게 하지마");
            event.reply(dpp::ir_channel_message_with_source, msg);
        }
    });

    bot.on_ready([&bot](const dpp::ready_t& event) 
    {
        if (dpp::run_once<struct register_commands>()) 
        {

            dpp::slashcommand play_cmd("play", "음악 재생맨.", bot.me.id);
            play_cmd.default_member_permissions  = dpp::permissions(dpp::p_use_application_commands);
            play_cmd.add_option(dpp::command_option(dpp::co_string, "url", "YouTube URL", true));
            bot.global_command_create(play_cmd);

            dpp::slashcommand stop_cmd("stop", "음악 중지인데 그냥 나가버리는.", bot.me.id);
            stop_cmd.default_member_permissions  = dpp::permissions(dpp::p_use_application_commands);   
            bot.global_command_create(stop_cmd);

            dpp::slashcommand next_cmd("skip", "어 다음곡이야.", bot.me.id);
            next_cmd.default_member_permissions  = dpp::permissions(dpp::p_use_application_commands);   
            bot.global_command_create(next_cmd);

            dpp::slashcommand add_cmd("add", "개보성용 음악 채널생 '개보성 소환술' 해도 됨 (이미 있으면 절대 건들지 말 것).", bot.me.id);
            add_cmd.default_member_permissions  = dpp::permissions(dpp::p_use_application_commands);  
            bot.global_command_create(add_cmd);

            dpp::slashcommand gpt_cmd("도와줘보성맨", "개보성이 제공하는 고급 정보", bot.me.id);
            gpt_cmd.default_member_permissions  = dpp::permissions(dpp::p_use_application_commands);
            gpt_cmd.add_option(dpp::command_option(dpp::co_string, "내용", "내용적으셈ㅇㅇ", true));
            bot.guild_command_create(gpt_cmd, sj_guild_id);

            dpp::slashcommand gambling("홀짝", "험한 꼴 보기 전에 시도하지 마라", bot.me.id);
            gambling.default_member_permissions  = dpp::permissions(dpp::p_use_application_commands);
            gambling.add_option(dpp::command_option(dpp::co_integer, "금액", "베팅할 금액 (예: 1 이상)", true));
            gambling.add_option(dpp::command_option(dpp::co_string, "홀짝", "홀 : 짝 적으셈", true));
            bot.guild_command_create(gambling, sj_guild_id);

            dpp::slashcommand money("돈가져와", "심기정이 선사하는 자애로운 총알", bot.me.id);
            money.default_member_permissions  = dpp::permissions(dpp::p_use_application_commands);
            bot.guild_command_create(money, sj_guild_id);

#ifdef USE_LAVALINK
            g_BotManager->BOT_USER_ID = std::to_string(bot.me.id);

            g_lavalink->initLavaLink();

            // g_BotManager->initMp3Encode();
#endif
        }

        // g_BotManager->add_embed_Channel(test_guild_id, target_channel_id);
        // g_BotManager->add_embed_Channel(sj_guild_id, sj_target_channel_id);
    });

    bot.on_message_create([&bot](const dpp::message_create_t& event) 
    {
        // 채널 등록 안된상태
        if (event.msg.channel_id == g_BotManager->voiceInfo[event.msg.guild_id].embed_channel_Id) 
        {
            // 메시지가 봇이 보낸 메시지가 아닌 경우
            if (event.msg.author.id != bot.me.id) 
            {

                dpp::guild* g = dpp::find_guild(event.msg.guild_id);
                if (!g->connect_member_voice(*event.owner, event.msg.author.id)) 
                {
                    // event.reply("들어와서 틀어임마");
                    return;
                }
                std::string option = event.msg.content;


                if (!option.empty()) 
                {
                    g_BotManager->voiceInfo[event.msg.guild_id].req_url = option;

                    g_BotManager->voiceInfo[event.msg.guild_id].req_username = event.msg.author.username;
                    g_BotManager->voiceInfo[event.msg.guild_id].req_userprofile = event.msg.author.get_avatar_url();


                    if(g_BotManager->is_connected(event.msg.guild_id))
                    {
                        sendSignal(event.msg.guild_id, SIG_GET_SONGINFO);
                    }
                    else
                    {
                        g_BotManager->voiceInfo[event.msg.guild_id].is_afterPlay = true;
                        g_BotManager->voiceInfo[event.msg.guild_id].msg_channel_Id = event.msg.channel_id;
                    }
                }

                bot.message_delete(event.msg.id, event.msg.channel_id, [](const dpp::confirmation_callback_t& callback) 
                {
                    if (callback.is_error()) 
                    {
                        BotLogError << "Failed to delete message: " << callback.get_error().message;
                    } 
                    else 
                    {
                        // BotLogDebug << "Message deleted successfully!";
                    }
                });
            }
        }
        else
        {
            if(event.msg.content.find("개보성 소환술") != std::string::npos)
            {
                g_BotManager->voiceInfo[event.msg.guild_id].embed_channel_Id = event.msg.channel_id;
                g_BotManager->add_or_update_guild_channel(std::to_string(event.msg.guild_id), std::to_string(event.msg.channel_id));
                g_BotManager->add_embed_Channel(event.msg.guild_id, g_BotManager->voiceInfo[event.msg.guild_id].embed_channel_Id);
                
                event.reply("채널 추가 완료");
            }

            if((event.msg.content.find("씨발 개보성 계엄선포") != std::string::npos) && event.msg.author.id == 683013993903947781)
            {
                g_BotManager->is_martial_law = true;
            }
            else if((event.msg.content.find("개보성 계엄해제") != std::string::npos) && event.msg.author.id == 683013993903947781)
            {
                g_BotManager->is_martial_law = false;
            }
        }
    });

    // voice Connection Event
    bot.on_voice_state_update([&bot](const dpp::voice_state_update_t& event) 
    {
        if (event.state.user_id == bot.me.id)
        {
            BotLogDebug << "[Voice Event] guild Id : " << event.state.guild_id;
            if (event.state.channel_id.empty()) // disconnect
            {
                g_BotManager->voiceInfo[event.state.guild_id].is_connected = false;
                BotLogInfo << "[Voice Event] Bot " << event.state.user_id << " left the voice channel.";

                // 초기화 작업
                Song tmp = {"", "", "", ""};
                g_BotManager->voiceInfo[event.state.guild_id].is_afterPlay = false;
                if(!g_BotManager->voiceInfo[event.state.guild_id].server_queues.queue.empty())
                    g_BotManager->voiceInfo[event.state.guild_id].server_queues.queue.clear();

                if(g_BotManager->voiceInfo[event.state.guild_id].embed_channel_Id != 0)
                    g_BotManager->update_embed(event.state.guild_id, EMBED_DEFUALT, tmp, "");

#ifdef USE_LAVALINK
                if(g_BotManager->voiceInfo[event.state.guild_id].is_trackPlaying)
                {
                    g_lavalink->stop_track(std::to_string(event.state.guild_id));
                }
#endif
                g_BotManager->voiceInfo[event.state.guild_id].c_voice = event.from()->get_voice(event.state.guild_id);

            } 
            else // connected
            {
                BotLogInfo << "[Voice Event] Bot " << event.state.user_id << " joined channel ID: " << event.state.channel_id;

                // channel id로 guild id 저장
                g_BotManager->voice_channel_map[event.state.channel_id] = event.state.guild_id;
                // voice c 저장
                g_BotManager->voiceInfo[event.state.guild_id].c_voice = event.from()->get_voice(event.state.guild_id);
                g_BotManager->voiceInfo[event.state.guild_id].client = event.from();

                if(!g_BotManager->voiceInfo[event.state.guild_id].server_queues.queue.empty())
                    g_BotManager->voiceInfo[event.state.guild_id].server_queues.queue.clear();
                
            }
        }
    });

    // voice Client Ready Event
    bot.on_voice_ready([](const dpp::voice_ready_t& event) {
        BotLogInfo << "[Voice Ready] Voice client is ready!";

        dpp::snowflake guild_id = g_BotManager->getGuildId(event.voice_channel_id);
        g_BotManager->voiceInfo[guild_id].c_voice = event.from()->get_voice(guild_id);

        if(!g_BotManager->voiceInfo[guild_id].c_voice)
            return;

        g_BotManager->voiceInfo[guild_id].is_connected = true;

        // DPP websocket Disconnect. 10.1.* 이후 websocket 이후로 socket event 이상 현상으로 cpu 100% 이상 올라가 disconnect 필요
        // g_BotManager->voiceInfo[guild_id].c_voice->voiceclient->on_disconnect();
        
        g_lavalink->update_websocket_status(std::to_string(guild_id));

        if(g_BotManager->voiceInfo[guild_id].is_afterPlay == true && g_BotManager->is_connected(guild_id) == true)
        {
            sendSignal(guild_id, SIG_GET_SONGINFO);
        }
        else if (g_BotManager->voiceInfo[guild_id].is_after_emoji == true && g_BotManager->is_connected(guild_id) == true)
        {
            sendSignal(guild_id, SIG_EMOJI_SONG);
        }
        g_BotManager->voiceInfo[guild_id].is_afterPlay = false;
        g_BotManager->voiceInfo[guild_id].is_after_emoji = false;

    });

    bot.on_button_click([&bot](const dpp::button_click_t& event)
    {

        if (event.custom_id == "stop") 
        {
            if(!g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.empty())
                g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.clear();

            if (g_BotManager->is_connected(event.command.guild_id)) 
            {
                g_taskManager->clear_guild_queue(event.command.guild_id);
                g_BotManager->voiceInfo[event.command.guild_id].is_connected = false;
                // if(vc->voiceclient->is_playing())
                {

#ifdef USE_LAVALINK

#else
                    vc->voiceclient->stop_audio();
#endif
                    // vc->voiceclient->send_audio_raw(nullptr, 0);
                }
                event.from()->disconnect_voice(event.command.guild_id);

            } 
            else 
            {
                event.from()->disconnect_voice(event.command.guild_id);
            }
        }
        else if (event.custom_id == "skip") 
        {
            if(g_BotManager->is_connected(event.command.guild_id))
            {  
                // 노래가 오류로 멈춘 상태에서도 동작
#ifdef USE_LAVALINK
                if(l_taskQueue.empty() && taskQueue.empty() && !g_BotManager->voiceInfo[event.command.guild_id].is_trackPlaying 
                    && !g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.empty())
#else
                if(l_taskQueue.empty() && taskQueue.empty() && !g_BotManager->voiceInfo[event.command.guild_id].c_voice->voiceclient->is_playing() 
                    && !g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.empty())
#endif
                {
                    sendSignal(event.command.guild_id, SIG_NEXT_SONG);
                }
                else
                {
#ifdef USE_LAVALINK
                    g_lavalink->stop_track(std::to_string(event.command.guild_id));
#else
                    g_BotManager->voiceInfo[event.command.guild_id].c_voice->voiceclient->stop_audio();
#endif
                }
            }
        }
        else if (event.custom_id == "smoke")
        {
            dpp::guild* g = dpp::find_guild(event.command.guild_id);

            if (!g->connect_member_voice(*event.owner, event.command.usr.id)) 
            {
                return;
            }

            if(g_BotManager->is_connected(event.command.guild_id) && l_taskQueue.empty() && taskQueue.empty())
            {
#ifdef USE_LAVALINK
                if (!g_BotManager->voiceInfo[event.command.guild_id].is_trackPlaying)
#else
                if (!g_BotManager->voiceInfo[event.command.guild_id].c_voice->voiceclient->is_playing())
#endif
                {
                    sendSignal(event.command.guild_id, SIG_EMOJI_SONG);
                }
            }
            else
            {
                BotLogInfo << "[Smoke] Voice Client is_after_emoji";
                g_BotManager->voiceInfo[event.command.guild_id].is_after_emoji = true;
            }

        }
        else if (event.custom_id == "shuffle")
        {
            if (g_BotManager->is_connected(event.command.guild_id) && l_taskQueue.empty() && taskQueue.empty())
            {
                if (g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.size() > 1)
                {
                    g_BotManager->shuffle_SongQueue(event.command.guild_id);
                }
            }

        }

        event.reply(dpp::ir_deferred_update_message, "", [](const dpp::confirmation_callback_t& callback) 
        {
            if (callback.is_error()) 
            {
                BotLogError << "Failed to process button click: " << callback.get_error().message;
            }
        });
    });

    //Drop Down 메뉴 select 무시하도록 
    bot.on_select_click([&bot](const dpp::select_click_t& event) 
    {
        BotLogInfo << "Select Index : " << event.values[0];

        if(!g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.empty() && 
            g_BotManager->voiceInfo[event.command.guild_id].is_trackPlaying)
        {
            try {
                int idx = std::stoi(event.values[0]);
                if(g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.size() >= idx)
                {
                    Song tmp_song = g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue[idx];
                    g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.erase(g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.begin() + idx);
                    g_BotManager->voiceInfo[event.command.guild_id].server_queues.queue.push_front(tmp_song);

                    g_lavalink->stop_track(std::to_string(event.command.guild_id));
                }

            } catch (const std::invalid_argument& e) {
                BotLogError << "invalid_argument : " << e.what();
            }catch (const std::out_of_range& e) {
                BotLogError << "out_of_range : " << e.what();
            }
        }
        event.reply(dpp::ir_deferred_update_message, "", [](const dpp::confirmation_callback_t& callback) 
        {
            if (callback.is_error()) 
            {
                BotLogError << "Failed to process button click: " << callback.get_error().message;
            }
        });
    });

    bot.start(dpp::st_wait);
    return 0;
}

```
</details>

## botManager.cpp

봇의 기본적인 채널 정보, 연결 상태를 관리하고 workthread에 메시지를 전달해 봇의 전체적인 흐름을 관리한다

개발 초기부터 여러 서버에서 구동 가능하게 설계해서 채널 정보나 음성 클라이언트 상태 관리를 map으로 관리해

직관적이고 편하게 관리가 가능하다

<details>
<summary>botManager.cpp </summary>
  
```cpp
  
#include <dpp/dpp.h>
#include <unordered_map>
#include <deque>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include "../include/global.h"

botManager::botManager()
{
}

botManager::~botManager()
{
	
}

botManager *botManager::getInstance()
{
	static botManager instance;
	return &instance;
}

void botManager::add_embed_Channel(dpp::snowflake guild, dpp::snowflake channel_id)
{
	dpp::snowflake channel = voiceInfo[guild].embed_channel_Id;
    if (channel == 0) 
    {
        BotLogError << "No valid channel for embedding.";
        return;
    }

	dpp::embed embed = dpp::embed()
		.set_color(0x3498DB) // 파란색
		.set_title("어 형은 노래를 틀 수 있어")
		.set_description("암것도 없노") // 새로운 상태
		.set_thumbnail("")
		.set_footer(
				dpp::embed_footer()
				.set_text("")
				.set_icon("")
			)
		.set_timestamp(time(0));

	// 메시지 생성 및 임베드 추가
	dpp::message msg(channel_id, ""); // 채널 ID 설정
	msg.add_embed(embed); // 임베드 추가

	// 버튼 생성
	if (channel_id == sj_target_channel_id || channel_id == target_channel_id)
	{
		msg.add_component(
			dpp::component()
				.add_component(
					dpp::component().set_type(dpp::cot_button).set_label("⏹️").set_style(dpp::cos_secondary).set_id("stop")
				)
				.add_component(
					dpp::component().set_type(dpp::cot_button).set_label("⏭️ ").set_style(dpp::cos_secondary).set_id("skip")
				)
				.add_component(
					dpp::component().set_type(dpp::cot_button).set_label("🔀 ").set_style(dpp::cos_secondary).set_id("shuffle")
				)
				.add_component(
					dpp::component().set_type(dpp::cot_button).set_label("🚬 ").set_style(dpp::cos_secondary).set_id("smoke")
				)
		);
	}
	else
	{
		msg.add_component(
			dpp::component()
				.add_component(
					dpp::component().set_type(dpp::cot_button).set_label("⏹️").set_style(dpp::cos_secondary).set_id("stop")
				)
				.add_component(
					dpp::component().set_type(dpp::cot_button).set_label("⏭️ 1").set_style(dpp::cos_secondary).set_id("skip")
				)
				.add_component(
					dpp::component().set_type(dpp::cot_button).set_label("🔀 ").set_style(dpp::cos_secondary).set_id("shuffle")
				)
		);
	}

    msg.add_component(
        dpp::component()
            .add_component(
                dpp::component().set_type(dpp::cot_selectmenu)
                .set_placeholder("어 형은 대기열도 보여줘")
                .add_select_option(dpp::select_option("empty", "0", ""))
                .set_id("dropdown")
                .set_disabled(true)
            )
    );

	bot.message_create(msg, [this, guild](const dpp::confirmation_callback_t& callback) {
        if (callback.is_error()) 
        {
            BotLogError << "Failed to create embed message: " << callback.get_error().message;
        } 
        else 
        {
            dpp::message sent_message = std::get<dpp::message>(callback.value);
            voiceInfo[guild].embed_message_Id = sent_message.id;
            dpp::snowflake message_id = sent_message.id;
            dpp::snowflake a_channel_id = sent_message.channel_id;
            BotLogInfo << "[Message Create] Embed message created with ID: " << sent_message.id;

            bot.messages_get(a_channel_id, 100, 0, 0, 0, [&bot, a_channel_id, message_id](const dpp::confirmation_callback_t& callback) 
            {
                if (callback.is_error()) {
                    BotLogError << "Failed to fetch messages: " << callback.get_error().message;
                    return;
                }

                // 메시지 목록 가져오기
                dpp::message_map messages = std::get<dpp::message_map>(callback.value);

                // 각 메시지 삭제
                for (const auto& [id, message] : messages) 
                {
                    if(message.id != message_id)
                    {
                        bot.message_delete(message.id, a_channel_id, [](const dpp::confirmation_callback_t& delete_callback) {
                            if (delete_callback.is_error()) 
                            {
                                BotLogError << "Failed to delete message: " << delete_callback.get_error().message;
                            }
                        });
                    }
                }
            });
        }
    });
	return;
}

void botManager::update_embed(dpp::snowflake guild, EMBED_CMD cmd, Song entry, std::string tumbnail)
{
	dpp::snowflake channel = voiceInfo[guild].embed_channel_Id;
    if (channel == 0) 
    {
        BotLogError << "No valid channel for embedding.";
        return;
    }

    dpp::snowflake message_id = voiceInfo[guild].embed_message_Id;
    if (message_id == 0) 
    {
        BotLogError << "No message ID available to edit.";
        return;
    }
 
    dpp::component dropdown;
	std::string title;
	std::string subtitle;
    std::string url = "";
	dpp::embed embed;
	dpp::message msg;
	if(cmd == EMBED_ADD_QUEUE)
	{
		if(voiceInfo[guild].server_queues.queue.empty())
        {
			subtitle = "total : 0";
            dropdown = dpp::component()
                .add_component(
                    dpp::component().set_type(dpp::cot_selectmenu)
                    .set_placeholder("어 형은 대기열도 보여줘")
                    .add_select_option(dpp::select_option("empty", "0", ""))
                    .set_id("dropdown")
                    .set_disabled(true)
                );
        }
		else
		{
			subtitle = "total : " + std::to_string(voiceInfo[guild].server_queues.queue.size());
            dropdown = dpp::component()
                .add_component(
                    dpp::component().set_type(dpp::cot_selectmenu)
                    .set_placeholder("어 형은 대기열도 보여줘")
                    .add_select_option(
                    dpp::select_option(voiceInfo[guild].server_queues.queue[0].title, "0", ""))
                    .set_id("dropdown")
                    .set_disabled(false)
                );

            for(int i = 1; i < voiceInfo[guild].server_queues.queue.size() && i < 24; i++)
            {
                dropdown.components[0].add_select_option(
                    dpp::select_option(voiceInfo[guild].server_queues.queue[i].title, std::to_string(i), "")
                );
            }
		}

		bot.message_get(message_id, channel, [&bot, channel, subtitle, dropdown](const dpp::confirmation_callback_t& callback) 
        {
			if (callback.is_error()) 
            {
				BotLogError << "Failed to fetch message: " << callback.get_error().message;
				return;
			}

			dpp::message existing_message = std::get<dpp::message>(callback.value);

			if (existing_message.embeds.empty()) 
            {
				BotLogError << "No embeds found in the message to update.";
				return;
			}

            existing_message.components.clear();
            if (channel == sj_target_channel_id || channel == target_channel_id)
            {
                existing_message.add_component(
                    dpp::component()
                        .add_component(
                            dpp::component().set_type(dpp::cot_button).set_label("⏹️").set_style(dpp::cos_secondary).set_id("stop")
                        )
                        .add_component(
                            dpp::component().set_type(dpp::cot_button).set_label("⏭️ ").set_style(dpp::cos_secondary).set_id("skip")
                        )
						.add_component(
							dpp::component().set_type(dpp::cot_button).set_label("🔀 ").set_style(dpp::cos_secondary).set_id("shuffle")
						)
                        .add_component(
                            dpp::component().set_type(dpp::cot_button).set_label("🚬 ").set_style(dpp::cos_secondary).set_id("smoke")
                        )
                );
            }
            else
            {
                existing_message.add_component(
                    dpp::component()
                        .add_component(
                            dpp::component().set_type(dpp::cot_button).set_label("⏹️").set_style(dpp::cos_secondary).set_id("stop")
                        )
                        .add_component(
                            dpp::component().set_type(dpp::cot_button).set_label("⏭️ 1").set_style(dpp::cos_secondary).set_id("skip")
                        )
						.add_component(
							dpp::component().set_type(dpp::cot_button).set_label("🔀 ").set_style(dpp::cos_secondary).set_id("shuffle")
						)
                );
            }
			existing_message.embeds[0].set_description(subtitle);
            existing_message.add_component(dropdown);

			// 수정된 메시지 전송
			bot.message_edit(existing_message, [](const dpp::confirmation_callback_t& edit_callback) 
            {
				if (edit_callback.is_error()) 
                {
					BotLogError << "Failed to edit message[202]: " << edit_callback.get_error().message;
				} 
                else 
                {
					// BotLogDebug<< "Message description updated successfully!";
				}
			});
		});
	}
	else
	{
		std::string username;
		std::string userprofile;

		if(cmd == EMBED_DEFUALT)
		{
			title = "어 형은 노래를 틀 수 있어";
			subtitle = "암것도 없노";
		}
		else
		{
			title = entry.title;
#ifdef USE_LAVALINK
            url = entry.url;
#endif
			if(voiceInfo[guild].server_queues.queue.empty())
				subtitle = "total : 0";
			else
			{
				subtitle = "total : " + std::to_string(voiceInfo[guild].server_queues.queue.size());
			}

			if(!entry.username.empty())
				username = entry.username;
			if(!entry.userprofile.empty())
				userprofile = entry.userprofile;
			
		}
		embed = dpp::embed()
			.set_color(0x3498DB) // 파란색
			.set_title(title)
            .set_url(url)
			.set_description(subtitle)
			.set_thumbnail(tumbnail)
			.set_footer(
				dpp::embed_footer()
				.set_text(username)
				.set_icon(userprofile)
			)
			.set_timestamp(time(0));

		// 메시지 생성 및 임베드 추가
		msg.id = message_id;
		msg.set_channel_id(channel);
		msg.add_embed(embed);

		if (channel == sj_target_channel_id || channel == target_channel_id)
		{
			msg.add_component(
				dpp::component()
					.add_component(
						dpp::component().set_type(dpp::cot_button).set_label("⏹️").set_style(dpp::cos_secondary).set_id("stop")
					)
					.add_component(
						dpp::component().set_type(dpp::cot_button).set_label("⏭️ ").set_style(dpp::cos_secondary).set_id("skip")
					)
					.add_component(
						dpp::component().set_type(dpp::cot_button).set_label("🔀 ").set_style(dpp::cos_secondary).set_id("shuffle")
					)
					.add_component(
						dpp::component().set_type(dpp::cot_button).set_label("🚬 ").set_style(dpp::cos_secondary).set_id("smoke")
					)
			);
		}
		else
		{
			msg.add_component(
				dpp::component()
					.add_component(
						dpp::component().set_type(dpp::cot_button).set_label("⏹️").set_style(dpp::cos_secondary).set_id("stop")
					)
					.add_component(
						dpp::component().set_type(dpp::cot_button).set_label("⏭️ ").set_style(dpp::cos_secondary).set_id("skip")
					)
					.add_component(
						dpp::component().set_type(dpp::cot_button).set_label("🔀 ").set_style(dpp::cos_secondary).set_id("shuffle")
					)
			);
		}
        

        if(voiceInfo[guild].server_queues.queue.empty())
        {
            dropdown = dpp::component()
                .add_component(
                    dpp::component().set_type(dpp::cot_selectmenu)
                    .set_placeholder("어 형은 대기열도 보여줘")
                    .add_select_option(dpp::select_option("Song", "0", ""))
                    .set_id("dropdown")
                    .set_disabled(true)
                );
        }
        else
        {
            dropdown = dpp::component()
                .add_component(
                    dpp::component().set_type(dpp::cot_selectmenu)
                    .set_placeholder("어 형은 대기열도 보여줘")
                    .add_select_option(dpp::select_option(voiceInfo[guild].server_queues.queue[0].title, "0", ""))
                    .set_id("dropdown")
                    .set_disabled(false)
                );

            for(int i = 1; i < voiceInfo[guild].server_queues.queue.size() && i < 24; i++)
            {
                dropdown.components[0].add_select_option(
                    dpp::select_option(voiceInfo[guild].server_queues.queue[i].title, std::to_string(i), "")
                );
            }
        }
    
        msg.add_component(dropdown);

		bot.message_edit(msg, [](const dpp::confirmation_callback_t& callback) 
		{
			if (callback.is_error()) {
				BotLogError << "Failed to edit message[323]: " << callback.get_error().message;
			} else {
				dpp::message sent_message = std::get<dpp::message>(callback.value);
				// BotLogDebug << "Message edited successfully! " << sent_message.id;
			}
		});
	}
	return;
}

void botManager::add_to_queue(dpp::snowflake guild, Song a_songInfo)
{
	voiceInfo[guild].server_queues.queue.push_back(a_songInfo);
	return;
}

void botManager::play_next(dpp::snowflake guild)
{
#ifdef USE_LAVALINK

    if (voiceInfo[guild].is_trackPlaying)
	{
		g_lavalink->stop_track(std::to_string(guild));
		BotLogInfo << "[Play Next] Stop Audio";
	}
#else
	if (voiceInfo[guild].c_voice->voiceclient->is_playing())
	{
		voiceInfo[guild].c_voice->voiceclient->stop_audio();
		BotLogInfo << "[Play Next] Stop Audio";
	}
 
#endif

	if (!voiceInfo[guild].server_queues.queue.empty())
	{
		Song a_SongInfo = voiceInfo[guild].server_queues.queue.front();
		voiceInfo[guild].server_queues.queue.pop_front();
		BotLogInfo << "[PLAY Next] Next Song : " << a_SongInfo.title;
		if (is_connected(guild))
		{
			std::string msg_s = "빙신같은 노래 시작 : " + a_SongInfo.title;
			if(voiceInfo[guild].embed_channel_Id != 0)
			{
				update_embed(guild, EMBED_PLAY_SONG, a_SongInfo, a_SongInfo.tumbnail);
			}
			else
				send_Msg(guild, voiceInfo[guild].msg_channel_Id, MSG_PLAY_SONG, msg_s);

#ifdef USE_LAVALINK
            play_audio_command(a_SongInfo.track_id, guild);
#else
			play_audio_command(a_SongInfo.url, guild);
#endif
		}
	}

	return;
}

void botManager::shuffle_SongQueue(dpp::snowflake guild)
{
	BotLogInfo << "[Shuffle] Shuffle Song " << guild;
	std::vector<Song> tmpQ;

	if (voiceInfo[guild].server_queues.queue.size() > 1)
	{
		while (!voiceInfo[guild].server_queues.queue.empty())
		{
			tmpQ.push_back(voiceInfo[guild].server_queues.queue.front());
			voiceInfo[guild].server_queues.queue.pop_front();
		}
		
		std::random_device rd;
    	std::mt19937 g(rd());

		std::shuffle(tmpQ.begin(), tmpQ.end(), g);

		for (const auto& elem : tmpQ) 
		{
        	voiceInfo[guild].server_queues.queue.push_back(elem);
    	}
	}

	update_embed(guild, EMBED_ADD_QUEUE, tmpQ.front(), ""); // tmpQ는 사용하지 않아 임시로 넣기만 함
}

void botManager::send_Msg(dpp::snowflake guild, dpp::snowflake channel_id, MSG_TYPE msg_type, std::string text)
{
	if(voiceInfo[sj_guild_id].embed_channel_Id == channel_id || channel_id == 0)
		return;

	if(voiceInfo[guild].c_voice && voiceInfo[guild].c_voice->voiceclient)
	{
		// if(voiceInfo[guild].c_voice->voiceclient->is_connected())
		{
			switch (msg_type)
			{
				case MSG_PLAY_SONG:
					voiceInfo[guild].c_voice->voiceclient->creator->message_create(dpp::message(channel_id, text));
				break;
				case MSG_QUEUE:
					voiceInfo[guild].c_voice->voiceclient->creator->message_create(dpp::message(channel_id, text));
				break;
				case MSG_DISCONNECT:
					voiceInfo[guild].c_voice->voiceclient->creator->message_create(dpp::message(channel_id, text));
				break;
				default:
				break;
			}
		}
	}
}
void botManager::initChannelInfo() 
{
	auto channels = get_all_channels();
	for (const auto& [channel_id, guild_id] : channels) 
	{
        // BotLogDebug << "Channel ID: " << channel_id << ", Guild ID: " << guild_id;
		voiceInfo[static_cast<dpp::snowflake>(guild_id)].embed_channel_Id = static_cast<dpp::snowflake>(channel_id);
		add_embed_Channel(static_cast<dpp::snowflake>(guild_id), voiceInfo[static_cast<dpp::snowflake>(guild_id)].embed_channel_Id);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void botManager::initMp3Encode() 
{
    std::string response = g_lavalink->load_track(MP3_DIR);
    json track_data = json::parse(response);
    if (track_data.contains("data") && track_data["data"].contains("encoded")) 
    {
		local_encoded = track_data["data"]["encoded"];
    } 
    else 
    {
        BotLogError << "Load MP3 INFO FAIL";
    }
}

json botManager::read_json_file(const std::string& file_path) 
{
    std::ifstream file(file_path);
    if (file.is_open()) 
    {
        json j;
        file >> j;
        file.close();
        return j;
    }
    return json{{"channels", json::array()}};
}

void botManager::write_json_file(const std::string& file_path, const json& j) 
{
    std::ofstream file(file_path);
    if (file.is_open()) 
    {
        file << j.dump(4);
        file.close();
    } 
    else 
    {
        BotLogError << "Failed to open file for writing: " << file_path;
    }
}

void botManager::add_or_update_guild_channel(const std::string& guild_id, const std::string& channel_id) 
{
    json data = read_json_file(JSONPATH);
    bool updated = false;

    for (auto& channel : data["channels"]) 
    {
        if (channel["channel_id"] == channel_id) 
        {
            channel["guild_id"] = guild_id;
            updated = true;
            break;
        }
    }

    if (!updated) 
    {
        data["channels"].push_back({
            {"channel_id", channel_id},
            {"guild_id", guild_id}
        });
		
    }

    write_json_file(JSONPATH, data);
    BotLogInfo << "[Update Channel] Saved/Updated channel: " << channel_id << " with guild: " << guild_id;
}
std::vector<std::tuple<std::string, std::string>> botManager::get_all_channels() 
{
    json data = read_json_file(JSONPATH);
    std::vector<std::tuple<std::string, std::string>> channels;

    if (data.contains("channels")) 
    {
        for (const auto& channel : data["channels"]) 
        {
            std::string channel_id = channel["channel_id"].get<std::string>();
            std::string guild_id = channel["guild_id"].get<std::string>();
            channels.emplace_back(channel_id, guild_id);
        }
    }

    return channels;
}

bool botManager::is_connected(dpp::snowflake guild)
{
	if (voiceInfo[guild].c_voice)
	{
		if (voiceInfo[guild].c_voice->voiceclient != nullptr && !voiceInfo[guild].c_voice->channel_id.empty())
		{
			return true;
		}
	}

	return false;
}
```

</details>

## lavalin.cpp
봇 기능 중 제일 중요한 역할을 하는 코드다 Lavalink 서버와 통신하는 소스며
RestAPI로 요청을 보내고 lavalink로 부터 곡 정보 현재 재생 상태 정보를 받고

lavalink로 연결한 디스코드 서버의 음성 상태 관리도 하고있다

<details>
<summary>lavalink.cpp </summary>

```cpp

#include "../include/global.h"


lavalink::lavalink()
{
}

lavalink::~lavalink()
{
	
}

lavalink *lavalink::getInstance()
{
	static lavalink instance;
	return &instance;
}

void lavalink::stop_track(const std::string& guild_id)
{
    BotLogInfo << "\033[33m" << "[Lavalink]" << "\033[0m" << " Req Stop Audio Track";
    if(!m_session_Id.empty())
        update_player_track(m_session_Id, guild_id, "NULL");
    else
        BotLogError << "Session Id Empty!!";
}

void lavalink::play_audio_track(const std::string& guild_id, const std::string& track_id)
{
    BotLogInfo << "\033[33m" << "[Lavalink]" << "\033[0m" << " Req Start Audio Track";
    if(!m_session_Id.empty())
        update_player_track(m_session_Id, guild_id, track_id);
    else
        BotLogError << "Session Id Empty!!";
}

void lavalink::update_websocket_status(const std::string& guild_id)
{
    BotLogInfo << "\033[33m" << "[Lavalink]" << "\033[0m" << " Req Websocket Update Status : " << guild_id;
    dpp::voiceconn *vc =  g_BotManager->voiceInfo[guild_id].c_voice;

    if(vc && !m_session_Id.empty())
        update_voice_state(m_session_Id, guild_id, vc->token, vc->websocket_hostname, vc->session_id);
    else
        BotLogError << "websocket update err!! vc : " << static_cast<bool>(vc);
}

void lavalink::clientThread(lavalink* instance)
{
    BotLogDebug << "\033[33m" << "[Lavalink]" << "\033[0m" << " Run Client!! ";
    instance->m_client.run();
}

size_t lavalink::write_callback(void* contents, size_t size, size_t nmemb, std::string* output) 
{
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

size_t throw_away(void* contents, size_t size, size_t nmemb, void* userp)
{
    return size * nmemb;
}

void lavalink::update_voice_state(const std::string& session_id, const std::string& guild_id,
                        const std::string& voice_token, const std::string& endpoint, const std::string& voice_session_id) 
{
    CURL* curl = curl_easy_init();
    if (!curl) 
    {
        BotLogError << "Failed to initialize CURL.";
        return;
    }

    std::ostringstream url;
    url << "http://" << LAVALINK_HOST << ":" << LAVALINK_PORT
        << "/v4/sessions/" << session_id << "/players/" << guild_id;

    json voice_payload = 
    {
        {"voice", {
            {"token", voice_token},
            {"endpoint", endpoint},
            {"sessionId", voice_session_id}
        }}
    };

    std::string payload = voice_payload.dump();
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: " + LAVALINK_PASSWORD).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, throw_away);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        BotLogError << "CURL error: " << curl_easy_strerror(res);

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

// HTTP GET 요청으로 Lavalink에서 트랙 데이터를 가져옴
std::string lavalink::load_track(const std::string& identifier) 
{
    CURL* curl = curl_easy_init();
    if (!curl) 
    {
        BotLogError << "Failed to initialize CURL.";
        return "";
    }

    std::ostringstream url;
    url << "http://" << LAVALINK_HOST << ":" << LAVALINK_PORT
        << "/v4/loadtracks?identifier=" << curl_easy_escape(curl, identifier.c_str(), identifier.length());

    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + LAVALINK_PASSWORD).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        BotLogError << "CURL error: " << curl_easy_strerror(res);


    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return response_data;
}

std::string lavalink::load_track_by_title(const std::string& identifier) 
{
    CURL* curl = curl_easy_init();
    if (!curl) 
    {
        BotLogError << "Failed to initialize CURL.";
        return "";
    }

    std::ostringstream url;
    url << "http://" << LAVALINK_HOST << ":" << LAVALINK_PORT
        << "/v4/loadtracks?identifier=ytsearch:" << curl_easy_escape(curl, identifier.c_str(), identifier.length());

    std::string response_data;
    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + LAVALINK_PASSWORD).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        BotLogError << "CURL error: " << curl_easy_strerror(res);
    

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return response_data;
}

//tack_id Null이면 중지
void lavalink::update_player_track(const std::string& session_id, const std::string& guild_id, const std::string& track_id) 
{
    CURL* curl = curl_easy_init();
    if (!curl) 
    {
        BotLogError << "Failed to initialize CURL.";
        return;
    }

    std::ostringstream url;
    url << "http://" << LAVALINK_HOST << ":" << LAVALINK_PORT
        << "/v4/sessions/" << session_id << "/players/" << guild_id;

    json play_payload;
    if(track_id != "NULL")
    {
        play_payload = 
        {
            {"encodedTrack", track_id},
            {"paused", false},
            {"volume", 100}
        };
    }
    else
    {
        play_payload = 
        {
            {"encodedTrack", nullptr}
        };
    }
    

    std::string payload = play_payload.dump();
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("Authorization: " + LAVALINK_PASSWORD).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, throw_away);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
        BotLogError << "CURL error: " << curl_easy_strerror(res);
    else
        // BotLogDebug << "[INFO] Sent player track update request: " << payload;

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
}

// WebSocket Handlers
void lavalink::on_open(client* c, websocketpp::connection_hdl hdl) 
{
    BotLogDebug << "WebSocket connection opened.";
    g_BotManager->initMp3Encode();
}

void lavalink::on_message(client* c, websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) 
{
    // BotLogDebug << "[INFO] Received WebSocket message: " << msg->get_payload();

    json parsed = json::parse(msg->get_payload());

    if (parsed.contains("op")) 
    {
        if(parsed["op"] == "ready")
        {
            m_session_Id = parsed["sessionId"];
            BotLogInfo << "Session ID: " << m_session_Id;
        }
        else if (parsed["op"] == "event" && parsed["type"] == "TrackStartEvent" && !parsed["guildId"].empty())
        {
            dpp::snowflake guild_id = static_cast<dpp::snowflake>(parsed["guildId"].get<std::string>());
            g_BotManager->voiceInfo[guild_id].is_trackPlaying = true;
        }
        else if (parsed["op"] == "event" && parsed["type"] == "TrackEndEvent" && !parsed["guildId"].empty())
        {
            dpp::snowflake guild_id = static_cast<dpp::snowflake>(parsed["guildId"].get<std::string>());
            g_BotManager->voiceInfo[guild_id].is_trackPlaying = false;
        }
    }
}

void lavalink::on_fail(client* c, websocketpp::connection_hdl hdl) 
{
    BotLogError << "WebSocket connection failed.";


    std::string command = "pidof java > /dev/null 2>&1";
    int result = system(command.c_str());

    if(result != 0)
    {
        BotLogError << "Lavalink Is Stopped!!";

        std::thread([this]() {
            std::string server_sh = PROJECT_DIR_PATH + "/lavalink-run.sh";
            system(server_sh.c_str());
        }).detach();
    }
    

    websocketpp::lib::error_code ec;
    m_client.close(hdl,websocketpp::close::status::bad_gateway, "Connection canceled", ec);
        if (ec) {
        BotLogError << "Failed to cancel connection: " << ec.message();
    } else {
        BotLogInfo << "Connection attempt canceled.";
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::string uri = "ws://" + LAVALINK_HOST + ":" + std::to_string(LAVALINK_PORT) + "/v4/websocket";
    client::connection_ptr con = m_client.get_connection(uri, ec);
    if (ec) 
    {
        BotLogError << "Connection initialization failed: " << ec.message();
        return;
    }
    con->append_header("Authorization", LAVALINK_PASSWORD);
    con->append_header("User-Id", g_BotManager->BOT_USER_ID);
    con->append_header("Client-Name", "dog_bs/1.0");

    BotLogInfo << "\033[33m" << "[Lavalink]" << "\033[0m" << " CONN STATUS : " << con->get_state();
    m_client.connect(con);
    
    std::thread(lavalink::clientThread, g_lavalink).detach();
    
}

void lavalink::on_close(client* c, websocketpp::connection_hdl hdl) 
{
    BotLogInfo << "WebSocket connection closed.";
}

void lavalink::initLavaLink() 
{
    BotLogInfo << "\033[33m" << "[Lavalink]" << "\033[0m" << "InIt Lavalink id : " << g_BotManager->BOT_USER_ID;
    std::string uri = "ws://" + LAVALINK_HOST + ":" + std::to_string(LAVALINK_PORT) + "/v4/websocket";

    try {
        m_client.init_asio();

        m_client.set_open_handler(bind(&lavalink::on_open, this, &m_client, _1));
        m_client.set_message_handler(bind(&lavalink::on_message, this, &m_client, _1, _2));
        m_client.set_fail_handler(bind(&lavalink::on_fail, this, &m_client, _1));
        m_client.set_close_handler(bind(&lavalink::on_close, this, &m_client, _1));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = m_client.get_connection(uri, ec);
        if (ec) 
        {
            BotLogError << "Connection initialization failed: " << ec.message();
            return;
        }

        con->append_header("Authorization", LAVALINK_PASSWORD);
        con->append_header("User-Id", g_BotManager->BOT_USER_ID);
        con->append_header("Client-Name", "dog_bs/1.0");

        m_client.connect(con);
        
        std::thread(lavalink::clientThread, g_lavalink).detach();
        // m_client.run();
    } catch (const std::exception& e) {
        BotLogError << "Exception: " << e.what();
    } catch (...) {
        BotLogError << "Unknown exception occurred.";
    }

    return;
}

```

</details>

## botManager.h
lavalink.cpp와 botManager.cpp에서 사용하는 헤더 파일이다 각 메소드 별 동작 설명이 있다

<details>
<summary>botManager.h </summary>

```cpp
#include <dpp/dpp.h>
#include <unordered_map>
#include <deque>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp> 
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <curl/curl.h>


typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using json = nlohmann::json;

struct Song
{
    std::string title;
    std::string url;
    std::string tumbnail;
    std::string track_id;
    std::string username;
    std::string userprofile;
};

struct ServerQueue
{
    std::deque<Song> queue;
};

typedef enum
{
    MSG_PLAY_SONG,
    MSG_QUEUE,
    MSG_DISCONNECT,
}MSG_TYPE;

typedef enum
{
    EMBED_DEFUALT,
    EMBED_PLAY_SONG,
    EMBED_ADD_QUEUE,
}EMBED_CMD;

typedef enum 
{
    LOG_DEBUG,
    LOG_INFO,
    LOG_ERROR,
    LOG_NONE
} LOG_LEVEL;

struct VoiceChanelInfo
{
    dpp::voiceconn* c_voice;
    dpp::discord_client * client;
    ServerQueue server_queues;
    dpp::snowflake msg_channel_Id;
    dpp::snowflake embed_channel_Id;
    dpp::snowflake embed_message_Id;
    bool is_afterPlay = false;
    bool is_connected = false;
    bool is_after_emoji = false;

    bool is_trackPlaying = false;

    std::string req_url;
    std::string req_username;
    std::string req_userprofile;
};

class botManager
{
    private:
        botManager();
        ~botManager();

    public:

        /**
         * @brief guild id로 관리되는 map, 각 서버의 봇 상태 정보를 저장 관리
         */
        std::unordered_map<dpp::snowflake, VoiceChanelInfo> voiceInfo; // guild id

        /**
         * @brief guild id를 callback으로 받을 수 없는 경우 channel id를 통해 guild id를 관리하는 map
         */
        std::unordered_map<dpp::snowflake, dpp::snowflake> voice_channel_map; // channel id -> guild id get

        static botManager *getInstance();

        /**
         * @brief channel id를 통해 voice_channel_map에 저장된 guild id를 return
         */
        dpp::snowflake getGuildId(dpp::snowflake a_channel_id) { return voice_channel_map[a_channel_id];}

        /**
         * @brief 서버의 특정 채널에 bot 채널 등록, json으로 관리하여 한 번 등록한 서버에 지속적으로 사용할 수 있도록 설정
         * @param  guild 특정 서버 id
         * @param channel_id 특정 서버의 채팅방 id
         */
        void add_embed_Channel(dpp::snowflake guild, dpp::snowflake channel_id);

        /**
         * @brief 노래가 바뀌거나 embed를 초기화 시 호출하여 embed update
         * @param guild 서버 id
         * @param cmd embed update type 초기화, 노래 추가, 노래 시작 등 type에 따라 정보 변경 내용이 다름
         * @param entry 노래 play 시 표시 될 해당 노래의 title 정보, cmd type이 play song 경우에만 사용 됨
         * @param tumbnail 노래 play 시 표시 될 해당 노래의 tumbnail 정보, cmd type이 play song 경우에만 사용 됨
         */
        void update_embed(dpp::snowflake guild, EMBED_CMD cmd, Song entry, std::string tumbnail);

        /**
         * @brief 특정 서버의 voiceInfo map에 노래 queue 등록 yt-dlp를 통해 노래 정보를 가져 온 뒤 등록 됨
         * @param guild 서버 id
         * @param a_title yt-dlp로 추출한 영상의 title 정보
         * @param a_stream_url yt-dlp로 추출한 영상의 stream url, 우선으로 opus 코덱의 url 전달 필요
         * @param a_tumbnail yt-dlp로 추출한 영상의 tumbnail 정보
         */
        void add_to_queue(dpp::snowflake guild, Song a_songInfo);

        /**
         * @brief 다음 곡으로 넘어가는 메소드 바로 다음 곡으로 넘기는 것이 아닌 현재 곡을 stop 후 monitor_voice_client thread로 stop 상태 감지 후 다음곡으로 변경 
         * @param guild 서버 id
         */
        void play_next(dpp::snowflake guild);

        /**
         * @brief 현재까지 저장된 queue를 랜덤하게 섞음 
         * @param guild 서버 id
         */
        void shuffle_SongQueue(dpp::snowflake guild);

        /**
         * @brief 봇 상태가 변경될 때 서버의 특정 채널에 메시지 전송 embed가 등록된 채널의 경우 사용되지 않음
         * @param guild 서버 id
         * @param channel_id 메시지를 전송 할 channel id
         * @param msg_type 메시지의 type [add queue, play song, disconnect]
         * @param text 전송 할 메시지 string
         */
        void send_Msg(dpp::snowflake guild, dpp::snowflake channel_id, MSG_TYPE msg_type, std::string text);
        
        /**
         * @brief 봇 초기화 시 embed channel 정보를 json을 통해 읽어 embed 채널 등록
         */
        void initChannelInfo();
        
        /**
         * @brief MP3의 Encode 정보 초기화
         */
        void initMp3Encode();

        /**
         * @brief json 파일을 읽어 저장 된 embed channel List를 json 형태로 리턴
         * @param file_path 저장 된 json 파일의 경로
         */
        json read_json_file(const std::string& file_path);

        /**
         * @brief 채널을 추가하거나 정보가 변경 될 때 json 파일에 write
         * @param file_path 저장 할 json 파일의 경로
         * @param j 채널 정보를 json 형식으로 전달
         */
        void write_json_file(const std::string& file_path, const json& j);

        /**
         * @brief embed 채널을 추가하거나 정보 업데이트 시 호출
         * @param guild_id 서버 id
         * @param channel_id embed channel id
         */
        void add_or_update_guild_channel(const std::string& guild_id, const std::string& channel_id);

        /**
         * @brief json에 저장 된 모든 채널 정보를 읽어 vector로 return
         */
        std::vector<std::tuple<std::string, std::string>> get_all_channels();

        /**
         * @brief DPP 10.1.3 이후로 websocket이 끊어지면 is_connected가 false로 되어 c_voice의 channel_id 값으로 연결 유무 판단
         * @param guild_id 서버 id
         */
        bool is_connected(dpp::snowflake guild);


        std::string LAVALINK_HOST = "localhost";
        int LAVALINK_PORT = 2333;
        std::string LAVALINK_PASSWORD = "youshallnotpass";
        std::string BOT_USER_ID; // Discord 봇 클라이언트 ID
        bool is_martial_law = false;

};

class gptManager 
{

    private:
        std::vector<std::pair<std::string, std::string>> m_gptHistory;
    public:

        static gptManager *getInstance();

        std::string req_gpt_message(std::string msg);

        void add_history(const std::string& Req, const std::string& Resp) 
        {
            if (m_gptHistory.size() >= 40) // req + resp Size
                m_gptHistory.erase(m_gptHistory.begin());
            m_gptHistory.emplace_back("user", Req);
            m_gptHistory.emplace_back("assistant", Resp);
        }

        std::vector<std::pair<std::string, std::string>> get_gptHistory() { return m_gptHistory; }

        int get_gptHistorySize() { return m_gptHistory.size(); }

};

class lavalink
{

    private:
        lavalink();
        ~lavalink();

    public:

        static lavalink *getInstance();
        
        /**
         * @brief lavalink로 play중인 track을 Stop
         * @param  guild 서버 id
         */
        void stop_track(const std::string& guild_id);
        
        /**
         * @brief lavalink에 encoded track을 전달하여 music play
         * @param  guild 서버 id
         * @param  track_id load_track & load_track_by_title을 통해 추출한 encoded String
         */
        void play_audio_track(const std::string& guild_id, const std::string& track_id);

        /**
         * @brief Voice 연결 시 Websocket 서버(lavalink)에 연결 상태를 업데이트
         * @param  guild 서버 id
         */
        void update_websocket_status(const std::string& guild_id);

        /**
         * @brief curl에서 사용하는 callback 함수
         */
        static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output);

         /**
         * @brief update_websocket_status에서 사용되는 메소드 voice 연결 상태를 업데이트
         * @param session_id lavalink의 Session Id
         * @param guild_id 업데이트 시킬 guild id
         * @param voice_token voiceconn의 Token 값
         * @param endpoint voiceconn의 websocket_hostname (websocket Endpoint)
         * @param voice_session_id voiceconn의 Session Id
         */
        void update_voice_state(const std::string& session_id, const std::string& guild_id,
                            const std::string& voice_token, const std::string& endpoint, const std::string& voice_session_id);
        
        /**
         * @brief lavalink를 통해 youtube track 정보 추출
         * @param identifier youtube url
         */
        std::string load_track(const std::string& identifier);

        /**
         * @brief lavalink를 통해 youtube track 정보 추출 (title)
         * @param identifier youtube 영상의 title
         */
        std::string load_track_by_title(const std::string& identifier);


        /**
         * @brief play_audio_track에서 사용되는 메소드 lavalink에 play시킬 track 정보 전달
         * @param session_id lavalink의 Session Id
         * @param guild_id 서버 id
         * @param track_id load_track & load_track_by_title을 통해 추출한 encoded String
         */
        void update_player_track(const std::string& session_id, const std::string& guild_id, const std::string& track_id);

        /**
         * @brief Lavalink에서 전달하는 event & Message Callback Handler, Track && Status 정보는 on_message를 통해 받아옴
         */
        void on_open(client* c, websocketpp::connection_hdl hdl);
        void on_message(client* c, websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg);
        void on_fail(client* c, websocketpp::connection_hdl hdl);
        void on_close(client* c, websocketpp::connection_hdl hdl);

        /**
         * @brief Lavalink 초기화 프로그램과 lavalink client websocket 연결
         */
        void initLavaLink();

        /**
         * @brief lavalink client와 연결 후 비동기로 실행
         */
        static void clientThread(lavalink* instance);

        /**
         * @brief lavalink와 연결 된 client
         */
        client m_client;

        /**
         * @brief lavalink의 Session Id
         */
        std::string m_session_Id;
};

void play_next_in_queue(dpp::snowflake guild_id);
```

</details>

## workthread.cpp
봇의 상호 작용을 받고 실제 이벤트들이 처리되는 코드다

비동기로 동작하며 발생한 이벤트를 workthread내에서 처리하게 된다 

장점은 비동기로 돌아가서 이벤트 처리동안 대기상태에 빠지지 않고 이벤트를 queue로 관리해서 

이벤트가 무시되는 일 없이 동작한다
<details>
<summary>workthread.cpp </summary>

```cpp

#include "../include/ytdlp.h"

TaskManager *TaskManager::getInstance()
{
	static TaskManager instance;
	return &instance;
}

void TaskManager::add_task(dpp::snowflake guild_id, const list_task_bot& task) 
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        guild_queues[guild_id].push(task);
    }
    cv.notify_one();
}

void TaskManager::start() 
{
    std::thread([this]() 
    {
        while (true) 
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this] 
            {
                return !all_queues_empty() || stop_signal;
            });

            if (stop_signal) 
            {
                BotLogDebug << "[Task Manager] Stopping.";
                break;
            }

            for (auto& [guild_id, queue] : guild_queues) 
            {
                if (!queue.empty()) 
                {
                    list_task_bot task = queue.front();
                    queue.pop();

                    lock.unlock();
                    process_task(guild_id, task);
                    lock.lock();
                }
            }
        }
    }).detach();
}

void TaskManager::stop() 
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop_signal = true;
    }
    cv.notify_all();
}
void TaskManager::process_task(dpp::snowflake guild_id, const list_task_bot& task) 
{
    list_task_bot msg = task;

    if(!msg.song_q.title.empty())
    {
        if(g_BotManager->is_connected(msg.guild) == false)
            return;
 
        Song songInfo = msg.song_q;
        if(g_BotManager->is_connected(msg.guild))
        {

            if(msg.msg_id == SIG_PLAYLIST)
            {
#ifdef USE_LAVALINK
                Song a_Song = songInfo;
#else
                Song a_Song = get_youtube_audio_url(songInfo.url);
#endif
                
#ifdef USE_LAVALINK
                if(g_BotManager->voiceInfo[msg.guild].server_queues.queue.empty() && 
                    g_BotManager->voiceInfo[msg.guild].is_trackPlaying == false)
#else
                if(g_BotManager->voiceInfo[msg.guild].server_queues.queue.empty() && 
                    g_BotManager->voiceInfo[msg.guild].c_voice->voiceclient->is_playing() == false)
#endif
                {

                    BotLogInfo << "[Play] Title : " << a_Song.title;
#ifdef USE_LAVALINK
                    play_audio_command(a_Song.track_id, msg.guild);
#else
                    play_audio_command(a_Song.url, msg.guild);
#endif
                    std::string msg_s = "빙신같은 노래 시작 : " + a_Song.title;
                    if(g_BotManager->voiceInfo[msg.guild].embed_channel_Id != 0)
                    {
                        g_BotManager->update_embed(msg.guild, EMBED_PLAY_SONG, a_Song, a_Song.tumbnail);
                    }
                    else
                        g_BotManager->send_Msg(msg.guild, g_BotManager->voiceInfo[msg.guild].msg_channel_Id, MSG_PLAY_SONG, msg_s);
                }
                else
                {
                    if(g_BotManager->voiceInfo[msg.guild].embed_channel_Id != 0)
                        g_BotManager->update_embed(msg.guild, EMBED_ADD_QUEUE, a_Song, a_Song.tumbnail);
                   
                    g_BotManager->add_to_queue(msg.guild, a_Song);
                }
            }
            else if (msg.msg_id == SIG_ADD_QUEUE)
            {
#ifdef USE_LAVALINK
                if(g_BotManager->voiceInfo[msg.guild].server_queues.queue.empty() && 
                    g_BotManager->voiceInfo[msg.guild].is_trackPlaying == false)
#else
                if(g_BotManager->voiceInfo[msg.guild].server_queues.queue.empty() && 
                    g_BotManager->voiceInfo[msg.guild].c_voice->voiceclient->is_playing() == false)
#endif
                {

#ifdef USE_LAVALINK
                    play_audio_command(songInfo.track_id, msg.guild);
#else
                    play_audio_command(songInfo.url, msg.guild);
#endif
                    std::string msg_s = "빙신같은 노래 시작 : " + songInfo.title;

                    if(g_BotManager->voiceInfo[msg.guild].embed_channel_Id != 0)
                    {
                        g_BotManager->update_embed(msg.guild, EMBED_PLAY_SONG, songInfo, songInfo.tumbnail);
                    }
                    else
                        g_BotManager->send_Msg(msg.guild, g_BotManager->voiceInfo[msg.guild].msg_channel_Id, MSG_PLAY_SONG, msg_s);
                }
                else
                {
                    g_BotManager->add_to_queue(msg.guild, songInfo);

                    std::string msg_s = "뭔 요상한 노래 추가 : " + songInfo.title;

                    if(g_BotManager->voiceInfo[msg.guild].embed_channel_Id != 0)
                    {
                        g_BotManager->update_embed(msg.guild, EMBED_ADD_QUEUE, songInfo, songInfo.tumbnail);
                    }
                    else
                        g_BotManager->send_Msg(msg.guild, g_BotManager->voiceInfo[msg.guild].msg_channel_Id, MSG_PLAY_SONG, msg_s);
                }
                BotLogInfo << "[Task Play] Title : " << songInfo.title;
                
            }

            if(!g_BotManager->voiceInfo[msg.guild].req_url.empty())
                g_BotManager->voiceInfo[msg.guild].req_url.clear();
        }
        else
            BotLogError << "[Task] IS NOT CONNECTED";
    }
}
void TaskManager::clear_guild_queue(dpp::snowflake guild_id) 
{
    std::unique_lock<std::mutex> lock(queue_mutex);

    if (guild_queues.find(guild_id) != guild_queues.end()) 
    {
        std::queue<list_task_bot>& queue = guild_queues[guild_id];

        // Queue Clear
        std::queue<list_task_bot> empty;
        std::swap(queue, empty);

    } 
}

bool TaskManager::all_queues_empty() 
{
    for (const auto& [guild_id, queue] : guild_queues) 
    {
        if (!queue.empty()) 
        {
            return false;
        }
    }
    return true;
}

void workerThread() 
{
    BotLogDebug << "[Bot] Thread Start";
    while (true) 
    {
        std::unique_lock<std::mutex> lock(mtx); // 뮤텍스 잠금

        // 시그널이 올 때까지 대기 
        cv.wait(lock, [] { return !taskQueue.empty() || stopSignal; });

        if (stopSignal && taskQueue.empty()) 
        {
            BotLogInfo << "[Thread] Worker thread stopping.";
            break;
        }

        while (!taskQueue.empty()) 
        {
            task_bot msg = taskQueue.front();
            taskQueue.pop();
            BotLogInfo << "[Thread] Processing task: " << msg.msg_id;

            lock.unlock();
            if(g_BotManager->is_connected(msg.guild))
            {
                switch (msg.msg_id)
                {
                    case SIG_START_SONG:
                    break;
                    case SIG_EMOJI_SONG:
                        if(g_BotManager->voiceInfo[msg.guild].embed_channel_Id != 0)
                        {
#ifdef USE_LAVALINK
                            play_audio_command(local_encoded, msg.guild);
#else
                            play_audio_command(MP3_DIR, msg.guild);
#endif
                            Song tmp_data;
                            tmp_data.title = "오늘도 람배타임";
                            g_BotManager->update_embed(msg.guild, EMBED_PLAY_SONG, tmp_data, "");
                        }
                    break;
                    case SIG_GET_SONGINFO:
                        if(!g_BotManager->voiceInfo[msg.guild].req_url.empty())
                        {
                            std::queue<Song> SongList;

                            list_task_bot list_task;
                            list_task.guild = msg.guild;
                            if(g_BotManager->voiceInfo[msg.guild].req_url.find(HTTPS_KEYWORD) != std::string::npos)
                            {
                                if(g_BotManager->voiceInfo[msg.guild].req_url.find("playlist?") != std::string::npos)
                                {
                                    get_youtube_playList(g_BotManager->voiceInfo[msg.guild].req_url, msg.guild, SongList);
                                    list_task.msg_id = SIG_PLAYLIST;
                                }
                                else
                                {
                                    list_task.song_q = get_youtube_audio_url(g_BotManager->voiceInfo[msg.guild].req_url);
                                    list_task.song_q.username = g_BotManager->voiceInfo[msg.guild].req_username;
                                    list_task.song_q.userprofile = g_BotManager->voiceInfo[msg.guild].req_userprofile;
                                    list_task.msg_id = SIG_ADD_QUEUE;
                                }
                            }
                            else
                            {
                                list_task.song_q = get_youtube_audio_by_title(g_BotManager->voiceInfo[msg.guild].req_url);
                                list_task.song_q.username = g_BotManager->voiceInfo[msg.guild].req_username;
                                list_task.song_q.userprofile = g_BotManager->voiceInfo[msg.guild].req_userprofile;
                                list_task.msg_id = SIG_ADD_QUEUE;
                            }

                            if(list_task.msg_id == SIG_PLAYLIST)
                            {
                                std::string size = std::to_string(SongList.size());
                                std::string msg_s = "노래 " + size + " 곡 추가맨";
#ifdef USE_LAVALINK
                                if(!SongList.empty())
                                {
                                    list_task.song_q = SongList.front();
                                    g_taskManager->add_task(list_task.guild, list_task);
                                    SongList.pop();

                                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                    while(!SongList.empty())
                                    {
                                        g_BotManager->add_to_queue(msg.guild, SongList.front());
                                        SongList.pop();
                                    }
                                }
#else
                                while(!SongList.empty())
                                {
                                    list_task.song_q = SongList.front();
                                    g_taskManager->add_task(list_task.guild, list_task);
                                    SongList.pop();
                                }
#endif
                                if(g_BotManager->voiceInfo[msg.guild].embed_channel_Id == 0)
                                    g_BotManager->send_Msg(msg.guild, g_BotManager->voiceInfo[msg.guild].msg_channel_Id, MSG_QUEUE, msg_s);
                                else
                                {
#ifdef USE_LAVALINK
                                    Song tmp;
                                    g_BotManager->update_embed(msg.guild, EMBED_ADD_QUEUE, tmp, "");
#endif
                                }
                            }
                            else
                            {
                                if(!list_task.song_q.url.empty())
                                {
                                    g_taskManager->add_task(list_task.guild, list_task);
                                }
                            }
                            g_BotManager->voiceInfo[msg.guild].req_url.clear();
                            g_BotManager->voiceInfo[msg.guild].req_username.clear();
                            g_BotManager->voiceInfo[msg.guild].req_userprofile.clear();
                        }
                    break;
                    case SIG_NEXT_SONG:
                        BotLogDebug << "[Thread Next PLAY] Next Song Q Size : " << g_BotManager->voiceInfo[msg.guild].server_queues.queue.size();
                        if(!g_BotManager->voiceInfo[msg.guild].server_queues.queue.empty())
                            g_BotManager->play_next(msg.guild);
                        else
                        {
                            if(g_BotManager->is_connected(msg.guild) && g_BotManager->voiceInfo[msg.guild].client)
                            {
                                BotLogInfo << "[Thread Next Play] Disconnect Try\n";
                                if(g_BotManager->voiceInfo[msg.guild].embed_channel_Id == 0)
                                    g_BotManager->send_Msg(msg.guild, g_BotManager->voiceInfo[msg.guild].msg_channel_Id, MSG_DISCONNECT, "대기열이 없노 형 나간다");

                                g_BotManager->voiceInfo[msg.guild].client->disconnect_voice(msg.guild);
                            }
                    
                        }
                    break;
                    case SIG_FFMPEG_INIT_ERROR:
                        if(g_BotManager->voiceInfo[msg.guild].server_queues.queue.size() > 1)
                        {
                            // init 에러가 발생하고 play가 안되고 있을때 다음곡
#ifdef USE_LAVALINK
                            if(g_BotManager->is_connected(msg.guild) && g_BotManager->voiceInfo[msg.guild].client &&
                                !g_BotManager->voiceInfo[msg.guild].is_trackPlaying)
#else
                            if(g_BotManager->is_connected(msg.guild) && g_BotManager->voiceInfo[msg.guild].client &&
                                !g_BotManager->voiceInfo[msg.guild].c_voice->voiceclient->is_playing())
#endif
                            {
                                sendSignal(msg.guild, SIG_NEXT_SONG);
                            }
                        }
                    break;
                
                    default:
                    break;
                }
            }
            else
            {

            }
            lock.lock(); // 다음 작업을 위해 다시 잠금
        }
    }
}

void sendSignal(dpp::snowflake guild_id, Sig_Bot a_msgId)
{
    if (g_BotManager->is_martial_law)
        return;

    BotLogDebug << "[SIG Send] guid : " << guild_id << ", Id : " << a_msgId;
    task_bot a_task;
    a_task.guild = guild_id;
    a_task.msg_id = a_msgId;

    taskQueue.push(a_task);
    cv.notify_one();
    return;
}

```

</details>


