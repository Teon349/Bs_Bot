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