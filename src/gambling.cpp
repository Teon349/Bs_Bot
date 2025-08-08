#include "../include/gambling.h"

json load_json(const std::string& filename) 
{
    std::ifstream ifs(filename);
    json j;
    if (ifs.is_open()) 
    {
        try {
            ifs >> j;
        }
        catch (const json::parse_error& e) {
            BotLogError << "JSON Parsing Error: " << e.what();
            j = json::object();
            j["users"] = json::array();
        }
    }
    else 
    {
        j = json::object();
        j["users"] = json::array();
    }
    return j;
}

void save_json(const std::string& filename, const json& j) 
{
    std::ofstream ofs(filename);
    if (ofs.is_open()) 
    {
        ofs << j.dump(4);
    }
    else 
    {
        BotLogError << "File Save Fail: " << filename;
    }
}

int find_user_index(const json& j, const std::string& user_id) 
{
    if (j.contains("users") && j["users"].is_array()) 
    {
        for (size_t i = 0; i < j["users"].size(); i++) 
        {
            if (j["users"][i].contains("user_id") && j["users"][i]["user_id"] == user_id) 
            {
                return static_cast<int>(i);
            }
        }
    }
    return -1;
}

void add_user(json& j, const std::string& user_id, int initial_money) 
{
    std::time_t now = std::time(nullptr);
    if (find_user_index(j, user_id) != -1) 
    {
        BotLogError << "User [" << user_id << "] Is Already in JSON File.";
        return;
    }
    json newUser;
    newUser["user_id"] = user_id;
    newUser["money"] = std::to_string(initial_money);
    newUser["last_claim"] = std::to_string(now);
    j["users"].push_back(newUser);
}

void update_money(json& j, const std::string& user_id, int new_money) 
{
    int index = find_user_index(j, user_id);
    if (index == -1) 
    {
        BotLogError << "User [" << user_id << "] Not Found.";
        return;
    }
    j["users"][index]["money"] = std::to_string(new_money);
}

long get_money(json& j, const std::string& user_id)
{
    int index = find_user_index(j, user_id);
    if (index == -1) 
    {
        return -1;
    }
    return std::stol(j["users"][index]["money"].get<std::string>());
}

bool claim_daily_reward(json& j, const std::string& user_id) 
{
    std::time_t now = std::time(nullptr);
    const int DAY_SECONDS = 86400; // 24h = 86400sec

    int index = find_user_index(j, user_id);

    if (index == -1) 
    {
        return false;
    }
    std::time_t last_claim = 0;
    if (j["users"][index].contains("last_claim"))
        last_claim = std::stoll(j["users"][index]["last_claim"].get<std::string>());

    if (now - last_claim >= DAY_SECONDS) 
    {
        j["users"][index]["last_claim"] = std::to_string(now);
        return true;
    }
    else 
    {
        int remaining = DAY_SECONDS - static_cast<int>(now - last_claim);
        int hours = remaining / 3600;
        int minutes = (remaining % 3600) / 60;
        int seconds = remaining % 60;
        BotLogDebug << "Time Update " << hours << "h " << minutes << "m " << seconds << "s";
        return false;
    }
}

bool gamble(int chance, int probability) 
{
    return chance <= probability;
}