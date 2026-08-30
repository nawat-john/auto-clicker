#include "config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

static std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if (c == '\b') o << "\\b";
        else if (c == '\f') o << "\\f";
        else if (c == '\n') o << "\\n";
        else if (c == '\r') o << "\\r";
        else if (c == '\t') o << "\\t";
        else if (static_cast<unsigned char>(c) <= 0x1f) {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
        } else {
            o << c;
        }
    }
    return o.str();
}

std::string ConfigManager::serializeToJson(const AppConfig& config) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": 1,\n";
    ss << "  \"mode\": " << (config.mode == ExecutionMode::SYNC ? "\"sync\"" : "\"async\"") << ",\n";
    ss << "  \"globalCpm\": " << config.globalCpm << ",\n";
    ss << "  \"globalIntervalMs\": " << config.globalIntervalMs << ",\n";
    ss << "  \"randomJitterMs\": " << config.randomJitterMs << ",\n";
    ss << "  \"repeatCount\": " << config.repeatCount << ",\n";
    ss << "  \"soundFeedback\": " << (config.soundFeedback ? "true" : "false") << ",\n";
    ss << "  \"hotkeyStartStop\": " << config.hotkeyStartStop << ",\n";
    ss << "  \"hotkeyPick\": " << config.hotkeyPick << ",\n";
    ss << "  \"hotkeyPanic\": " << config.hotkeyPanic << ",\n";
    ss << "  \"items\": [\n";

    for (size_t i = 0; i < config.items.size(); ++i) {
        const auto& item = config.items[i];
        ss << "    {\n";
        ss << "      \"id\": " << item.id << ",\n";
        ss << "      \"name\": \"" << escapeJson(item.name) << "\",\n";
        ss << "      \"actionType\": " << static_cast<int>(item.actionType) << ",\n";
        ss << "      \"coordType\": " << static_cast<int>(item.coordType) << ",\n";
        ss << "      \"x\": " << item.x << ",\n";
        ss << "      \"y\": " << item.y << ",\n";
        ss << "      \"restoreMouse\": " << (item.restoreMouse ? "true" : "false") << ",\n";
        ss << "      \"keyVk\": " << item.keyVk << ",\n";
        ss << "      \"keyName\": \"" << escapeJson(item.keyName) << "\",\n";
        ss << "      \"useCustomRate\": " << (item.useCustomRate ? "true" : "false") << ",\n";
        ss << "      \"cpm\": " << item.cpm << ",\n";
        ss << "      \"intervalMs\": " << item.intervalMs << ",\n";
        ss << "      \"delayBeforeMs\": " << item.delayBeforeMs << ",\n";
        ss << "      \"delayAfterMs\": " << item.delayAfterMs << ",\n";
        ss << "      \"enabled\": " << (item.enabled ? "true" : "false") << "\n";
        ss << "    }" << (i + 1 < config.items.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

static void skipWhitespace(const std::string& s, size_t& pos) {
    while (pos < s.length() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) {
        pos++;
    }
}

static std::string extractString(const std::string& s, size_t& pos) {
    if (pos >= s.length() || s[pos] != '"') return "";
    pos++; // skip open quote
    std::string res;
    while (pos < s.length()) {
        char c = s[pos++];
        if (c == '"') break;
        if (c == '\\' && pos < s.length()) {
            char next = s[pos++];
            if (next == 'n') res += '\n';
            else if (next == 't') res += '\t';
            else if (next == 'r') res += '\r';
            else if (next == '"') res += '"';
            else if (next == '\\') res += '\\';
            else res += next;
        } else {
            res += c;
        }
    }
    return res;
}

static std::string extractValueToken(const std::string& s, size_t& pos) {
    skipWhitespace(s, pos);
    if (pos >= s.length()) return "";
    if (s[pos] == '"') {
        return extractString(s, pos);
    }
    size_t start = pos;
    while (pos < s.length() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']' && s[pos] != '\r' && s[pos] != '\n') {
        pos++;
    }
    std::string token = s.substr(start, pos - start);
    // trim
    while (!token.empty() && isspace(static_cast<unsigned char>(token.back()))) token.pop_back();
    while (!token.empty() && isspace(static_cast<unsigned char>(token.front()))) token.erase(token.begin());
    return token;
}

bool ConfigManager::deserializeFromJson(AppConfig& config, const std::string& json) {
    try {
        AppConfig temp = getDefaultConfig();
        temp.items.clear();

        size_t pos = 0;
        while (pos < json.length()) {
            size_t keyStart = json.find('"', pos);
            if (keyStart == std::string::npos) break;
            pos = keyStart;
            std::string key = extractString(json, pos);
            skipWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ':') pos++;
            skipWhitespace(json, pos);

            if (key == "mode") {
                std::string val = extractValueToken(json, pos);
                temp.mode = (val == "async" || val == "\"async\"") ? ExecutionMode::ASYNC : ExecutionMode::SYNC;
            } else if (key == "globalCpm") {
                temp.globalCpm = std::stoul(extractValueToken(json, pos));
            } else if (key == "globalIntervalMs") {
                temp.globalIntervalMs = std::stoul(extractValueToken(json, pos));
            } else if (key == "randomJitterMs") {
                temp.randomJitterMs = std::stoul(extractValueToken(json, pos));
            } else if (key == "repeatCount") {
                temp.repeatCount = std::stoul(extractValueToken(json, pos));
            } else if (key == "soundFeedback") {
                std::string val = extractValueToken(json, pos);
                temp.soundFeedback = (val == "true");
            } else if (key == "hotkeyStartStop") {
                temp.hotkeyStartStop = std::stoul(extractValueToken(json, pos));
            } else if (key == "hotkeyPick") {
                temp.hotkeyPick = std::stoul(extractValueToken(json, pos));
            } else if (key == "hotkeyPanic") {
                temp.hotkeyPanic = std::stoul(extractValueToken(json, pos));
            } else if (key == "items") {
                // Find array '['
                size_t arrStart = json.find('[', pos);
                if (arrStart != std::string::npos) {
                    pos = arrStart + 1;
                    while (pos < json.length()) {
                        size_t objStart = json.find('{', pos);
                        size_t arrEnd = json.find(']', pos);
                        if (objStart == std::string::npos || (arrEnd != std::string::npos && arrEnd < objStart)) {
                            pos = (arrEnd != std::string::npos) ? arrEnd + 1 : json.length();
                            break;
                        }
                        size_t objEnd = json.find('}', objStart);
                        if (objEnd == std::string::npos) break;

                        std::string itemBlock = json.substr(objStart, objEnd - objStart + 1);
                        ActionItem item;
                        size_t ipos = 0;
                        while (ipos < itemBlock.length()) {
                            size_t ikStart = itemBlock.find('"', ipos);
                            if (ikStart == std::string::npos) break;
                            ipos = ikStart;
                            std::string ikey = extractString(itemBlock, ipos);
                            skipWhitespace(itemBlock, ipos);
                            if (ipos < itemBlock.length() && itemBlock[ipos] == ':') ipos++;
                            skipWhitespace(itemBlock, ipos);
                            std::string ival = extractValueToken(itemBlock, ipos);

                            if (ikey == "id") item.id = std::stoi(ival);
                            else if (ikey == "name") item.name = ival;
                            else if (ikey == "actionType") item.actionType = static_cast<ActionType>(std::stoi(ival));
                            else if (ikey == "coordType") item.coordType = static_cast<CoordType>(std::stoi(ival));
                            else if (ikey == "x") item.x = std::stoi(ival);
                            else if (ikey == "y") item.y = std::stoi(ival);
                            else if (ikey == "restoreMouse") item.restoreMouse = (ival == "true");
                            else if (ikey == "keyVk") item.keyVk = static_cast<WORD>(std::stoul(ival));
                            else if (ikey == "keyName") item.keyName = ival;
                            else if (ikey == "useCustomRate") item.useCustomRate = (ival == "true");
                            else if (ikey == "cpm") item.cpm = std::stoul(ival);
                            else if (ikey == "intervalMs") item.intervalMs = std::stoul(ival);
                            else if (ikey == "delayBeforeMs") item.delayBeforeMs = std::stoul(ival);
                            else if (ikey == "delayAfterMs") item.delayAfterMs = std::stoul(ival);
                            else if (ikey == "enabled") item.enabled = (ival == "true");
                        }
                        temp.items.push_back(item);
                        pos = objEnd + 1;
                    }
                }
            }
        }

        if (temp.items.empty()) {
            temp = getDefaultConfig();
        }
        config = temp;
        return true;
    } catch (...) {
        return false;
    }
}

bool ConfigManager::saveToFile(const AppConfig& config, const std::string& filePath) {
    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << serializeToJson(config);
    return true;
}

bool ConfigManager::loadFromFile(AppConfig& config, const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f.is_open()) return false;
    std::stringstream buffer;
    buffer << f.rdbuf();
    return deserializeFromJson(config, buffer.str());
}

AppConfig ConfigManager::getDefaultConfig() {
    AppConfig cfg;
    cfg.mode = ExecutionMode::SYNC;
    cfg.globalCpm = 600;
    cfg.globalIntervalMs = 100;
    cfg.randomJitterMs = 0;
    cfg.repeatCount = 0; // Infinite
    cfg.soundFeedback = true;
    cfg.hotkeyStartStop = VK_F6;
    cfg.hotkeyPick = VK_F8;
    cfg.hotkeyPanic = VK_F7;

    // Preset items
    ActionItem item1;
    item1.id = 1;
    item1.name = "Target 1 (Mouse Left)";
    item1.actionType = ActionType::MOUSE_LEFT_CLICK;
    item1.coordType = CoordType::FOLLOW_MOUSE;
    item1.x = 0;
    item1.y = 0;
    item1.restoreMouse = false;
    item1.useCustomRate = false;
    item1.cpm = 600;
    item1.intervalMs = 100;
    item1.delayBeforeMs = 0;
    item1.delayAfterMs = 0;
    item1.enabled = true;

    ActionItem item2;
    item2.id = 2;
    item2.name = "Target 2 (Key Spam E)";
    item2.actionType = ActionType::KEY_PRESS;
    item2.keyVk = 'E';
    item2.keyName = "E";
    item2.coordType = CoordType::FOLLOW_MOUSE;
    item2.useCustomRate = false;
    item2.cpm = 600;
    item2.intervalMs = 100;
    item2.delayBeforeMs = 0;
    item2.delayAfterMs = 0;
    item2.enabled = false;

    cfg.items.push_back(item1);
    cfg.items.push_back(item2);
    return cfg;
}
