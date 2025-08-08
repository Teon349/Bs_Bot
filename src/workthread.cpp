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