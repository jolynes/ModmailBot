#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

const std::string BOT_TOKEN = "paste token"; // Replace with your token
const std::string GUILD_ID = "paste server id"; // Replace with your server ID
const std::string MODMAIL_CATEGORY_NAME = "ModMail"; // create category named ModMail or change it urself
const std::string MODMAIL_LOG_CHANNEL = "modmail_log_channel_id"; // Replace with modmail-log channel ID

std::map<std::string, std::string> ticket_start_times;

std::string now() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append((char*)contents, total_size);
    return total_size;
}

void send_message(httplib::Client& client, const std::string& channel_id, const json& payload) {
    auto res = client.Post(("/channels/" + channel_id + "/messages").c_str(),
                           "Bot " + BOT_TOKEN,
                           payload.dump(),
                           "application/json");
    if (res && (res->status == 200 || res->status == 204)) {
        std::cout << "[" << now() << "] Message sent: " << payload["content"] << "\n";
    } else {
        std::cout << "[" << now() << "] Failed to send message: " << (res ? res->status : -1) << "\n";
    }
}

void send_embed(httplib::Client& client, const std::string& channel_id, const json& embed) {
    json payload = {{"embeds", {embed}}};
    send_message(client, channel_id, payload);
}

void create_channel(httplib::Client& client, const std::string& guild_id, const std::string& name, const std::string& category_id) {
    json payload = {
        {"name", name},
        {"type", 0}, // GUILD_TEXT
        {"parent_id", category_id}
    };
    auto res = client.Post(("/guilds/" + guild_id + "/channels").c_str(),
                           "Bot " + BOT_TOKEN,
                           payload.dump(),
                           "application/json");
    if (res && res->status == 200) {
        std::cout << "[" << now() << "] Created channel: " << name << "\n";
    } else {
        std::cout << "[" << now() << "] Failed to create channel: " << (res ? res->status : -1) << "\n";
    }
}

void delete_channel(httplib::Client& client, const std::string& channel_id) {
    auto res = client.Delete(("/channels/" + channel_id).c_str(),
                             "Bot " + BOT_TOKEN,
                             "application/json");
    if (res && (res->status == 200 || res->status == 204)) {
        std::cout << "[" << now() << "] Deleted channel: " << channel_id << "\n";
    } else {
        std::cout << "[" << now() << "] Failed to delete channel: " << (res ? res->status : -1) << "\n";
    }
}

void on_ready(httplib::Client& client) {
    std::cout << "[" << now() << "] Bot online\n";
}

void handle_message(httplib::Client& client, const json& message) {
    if (message["author"]["bot"].get<bool>()) return;

    if (message["channel_id"].get<std::string>().find("dm") != std::string::npos) {
        std::string user_id = message["author"]["id"].get<std::string>();
        std::string channel_name = "modmail-" + user_id;

        // Check for category
        auto category_res = client.Get(("/guilds/" + GUILD_ID + "/channels").c_str(),
                                      "Bot " + BOT_TOKEN);
        std::string category_id;
        if (category_res && category_res->status == 200) {
            json channels = json::parse(category_res->body);
            for (const auto& channel : channels) {
                if (channel["type"] == 4 && channel["name"] == MODMAIL_CATEGORY_NAME) {
                    category_id = channel["id"].get<std::string>();
                    break;
                }
            }
        }

        if (category_id.empty()) {
            json payload = {{"name", MODMAIL_CATEGORY_NAME}, {"type", 4}};
            auto res = client.Post(("/guilds/" + GUILD_ID + "/channels").c_str(),
                                   "Bot " + BOT_TOKEN,
                                   payload.dump(),
                                   "application/json");
            if (res && res->status == 200) {
                category_id = json::parse(res->body)["id"].get<std::string>();
            }
        }

        // Check for existing channel
        std::string channel_id;
        if (!category_id.empty()) {
            auto res = client.Get(("/guilds/" + GUILD_ID + "/channels").c_str(),
                                  "Bot " + BOT_TOKEN);
            if (res && res->status == 200) {
                json channels = json::parse(res->body);
                for (const auto& channel : channels) {
                    if (channel["name"] == channel_name) {
                        channel_id = channel["id"].get<std::string>();
                        break;
                    }
                }
            }
        }

        if (channel_id.empty()) {
            create_channel(client, GUILD_ID, channel_name, category_id);
            // Fetch new channel ID (simplified; in practice, use the response from create_channel)
            auto res = client.Get(("/guilds/" + GUILD_ID + "/channels").c_str(),
                                  "Bot " + BOT_TOKEN);
            if (res && res->status == 200) {
                json channels = json::parse(res->body);
                for (const auto& channel : channels) {
                    if (channel["name"] == channel_name) {
                        channel_id = channel["id"].get<std::string>();
                        break;
                    }
                }
            }

            json message_payload = {{"content", "📬 New ModMail started by <@" + user_id + ">"}};
            send_message(client, channel_id, message_payload);
            json user_message = {{"content", "**" + message["author"]["username"].get<std::string>() + ":** " + message["content"].get<std::string>()}};
            send_message(client, channel_id, user_message);

            json user_response = {{"content", "✅ Your message has been sent to the support team!"}};
            send_message(client, message["channel_id"].get<std::string>(), user_response);

            ticket_start_times[channel_id] = now();

            json log_embed = {
                {"title", "📉 New Ticket"},
                {"description", message["author"]["username"].get<std::string>() + " | `" + user_id + "` • " + now()},
                {"color", 0x5865F2}
            };
            send_embed(client, MODMAIL_LOG_CHANNEL, log_embed);
        } else {
            json user_message = {{"content", "**" + message["author"]["username"].get<std::string>() + ":** " + message["content"].get<std::string>()}};
            send_message(client, channel_id, user_message);
        }
    } else if (message["guild_id"] == GUILD_ID) {
        auto res = client.Get(("/channels/" + message["channel_id"].get<std::string>()).c_str(),
                              "Bot " + BOT_TOKEN);
        if (res && res->status == 200) {
            json channel = json::parse(res->body);
            if (channel["parent_id"].is_string()) {
                auto category_res = client.Get(("/channels/" + channel["parent_id"].get<std::string>()).c_str(),
                                               "Bot " + BOT_TOKEN);
                if (category_res && category_res->status == 200) {
                    json category = json::parse(category_res->body);
                    if (category["name"] == MODMAIL_CATEGORY_NAME) {
                        std::string user_id = channel["name"].get<std::string>().replace(0, 8, "");
                        json user_message = {{"content", message["content"].get<std::string>()}};
                        send_message(client, user_id, user_message);
                    }
                }
            }
        }
    }
}

void handle_close_command(httplib::Client& client, const json& message, const std::string& reason) {
    auto res = client.Get(("/channels/" + message["channel_id"].get<std::string>()).c_str(),
                          "Bot " + BOT_TOKEN);
    if (res && res->status == 200) {
        json channel = json::parse(res->body);
        if (!channel["parent_id"].is_string() || channel["name"] != ("modmail-" + message["author"]["id"].get<std::string>())) {
            json payload = {{"content", "❌ This command can only be used in a modmail thread."}};
            send_message(client, message["channel_id"].get<std::string>(), payload);
            return;
        }
    }

    if (reason.empty()) {
        json payload = {{"content", "❌ You must provide a reason. Usage: `!close <reason>`"}};
        send_message(client, message["channel_id"].get<std::string>(), payload);
        return;
    }

    std::string user_id = message["channel_id"].get<std::string>().replace(0, 8, "");
    std::string start_time = ticket_start_times[message["channel_id"].get<std::string>()];
    auto now_time = std::time(nullptr);
    auto start_time_t = std::mktime(std::localtime(&now_time));
    std::string formatted_duration = start_time.empty() ? "Unknown" : std::to_string((now_time - start_time_t) / 3600) + "h " + std::to_string((now_time - start_time_t % 3600) / 60) + "m " + std::to_string(now_time - start_time_t % 60) + "s";

    json embed = {
        {"title", "📫 ModMail Closed"},
        {"color", 0xFF0000},
        {"fields", {
            {{"name", "Closed By"}, {"value", message["author"]["username"].get<std::string>() + " (`" + message["author"]["id"].get<std::string>() + "`)"}, {"inline", false}},
            {{"name", "Time Open"}, {"value", formatted_duration}, {"inline", false}},
            {{"name", "Reason"}, {"value", reason}, {"inline", false}}
        }},
        {"timestamp", now()}
    };
    send_embed(client, user_id, embed);

    json log_embed = {
        {"title", "🗑️ Ticket Closed"},
        {"description", "Thread: <#" + message["channel_id"].get<std::string>() + "> was closed."},
        {"color", 0xFF0000},
        {"fields", embed["fields"]}
    };
    send_embed(client, MODMAIL_LOG_CHANNEL, log_embed);

    json response = {{"content", "✅ Ticket closed."}};
    send_message(client, message["channel_id"].get<std::string>(), response);

    delete_channel(client, message["channel_id"].get<std::string>());
}

int main() {
    httplib::Client client("https://discord.com/api/v10");
    client.set_default_headers({{"Authorization", "Bot " + BOT_TOKEN}});

    on_ready(client);

    // Simulate message handling (in practice, use WebSocket or DPP)
    json sample_message = {
        {"author", {{"id", "user_id"}, {"username", "TestUser"}, {"bot", false}}},
        {"channel_id", "channel_id"},
        {"guild_id", GUILD_ID},
        {"content", "Hello!"}
    };
    handle_message(client, sample_message);

    json close_command = {
        {"author", {{"id", "user_id"}, {"username", "TestUser"}, {"bot", false}}},
        {"channel_id", "modmail-user_id"},
        {"guild_id", GUILD_ID},
        {"content", "!close Test reason"}
    };
    handle_close_command(client, close_command, "Test reason");

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        std::cout << "[" << now() << "] Bot running...\n";
    }

    return 0;
}
