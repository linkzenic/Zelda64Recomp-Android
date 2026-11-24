#include "ui_save_editor_config.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "json/json.hpp"
#if defined(__ANDROID__)
#include <android/log.h>
#define SAVE_EDITOR_UI_LOG(...) __android_log_print(ANDROID_LOG_INFO, "ZeldaNative", __VA_ARGS__)
#else
#define SAVE_EDITOR_UI_LOG(...)
#endif

#include "recomp_ui.h"
#include "elements/ui_button.h"
#include "elements/ui_container.h"
#include "ultramodern/ultramodern.hpp"
#include "zelda_config.h"
#include "zelda_save_editor.h"
#include "zelda_support.h"

namespace recompui {

namespace {
    SaveEditorConfigPanel* active_save_editor_panel = nullptr;

    const std::vector<std::string> off_on_options{ "Off", "On" };
    const std::vector<std::string> target_file_options{ "Next", "File 1", "File 2" };
    const std::vector<std::string> preserve_day_options{ "Preserve", "Day 1", "Day 2", "Day 3", "Final" };
    const std::vector<std::string> preserve_bool_options{ "Preserve", "No", "Yes" };
    const std::vector<std::string> wallet_options{ "Child", "Adult", "Giant" };
    const std::vector<std::string> sword_options{ "None", "Kokiri", "Razor", "Gilded" };
    const std::vector<std::string> shield_options{ "None", "Hero", "Mirror" };
    const std::vector<std::string> magic_options{ "None", "Single", "Double" };
    const std::vector<std::string> quiver_options{ "None", "30", "40", "50" };
    const std::vector<std::string> bomb_bag_options{ "None", "20", "30", "40" };
    const std::vector<std::string> stick_upgrade_options{ "None", "10", "20", "30" };
    const std::vector<std::string> nut_upgrade_options{ "None", "20", "30", "40" };
    const std::vector<std::string> spin_attack_options{ "Level 1", "Level 2" };
    const std::vector<std::string> dungeon_options{ "Woodfall", "Snowhead", "Great Bay", "Stone Tower" };
    const std::vector<std::string> tingle_map_options{ "Clock Town", "Woodfall", "Snowhead", "Romani", "Great Bay", "Stone Tower" };
    const std::vector<std::string> bottle_slot_options{ "Bottle 1", "Bottle 2", "Bottle 3", "Bottle 4", "Bottle 5", "Bottle 6" };
    const std::vector<std::string> bottle_slot_short_options{ "1", "2", "3", "4", "5", "6" };
    const std::vector<std::string> bottle_options{
        "None",
        "Empty Bottle",
        "Red Potion",
        "Green Potion",
        "Blue Potion",
        "Chateau Romani",
        "Spring Water",
        "Hot Spring Water",
        "Milk",
        "Half Milk",
        "Fairy",
        "Fish",
        "Bugs",
        "Poe",
        "Big Poe",
        "Zora Egg",
        "Deku Princess",
        "Gold Dust",
        "Mushroom",
        "Seahorse",
        "Blue Fire",
        "Hylian Loach"
    };
    constexpr std::array<int32_t, 22> bottle_items{
        0xFF, // ITEM_NONE
        0x12, // ITEM_BOTTLE
        0x13, // ITEM_POTION_RED
        0x14, // ITEM_POTION_GREEN
        0x15, // ITEM_POTION_BLUE
        0x25, // ITEM_CHATEAU
        0x1F, // ITEM_SPRING_WATER
        0x20, // ITEM_HOT_SPRING_WATER
        0x18, // ITEM_MILK_BOTTLE
        0x19, // ITEM_MILK_HALF
        0x16, // ITEM_FAIRY
        0x1A, // ITEM_FISH
        0x1B, // ITEM_BUG
        0x1D, // ITEM_POE
        0x1E, // ITEM_BIG_POE
        0x21, // ITEM_ZORA_EGG
        0x17, // ITEM_DEKU_PRINCESS
        0x22, // ITEM_GOLD_DUST
        0x23, // ITEM_MUSHROOM
        0x24, // ITEM_SEAHORSE
        0x1C, // ITEM_BLUE_FIRE
        0x26, // ITEM_HYLIAN_LOACH
    };
    constexpr std::array<zelda64::save_editor::ValueId, 6> bottle_value_ids{
        zelda64::save_editor::Bottle1,
        zelda64::save_editor::Bottle2,
        zelda64::save_editor::Bottle3,
        zelda64::save_editor::Bottle4,
        zelda64::save_editor::Bottle5,
        zelda64::save_editor::Bottle6,
    };
    constexpr uint32_t no_bottle_slot_selected = static_cast<uint32_t>(-1);
    uint32_t selected_bottle_slot = no_bottle_slot_selected;

    uint32_t setup_auto_apply = 0;
    uint32_t setup_target_file = 0;
    std::string setup_name;
    uint32_t setup_day = 0;
    uint32_t setup_has_tatl = 0;
    uint32_t setup_intro_complete = 0;
    uint32_t setup_owl_save = 0;
    bool setup_has_2ship_import = false;
    std::filesystem::path setup_2ship_import_path;
    std::string setup_2ship_import_name;
    bool setup_2ship_import_in_progress = false;
    bool setup_2ship_import_result_ready = false;
    bool setup_2ship_import_result_success = false;
    bool setup_2ship_import_failed = false;
    bool setup_2ship_import_ui_dirty = false;
    std::string setup_2ship_import_result_error;
    std::mutex setup_2ship_import_mutex;

    constexpr size_t flash_size = 0x20000;
    constexpr size_t save_size = 0x100C;
    constexpr size_t save_context_file_num_offset = 0x3CA0;
    constexpr size_t checksum_offset = 0x24 + 0xFE6;
    constexpr std::array<std::array<size_t, 2>, 2> normal_slot_offsets{{
        {{0x0000, 0x2000}},
        {{0x4000, 0x6000}},
    }};
    constexpr std::array<std::array<size_t, 2>, 2> owl_slot_offsets{{
        {{0x8000, 0xC000}},
        {{0x10000, 0x14000}},
    }};

    class SaveWriter {
    public:
        std::vector<uint8_t> data;

        void bytes(const nlohmann::json& values, size_t count, uint8_t default_value = 0) {
            if (!values.is_array() || values.empty()) {
                data.insert(data.end(), count, default_value);
                return;
            }
            if (values.size() != count) {
                throw std::runtime_error("2Ship save array has an unexpected size");
            }
            for (const auto& value : values) {
                data.push_back(static_cast<uint8_t>(value.get<int32_t>() & 0xFF));
            }
        }

        void bytes_default(size_t count, uint8_t default_value) {
            data.insert(data.end(), count, default_value);
        }

        void pad(size_t count) {
            data.insert(data.end(), count, 0);
        }

        void u8(int64_t value) { data.push_back(static_cast<uint8_t>(value & 0xFF)); }
        void s8(int64_t value) { u8(value); }
        void u16(int64_t value) {
            data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>(value & 0xFF));
        }
        void s16(int64_t value) { u16(value); }
        void u32(int64_t value) {
            data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            data.push_back(static_cast<uint8_t>(value & 0xFF));
        }
        void s32(int64_t value) { u32(value); }
        void u64(uint64_t value) {
            for (int shift = 56; shift >= 0; shift -= 8) {
                data.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
            }
        }
    };

    int64_t json_get(const nlohmann::json& obj, const char* key, int64_t default_value = 0) {
        auto it = obj.find(key);
        return it != obj.end() && it->is_number_integer() ? it->get<int64_t>() : default_value;
    }

    const nlohmann::json& json_array_or_empty(const nlohmann::json& obj, const char* key) {
        static const nlohmann::json empty = nlohmann::json::array();
        auto it = obj.find(key);
        return it != obj.end() ? *it : empty;
    }

    uint16_t save_checksum(const std::vector<uint8_t>& data) {
        uint32_t sum = 0;
        for (uint8_t byte : data) {
            sum += byte;
        }
        return static_cast<uint16_t>(sum & 0xFFFF);
    }

    void write_checksum(std::vector<uint8_t>& data) {
        data[checksum_offset] = 0;
        data[checksum_offset + 1] = 0;
        const uint16_t sum = save_checksum(data);
        data[checksum_offset] = static_cast<uint8_t>((sum >> 8) & 0xFF);
        data[checksum_offset + 1] = static_cast<uint8_t>(sum & 0xFF);
    }

    void pack_vec3s(SaveWriter& writer, const nlohmann::json& pos) {
        writer.s16(json_get(pos, "x"));
        writer.s16(json_get(pos, "y"));
        writer.s16(json_get(pos, "z"));
    }

    std::vector<uint8_t> pack_player_data(const nlohmann::json& data) {
        SaveWriter writer;
        if (auto it = data.find("newf"); it != data.end()) {
            writer.bytes(*it, 6);
        }
        else {
            for (char c : std::string("ZELDA3")) {
                writer.u8(c);
            }
        }
        writer.u16(json_get(data, "threeDayResetCount"));
        writer.bytes(data.value("playerName", nlohmann::json::array()), 8, 0x3E);
        writer.s16(json_get(data, "healthCapacity"));
        writer.s16(json_get(data, "health"));
        writer.s8(json_get(data, "magicLevel"));
        writer.s8(json_get(data, "magic"));
        writer.s16(json_get(data, "rupees"));
        writer.u16(json_get(data, "swordHealth"));
        writer.u16(json_get(data, "tatlTimer"));
        writer.u8(json_get(data, "isMagicAcquired"));
        writer.u8(json_get(data, "isDoubleMagicAcquired"));
        writer.u8(json_get(data, "doubleDefense"));
        writer.u8(json_get(data, "unk_1F"));
        writer.u8(json_get(data, "unk_20"));
        writer.pad(1);
        writer.u16(json_get(data, "owlActivationFlags"));
        writer.u8(json_get(data, "unk_24"));
        writer.pad(1);
        writer.s16(json_get(data, "savedSceneId"));
        return writer.data;
    }

    std::vector<uint8_t> pack_equips(const nlohmann::json& data) {
        SaveWriter writer;
        for (const auto& row : data.value("buttonItems", nlohmann::json::array())) {
            writer.bytes(row, 4);
        }
        if (!data.contains("buttonItems")) {
            writer.bytes_default(16, 0xFF);
        }
        for (const auto& row : data.value("cButtonSlots", nlohmann::json::array())) {
            writer.bytes(row, 4);
        }
        if (!data.contains("cButtonSlots")) {
            writer.bytes_default(16, 0xFF);
        }
        writer.u16(json_get(data, "equipment"));
        return writer.data;
    }

    std::vector<uint8_t> pack_inventory(const nlohmann::json& data) {
        SaveWriter writer;
        writer.bytes(data.value("items", nlohmann::json::array()), 48, 0xFF);
        for (const auto& value : data.value("ammo", nlohmann::json::array())) {
            writer.s8(value.get<int32_t>());
        }
        if (!data.contains("ammo")) {
            writer.bytes_default(24, 0);
        }
        writer.u32(json_get(data, "upgrades"));
        writer.u32(json_get(data, "questItems"));
        writer.bytes(data.value("dungeonItems", nlohmann::json::array()), 10);
        for (const auto& value : data.value("dungeonKeys", nlohmann::json::array())) {
            writer.s8(value.get<int32_t>());
        }
        if (!data.contains("dungeonKeys")) {
            writer.bytes_default(9, 0);
        }
        writer.s8(json_get(data, "defenseHearts"));
        for (const auto& value : data.value("strayFairies", nlohmann::json::array())) {
            writer.s8(value.get<int32_t>());
        }
        if (!data.contains("strayFairies")) {
            writer.bytes_default(10, 0);
        }
        if (auto it = data.find("dekuPlaygroundPlayerName"); it != data.end()) {
            for (const auto& row : *it) {
                writer.bytes(row, 8);
            }
        }
        else {
            writer.bytes_default(24, 0x3E);
        }
        writer.pad(2);
        return writer.data;
    }

    std::vector<uint8_t> pack_permanent_scene_flags(const nlohmann::json& flags) {
        SaveWriter writer;
        if (!flags.is_array() || flags.size() != 120) {
            throw std::runtime_error("2Ship save has invalid scene flags");
        }
        for (const auto& flag : flags) {
            writer.u32(json_get(flag, "chest"));
            writer.u32(json_get(flag, "switch0"));
            writer.u32(json_get(flag, "switch1"));
            writer.u32(json_get(flag, "clearedRoom"));
            writer.u32(json_get(flag, "collectible"));
            writer.u32(json_get(flag, "unk_14"));
            writer.u32(json_get(flag, "rooms"));
        }
        return writer.data;
    }

    std::vector<uint8_t> pack_horse_data(const nlohmann::json& data) {
        SaveWriter writer;
        writer.s16(json_get(data, "sceneId"));
        pack_vec3s(writer, data.value("pos", nlohmann::json::object()));
        writer.s16(json_get(data, "yaw"));
        return writer.data;
    }

    std::vector<uint8_t> pack_save_info(const nlohmann::json& data) {
        SaveWriter writer;
        auto append = [&writer](const std::vector<uint8_t>& bytes) {
            writer.data.insert(writer.data.end(), bytes.begin(), bytes.end());
        };
        append(pack_player_data(data.at("playerData")));
        append(pack_equips(data.at("equips")));
        writer.pad(2);
        append(pack_inventory(data.at("inventory")));
        append(pack_permanent_scene_flags(data.at("permanentSceneFlags")));
        writer.bytes(data.value("unk_DF4", nlohmann::json::array()), 0x54);
        for (const auto& value : data.value("dekuPlaygroundHighScores", nlohmann::json::array())) writer.u32(value.get<int64_t>());
        if (!data.contains("dekuPlaygroundHighScores")) writer.pad(12);
        writer.u32(json_get(data, "pictoFlags0"));
        writer.u32(json_get(data, "pictoFlags1"));
        writer.u32(json_get(data, "unk_E5C"));
        writer.u32(json_get(data, "unk_E60"));
        for (const auto& value : data.value("unk_E64", nlohmann::json::array())) writer.u32(value.get<int64_t>());
        if (!data.contains("unk_E64")) writer.pad(28);
        for (const auto& value : data.value("scenesVisible", nlohmann::json::array())) writer.u32(value.get<int64_t>());
        if (!data.contains("scenesVisible")) writer.pad(28);
        writer.u32(json_get(data, "skullTokenCount"));
        writer.u32(json_get(data, "unk_EA0"));
        writer.u32(json_get(data, "unk_EA4"));
        for (const auto& value : data.value("unk_EA8", nlohmann::json::array())) writer.u32(value.get<int64_t>());
        if (!data.contains("unk_EA8")) writer.pad(8);
        writer.u32(json_get(data, "stolenItems"));
        writer.u32(json_get(data, "unk_EB4"));
        for (const auto& value : data.value("highScores", nlohmann::json::array())) writer.u32(value.get<int64_t>());
        if (!data.contains("highScores")) writer.pad(28);
        writer.bytes(data.value("weekEventReg", nlohmann::json::array()), 100);
        writer.u32(json_get(data, "regionsVisited"));
        writer.u32(json_get(data, "worldMapCloudVisibility"));
        writer.u8(json_get(data, "unk_F40"));
        writer.u8(json_get(data, "scarecrowSpawnSongSet"));
        writer.bytes(data.value("scarecrowSpawnSong", nlohmann::json::array()), 128);
        writer.s8(json_get(data, "bombersCaughtNum"));
        for (const auto& value : data.value("bombersCaughtOrder", nlohmann::json::array())) writer.s8(value.get<int32_t>());
        if (!data.contains("bombersCaughtOrder")) writer.pad(5);
        if (auto it = data.find("lotteryCodes"); it != data.end()) {
            for (const auto& row : *it) {
                for (const auto& value : row) writer.s8(value.get<int32_t>());
            }
        }
        else {
            writer.pad(9);
        }
        for (const auto& value : data.value("spiderHouseMaskOrder", nlohmann::json::array())) writer.s8(value.get<int32_t>());
        if (!data.contains("spiderHouseMaskOrder")) writer.pad(6);
        for (const auto& value : data.value("bomberCode", nlohmann::json::array())) writer.s8(value.get<int32_t>());
        if (!data.contains("bomberCode")) writer.pad(5);
        append(pack_horse_data(data.at("horseData")));
        writer.u16(0);
        if (writer.data.size() != 0xFE8) {
            throw std::runtime_error("2Ship save info packed to an unexpected size");
        }
        return writer.data;
    }

    std::vector<uint8_t> pack_save(const nlohmann::json& save) {
        SaveWriter writer;
        auto append = [&writer](const std::vector<uint8_t>& bytes) {
            writer.data.insert(writer.data.end(), bytes.begin(), bytes.end());
        };
        writer.s32(json_get(save, "entrance"));
        writer.u8(json_get(save, "equippedMask"));
        writer.u8(json_get(save, "isFirstCycle"));
        writer.u8(json_get(save, "unk_06"));
        writer.u8(json_get(save, "linkAge"));
        writer.s32(json_get(save, "cutsceneIndex"));
        writer.u16(json_get(save, "time"));
        writer.u16(json_get(save, "owlSaveLocation"));
        writer.s32(json_get(save, "isNight"));
        writer.s32(json_get(save, "timeSpeedOffset"));
        writer.s32(json_get(save, "day"));
        writer.s32(json_get(save, "eventDayCount"));
        writer.u8(json_get(save, "playerForm"));
        writer.u8(json_get(save, "snowheadCleared"));
        writer.u8(json_get(save, "hasTatl"));
        writer.u8(json_get(save, "isOwlSave"));
        append(pack_save_info(save.at("saveInfo")));
        if (writer.data.size() != save_size) {
            throw std::runtime_error("2Ship save packed to an unexpected size");
        }
        write_checksum(writer.data);
        return writer.data;
    }

    std::vector<uint8_t> pack_owl_save_context(const nlohmann::json& owl) {
        SaveWriter writer;
        auto save = pack_save(owl.at("save"));
        save[0x23] = 1;
        save[checksum_offset] = 0;
        save[checksum_offset + 1] = 0;
        writer.data.insert(writer.data.end(), save.begin(), save.end());
        writer.bytes(owl.value("eventInf", nlohmann::json::array()), 8);
        writer.u8(json_get(owl, "unk_1014"));
        writer.u8(json_get(owl, "bButtonStatus"));
        writer.u16(json_get(owl, "jinxTimer"));
        writer.s16(json_get(owl, "rupeeAccumulator"));
        for (const auto& value : owl.value("bottleTimerStates", nlohmann::json::array())) writer.u8(value.get<int64_t>());
        if (!owl.contains("bottleTimerStates")) writer.pad(6);
        for (const char* field : { "bottleTimerStartOsTimes", "bottleTimerTimeLimits", "bottleTimerCurTimes", "bottleTimerPausedOsTimes" }) {
            for (const auto& value : owl.value(field, nlohmann::json::array())) writer.u64(value.get<uint64_t>());
            if (!owl.contains(field)) writer.pad(48);
        }
        writer.bytes(owl.value("pictoPhotoI5", nlohmann::json::array()), 11200);
        if (writer.data.size() > save_context_file_num_offset) {
            throw std::runtime_error("2Ship owl save is too large");
        }
        writer.pad(save_context_file_num_offset - writer.data.size());
        write_checksum(writer.data);
        return writer.data;
    }

    std::filesystem::path recomp_save_path() {
        return zelda64::get_app_folder_path() / "saves" / "mm.n64.us.1.0.bin";
    }

    std::string make_backup_suffix() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &now_time);
#else
        localtime_r(&now_time, &tm);
#endif
        std::ostringstream stream;
        stream << ".bak-" << std::put_time(&tm, "%Y%m%d-%H%M%S");
        return stream.str();
    }

    bool import_2ship_save(const std::filesystem::path& input_path, uint32_t target_file, std::string& error) {
        try {
            if (target_file < 1 || target_file > 2) {
                error = "Choose File 1 or File 2 before importing.";
                return false;
            }

            std::ifstream input(input_path);
            if (!input.good()) {
                error = "Unable to open the selected JSON file.";
                return false;
            }

            nlohmann::json source = nlohmann::json::parse(input);
            if (source.value("type", std::string{}) != "2S2H_SAVE") {
                error = "The selected file is not a 2Ship save JSON.";
                return false;
            }

            std::vector<uint8_t> flash(flash_size, 0);
            const std::filesystem::path output_path = recomp_save_path();
            std::filesystem::create_directories(output_path.parent_path());
            if (std::filesystem::exists(output_path)) {
                std::ifstream existing(output_path, std::ios::binary);
                flash.assign(std::istreambuf_iterator<char>(existing), std::istreambuf_iterator<char>());
                if (flash.size() != flash_size) {
                    error = "Existing Recompiled save has an unexpected size.";
                    return false;
                }
                std::filesystem::copy_file(output_path, output_path.string() + make_backup_suffix(), std::filesystem::copy_options::overwrite_existing);
            }

            const auto normal_save = pack_save(source.at("newCycleSave").at("save"));
            for (size_t offset : normal_slot_offsets[target_file - 1]) {
                std::copy(normal_save.begin(), normal_save.end(), flash.begin() + offset);
            }

            if (auto owl_it = source.find("owlSave"); owl_it != source.end() && !owl_it->is_null()) {
                const auto owl_save = pack_owl_save_context(*owl_it);
                for (size_t offset : owl_slot_offsets[target_file - 1]) {
                    std::copy(owl_save.begin(), owl_save.end(), flash.begin() + offset);
                }
            }

            std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
            output.write(reinterpret_cast<const char*>(flash.data()), static_cast<std::streamsize>(flash.size()));
            if (!output.good()) {
                error = "Unable to write the Recompiled save file.";
                return false;
            }
            return true;
        }
        catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }

    int32_t value(zelda64::save_editor::ValueId id) {
        return zelda64::save_editor::get_pending_value(id);
    }

    void set_value(zelda64::save_editor::ValueId id, int32_t new_value) {
        zelda64::save_editor::set_pending_value(id, new_value);
    }

    int32_t raw_time_to_hour(int32_t raw_time) {
        return ((raw_time & 0xFFFF) * 24) / 0x10000;
    }

    int32_t hour_to_raw_time(int32_t hour) {
        return (hour % 24) * 0x10000 / 24;
    }

    std::string bomber_code_text() {
        constexpr std::array<zelda64::save_editor::ValueId, 5> bomber_code_ids{
            zelda64::save_editor::BomberCode1,
            zelda64::save_editor::BomberCode2,
            zelda64::save_editor::BomberCode3,
            zelda64::save_editor::BomberCode4,
            zelda64::save_editor::BomberCode5,
        };
        std::string code;
        code.reserve(bomber_code_ids.size());
        for (zelda64::save_editor::ValueId id : bomber_code_ids) {
            code.push_back(static_cast<char>('0' + std::clamp(zelda64::save_editor::get_pending_value(id), 1, 5)));
        }
        return code;
    }

    void set_bomber_code_text(const std::string& code) {
        constexpr std::array<zelda64::save_editor::ValueId, 5> bomber_code_ids{
            zelda64::save_editor::BomberCode1,
            zelda64::save_editor::BomberCode2,
            zelda64::save_editor::BomberCode3,
            zelda64::save_editor::BomberCode4,
            zelda64::save_editor::BomberCode5,
        };
        if (code.size() != bomber_code_ids.size()) {
            return;
        }
        for (char digit : code) {
            if (digit < '1' || digit > '5') {
                return;
            }
        }
        for (size_t i = 0; i < bomber_code_ids.size(); i++) {
            zelda64::save_editor::set_pending_value(bomber_code_ids[i], code[i] - '0');
        }
    }

    uint32_t bottle_option_index(int32_t item) {
        for (uint32_t i = 0; i < bottle_items.size(); i++) {
            if (bottle_items[i] == item) {
                return i;
            }
        }
        return 0;
    }

    constexpr std::string_view live_category_name(SaveEditorConfigPanel::LiveCategory category) {
        using LiveCategory = SaveEditorConfigPanel::LiveCategory;
        switch (category) {
        case LiveCategory::Time:
            return "Time";
        case LiveCategory::State:
            return "State";
        case LiveCategory::Health:
            return "Health";
        case LiveCategory::Equipment:
            return "Equipment";
        case LiveCategory::Inventory:
            return "Inventory";
        case LiveCategory::Masks:
            return "Masks";
        case LiveCategory::Quest:
            return "Quest";
        case LiveCategory::Songs:
            return "Songs";
        case LiveCategory::Dungeons:
            return "Dungeons";
        case LiveCategory::Count:
            break;
        }

        return "";
    }

    bool consume_2ship_import_ui_dirty() {
        std::lock_guard lock(setup_2ship_import_mutex);
        if (!setup_2ship_import_ui_dirty) {
            return false;
        }
        setup_2ship_import_ui_dirty = false;
        return true;
    }

    bool consume_2ship_import_result(bool& success, std::string& error) {
        std::lock_guard lock(setup_2ship_import_mutex);
        if (!setup_2ship_import_result_ready) {
            return false;
        }
        success = setup_2ship_import_result_success;
        error = setup_2ship_import_result_error;
        setup_2ship_import_result_ready = false;
        setup_2ship_import_ui_dirty = false;
        return true;
    }

    void open_2ship_import_dialog() {
        {
            std::lock_guard lock(setup_2ship_import_mutex);
            if (setup_2ship_import_in_progress) {
                return;
            }
            setup_has_2ship_import = true;
            setup_2ship_import_failed = false;
            setup_2ship_import_path.clear();
            setup_2ship_import_name = "Choose JSON";
            setup_2ship_import_ui_dirty = true;
        }

        zelda64::open_json_file_dialog([](bool success, const std::filesystem::path& path) {
            if (!success || path.empty()) {
                std::lock_guard lock(setup_2ship_import_mutex);
                setup_has_2ship_import = false;
                setup_2ship_import_failed = false;
                setup_2ship_import_path.clear();
                setup_2ship_import_name.clear();
                setup_2ship_import_ui_dirty = true;
                return;
            }

            const uint32_t target_file = setup_target_file;
            if (target_file == 0) {
                std::lock_guard lock(setup_2ship_import_mutex);
                setup_2ship_import_result_ready = true;
                setup_2ship_import_result_success = false;
                setup_2ship_import_result_error = "Choose File 1 or File 2 before importing.";
                setup_2ship_import_ui_dirty = true;
                return;
            }

            {
                std::lock_guard lock(setup_2ship_import_mutex);
                setup_2ship_import_in_progress = true;
                setup_has_2ship_import = true;
                setup_2ship_import_failed = false;
                setup_2ship_import_path = path;
                setup_2ship_import_name = path.filename().string();
                setup_2ship_import_ui_dirty = true;
            }

            std::thread([path, target_file]() {
                std::string error;
                const bool imported = import_2ship_save(path, target_file, error);
                std::lock_guard lock(setup_2ship_import_mutex);
                setup_2ship_import_in_progress = false;
                setup_2ship_import_result_ready = true;
                setup_2ship_import_result_success = imported;
                setup_2ship_import_result_error = error;
                setup_2ship_import_failed = !imported;
                setup_2ship_import_ui_dirty = true;
            }).detach();
        });
    }

    void clear_2ship_import() {
        std::lock_guard lock(setup_2ship_import_mutex);
        setup_has_2ship_import = false;
        setup_2ship_import_failed = false;
        setup_2ship_import_path.clear();
        setup_2ship_import_name.clear();
        setup_2ship_import_ui_dirty = true;
    }
}

SaveEditorConfigPanel::SaveEditorConfigPanel(Element *parent, Rml::Element *host_element) : Element(parent, Events(EventType::Update)), host_element(host_element) {
    active_save_editor_panel = this;

    set_display(Display::Flex);
    set_flex_direction(FlexDirection::Column);
    set_flex(1, 1, 100.0f, Unit::Percent);
    set_height(100.0f, Unit::Percent);

    live_category_container = get_current_context().create_element<Container>(this, FlexDirection::Row, JustifyContent::FlexStart);
    live_category_container->set_flex_grow(0.0f);
    live_category_container->set_gap(14.0f);
    live_category_container->set_padding_left(32.0f);
    live_category_container->set_padding_right(32.0f);
    live_category_container->set_padding_top(8.0f);
    live_category_container->set_padding_bottom(14.0f);
    live_category_container->set_display(Display::None);
    build_live_category_tabs();

    editor_body_container = get_current_context().create_element<Container>(this, FlexDirection::Row, JustifyContent::FlexStart);
    editor_body_container->set_flex(1.0f, 1.0f, 100.0f, Unit::Percent);
    editor_body_container->set_height(100.0f, Unit::Percent);
    editor_body_container->set_gap(24.0f);

    config_sub_menu = get_current_context().create_element<ConfigSubMenu>(editor_body_container);
    config_sub_menu->set_flex(1.0f, 1.0f, 0.0f, Unit::Percent);
    config_sub_menu->set_header_visible(false);
    config_sub_menu->set_back_button_visible(false);
    config_sub_menu->set_description_visible(false);
    config_sub_menu->set_large_touch_style(true);
    config_sub_menu->enter("Save Editor");

    bottle_contents_sub_menu = get_current_context().create_element<ConfigSubMenu>(editor_body_container);
    bottle_contents_sub_menu->set_flex(0.0f, 0.0f, 620.0f, Unit::Dp);
    bottle_contents_sub_menu->set_max_width(44.0f, Unit::Percent);
    bottle_contents_sub_menu->set_header_visible(false);
    bottle_contents_sub_menu->set_back_button_visible(false);
    bottle_contents_sub_menu->set_description_visible(false);
    bottle_contents_sub_menu->set_large_touch_style(true);
    bottle_contents_sub_menu->enter("Bottle Contents");
    bottle_contents_sub_menu->set_display(Display::None);

    rebuild_if_ready();
}

SaveEditorConfigPanel::~SaveEditorConfigPanel() {
    if (active_save_editor_panel == this) {
        active_save_editor_panel = nullptr;
    }
}

void SaveEditorConfigPanel::refresh() {
    mode = Mode::None;
    rebuild_if_ready();
}

void SaveEditorConfigPanel::process_event(const Event &e) {
    if (e.type == EventType::Update) {
        if (live_editor_refresh_pending && mode == Mode::LiveEditor) {
            live_editor_refresh_pending = false;
            populate_live_editor_category();
            return;
        }

        bool import_success = false;
        std::string import_error;
        if (consume_2ship_import_result(import_success, import_error)) {
            if (!import_success) {
                zelda64::show_error_message_box("2Ship Import", import_error.c_str());
            }
            if (mode == Mode::PregameSetup) {
                populate_setup_options();
                return;
            }
        }
        if (consume_2ship_import_ui_dirty() && mode == Mode::PregameSetup) {
            populate_setup_options();
            return;
        }
        rebuild_if_ready();
    }
}

void SaveEditorConfigPanel::rebuild_if_ready() {
    Rml::ElementDocument* doc = host_element != nullptr ? host_element->GetOwnerDocument() : nullptr;
    Rml::Element* tabset_el = doc != nullptr ? doc->GetElementById("config_tabset") : nullptr;
    Rml::ElementTabSet* tabset = rmlui_dynamic_cast<Rml::ElementTabSet*>(tabset_el);
    bool editor_tab_active = tabset != nullptr && tabset->GetActiveTab() == recompui::config_tab_to_index(recompui::ConfigTab::Editor);
    if (!editor_tab_active) {
        set_mode(Mode::None);
        return;
    }

    Mode desired_mode = zelda64::save_editor::has_snapshot() ? Mode::LiveEditor : Mode::PregameSetup;
    if (desired_mode == mode) {
        return;
    }

    set_mode(desired_mode);
}

void SaveEditorConfigPanel::set_mode(Mode new_mode) {
    if (new_mode == mode) {
        return;
    }

    mode = new_mode;
    live_category_container->set_display(Display::None);
    editor_body_container->set_display(Display::None);
    config_sub_menu->set_display(Display::None);
    bottle_contents_sub_menu->set_display(Display::None);

    if (mode == Mode::None) {
        config_sub_menu->clear_options();
        bottle_contents_sub_menu->clear_options();
    }
    else if (mode == Mode::PregameSetup) {
        editor_body_container->set_display(Display::Flex);
        config_sub_menu->set_display(Display::Flex);
        populate_setup_options();
    }
    else if (mode == Mode::LiveEditor) {
        zelda64::save_editor::reset_pending_to_snapshot();
        live_category_container->set_display(Display::Flex);
        editor_body_container->set_display(Display::Flex);
        config_sub_menu->set_display(Display::Flex);
        populate_live_editor();
    }
}

void SaveEditorConfigPanel::build_live_category_tabs() {
    for (uint32_t index = 0; index < static_cast<uint32_t>(LiveCategory::Count); index++) {
        LiveCategory category = static_cast<LiveCategory>(index);
        Button *button = get_current_context().create_element<Button>(live_category_container, std::string(live_category_name(category)), ButtonStyle::Secondary);
        button->set_min_height(66.0f);
        button->set_padding_top(18.0f);
        button->set_padding_bottom(18.0f);
        button->set_padding_left(22.0f);
        button->set_padding_right(22.0f);
        button->set_font_size(28.0f);
        button->set_letter_spacing(2.52f);
        button->set_line_height(30.0f);
        button->set_border_radius(18.0f);
        button->add_pressed_callback([this, category]() { set_live_category(category); });
        live_category_buttons.emplace_back(button);
    }

    update_live_category_tabs();
}

void SaveEditorConfigPanel::set_live_category(LiveCategory category) {
    if (category == live_category) {
        return;
    }

    live_category = category;
    selected_bottle_slot = no_bottle_slot_selected;
    update_live_category_tabs();
    populate_live_editor_category();
}

void SaveEditorConfigPanel::update_live_category_tabs() {
    for (uint32_t index = 0; index < live_category_buttons.size(); index++) {
        bool selected = static_cast<LiveCategory>(index) == live_category;
        Button *button = live_category_buttons[index];
        if (selected) {
            button->set_color(Color{ 242, 242, 242, 255 });
            button->set_border_color(Color{ 185, 125, 242, 242 });
            button->set_background_color(Color{ 185, 125, 242, 72 });
        }
        else {
            button->set_color(Color{ 204, 204, 204, 255 });
            button->set_border_color(Color{ 255, 255, 255, 46 });
            button->set_background_color(Color{ 255, 255, 255, 14 });
        }
    }
}

void SaveEditorConfigPanel::populate_setup_options() {
    SAVE_EDITOR_UI_LOG("SaveEditor UI setup populate begin");
    config_sub_menu->clear_options();
    config_sub_menu->enter("Setup");
    bool has_2ship_import = false;
    bool import_in_progress = false;
    bool import_failed = false;
    std::string import_name;
    {
        std::lock_guard lock(setup_2ship_import_mutex);
        has_2ship_import = setup_has_2ship_import;
        import_in_progress = setup_2ship_import_in_progress;
        import_failed = setup_2ship_import_failed;
        import_name = setup_2ship_import_name;
    }

    config_sub_menu->add_toggle_option("configure_auto_apply", "Apply Setup on Load",
        "Apply the setup values below when the chosen save file opens. Changes made during gameplay are applied live.",
        setup_auto_apply != 0, [](const std::string&, bool value) { setup_auto_apply = value ? 1 : 0; });
    config_sub_menu->add_radio_option("configure_target_file", "Save File",
        "Choose which save file receives setup values. Next opened file applies to whichever save is loaded next.",
        setup_target_file, target_file_options, [](const std::string&, uint32_t value) { setup_target_file = value; });
    config_sub_menu->add_button_option("configure_2ship_import", "2Ship Save",
        "Import a 2Ship JSON save into the selected Recompiled save file.",
        import_in_progress ? "Importing..." : (has_2ship_import ? "Change Import" : "Import JSON"),
        [](const std::string&) { open_2ship_import_dialog(); })->set_option_enabled(!import_in_progress);
    if (has_2ship_import || import_in_progress) {
        std::string import_label = import_in_progress ? std::string("Importing: ") + import_name : std::string(import_failed ? "Import Failed: " : "Imported: ") + import_name;
        config_sub_menu->add_button_option("configure_2ship_clear", import_label,
            "The imported 2Ship save is selected. Manual setup values below are disabled while this import is active.",
            "Clear Import",
            [](const std::string&) { clear_2ship_import(); })->set_option_enabled(!import_in_progress);
    }

    const bool manual_setup_enabled = true;
    config_sub_menu->add_text_option("configure_name", "Name",
        "Player name to apply. Leave blank to keep the current name.",
        setup_name, [](const std::string&, const std::string& value) { setup_name = value; })->set_option_enabled(manual_setup_enabled);
    config_sub_menu->add_radio_option("configure_day", "Day",
        "Day to apply. Preserve keeps the current day.",
        setup_day, preserve_day_options, [](const std::string&, uint32_t value) { setup_day = value; })->set_option_enabled(manual_setup_enabled);
    config_sub_menu->add_radio_option("configure_has_tatl", "Tatl",
        "Set whether the save has Tatl.",
        setup_has_tatl, preserve_bool_options, [](const std::string&, uint32_t value) { setup_has_tatl = value; })->set_option_enabled(manual_setup_enabled);
    config_sub_menu->add_radio_option("configure_intro_complete", "Intro Complete",
        "Set the intro-complete flag.",
        setup_intro_complete, preserve_bool_options, [](const std::string&, uint32_t value) { setup_intro_complete = value; })->set_option_enabled(manual_setup_enabled);
    config_sub_menu->add_radio_option("configure_owl_save", "Owl Save",
        "Set the owl-save flag.",
        setup_owl_save, preserve_bool_options, [](const std::string&, uint32_t value) { setup_owl_save = value; })->set_option_enabled(manual_setup_enabled);

    SAVE_EDITOR_UI_LOG("SaveEditor UI setup populate end");
}

void refresh_save_editor_config() {
    if (active_save_editor_panel != nullptr) {
        recompui::ContextId ui_context = recompui::get_config_context_id();
        bool opened = ui_context.open_if_not_already();

        active_save_editor_panel->refresh();

        if (opened) {
            ui_context.close();
        }
    }
}

void SaveEditorConfigPanel::populate_live_editor() {
    update_live_category_tabs();
    populate_live_editor_category();
}

void SaveEditorConfigPanel::request_live_editor_refresh() {
    live_editor_refresh_pending = true;
    queue_update();
}

void SaveEditorConfigPanel::populate_bottle_contents_panel() {
    using namespace zelda64::save_editor;

    bottle_contents_sub_menu->clear_options();
    bottle_contents_sub_menu->enter("Bottle Contents");
    bottle_contents_sub_menu->set_display(Display::None);
    if (selected_bottle_slot >= bottle_value_ids.size()) {
        return;
    }

    const ValueId value_id = bottle_value_ids[selected_bottle_slot];
    const uint32_t current_index = bottle_option_index(value(value_id));
    config_sub_menu->add_section_header(bottle_slot_options[selected_bottle_slot]);
    config_sub_menu->add_toggle_option(
        "bottle_enabled",
        "Bottle",
        "Turns this bottle slot on or off.",
        value(value_id) != bottle_items[0],
        [this, value_id](const std::string &, bool new_value) {
            set_value(value_id, new_value ? bottle_items[1] : bottle_items[0]);
            request_live_editor_refresh();
        });

    if (value(value_id) == bottle_items[0]) {
        return;
    }

    for (uint32_t i = 1; i < bottle_options.size(); i++) {
        config_sub_menu->add_toggle_option(
            "bottle_content_" + std::to_string(i),
            bottle_options[i],
            "Sets the selected bottle to " + bottle_options[i] + ".",
            i == current_index,
            [this, value_id, i](const std::string &, bool new_value) {
                if (new_value && i < bottle_items.size()) {
                    set_value(value_id, bottle_items[i]);
                    request_live_editor_refresh();
                }
            });
    }
}

void SaveEditorConfigPanel::populate_live_editor_category() {
    using namespace zelda64::save_editor;

    SAVE_EDITOR_UI_LOG("SaveEditor UI populate begin");
    config_sub_menu->clear_options();
    config_sub_menu->enter("Save Editor");
    bottle_contents_sub_menu->clear_options();
    bottle_contents_sub_menu->enter("Bottle Contents");
    bottle_contents_sub_menu->set_display(Display::None);

    auto add_bool = [this](std::string_view id, std::string_view name, std::string_view description, ValueId value_id) {
        config_sub_menu->add_toggle_option(id, name, description, value(value_id) != 0,
            [value_id](const std::string &, bool new_value) { set_value(value_id, new_value ? 1 : 0); });
    };

    auto add_slider = [this](std::string_view id, std::string_view name, std::string_view description, ValueId value_id,
                             double min, double max, double step) {
        config_sub_menu->add_slider_option(id, name, description, value(value_id), min, max, step, false,
            [value_id](const std::string &, double new_value) { set_value(value_id, static_cast<int32_t>(new_value)); });
    };

    auto add_radio = [this](std::string_view id, std::string_view name, std::string_view description, ValueId value_id,
                            const std::vector<std::string> &options) {
        config_sub_menu->add_radio_option(id, name, description, value(value_id), options,
            [value_id](const std::string &, uint32_t new_value) { set_value(value_id, static_cast<int32_t>(new_value)); });
    };

    auto add_bottles = [this]() {
        config_sub_menu->add_radio_option(
            "bottles", "Bottles", "Choose a bottle slot to edit below.",
            selected_bottle_slot,
            bottle_slot_short_options,
            [this](const std::string &, uint32_t index) {
                if (index < bottle_value_ids.size()) {
                    selected_bottle_slot = index;
                    request_live_editor_refresh();
                }
            });

        populate_bottle_contents_panel();
    };

    auto add_tingle_maps = [this]() {
        const std::array<ValueId, 6> tingle_map_ids{
            TingleMapClockTown,
            TingleMapWoodfall,
            TingleMapSnowhead,
            TingleMapRomaniRanch,
            TingleMapGreatBay,
            TingleMapStoneTower,
        };
        std::vector<bool> values;
        values.reserve(tingle_map_ids.size());
        for (ValueId id : tingle_map_ids) {
            values.push_back(value(id) != 0);
        }

        config_sub_menu->add_multi_toggle_option(
            "tingle_maps", "Tingle Maps", "Controls purchased Tingle regional maps and world-map visibility.",
            tingle_map_options, values,
            [tingle_map_ids](const std::string &, uint32_t index, bool new_value) {
                if (index < tingle_map_ids.size()) {
                    set_value(tingle_map_ids[index], new_value ? 1 : 0);
                }
            });
    };

    switch (live_category) {
    case LiveCategory::Time:
        {
            config_sub_menu->add_slider_option("day", "Day", "Sets the current three-day cycle day.", value(Day), 1, 4, 1, false,
                [](const std::string &, double new_value) { set_value(Day, static_cast<int32_t>(new_value)); });
            config_sub_menu->add_slider_option("time_of_day", "Time of Day", "Sets the current hour of the day.", raw_time_to_hour(value(Time)), 0, 23, 1, false,
                [](const std::string &, double new_value) { set_value(Time, hour_to_raw_time(static_cast<int32_t>(new_value))); });
            config_sub_menu->add_slider_option("time_speed", "Time Speed", "Adjusts the current flow of time.", value(TimeSpeed), -2, 18, 1, false,
                [](const std::string &, double new_value) { set_value(TimeSpeed, static_cast<int32_t>(new_value)); });
        }
        break;
    case LiveCategory::State:
        add_bool("tatl", "Tatl", "Controls whether Tatl is with Link.", Tatl);
        add_bool("intro", "Intro Complete", "Controls whether the opening cycle has been completed.", IntroComplete);
        add_bool("owl_save", "Owl Save", "Controls whether the active save is marked as an owl save.", OwlSave);
        config_sub_menu->add_numeric_text_option("bomber_code", "Bombers Code",
            "Sets the five-digit Bombers Code. Use digits 1 through 5.",
            bomber_code_text(), 5,
            [](const std::string &, const std::string &new_value) {
                set_bomber_code_text(new_value);
            });
        break;
    case LiveCategory::Health:
        add_radio("wallet", "Wallet", "Sets the active wallet upgrade.", Wallet, wallet_options);
        add_slider("rupees", "Rupees", "Sets carried rupees.", Rupees, 0, 500, 1);
        add_slider("bank", "Bank Rupees", "Sets banked rupees.", BankRupees, 0, 5000, 1);
        add_slider("hearts", "Heart Containers", "Sets maximum heart containers.", Hearts, 3, 20, 1);
        add_slider("heart_pieces", "Heart Pieces", "Sets the current heart piece count.", HeartPieces, 0, 3, 1);
        add_slider("health", "Health", "Sets current health in raw quarter-heart units.", Health, 0, 320, 1);
        add_bool("double_defense", "Double Defense", "Controls double defense hearts.", DoubleDefense);
        add_radio("magic_level", "Magic Level", "Sets the active magic meter size.", MagicLevel, magic_options);
        add_slider("magic", "Magic", "Sets current magic.", Magic, 0, 96, 1);
        break;
    case LiveCategory::Equipment:
        add_radio("sword", "Sword", "Sets the equipped sword upgrade.", Sword, sword_options);
        add_radio("shield", "Shield", "Sets the equipped shield upgrade.", Shield, shield_options);
        add_radio("quiver", "Quiver", "Sets the quiver upgrade.", Quiver, quiver_options);
        add_radio("bomb_bag", "Bomb Bag", "Sets the bomb bag upgrade.", BombBag, bomb_bag_options);
        add_radio("stick_upgrade", "Stick Upgrade", "Sets the Deku Stick upgrade.", StickUpgrade, stick_upgrade_options);
        add_radio("nut_upgrade", "Nut Upgrade", "Sets the Deku Nut upgrade.", NutUpgrade, nut_upgrade_options);
        add_radio("spin_attack", "Spin Attack", "Sets the Great Spin Attack upgrade.", SpinAttackLevel, spin_attack_options);
        break;
    case LiveCategory::Inventory:
        add_slider("arrows", "Arrows", "Sets bow ammo and grants the bow if above zero.", Arrows, 0, 99, 1);
        add_slider("bombs", "Bombs", "Sets bomb ammo and grants bombs if above zero.", Bombs, 0, 99, 1);
        add_slider("bombchus", "Bombchus", "Sets bombchu ammo and grants bombchus if above zero.", Bombchus, 0, 99, 1);
        add_slider("sticks", "Deku Sticks", "Sets Deku Stick ammo and grants Deku Sticks if above zero.", DekuSticks, 0, 99, 1);
        add_slider("nuts", "Deku Nuts", "Sets Deku Nut ammo and grants Deku Nuts if above zero.", DekuNuts, 0, 99, 1);
        add_tingle_maps();
        add_bool("powder_keg", "Powder Keg", "Controls whether the Powder Keg is in inventory.", PowderKeg);
        add_bool("ocarina", "Ocarina", "Controls whether the Ocarina of Time is in inventory.", Ocarina);
        add_bool("fire_arrows", "Fire Arrows", "Controls whether Fire Arrows are in inventory.", FireArrows);
        add_bool("ice_arrows", "Ice Arrows", "Controls whether Ice Arrows are in inventory.", IceArrows);
        add_bool("light_arrows", "Light Arrows", "Controls whether Light Arrows are in inventory.", LightArrows);
        add_bool("magic_beans", "Magic Beans", "Controls whether Magic Beans are in inventory.", MagicBeans);
        add_slider("magic_beans_count", "Bean Count", "Sets the number of Magic Beans.", MagicBeansCount, 0, 20, 1);
        add_bool("pictograph_box", "Pictograph Box", "Controls whether the Pictograph Box is in inventory.", PictographBox);
        add_bool("lens", "Lens of Truth", "Controls whether the Lens of Truth is in inventory.", LensOfTruth);
        add_bool("hookshot", "Hookshot", "Controls whether the Hookshot is in inventory.", Hookshot);
        add_bool("great_fairy_sword", "Great Fairy Sword", "Controls whether the Great Fairy Sword is in inventory.", GreatFairySword);
        add_bool("room_key", "Room Key", "Controls whether the Room Key is in inventory.", RoomKey);
        add_bool("letter_mama", "Letter to Mama", "Controls whether Letter to Mama is in inventory.", LetterMama);
        add_bool("letter_kafei", "Letter to Kafei", "Controls whether Letter to Kafei is in inventory.", LetterKafei);
        add_bool("pendant", "Pendant of Memories", "Controls whether the Pendant of Memories is in inventory.", PendantOfMemories);
        add_bottles();
        break;
    case LiveCategory::Masks:
        add_bool("deku_mask", "Deku Mask", "Controls whether the Deku Mask is in inventory.", DekuMask);
        add_bool("goron_mask", "Goron Mask", "Controls whether the Goron Mask is in inventory.", GoronMask);
        add_bool("zora_mask", "Zora Mask", "Controls whether the Zora Mask is in inventory.", ZoraMask);
        add_bool("fierce_deity_mask", "Fierce Deity Mask", "Controls whether the Fierce Deity Mask is in inventory.", FierceDeityMask);
        add_bool("mask_truth", "Mask of Truth", "Controls whether the Mask of Truth is in inventory.", MaskOfTruth);
        add_bool("kafeis_mask", "Kafei's Mask", "Controls whether Kafei's Mask is in inventory.", KafeisMask);
        add_bool("all_night_mask", "All-Night Mask", "Controls whether the All-Night Mask is in inventory.", AllNightMask);
        add_bool("bunny_hood", "Bunny Hood", "Controls whether the Bunny Hood is in inventory.", BunnyHood);
        add_bool("keaton_mask", "Keaton Mask", "Controls whether the Keaton Mask is in inventory.", KeatonMask);
        add_bool("garo_mask", "Garo's Mask", "Controls whether Garo's Mask is in inventory.", GaroMask);
        add_bool("romani_mask", "Romani's Mask", "Controls whether Romani's Mask is in inventory.", RomaniMask);
        add_bool("circus_leader_mask", "Circus Leader's Mask", "Controls whether Circus Leader's Mask is in inventory.", CircusLeaderMask);
        add_bool("postman_hat", "Postman's Hat", "Controls whether the Postman's Hat is in inventory.", PostmanHat);
        add_bool("couples_mask", "Couple's Mask", "Controls whether the Couple's Mask is in inventory.", CouplesMask);
        add_bool("great_fairy_mask", "Great Fairy's Mask", "Controls whether Great Fairy's Mask is in inventory.", GreatFairyMask);
        add_bool("gibdo_mask", "Gibdo Mask", "Controls whether the Gibdo Mask is in inventory.", GibdoMask);
        add_bool("don_gero_mask", "Don Gero's Mask", "Controls whether the Don Gero's Mask is in inventory.", DonGeroMask);
        add_bool("kamaro_mask", "Kamaro's Mask", "Controls whether Kamaro's Mask is in inventory.", KamaroMask);
        add_bool("captains_hat", "Captain's Hat", "Controls whether Captain's Hat is in inventory.", CaptainsHat);
        add_bool("stone_mask", "Stone Mask", "Controls whether the Stone Mask is in inventory.", StoneMask);
        add_bool("bremen_mask", "Bremen Mask", "Controls whether Bremen Mask is in inventory.", BremenMask);
        add_bool("blast_mask", "Blast Mask", "Controls whether the Blast Mask is in inventory.", BlastMask);
        add_bool("mask_scents", "Mask of Scents", "Controls whether Mask of Scents is in inventory.", MaskOfScents);
        add_bool("giants_mask", "Giant's Mask", "Controls whether the Giant's Mask is in inventory.", GiantsMask);
        break;
    case LiveCategory::Quest:
        add_bool("notebook", "Bombers Notebook", "Controls whether the Bombers Notebook is owned.", BombersNotebook);
        add_bool("odolwa", "Odolwa Remains", "Controls whether Odolwa's Remains are owned.", OdolwaRemains);
        add_bool("goht", "Goht Remains", "Controls whether Goht's Remains are owned.", GohtRemains);
        add_bool("gyorg", "Gyorg Remains", "Controls whether Gyorg's Remains are owned.", GyorgRemains);
        add_bool("twinmold", "Twinmold Remains", "Controls whether Twinmold's Remains are owned.", TwinmoldRemains);
        add_slider("swamp_skulltula_tokens", "Swamp Skulltula Tokens", "Sets Swamp Spider House gold skulltula tokens.", SwampSkulltulaTokens, 0, 30, 1);
        add_slider("ocean_skulltula_tokens", "Ocean Skulltula Tokens", "Sets Oceanside Spider House gold skulltula tokens.", OceanSkulltulaTokens, 0, 30, 1);
        break;
    case LiveCategory::Songs:
        add_bool("sonata", "Sonata of Awakening", "Controls whether Sonata of Awakening is learned.", Sonata);
        add_bool("lullaby", "Goron Lullaby", "Controls whether Goron Lullaby is learned.", Lullaby);
        add_bool("bossa_nova", "New Wave Bossa Nova", "Controls whether New Wave Bossa Nova is learned.", BossaNova);
        add_bool("elegy", "Elegy of Emptiness", "Controls whether Elegy of Emptiness is learned.", Elegy);
        add_bool("oath", "Oath to Order", "Controls whether Oath to Order is learned.", Oath);
        add_bool("saria", "Saria's Song", "Controls whether Saria's Song is learned.", SariasSong);
        add_bool("time_song", "Song of Time", "Controls whether Song of Time is learned.", SongOfTime);
        add_bool("healing", "Song of Healing", "Controls whether Song of Healing is learned.", SongOfHealing);
        add_bool("epona", "Epona's Song", "Controls whether Epona's Song is learned.", EponasSong);
        add_bool("soaring", "Song of Soaring", "Controls whether Song of Soaring is learned.", SongOfSoaring);
        add_bool("storms", "Song of Storms", "Controls whether Song of Storms is learned.", SongOfStorms);
        add_bool("sun", "Sun's Song", "Controls whether Sun's Song is learned.", SunsSong);
        break;
    case LiveCategory::Dungeons:
        config_sub_menu->add_radio_option("dungeon", "Dungeon", "Choose which dungeon to edit.",
            static_cast<uint32_t>(dungeon_category), dungeon_options,
            [this](const std::string &, uint32_t new_value) {
                if (new_value < static_cast<uint32_t>(DungeonCategory::StoneTower) + 1) {
                    dungeon_category = static_cast<DungeonCategory>(new_value);
                    request_live_editor_refresh();
                }
            });
        switch (dungeon_category) {
        case DungeonCategory::Woodfall:
            add_bool("woodfall_map", "Map", "Controls whether the Woodfall map is owned.", WoodfallMap);
            add_bool("woodfall_compass", "Compass", "Controls whether the Woodfall compass is owned.", WoodfallCompass);
            add_bool("woodfall_boss_key", "Boss Key", "Controls whether the Woodfall boss key is owned.", WoodfallBossKey);
            add_slider("woodfall_keys", "Small Keys", "Sets Woodfall small keys.", WoodfallSmallKeys, 0, 1, 1);
            add_slider("woodfall_fairies", "Stray Fairies", "Sets Woodfall stray fairies.", WoodfallStrayFairies, 0, 15, 1);
            break;
        case DungeonCategory::Snowhead:
            add_bool("snowhead_map", "Map", "Controls whether the Snowhead map is owned.", SnowheadMap);
            add_bool("snowhead_compass", "Compass", "Controls whether the Snowhead compass is owned.", SnowheadCompass);
            add_bool("snowhead_boss_key", "Boss Key", "Controls whether the Snowhead boss key is owned.", SnowheadBossKey);
            add_slider("snowhead_keys", "Small Keys", "Sets Snowhead small keys.", SnowheadSmallKeys, 0, 3, 1);
            add_slider("snowhead_fairies", "Stray Fairies", "Sets Snowhead stray fairies.", SnowheadStrayFairies, 0, 15, 1);
            break;
        case DungeonCategory::GreatBay:
            add_bool("great_bay_map", "Map", "Controls whether the Great Bay map is owned.", GreatBayMap);
            add_bool("great_bay_compass", "Compass", "Controls whether the Great Bay compass is owned.", GreatBayCompass);
            add_bool("great_bay_boss_key", "Boss Key", "Controls whether the Great Bay boss key is owned.", GreatBayBossKey);
            add_slider("great_bay_keys", "Small Keys", "Sets Great Bay small keys.", GreatBaySmallKeys, 0, 1, 1);
            add_slider("great_bay_fairies", "Stray Fairies", "Sets Great Bay stray fairies.", GreatBayStrayFairies, 0, 15, 1);
            break;
        case DungeonCategory::StoneTower:
            add_bool("stone_tower_map", "Map", "Controls whether the Stone Tower map is owned.", StoneTowerMap);
            add_bool("stone_tower_compass", "Compass", "Controls whether the Stone Tower compass is owned.", StoneTowerCompass);
            add_bool("stone_tower_boss_key", "Boss Key", "Controls whether the Stone Tower boss key is owned.", StoneTowerBossKey);
            add_slider("stone_tower_keys", "Small Keys", "Sets Stone Tower small keys.", StoneTowerSmallKeys, 0, 4, 1);
            add_slider("stone_tower_fairies", "Stray Fairies", "Sets Stone Tower stray fairies.", StoneTowerStrayFairies, 0, 15, 1);
            break;
        }
        break;
        break;
    case LiveCategory::Count:
        break;
    }

    SAVE_EDITOR_UI_LOG("SaveEditor UI category populate end");
    return;

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Time");
    config_sub_menu->add_section_header("Time");
    config_sub_menu->add_slider_option("day", "Day", "Sets the current three-day cycle day.", value(Day), 1, 4, 1, false,
        [](const std::string &, double new_value) { set_value(Day, static_cast<int32_t>(new_value)); });
    config_sub_menu->add_slider_option("time_of_day", "Time of Day", "Sets the current hour of the day.", raw_time_to_hour(value(Time)), 0, 23, 1, false,
        [](const std::string &, double new_value) { set_value(Time, hour_to_raw_time(static_cast<int32_t>(new_value))); });
    config_sub_menu->add_slider_option("time_speed", "Time Speed", "Adjusts the current flow of time.", value(TimeSpeed), -2, 18, 1, false,
        [](const std::string &, double new_value) { set_value(TimeSpeed, static_cast<int32_t>(new_value)); });

    SAVE_EDITOR_UI_LOG("SaveEditor UI section State");
    config_sub_menu->add_section_header("State");
    add_bool("tatl", "Tatl", "Controls whether Tatl is with Link.", Tatl);
    add_bool("intro", "Intro Complete", "Controls whether the opening cycle has been completed.", IntroComplete);
    add_bool("owl_save", "Owl Save", "Controls whether the active save is marked as an owl save.", OwlSave);

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Health and Currency");
    config_sub_menu->add_section_header("Health and Currency");
    add_radio("wallet", "Wallet", "Sets the active wallet upgrade.", Wallet, wallet_options);
    add_slider("rupees", "Rupees", "Sets carried rupees.", Rupees, 0, 500, 1);
    add_slider("bank", "Bank Rupees", "Sets banked rupees.", BankRupees, 0, 5000, 1);
    add_slider("hearts", "Heart Containers", "Sets maximum heart containers.", Hearts, 3, 20, 1);
    add_slider("heart_pieces", "Heart Pieces", "Sets the current heart piece count.", HeartPieces, 0, 3, 1);
    add_slider("health", "Health", "Sets current health in raw quarter-heart units.", Health, 0, 320, 1);
    add_bool("double_defense", "Double Defense", "Controls double defense hearts.", DoubleDefense);
    add_radio("magic_level", "Magic Level", "Sets the active magic meter size.", MagicLevel, magic_options);
    add_slider("magic", "Magic", "Sets current magic.", Magic, 0, 96, 1);

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Equipment");
    config_sub_menu->add_section_header("Equipment");
    add_radio("sword", "Sword", "Sets the equipped sword upgrade.", Sword, sword_options);
    add_radio("shield", "Shield", "Sets the equipped shield upgrade.", Shield, shield_options);
    add_radio("quiver", "Quiver", "Sets the quiver upgrade.", Quiver, quiver_options);
    add_radio("bomb_bag", "Bomb Bag", "Sets the bomb bag upgrade.", BombBag, bomb_bag_options);
    add_radio("stick_upgrade", "Stick Upgrade", "Sets the Deku Stick upgrade.", StickUpgrade, stick_upgrade_options);
    add_radio("nut_upgrade", "Nut Upgrade", "Sets the Deku Nut upgrade.", NutUpgrade, nut_upgrade_options);

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Inventory");
    config_sub_menu->add_section_header("Inventory");
    add_slider("arrows", "Arrows", "Sets bow ammo and grants the bow if above zero.", Arrows, 0, 99, 1);
    add_slider("bombs", "Bombs", "Sets bomb ammo and grants bombs if above zero.", Bombs, 0, 99, 1);
    add_slider("bombchus", "Bombchus", "Sets bombchu ammo and grants bombchus if above zero.", Bombchus, 0, 99, 1);
    add_slider("sticks", "Deku Sticks", "Sets Deku Stick ammo and grants Deku Sticks if above zero.", DekuSticks, 0, 99, 1);
    add_slider("nuts", "Deku Nuts", "Sets Deku Nut ammo and grants Deku Nuts if above zero.", DekuNuts, 0, 99, 1);
    add_bool("powder_keg", "Powder Keg", "Controls whether the Powder Keg is in inventory.", PowderKeg);
    add_bool("ocarina", "Ocarina", "Controls whether the Ocarina of Time is in inventory.", Ocarina);
    add_bool("fire_arrows", "Fire Arrows", "Controls whether Fire Arrows are in inventory.", FireArrows);
    add_bool("ice_arrows", "Ice Arrows", "Controls whether Ice Arrows are in inventory.", IceArrows);
    add_bool("light_arrows", "Light Arrows", "Controls whether Light Arrows are in inventory.", LightArrows);
    add_bool("magic_beans", "Magic Beans", "Controls whether Magic Beans are in inventory.", MagicBeans);
    add_slider("magic_beans_count", "Bean Count", "Sets the number of Magic Beans.", MagicBeansCount, 0, 20, 1);
    add_bottles();
    add_bool("pictograph_box", "Pictograph Box", "Controls whether the Pictograph Box is in inventory.", PictographBox);
    add_bool("lens", "Lens of Truth", "Controls whether the Lens of Truth is in inventory.", LensOfTruth);
    add_bool("hookshot", "Hookshot", "Controls whether the Hookshot is in inventory.", Hookshot);
    add_bool("great_fairy_sword", "Great Fairy Sword", "Controls whether the Great Fairy Sword is in inventory.", GreatFairySword);
    add_bool("room_key", "Room Key", "Controls whether the Room Key is in inventory.", RoomKey);
    add_bool("letter_mama", "Letter to Mama", "Controls whether Letter to Mama is in inventory.", LetterMama);
    add_bool("letter_kafei", "Letter to Kafei", "Controls whether Letter to Kafei is in inventory.", LetterKafei);
    add_bool("pendant", "Pendant of Memories", "Controls whether the Pendant of Memories is in inventory.", PendantOfMemories);
    add_tingle_maps();

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Masks");
    config_sub_menu->add_section_header("Masks");
    add_bool("deku_mask", "Deku Mask", "Controls whether the Deku Mask is in inventory.", DekuMask);
    add_bool("goron_mask", "Goron Mask", "Controls whether the Goron Mask is in inventory.", GoronMask);
    add_bool("zora_mask", "Zora Mask", "Controls whether the Zora Mask is in inventory.", ZoraMask);
    add_bool("fierce_deity_mask", "Fierce Deity Mask", "Controls whether the Fierce Deity Mask is in inventory.", FierceDeityMask);
    add_bool("mask_truth", "Mask of Truth", "Controls whether the Mask of Truth is in inventory.", MaskOfTruth);
    add_bool("kafeis_mask", "Kafei's Mask", "Controls whether Kafei's Mask is in inventory.", KafeisMask);
    add_bool("all_night_mask", "All-Night Mask", "Controls whether the All-Night Mask is in inventory.", AllNightMask);
    add_bool("bunny_hood", "Bunny Hood", "Controls whether the Bunny Hood is in inventory.", BunnyHood);
    add_bool("keaton_mask", "Keaton Mask", "Controls whether the Keaton Mask is in inventory.", KeatonMask);
    add_bool("garo_mask", "Garo's Mask", "Controls whether Garo's Mask is in inventory.", GaroMask);
    add_bool("romani_mask", "Romani's Mask", "Controls whether Romani's Mask is in inventory.", RomaniMask);
    add_bool("circus_leader_mask", "Circus Leader's Mask", "Controls whether Circus Leader's Mask is in inventory.", CircusLeaderMask);
    add_bool("postman_hat", "Postman's Hat", "Controls whether the Postman's Hat is in inventory.", PostmanHat);
    add_bool("couples_mask", "Couple's Mask", "Controls whether the Couple's Mask is in inventory.", CouplesMask);
    add_bool("great_fairy_mask", "Great Fairy's Mask", "Controls whether Great Fairy's Mask is in inventory.", GreatFairyMask);
    add_bool("gibdo_mask", "Gibdo Mask", "Controls whether the Gibdo Mask is in inventory.", GibdoMask);
    add_bool("don_gero_mask", "Don Gero's Mask", "Controls whether Don Gero's Mask is in inventory.", DonGeroMask);
    add_bool("kamaro_mask", "Kamaro's Mask", "Controls whether Kamaro's Mask is in inventory.", KamaroMask);
    add_bool("captains_hat", "Captain's Hat", "Controls whether Captain's Hat is in inventory.", CaptainsHat);
    add_bool("stone_mask", "Stone Mask", "Controls whether the Stone Mask is in inventory.", StoneMask);
    add_bool("bremen_mask", "Bremen Mask", "Controls whether the Bremen Mask is in inventory.", BremenMask);
    add_bool("blast_mask", "Blast Mask", "Controls whether the Blast Mask is in inventory.", BlastMask);
    add_bool("mask_scents", "Mask of Scents", "Controls whether the Mask of Scents is in inventory.", MaskOfScents);
    add_bool("giants_mask", "Giant's Mask", "Controls whether the Giant's Mask is in inventory.", GiantsMask);

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Quest");
    config_sub_menu->add_section_header("Quest");
    add_bool("notebook", "Bombers Notebook", "Controls whether the Bombers Notebook is owned.", BombersNotebook);
    add_bool("odolwa", "Odolwa Remains", "Controls whether Odolwa's Remains are owned.", OdolwaRemains);
    add_bool("goht", "Goht Remains", "Controls whether Goht's Remains are owned.", GohtRemains);
    add_bool("gyorg", "Gyorg Remains", "Controls whether Gyorg's Remains are owned.", GyorgRemains);
    add_bool("twinmold", "Twinmold Remains", "Controls whether Twinmold's Remains are owned.", TwinmoldRemains);

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Songs");
    config_sub_menu->add_section_header("Songs");
    add_bool("sonata", "Sonata of Awakening", "Controls whether Sonata of Awakening is learned.", Sonata);
    add_bool("lullaby", "Goron Lullaby", "Controls whether Goron Lullaby is learned.", Lullaby);
    add_bool("bossa_nova", "New Wave Bossa Nova", "Controls whether New Wave Bossa Nova is learned.", BossaNova);
    add_bool("elegy", "Elegy of Emptiness", "Controls whether Elegy of Emptiness is learned.", Elegy);
    add_bool("oath", "Oath to Order", "Controls whether Oath to Order is learned.", Oath);
    add_bool("saria", "Saria's Song", "Controls whether Saria's Song is learned.", SariasSong);
    add_bool("time_song", "Song of Time", "Controls whether Song of Time is learned.", SongOfTime);
    add_bool("healing", "Song of Healing", "Controls whether Song of Healing is learned.", SongOfHealing);
    add_bool("epona", "Epona's Song", "Controls whether Epona's Song is learned.", EponasSong);
    add_bool("soaring", "Song of Soaring", "Controls whether Song of Soaring is learned.", SongOfSoaring);
    add_bool("storms", "Song of Storms", "Controls whether Song of Storms is learned.", SongOfStorms);
    add_bool("sun", "Sun's Song", "Controls whether Sun's Song is learned.", SunsSong);

    SAVE_EDITOR_UI_LOG("SaveEditor UI section Dungeons");
    config_sub_menu->add_section_header("Dungeons");
    add_bool("woodfall_map", "Woodfall Map", "Controls whether the Woodfall map is owned.", WoodfallMap);
    add_bool("woodfall_compass", "Woodfall Compass", "Controls whether the Woodfall compass is owned.", WoodfallCompass);
    add_bool("woodfall_boss_key", "Woodfall Boss Key", "Controls whether the Woodfall boss key is owned.", WoodfallBossKey);
    add_slider("woodfall_keys", "Woodfall Small Keys", "Sets Woodfall small keys.", WoodfallSmallKeys, 0, 1, 1);
    add_slider("woodfall_fairies", "Woodfall Stray Fairies", "Sets Woodfall stray fairies.", WoodfallStrayFairies, 0, 15, 1);
    add_bool("snowhead_map", "Snowhead Map", "Controls whether the Snowhead map is owned.", SnowheadMap);
    add_bool("snowhead_compass", "Snowhead Compass", "Controls whether the Snowhead compass is owned.", SnowheadCompass);
    add_bool("snowhead_boss_key", "Snowhead Boss Key", "Controls whether the Snowhead boss key is owned.", SnowheadBossKey);
    add_slider("snowhead_keys", "Snowhead Small Keys", "Sets Snowhead small keys.", SnowheadSmallKeys, 0, 3, 1);
    add_slider("snowhead_fairies", "Snowhead Stray Fairies", "Sets Snowhead stray fairies.", SnowheadStrayFairies, 0, 15, 1);
    add_bool("great_bay_map", "Great Bay Map", "Controls whether the Great Bay map is owned.", GreatBayMap);
    add_bool("great_bay_compass", "Great Bay Compass", "Controls whether the Great Bay compass is owned.", GreatBayCompass);
    add_bool("great_bay_boss_key", "Great Bay Boss Key", "Controls whether the Great Bay boss key is owned.", GreatBayBossKey);
    add_slider("great_bay_keys", "Great Bay Small Keys", "Sets Great Bay small keys.", GreatBaySmallKeys, 0, 1, 1);
    add_slider("great_bay_fairies", "Great Bay Stray Fairies", "Sets Great Bay stray fairies.", GreatBayStrayFairies, 0, 15, 1);
    add_bool("stone_tower_map", "Stone Tower Map", "Controls whether the Stone Tower map is owned.", StoneTowerMap);
    add_bool("stone_tower_compass", "Stone Tower Compass", "Controls whether the Stone Tower compass is owned.", StoneTowerCompass);
    add_bool("stone_tower_boss_key", "Stone Tower Boss Key", "Controls whether the Stone Tower boss key is owned.", StoneTowerBossKey);
    add_slider("stone_tower_keys", "Stone Tower Small Keys", "Sets Stone Tower small keys.", StoneTowerSmallKeys, 0, 4, 1);
    add_slider("stone_tower_fairies", "Stone Tower Stray Fairies", "Sets Stone Tower stray fairies.", StoneTowerStrayFairies, 0, 15, 1);
    SAVE_EDITOR_UI_LOG("SaveEditor UI populate end");
}

ElementSaveEditorConfig::ElementSaveEditorConfig(const Rml::String &tag) : Rml::Element(tag) {
    SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Flex);
    SetProperty("width", "100%");
    SetProperty("height", "100%");

    recompui::Element this_compat(this);
    panel = recompui::get_current_context().create_element<SaveEditorConfigPanel>(&this_compat, this);
}

ElementSaveEditorConfig::~ElementSaveEditorConfig() {
}

}
