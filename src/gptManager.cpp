#include "../include/global.h"

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) 
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}


gptManager *gptManager::getInstance()
{
	static gptManager instance;
	return &instance;
}

std::string gptManager::req_gpt_message(std::string msg)
{
    std::vector<std::string> keywords = {"좆까", "엿먹어", "엿 먹어", "엿이나", "엿 이나", "쳐먹어", "쳐 먹어", "쌉쳐", "시발", "씨발", "씨빨", "씨2발", "시2발", "병신", "병2신", "장애", "뒤져", "죽어", "죽여"};
    
    if (msg.find("기정") != std::string::npos || msg.find("김보성") != std::string::npos) 
    {
        return "존함을 감히 언급하지 마라.";
    }

    for (const auto& kw : keywords) 
    {
        if (msg.find(kw) != std::string::npos) 
        {
            return "엿이나 쳐먹어";
        }
    }

    BotLogInfo << "[GPT] History Size : " << get_gptHistorySize();
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    std::string answer;
    if (curl) {

        std::vector<json> messages = {
            {{"role", "system"}, {"content", "모든 대답은 최대한 요약해서 대답하며 2000자(띄어쓰기 포함)를 넘지 않게 해주세요. 만약 더 길게 설명이 필요하면 요약해서 2000자 이내로만 답하세요."}},
            {{"role", "system"}, {"content", "당신은 항상 자기 소개를 할 때 항상 '어 그래 개보성이다'라고 해야하며 사용자의 물음에 답변 시 '어 그래'로 답변을 시작하거나 내용 답변 시 '~다'로 대답하세요."}},
            {{"role", "system"}, {"content", "사용자가 비속어를 사용할 시 사용자가 사용한 비속어 그대로 전달하거나 '엿이나 쳐먹어'라고 답변하세요."}},
            {{"role", "system"}, {"content", "사용자가 심기정 또는 기정 관련된 질문을 할 시 '엿이나 쳐먹어'라고 답변하며 '기,정' '기 + 정'과 같이 '기'와 '정' 사이에 특수 문자를 섞은 문자도 마찬가지로 답변하세요."}}
        };

        if (0 < get_gptHistorySize())
        {
             for (const auto& h : get_gptHistory())
                messages.push_back({{"role", h.first}, {"content", h.second}});
        }
        messages.push_back({{"role", "user"}, {"content", msg}});

        json data = {
            {"model", "gpt-4o-mini"},
            {"max_tokens", 8000},
            {"messages", messages}
        };
        std::string json_body = data.dump();

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + GPT_APIKEY).c_str());
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            BotLogError << "curl_easy_perform() failed: " << curl_easy_strerror(res);
            answer = "어쩔";
        }
        else
        {
            json res = json::parse(readBuffer);
            if (res.contains("choices") && !res["choices"].empty() &&
                res["choices"][0].contains("message") &&
                res["choices"][0]["message"].contains("content") &&
                !res["choices"][0]["message"]["content"].get<std::string>().empty())
            {
                answer = res["choices"][0]["message"]["content"];
                BotLogDebug << "[GPT] Response: " << answer;

                add_history(msg, answer);
            }
            else
            {
                BotLogError << "[GPT] Json Error Response: " << res.dump();
                BotLogError << "[GPT] Json Error Request: " << json_body;
                answer = "어쩔";
            }
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    return answer;
}