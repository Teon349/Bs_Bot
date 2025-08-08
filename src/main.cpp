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
