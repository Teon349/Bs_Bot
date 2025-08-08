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