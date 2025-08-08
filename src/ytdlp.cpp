#include <iostream>
#include <string>
#include "../include/ytdlp.h"
#include <time.h>

#define xDEBUG_YTDLP 

Song get_youtube_playList(std::string youtube_url, dpp::snowflake guild, std::queue<Song> &list_task) 
{
    std::string result_url;
    std::string result_title;
    Song a_SongInfo;

    // BotLogDebug << "Get youtube Url : " << youtube_url;

    std::string response = g_lavalink->load_track(youtube_url);
    json track_data = json::parse(response);

    if (track_data.contains("data") && track_data["data"].contains("tracks")) 
    {
        for(int i = 0; i < track_data["data"]["tracks"].size(); i++)
        {
            json Song_data = track_data["data"]["tracks"][i];
            a_SongInfo = {"", "", "", ""};
            if(Song_data.contains("encoded"))
            {
                a_SongInfo.track_id = Song_data["encoded"];
                // BotLogDebug << a_SongInfo.track_id;
                a_SongInfo.title = Song_data["info"]["title"];
                // BotLogDebug << a_SongInfo.title;
                a_SongInfo.url = Song_data["info"]["uri"]; // 왠진 모르겠는데 url이 아니라 uri임 ?????
                // BotLogDebug << a_SongInfo.url;
                a_SongInfo.tumbnail = Song_data["info"]["artworkUrl"];
                a_SongInfo.username = g_BotManager->voiceInfo[guild].req_username;
                a_SongInfo.userprofile = g_BotManager->voiceInfo[guild].req_userprofile;
                // BotLogDebug << a_SongInfo.tumbnail;
                list_task.push(a_SongInfo);
            }
        }
    } 
    else 
    {
        BotLogError << "No valid track data found.";
    }
    g_BotManager->voiceInfo[guild].req_username.clear();
    g_BotManager->voiceInfo[guild].req_userprofile.clear();
    return a_SongInfo;
}

Song get_youtube_audio_url(std::string youtube_url) 
{
    std::string result_url;
    std::string result_title;
    std::string thumbnail_url;
    Song a_SongInfo;

    // BotLogDebug << "Get youtube Url : " << youtube_url;

    std::string response = g_lavalink->load_track(youtube_url);
    json track_data = json::parse(response);
    if (track_data.contains("data") && track_data["data"].contains("encoded")) 
    {
        a_SongInfo.track_id = track_data["data"]["encoded"];
        // BotLogDebug << a_SongInfo.track_id;
        a_SongInfo.title = track_data["data"]["info"]["title"];
        // BotLogDebug << a_SongInfo.title;
        a_SongInfo.url = track_data["data"]["info"]["uri"]; // 왠진 모르겠는데 url이 아니라 uri임 ?????
        // BotLogDebug << a_SongInfo.url;
        a_SongInfo.tumbnail = track_data["data"]["info"]["artworkUrl"];
        // BotLogDebug << a_SongInfo.tumbnail;
    } 
    else 
    {
        BotLogError << "No valid track data found.";
    }

    return a_SongInfo;
}

Song get_youtube_audio_by_title(std::string youtube_url) 
{
    std::string result_url;
    std::string result_title;
    std::string thumbnail_url;
    Song a_SongInfo;

    // BotLogDebug << "Get youtube Title : " << youtube_url;

    std::string response = g_lavalink->load_track_by_title(youtube_url);
    json track_data = json::parse(response);
    // BotLogDebug << "BOT : " << track_data.dump(4);
    if (track_data.contains("data") && !track_data["data"].empty() && track_data["data"].is_array()) 
    {
        json search_track = track_data["data"][0];

        a_SongInfo.track_id = search_track["encoded"];
        // BotLogDebug << a_SongInfo.track_id;
        a_SongInfo.title = search_track["info"]["title"];
        // BotLogDebug << a_SongInfo.title;
        a_SongInfo.url = search_track["info"]["uri"]; // 왠진 모르겠는데 url이 아니라 uri임 ?????
        // BotLogDebug << a_SongInfo.url;
        a_SongInfo.tumbnail = search_track["info"]["artworkUrl"];
        // BotLogDebug << a_SongInfo.tumbnail;
    } 
    else 
    {
        BotLogError << "No valid track data found.";
    }

    return a_SongInfo;
}
