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
