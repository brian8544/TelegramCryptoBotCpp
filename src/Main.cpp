#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <map>
#include <queue>
#include <vector>
#include <chrono>
#include <thread>
#include <ctime>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <cmath>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <tgbot/tgbot.h>

#define HISTORY_POINTS 12 // Keeps 6 hours of data with 30-minute intervals

using namespace std;
using namespace nlohmann;

// Function to handle the response data from curl
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class ConfigReader {
private:
    map<string, string> config;

public:
    ConfigReader(const string& filePath) {
        ifstream file(filePath);
        string line;

        while (getline(file, line)) {
            if (line.empty() || line[0] == '#' || line.find('=') == string::npos)
                continue;

            size_t pos = line.find('=');
            string key = line.substr(0, pos);
            string value = line.substr(pos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            config[key] = value;
        }
    }

    string get(const string& key) const {
        auto it = config.find(key);
        return it != config.end() ? it->second : "";
    }
};

class CryptoBot {
private:
    unique_ptr<TgBot::Bot> bot;
    string coinmarketcap_api_key;
    string telegram_channel;
    bool ai_enabled;
    string ai_api_key;
    int update_interval_ms;
    int countdown_interval_ms;

    chrono::system_clock::time_point last_update_time;
    map<string, double> last_prices;
    queue<map<string, double>> price_history;
    vector<string> crypto_symbols{ "BTC", "XRP", "ETH", "SOL" };
    chrono::system_clock::time_point current_hour_start;
    int requests_this_hour;

    CURL* curl;
    struct curl_slist* headers;
    CURL* openrouter_curl;
    struct curl_slist* openrouter_headers;

public:
    CryptoBot(const ConfigReader& config)
        : bot(make_unique<TgBot::Bot>(config.get("TELEGRAM_BOT_TOKEN"))),
        coinmarketcap_api_key(config.get("COINMARKETCAP_API_KEY")),
        telegram_channel(config.get("TELEGRAM_CHANNEL")),
        ai_enabled(config.get("AI_ENABLED") == "true"),
        ai_api_key(config.get("AI_API_KEY")),
        update_interval_ms(stoi(config.get("UPDATE_INTERVAL_MS"))),
        countdown_interval_ms(stoi(config.get("COUNTDOWN_INTERVAL_MS"))),
        current_hour_start(chrono::system_clock::now()),
        requests_this_hour(1),
        curl(curl_easy_init()),
        openrouter_curl(curl_easy_init()) {

        if (!curl || !openrouter_curl) {
            throw runtime_error("Failed to initialize CURL.");
        }

        last_update_time = chrono::system_clock::now();

        // Setup CoinMarketCap headers
        headers = curl_slist_append(nullptr, ("X-CMC_PRO_API_KEY: " + coinmarketcap_api_key).c_str());
        headers = curl_slist_append(headers, "Accepts: application/json");

        // Setup OpenRouter headers
        openrouter_headers = curl_slist_append(nullptr, ("Authorization: Bearer " + ai_api_key).c_str());
        openrouter_headers = curl_slist_append(openrouter_headers, "HTTP-Referer: https://brianoost.com/");
        openrouter_headers = curl_slist_append(openrouter_headers, "X-Title: Crypto Analysis Bot");
        openrouter_headers = curl_slist_append(openrouter_headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(openrouter_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    }

    ~CryptoBot() {
        if (curl) curl_easy_cleanup(curl);
        if (openrouter_curl) curl_easy_cleanup(openrouter_curl);
        if (headers) curl_slist_free_all(headers);
        if (openrouter_headers) curl_slist_free_all(openrouter_headers);
    }

    void start() {
        send_startup_message();
        start_countdown_timer();
        run_update_loop();
    }

private:
    void send_startup_message() {
        auto now = chrono::system_clock::now();
        time_t current_time = chrono::system_clock::to_time_t(now);
        tm local_tm;
        localtime_s(&local_tm, &current_time);

        stringstream ss;
        ss << "🤖 Bot Started\n"
            << "✅ Status: Online\n"
            << "🪙 Tracking: ";

        for (size_t i = 0; i < crypto_symbols.size(); ++i) {
            ss << crypto_symbols[i];
            if (i < crypto_symbols.size() - 1) ss << ", ";
        }

        ss << "\n⏱️ Update Interval: " << (update_interval_ms / 60000) << " minutes.\n"
            << "🕒 Start Time: " << put_time(&local_tm, "%d-%m-%Y %H:%M:%S") << "\n"
            << "\nMade with ❤️ by: @brian8544";

        send_telegram_message(ss.str());
    }

    void start_countdown_timer() {
        thread countdown_thread([this]() {
            while (true) {
                auto now = chrono::system_clock::now();
                auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - last_update_time);
                auto remaining = chrono::milliseconds(update_interval_ms) - elapsed;

                if (remaining.count() > 0) {
                    auto minutes = chrono::duration_cast<chrono::minutes>(remaining);
                    auto seconds = chrono::duration_cast<chrono::seconds>(remaining - minutes);
                    cout << "Time remaining for next update: "
                        << minutes.count() << " minutes and "
                        << seconds.count() << " seconds." << endl;
                }

                this_thread::sleep_for(chrono::milliseconds(countdown_interval_ms));
            }
            });
        countdown_thread.detach();
    }

    void run_update_loop() {
        int update_count = 0;
        auto last_successful_update = chrono::system_clock::now();

        while (true) {
            try {
                auto prices = fetch_crypto_prices();
                string message = format_update_message(prices, ++update_count);
                send_telegram_message(message);

                requests_this_hour++;
                last_successful_update = chrono::system_clock::now();
                last_update_time = chrono::system_clock::now();
                this_thread::sleep_for(chrono::milliseconds(update_interval_ms));
            }
            catch (const exception& e) {
                handle_error(e);
                this_thread::sleep_for(chrono::seconds(5));
            }
        }
    }

    double calculate_volatility(const vector<double>& prices) {
        if (prices.size() < 2) return 0;

        vector<double> returns;
        for (size_t i = 1; i < prices.size(); i++) {
            double return_value = ((prices[i] - prices[i - 1]) / prices[i - 1]) * 100;
            returns.push_back(return_value);
        }

        double mean = accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double sum_squares = 0;
        for (double r : returns) {
            sum_squares += pow(r - mean, 2);
        }
        double variance = sum_squares / (returns.size() - 1);
        return sqrt(variance);
    }

    map<string, pair<double, double>> fetch_crypto_prices() {
        map<string, pair<double, double>> prices;
        string symbols_str;
        for (const auto& symbol : crypto_symbols) {
            if (!symbols_str.empty()) symbols_str += ",";
            symbols_str += symbol;
        }

        string url_eur = "https://pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest?symbol=" + symbols_str + "&convert=EUR";
        string url_usd = "https://pro-api.coinmarketcap.com/v1/cryptocurrency/quotes/latest?symbol=" + symbols_str + "&convert=USD";

        string response_eur, response_usd;
        if (perform_curl_request(url_eur, response_eur) && perform_curl_request(url_usd, response_usd)) {
            json json_eur = json::parse(response_eur);
            json json_usd = json::parse(response_usd);

            for (const auto& symbol : crypto_symbols) {
                double eur_price = json_eur["data"][symbol]["quote"]["EUR"]["price"];
                double usd_price = json_usd["data"][symbol]["quote"]["USD"]["price"];
                prices[symbol] = make_pair(eur_price, usd_price);
                cout << "Fetched " << symbol << " prices: " << eur_price << " EUR, " << usd_price << " USD" << endl;
            }
        }

        return prices;
    }

    bool perform_curl_request(const string& url, string& response) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            cerr << "CURL request failed: " << curl_easy_strerror(res) << endl;
            return false;
        }

        return true;
    }

    string get_ai_analysis(const map<string, tuple<double, double, double>>& price_data) {
        if (!ai_enabled) {
            return "⚠️ AI Analysis is currently disabled.";
        }

        try {
            stringstream context;
            context << "Current Prices:\n";

            for (const auto& [symbol, data] : price_data) {
                auto [price, change, percent_change] = data;
                string sign = change >= 0 ? "+" : "";
                context << symbol << ": Current: €" << fixed << setprecision(2) << price
                    << ", Change: " << sign << change
                    << " (" << sign << percent_change << "%)\n";
            }

            json request = {
                {"model", "meta-llama/llama-3.1-70b-instruct:free"},
                {"messages", json::array({
                    {
                        {"role", "user"},
                        {"content", "Analyze this cryptocurrency data and provide a simple trading summary:\n\n" +
                                  context.str() +
                                  "\nProvide a brief overview:\n"
                                  "1. Which coins are trending up/down?\n"
                                  "2. Which coins have high volatility?\n"
                                  "3. One key trading opportunity, if any.\n"
                                  "4. Main risk to watch for.\n\n"
                                  "Keep the analysis simple and actionable. Focus on the most important points only."}
                    }
                })}
            };

            string request_str = request.dump();
            string response;

            curl_easy_setopt(openrouter_curl, CURLOPT_URL, "https://openrouter.ai/api/v1/chat/completions");
            curl_easy_setopt(openrouter_curl, CURLOPT_HTTPHEADER, openrouter_headers);
            curl_easy_setopt(openrouter_curl, CURLOPT_POSTFIELDS, request_str.c_str());
            curl_easy_setopt(openrouter_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(openrouter_curl, CURLOPT_WRITEDATA, &response);

            if (curl_easy_perform(openrouter_curl) != CURLE_OK) {
                return "⚠️ AI Analysis unavailable at this time.";
            }

            json response_json = json::parse(response);
            string analysis = response_json["choices"][0]["message"]["content"];

            return "\n\n📊 Quick Analysis:\n" + analysis;
        }
        catch (const exception& e) {
            cerr << "AI Analysis Error: " << e.what() << endl;
            return "⚠️ AI Analysis unavailable at this time.";
        }
    }

    string format_update_message(const map<string, pair<double, double>>& current_prices, int update_count) {
        map<string, tuple<double, double, double>> price_changes;

        // Get current time in Netherlands timezone
        auto now = chrono::system_clock::now();
        time_t current_time = chrono::system_clock::to_time_t(now);
        tm local_tm;
        localtime_s(&local_tm, &current_time);

        // Calculate next update time
        auto next_update = now + chrono::milliseconds(update_interval_ms);
        time_t next_update_time = chrono::system_clock::to_time_t(next_update);
        tm next_update_tm;
        localtime_s(&next_update_tm, &next_update_time);

        for (const auto& entry : current_prices) {
            const auto& symbol = entry.first;
            const auto& [eur_price, usd_price] = entry.second;

            double last_price = last_prices.count(symbol) ? last_prices[symbol] : eur_price;
            double change = eur_price - last_price;
            double percent_change = last_price > 0 ? (change / last_price) * 100 : 0;

            price_changes[symbol] = make_tuple(eur_price, change, percent_change);
        }

        stringstream message;
        message << "🔄 Crypto Update • "
            << put_time(&local_tm, "%H:%M %d-%m-%Y") << "\n"
            << "⏳ Next update at: "
            << put_time(&next_update_tm, "%H:%M") << "\n\n";

        for (const auto& entry : current_prices) {
            const auto& symbol = entry.first;
            const auto& [eur_price, usd_price] = entry.second;
            const auto& [_, change, percent_change] = price_changes[symbol];

            string emoji = get_crypto_emoji(symbol);
            string change_emoji = change >= 0 ? "📈" : "📉";
            string sign = change >= 0 ? "+" : "";

            message << emoji << " " << symbol << ": €" << fixed << setprecision(2) << eur_price
                << " | $" << usd_price;

            if (last_prices.count(symbol)) {
                message << " " << change_emoji << " " << sign << percent_change << "%";
            }
            message << "\n";
        }

        // Update price history
        if (price_history.size() >= HISTORY_POINTS) {
            price_history.pop();
        }
        price_history.push(last_prices);

        // Update last prices
        for (const auto& entry : current_prices) {
            last_prices[entry.first] = entry.second.first;
        }

        // Add AI analysis
        message << get_ai_analysis(price_changes);

        return message.str();
    }

    string get_crypto_emoji(const string& symbol) {
        if (symbol == "BTC") return "₿";
        if (symbol == "ETH") return "⟠";
        if (symbol == "SOL") return "◎";
        if (symbol == "XRP") return "✖";
        return "🪙";
    }

    void send_telegram_message(const string& message) {
        try {
            bot->getApi().sendMessage(telegram_channel, message);
            cout << "\n=== SENT TO TELEGRAM ===\n"
                << message
                << "\n======================\n" << endl;
        }
        catch (const TgBot::TgException& e) {
            cerr << "\nERROR SENDING TELEGRAM MESSAGE: \"" << e.what() << "\"" << endl;
        }
    }

    void handle_error(const exception& e) {
        auto now = chrono::system_clock::now();
        time_t current_time = chrono::system_clock::to_time_t(now);
        time_t last_update_time_t = chrono::system_clock::to_time_t(last_update_time);

        stringstream error_message;
        error_message << "⚠️ Bot Error Occurred\n"
            << "❌ Error: " << e.what() << "\n"
            << "🔄 Last Successful Update: " << put_time(gmtime(&last_update_time_t), "%Y-%m-%d %H:%M:%S") << " UTC\n"
            << "🕒 Error Time: " << put_time(gmtime(&current_time), "%Y-%m-%d %H:%M:%S") << " UTC\n"
            << "♻️ Attempting to reconnect...";

        cerr << "\n=== ERROR OCCURRED ===\n"
            << "Error: " << e.what() << "\n"
            << "Stack trace: " << "\n"
            << "===================\n";

        send_telegram_message(error_message.str());
    }
};

int main() {
    try {
        cout << "Starting CryptoBot..." << endl;

        ConfigReader config("Config.conf");
        CryptoBot bot(config);
        bot.start();
    }
    catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}