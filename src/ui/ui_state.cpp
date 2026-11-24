#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <SDL_video.h>
#else
#include <SDL2/SDL_video.h>
#endif
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <cstdlib>
#include <initializer_list>
#include <atomic>
#include <unordered_set>
#include <cmath>
#include <string_view>

#if defined(__ANDROID__)
#include <android/log.h>
#define ZELDA_UI_LOG(...) __android_log_print(ANDROID_LOG_WARN, "ZeldaUI", __VA_ARGS__)
#else
#define ZELDA_UI_LOG(...)
#endif

#include "rt64_render_hooks.h"

#include "concurrentqueue.h"

#include "RmlUi/Core.h"
#include "RmlUi/Debugger.h"
#include "RmlUi/Core/RenderInterfaceCompatibility.h"
#include "RmlUi/../../Source/Core/Elements/ElementLabel.h"
#include "RmlUi_Platform_SDL.h"

#include "recomp_ui.h"
#include "recomp_input.h"
#include "librecomp/mods.hpp"
#include "librecomp/game.hpp"
#include "zelda_config.h"
#include "zelda_clock_overlay.h"
#include "zelda_support.h"
#include "ui_rml_hacks.hpp"
#include "ui_elements.h"
#include "ui_mod_menu.h"
#include "ui_mod_installer.h"
#include "ui_renderer.h"
#include "../../lib/rt64/src/contrib/stb/stb_image.h"

bool can_focus(Rml::Element* element) {
    return element->GetOwnerDocument() != nullptr && element->GetProperty(Rml::PropertyId::TabIndex)->Get<Rml::Style::TabIndex>() != Rml::Style::TabIndex::None;
}
//! Copied from lib\RmlUi\Source\Core\Elements\ElementLabel.cpp
// Get the first descending element whose tag name matches one of tags.
static Rml::Element* TagMatchRecursive(const Rml::StringList& tags, Rml::Element* element)
{
	const int num_children = element->GetNumChildren();

	for (int i = 0; i < num_children; i++)
	{
		Rml::Element* child = element->GetChild(i);

		for (const Rml::String& tag : tags)
		{
			if (child->GetTagName() == tag)
				return child;
		}

		Rml::Element* matching_element = TagMatchRecursive(tags, child);
		if (matching_element)
			return matching_element;
	}

	return nullptr;
}

Rml::Element* get_target(Rml::ElementDocument* document, Rml::Element* element) {
    // Labels can have targets, so check if this element is a label.
    if (element->GetTagName() == "label") {
        Rml::ElementLabel* labelElement = (Rml::ElementLabel*)element;
        const Rml::String target_id = labelElement->GetAttribute<Rml::String>("for", "");

        if (target_id.empty())
        {
            const Rml::StringList matching_tags = {"button", "input", "textarea", "progress", "progressbar", "select"};

            return TagMatchRecursive(matching_tags, element);
        }
        else
        {
            Rml::Element* target = labelElement->GetElementById(target_id);
            if (target != element)
                return target;
        }

        return nullptr;
    }
    // Return the element directly if no target exists.
    return element;
}

namespace recompui {
    class UiEventListener : public Rml::EventListener {
        event_handler_t* handler_;
        Rml::String param_;
    public:
        UiEventListener(event_handler_t* handler, Rml::String&& param) : handler_(handler), param_(std::move(param)) {}
        void ProcessEvent(Rml::Event& event) override {
            handler_(param_, event);
        }
    };

    class UiEventListenerInstancer : public Rml::EventListenerInstancer {
        std::unordered_map<Rml::String, event_handler_t*> handler_map_;
        std::unordered_map<Rml::String, UiEventListener> listener_map_;
    public:
        Rml::EventListener* InstanceEventListener(const Rml::String& value, Rml::Element* element) override {
            // Check if a listener has already been made for the full event string and return it if so.
            auto find_listener_it = listener_map_.find(value);
            if (find_listener_it != listener_map_.end()) {
                return &find_listener_it->second;
            }

            // No existing listener, so check if a handler has been registered for this event type and create a listener for it if so.
            size_t delimiter_pos = value.find(':');
            Rml::String event_type = value.substr(0, delimiter_pos);
            auto find_handler_it = handler_map_.find(event_type);
            if (find_handler_it != handler_map_.end()) {
                // A handler was found, create a listener and return it.
                Rml::String event_param = value.substr(std::min(delimiter_pos, value.size()));
                return &listener_map_.emplace(value, UiEventListener{ find_handler_it->second, std::move(event_param) }).first->second;
            }

            return nullptr;
        }

        void register_event(const Rml::String& value, event_handler_t* handler) {
            handler_map_.emplace(value, handler);
        }
    };
}

void recompui::register_event(UiEventListenerInstancer& listener, const std::string& name, event_handler_t* handler) {
    listener.register_event(name, handler);
}

Rml::Element* find_autofocus_element(Rml::Element* start) {
    Rml::Element* cur_element = start;
    Rml::Element* first_found = nullptr;

    while (cur_element) {
        if (cur_element->HasAttribute("autofocus")) {
            break;
        }
        cur_element = RecompRml::FindNextTabElement(cur_element, true);
        // Track the first element that was found to know when we've wrapped around.
        if (!first_found) {
            first_found = cur_element;
        }
        // Stop searching if we found the first element again.
        else {
            if (cur_element == first_found) {
                // Return the first tab element as there was nothing marked with autofocus.
                return first_found;
            }
        }
    }

    return cur_element;
}

struct ContextDetails {
    recompui::ContextId context;
    Rml::ElementDocument* document;
};

inline std::vector<char> read_file_to_bytes(std::filesystem::path path);

class UIState {
    Rml::Element* prev_focused = nullptr;
    bool mouse_is_active_changed = false;
    std::unique_ptr<recompui::MenuController> launcher_menu_controller{};
    std::unique_ptr<recompui::MenuController> config_menu_controller{};
    std::vector<ContextDetails> shown_contexts{};
    recompui::ContextId clock_overlay_context = recompui::ContextId::null();
    recompui::ContextId pause_save_prompt_overlay_context = recompui::ContextId::null();
    std::string clock_overlay_markup;
    std::string pause_save_prompt_overlay_markup;
    std::atomic_bool clock_texture_reload_requested = false;
    bool clock_sun_hour_loaded = false;
    bool clock_moon_hour_loaded = false;
    bool clock_final_moon_loaded = false;
public:
    bool mouse_is_active_initialized = false;
    bool mouse_is_active = false;
    bool cont_is_active = false;
    bool await_stick_return_x = false;
    bool await_stick_return_y = false;
    int last_active_mouse_position[2] = {0, 0};
    std::unique_ptr<recompui::MenuController> config_controller;
    std::unique_ptr<recompui::MenuController> launcher_controller;
    std::unique_ptr<SystemInterface_SDL> system_interface;
    recompui::RmlRenderInterface_RT64 render_interface;
    Rml::Context* context;
    bool debugger_initialized = false;
    recompui::UiEventListenerInstancer event_listener_instancer;

    UIState(const UIState& rhs) = delete;
    UIState& operator=(const UIState& rhs) = delete;
    UIState(UIState&& rhs) = delete;
    UIState& operator=(UIState&& rhs) = delete;

    UIState(SDL_Window* window, RT64::RenderInterface* interface, RT64::RenderDevice* device) {
        ZELDA_UI_LOG("UIState ctor begin window=%p interface=%p device=%p", window, interface, device);
        launcher_menu_controller = recompui::create_launcher_menu();
        config_menu_controller = recompui::create_config_menu();

        system_interface = std::make_unique<SystemInterface_SDL>();
        system_interface->SetWindow(window);
        render_interface.init(interface, device);
        load_launcher_images();
        ZELDA_UI_LOG("render interface initialized");

        launcher_menu_controller->register_events(event_listener_instancer);
        config_menu_controller->register_events(event_listener_instancer);

        Rml::SetSystemInterface(system_interface.get());
        Rml::SetRenderInterface(render_interface.get_rml_interface());
        Rml::Factory::RegisterEventListenerInstancer(&event_listener_instancer);

        recompui::register_custom_elements();

        Rml::Initialise();
        ZELDA_UI_LOG("Rml initialized");
        
        // Apply the hack to replace RmlUi's default color parser with one that conforms to HTML5 alpha parsing for SASS compatibility
        recompui::apply_color_hack();

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        
        context = Rml::CreateContext("main", Rml::Vector2i(width, height));
        ZELDA_UI_LOG("Rml context created size=%dx%d context=%p", width, height, context);
        launcher_menu_controller->make_bindings(context);
        config_menu_controller->make_bindings(context);

#if defined(ZELDA_ENABLE_RML_DEBUGGER) && !defined(__ANDROID__)
        if (!recomp::android_should_use_sync_boot_dma()) {
            Rml::Debugger::Initialise(context);
            debugger_initialized = true;
        } else {
            ZELDA_UI_LOG("Rml debugger skipped for Samsung sync boot path");
        }
#else
        ZELDA_UI_LOG("Rml debugger skipped");
#endif
        {
            struct FontFace {
                const char* filename;
                bool fallback_face;
            };
            FontFace font_faces[] = {
                {"LatoLatin-Regular.ttf", false},
                {"ChiaroNormal.otf", false},
                {"ChiaroBold.otf", false},
                {"LatoLatin-Italic.ttf", false},
                {"LatoLatin-Bold.ttf", false},
                {"LatoLatin-BoldItalic.ttf", false},
                {"NotoEmoji-Regular.ttf", true},
                {"promptfont/promptfont.ttf", false},
            };

            for (const FontFace& face : font_faces) {
                auto font = zelda64::get_asset_path(face.filename);
                bool loaded = Rml::LoadFontFace(font.string(), face.fallback_face);
                ZELDA_UI_LOG("font %s loaded=%d", font.string().c_str(), loaded ? 1 : 0);
            }
        }
        ZELDA_UI_LOG("UIState ctor end");
    }

    void create_menus() {
        auto stylesheet = zelda64::get_asset_path("recomp.rcss");
        ZELDA_UI_LOG("create_menus stylesheet=%s exists=%d", stylesheet.string().c_str(), std::filesystem::exists(stylesheet) ? 1 : 0);
        recompui::init_styling(stylesheet);
        launcher_menu_controller->load_document();
        ZELDA_UI_LOG("launcher document loaded");
        config_menu_controller->load_document();
        ZELDA_UI_LOG("config document loaded");
        recompui::init_prompt_context();
        ZELDA_UI_LOG("prompt context initialized");
        create_clock_overlay_context();
        create_pause_save_prompt_overlay_context();
    }

    void unload() {
        render_interface.reset();
    }

    void update_primary_input(bool mouse_moved, bool non_mouse_interacted) {
        mouse_is_active_changed = false;
        if (non_mouse_interacted) {
            // controller newly interacted with
            if (mouse_is_active) {
                mouse_is_active = false;
                mouse_is_active_changed = true;
            }
        }
        else if (mouse_moved) {
            // mouse newly interacted with
            if (!mouse_is_active) {
                mouse_is_active = true;
                mouse_is_active_changed = true;
            }
        }

        if (mouse_moved || non_mouse_interacted) {
            mouse_is_active_initialized = true;
        }

        if (mouse_is_active_initialized) {
            recompui::set_cursor_visible(mouse_is_active);
        }

        Rml::ElementDocument* current_document = top_mouse_document();
        if (current_document == nullptr) {
            return;
        }

        // TODO is this needed?
        Rml::Element* window_el = current_document->GetElementById("window");
        if (window_el != nullptr) {
            if (mouse_is_active) {
                if (!window_el->HasAttribute("mouse-active")) {
                    window_el->SetAttribute("mouse-active", true);
                }
            }
            else if (window_el->HasAttribute("mouse-active")) {
                window_el->RemoveAttribute("mouse-active");
            }
        }
    }

    void update_focus(bool mouse_moved, bool non_mouse_interacted) {
        Rml::ElementDocument* current_document = top_mouse_document();

        if (current_document == nullptr) {
            return;
        }

        if (cont_is_active || non_mouse_interacted) {
            if (non_mouse_interacted) {
                auto focusedEl = current_document->GetFocusLeafNode();
                if (focusedEl == nullptr || RecompRml::CanFocusElement(focusedEl) != RecompRml::CanFocus::Yes) {
                    Rml::Element* element = find_autofocus_element(current_document);
                    if (element != nullptr) {
                        element->Focus();
                    }
                }
            }
            return;
        }

        // If there was mouse motion, get the current hovered element (or its target if it points to one) and focus that if applicable.
        if (mouse_is_active) {
            if (mouse_is_active_changed) {
                Rml::Element* focused = current_document->GetFocusLeafNode();
                if (focused) focused->Blur();
            } else if (mouse_moved) {
                Rml::Element* hovered = context->GetHoverElement();
                if (hovered) {
                    Rml::Element* hover_target = get_target(current_document, hovered);
                    if (hover_target && can_focus(hover_target)) {
                        prev_focused = hover_target;
                    }
                }
            }
        }

        if (!mouse_is_active) {
            if (!prev_focused || !can_focus(prev_focused)) {
                // Find the autofocus element in the tab chain
                Rml::Element* element = find_autofocus_element(current_document);
                if (element && can_focus(element)) {
                    prev_focused = element;
                }
            }

            if (mouse_is_active_changed && prev_focused && can_focus(prev_focused)) {
                prev_focused->Focus();
            }
        }
    }

    void show_context(recompui::ContextId context) {
        ZELDA_UI_LOG("show_context document=%p shown_before=%zu", context.get_document(), shown_contexts.size());
        if (std::find_if(shown_contexts.begin(), shown_contexts.end(), [context](auto& c){ return c.context == context; }) != shown_contexts.end()) {
            recompui::message_box("Attemped to show the same context twice");
            assert(false);
        }
        Rml::ElementDocument* document = context.get_document();
        shown_contexts.push_back(ContextDetails{
            .context = context,
            .document = document
        });

        // auto& on_show = context.on_show;
        // if (on_show) {
        //     context.open();
        //     on_show();
        //     context.close();
        // }

        document->PullToFront();
        document->Show();
        recompui::Element* default_element = context.get_autofocus_element();
        if (default_element) {
            default_element->focus();
        }
    }

    void hide_context(recompui::ContextId context) {
        auto remove_it = std::remove_if(shown_contexts.begin(), shown_contexts.end(), [context](auto& c) { return c.context == context; });
        if (remove_it == shown_contexts.end()) {
            recompui::message_box("Attemped to hide a context that isn't shown");
            assert(false);
        }
        shown_contexts.erase(remove_it, shown_contexts.end());

        context.get_document()->Hide();
    }
    
    void hide_all_contexts() {
        for (auto& context : shown_contexts) {
            context.document->Hide();
        }

        shown_contexts.clear();
    }

    bool is_context_shown(recompui::ContextId context) {
        return std::find_if(shown_contexts.begin(), shown_contexts.end(), [context](auto& c){ return c.context == context; }) != shown_contexts.end();
    }

    bool is_context_capturing_input() {
        return std::find_if(shown_contexts.begin(), shown_contexts.end(), [](auto& c){ return c.context.captures_input(); }) != shown_contexts.end();
    }

    bool is_context_capturing_mouse() {
        return std::find_if(shown_contexts.begin(), shown_contexts.end(), [](auto& c){ return c.context.captures_mouse(); }) != shown_contexts.end();
    }

    bool is_any_context_shown() {
        return !shown_contexts.empty();
    }

    static const std::vector<std::pair<const char*, const char*>>& clock_image_resources() {
        static const std::vector<std::pair<const char*, const char*>> clock_images = {
            {"clock3ds_edge", "clock_3ds/gThreeDayClock3DSEdgeTex.ia8.png"},
            {"clock3ds_edge_right", "clock_3ds/gThreeDayClock3DSEdgeRightTex.ia8.png"},
            {"clock3ds_middle", "clock_3ds/gThreeDayClock3DSMiddleTex.ia8.png"},
            {"clock3ds_fill", "clock_3ds/gThreeDayClock3DSFillTex.ia8.png"},
            {"clock3ds_backdrop", "clock_3ds/gThreeDayClock3DSTimeBackdropTex.ia8.png"},
            {"clock3ds_arrow", "clock_3ds/gThreeDayClock3DSArrowTex.ia8.png"},
            {"clock3ds_slow", "clock_3ds/gThreeDayClock3DSSlowTimeTex.ia8.png"},
            {"clock3ds_final_moon", "clock_3ds/gThreeDayClock3DSFinalHoursMoonTex.ia8.png"},
            {"clock3ds_sun_hour", "clock_3ds/gThreeDayClockSunHourTex.ia8.png"},
            {"clock3ds_moon_hour", "clock_3ds/gThreeDayClockMoonHourTex.ia8.png"},
            {"clock3ds_colon", "clock_3ds/gThreeDayClock3DSFinalHoursColonTex.ia8.png"},
            {"clock3ds_digit0", "clock_3ds/gThreeDayClock3DSFinalHoursDigit0Tex.ia8.png"},
            {"clock3ds_digit1", "clock_3ds/gThreeDayClock3DSFinalHoursDigit1Tex.ia8.png"},
            {"clock3ds_digit2", "clock_3ds/gThreeDayClock3DSFinalHoursDigit2Tex.ia8.png"},
            {"clock3ds_digit3", "clock_3ds/gThreeDayClock3DSFinalHoursDigit3Tex.ia8.png"},
            {"clock3ds_digit4", "clock_3ds/gThreeDayClock3DSFinalHoursDigit4Tex.ia8.png"},
            {"clock3ds_digit5", "clock_3ds/gThreeDayClock3DSFinalHoursDigit5Tex.ia8.png"},
            {"clock3ds_digit6", "clock_3ds/gThreeDayClock3DSFinalHoursDigit6Tex.ia8.png"},
            {"clock3ds_digit7", "clock_3ds/gThreeDayClock3DSFinalHoursDigit7Tex.ia8.png"},
            {"clock3ds_digit8", "clock_3ds/gThreeDayClock3DSFinalHoursDigit8Tex.ia8.png"},
            {"clock3ds_digit9", "clock_3ds/gThreeDayClock3DSFinalHoursDigit9Tex.ia8.png"},
        };
        return clock_images;
    }

    void load_launcher_images() {
        auto asset_path = zelda64::get_asset_path("launcher-mask.png");
        if (std::filesystem::exists(asset_path)) {
            const std::vector<char> image_bytes = read_file_to_bytes(asset_path);
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* decoded = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(image_bytes.data()),
                static_cast<int>(image_bytes.size()),
                &width,
                &height,
                &channels,
                4);

            if (decoded != nullptr && width > 0 && height > 0) {
                std::vector<char> rgba(
                    reinterpret_cast<const char*>(decoded),
                    reinterpret_cast<const char*>(decoded) + (static_cast<size_t>(width) * static_cast<size_t>(height) * 4));
                ZELDA_UI_LOG("Launcher background decoded %dx%d", width, height);
                render_interface.queue_image_from_bytes_rgba32(
                    "launcher_background",
                    rgba,
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height));
                stbi_image_free(decoded);
            }
            else {
                ZELDA_UI_LOG("Launcher background decode failed: %s", stbi_failure_reason());
                render_interface.queue_image_from_bytes_file("launcher_background", image_bytes);
            }
        }
    }

    void load_clock_overlay_images() {
        const auto& clock_images = clock_image_resources();
        std::unordered_set<std::string> loaded_clock_images;
        clock_sun_hour_loaded = false;
        clock_moon_hour_loaded = false;
        clock_final_moon_loaded = false;

        for (const auto& [name, _] : clock_images) {
            recompui::release_image(name);
        }

        const auto read_le32 = [](const std::vector<char>& bytes, size_t offset) -> uint32_t {
            if (offset + 4 > bytes.size()) {
                return 0;
            }
            return static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset])) |
                (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 1])) << 8) |
                (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 2])) << 16) |
                (static_cast<uint32_t>(static_cast<unsigned char>(bytes[offset + 3])) << 24);
        };

        const auto tint_hour_icon = [](std::vector<char>& rgba, const char* src) {
            const bool sun_icon = std::string_view{src} == "clock3ds_sun_hour";
            const bool moon_icon = std::string_view{src} == "clock3ds_moon_hour";
            if (!sun_icon && !moon_icon) {
                return;
            }

            const unsigned char tint_r = 200;
            const unsigned char tint_g = sun_icon ? 0 : 200;
            const unsigned char tint_b = 0;
            for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
                const unsigned char red = static_cast<unsigned char>(rgba[i + 0]);
                const unsigned char green = static_cast<unsigned char>(rgba[i + 1]);
                const unsigned char blue = static_cast<unsigned char>(rgba[i + 2]);
                const unsigned char intensity = static_cast<unsigned char>((static_cast<unsigned int>(red) + green + blue) / 3);
                rgba[i + 0] = static_cast<char>((static_cast<unsigned int>(tint_r) * intensity) / 255);
                rgba[i + 1] = static_cast<char>((static_cast<unsigned int>(tint_g) * intensity) / 255);
                rgba[i + 2] = static_cast<char>((static_cast<unsigned int>(tint_b) * intensity) / 255);
            }
        };

        const auto queue_o2r_texture = [&](const recomp::mods::ZipModFileHandle& handle, const char* src, const char* entry_path, bool flip_x = false) -> bool {
            bool exists = false;
            std::vector<char> bytes = handle.read_file(entry_path, exists);
            if (!exists || bytes.size() < 92) {
                return false;
            }

            constexpr size_t OtrHeaderSize = 64;
            constexpr size_t TextureDataOffset = 92;
            const uint32_t width = read_le32(bytes, OtrHeaderSize + 4);
            const uint32_t height = read_le32(bytes, OtrHeaderSize + 8);
            const uint32_t image_data_size = read_le32(bytes, OtrHeaderSize + 24);
            const uint64_t texel_count = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
            const uint64_t expected_rgba32_size = texel_count * 4;
            const uint64_t expected_ia8_size = texel_count * 2;
            if (width == 0 || height == 0 ||
                (image_data_size != expected_rgba32_size && image_data_size != expected_ia8_size) ||
                TextureDataOffset + image_data_size > bytes.size()) {
                ZELDA_UI_LOG("Clock texture pack entry rejected: %s", entry_path);
                return false;
            }

            std::vector<char> rgba;
            if (image_data_size == expected_rgba32_size) {
                rgba.assign(bytes.begin() + TextureDataOffset, bytes.begin() + TextureDataOffset + image_data_size);
            }
            else {
                rgba.resize(static_cast<size_t>(texel_count) * 4);
                const unsigned char* ia = reinterpret_cast<const unsigned char*>(bytes.data() + TextureDataOffset);
                for (size_t i = 0; i < static_cast<size_t>(texel_count); i++) {
                    const unsigned char intensity = ia[i * 2 + 0];
                    const unsigned char alpha = ia[i * 2 + 1];
                    rgba[i * 4 + 0] = static_cast<char>(intensity);
                    rgba[i * 4 + 1] = static_cast<char>(intensity);
                    rgba[i * 4 + 2] = static_cast<char>(intensity);
                    rgba[i * 4 + 3] = static_cast<char>(alpha);
                }
            }
            if (flip_x) {
                const size_t row_stride = static_cast<size_t>(width) * 4;
                for (uint32_t y = 0; y < height; y++) {
                    char* row = rgba.data() + static_cast<size_t>(y) * row_stride;
                    for (uint32_t x = 0; x < width / 2; x++) {
                        char* left = row + static_cast<size_t>(x) * 4;
                        char* right = row + static_cast<size_t>(width - 1 - x) * 4;
                        for (int c = 0; c < 4; c++) {
                            std::swap(left[c], right[c]);
                        }
                    }
                }
            }
            tint_hour_icon(rgba, src);
            recompui::queue_image_from_bytes_rgba32(src, rgba, width, height);
            loaded_clock_images.emplace(src);
            return true;
        };

        const auto queue_first_o2r_texture = [&](const recomp::mods::ZipModFileHandle& handle, const char* src, std::initializer_list<const char*> entry_paths, bool flip_x = false) -> bool {
            for (const char* entry_path : entry_paths) {
                if (queue_o2r_texture(handle, src, entry_path, flip_x)) {
                    return true;
                }
            }
            ZELDA_UI_LOG("Clock texture pack missing: %s", src);
            return false;
        };

        const auto queue_generated_hour_icon = [&](const char* src, bool night) {
            constexpr uint32_t width = 64;
            constexpr uint32_t height = 64;
            std::vector<char> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
            const float cx = 31.5f;
            const float cy = 31.5f;
            const float radius = 22.0f;
            for (uint32_t y = 0; y < height; y++) {
                for (uint32_t x = 0; x < width; x++) {
                    const float dx = static_cast<float>(x) - cx;
                    const float dy = static_cast<float>(y) - cy;
                    const float dist = std::sqrt(dx * dx + dy * dy);
                    float alpha = 0.0f;
                    unsigned char red = 245;
                    unsigned char green = 215;
                    unsigned char blue = 64;
                    if (dist <= radius) {
                        alpha = dist >= radius - 2.0f ? 190.0f : 255.0f;
                        if (!night) {
                            red = 246;
                            green = 165;
                            blue = 36;
                        }
                    }
                    if (night) {
                        const float shadow_dx = static_cast<float>(x) - (cx + 8.0f);
                        const float shadow_dy = static_cast<float>(y) - (cy - 1.0f);
                        const float shadow_dist = std::sqrt(shadow_dx * shadow_dx + shadow_dy * shadow_dy);
                        if (shadow_dist <= radius - 2.0f) {
                            alpha = 0.0f;
                        }
                    }
                    const size_t pixel = (static_cast<size_t>(y) * width + x) * 4;
                    rgba[pixel + 0] = static_cast<char>(red);
                    rgba[pixel + 1] = static_cast<char>(green);
                    rgba[pixel + 2] = static_cast<char>(blue);
                    rgba[pixel + 3] = static_cast<char>(static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, alpha))));
                }
            }
            recompui::queue_image_from_bytes_rgba32(src, rgba, width, height);
            loaded_clock_images.emplace(src);
        };

        const auto load_o2r_clock_textures = [&]() -> bool {
            if (zelda64::get_clock_style() != zelda64::ClockStyle::Import) {
                return false;
            }

            const char* pack_path = std::getenv("APP_CLOCK_TEXTURE_PACK_PATH");
            if (pack_path == nullptr || pack_path[0] == '\0') {
                return false;
            }

            recomp::mods::ModOpenError open_error = recomp::mods::ModOpenError::Good;
            recomp::mods::ZipModFileHandle handle{std::filesystem::path{pack_path}, open_error};
            if (open_error != recomp::mods::ModOpenError::Good) {
                ZELDA_UI_LOG("Clock texture pack failed to open: %s", pack_path);
                return false;
            }

            bool loaded_any = false;
            for (const auto& [name, _] : clock_images) {
                std::string resource_name{name};
                if (resource_name.rfind("clock3ds_digit", 0) == 0) {
                    const char digit = resource_name.back();
                    std::string path = "alt/textures/parameter_static/gThreeDayClock3DSFinalHoursDigit";
                    path.push_back(digit);
                    path += "Tex";
                    loaded_any |= queue_o2r_texture(handle, name, path.c_str());
                }
            }

            loaded_any |= queue_o2r_texture(handle, "clock3ds_edge", "alt/textures/parameter_static/gThreeDayClock3DSEdgeTex");
            loaded_any |= queue_o2r_texture(handle, "clock3ds_edge_right", "alt/textures/parameter_static/gThreeDayClock3DSEdgeTex", true);
            loaded_any |= queue_o2r_texture(handle, "clock3ds_middle", "alt/textures/parameter_static/gThreeDayClock3DSMiddleTex");
            loaded_any |= queue_o2r_texture(handle, "clock3ds_fill", "alt/textures/parameter_static/gThreeDayClock3DSFillTex");
            loaded_any |= queue_o2r_texture(handle, "clock3ds_backdrop", "alt/textures/parameter_static/gThreeDayClock3DSTimeBackdropTex");
            loaded_any |= queue_o2r_texture(handle, "clock3ds_arrow", "alt/textures/parameter_static/gThreeDayClock3DSArrowTex");
            loaded_any |= queue_o2r_texture(handle, "clock3ds_slow", "alt/textures/parameter_static/gThreeDayClock3DSSlowTimeTex");
            clock_final_moon_loaded = queue_first_o2r_texture(handle, "clock3ds_final_moon", {
                "alt/textures/parameter_static/gThreeDayClock3DSFinalHoursMoonTex",
                "textures/parameter_static/gThreeDayClock3DSFinalHoursMoonTex",
            });
            loaded_any |= clock_final_moon_loaded;
            clock_sun_hour_loaded = queue_first_o2r_texture(handle, "clock3ds_sun_hour", {
                "alt/textures/parameter_static/gThreeDayClockSunHourTex",
                "alt/textures/parameter_static/gThreeDayClock3DSSunHourTex",
                "alt/parameter_static/gThreeDayClockSunHourTex",
                "alt/parameter_static/gThreeDayClock3DSSunHourTex",
                "textures/parameter_static/gThreeDayClockSunHourTex",
                "textures/parameter_static/gThreeDayClock3DSSunHourTex",
            });
            loaded_any |= clock_sun_hour_loaded;
            clock_moon_hour_loaded = queue_first_o2r_texture(handle, "clock3ds_moon_hour", {
                "alt/textures/parameter_static/gThreeDayClockMoonHourTex",
                "alt/textures/parameter_static/gThreeDayClock3DSMoonHourTex",
                "alt/parameter_static/gThreeDayClockMoonHourTex",
                "alt/parameter_static/gThreeDayClock3DSMoonHourTex",
                "textures/parameter_static/gThreeDayClockMoonHourTex",
                "textures/parameter_static/gThreeDayClock3DSMoonHourTex",
            });
            loaded_any |= clock_moon_hour_loaded;
            loaded_any |= queue_o2r_texture(handle, "clock3ds_colon", "alt/textures/parameter_static/gThreeDayClock3DSFinalHoursColonTex");
            if (loaded_any) {
                ZELDA_UI_LOG("Clock texture pack loaded: %s", pack_path);
            }
            return loaded_any;
        };

        const bool imported_clock_textures_loaded = load_o2r_clock_textures();
        zelda64::set_clock_texture_pack_loaded(imported_clock_textures_loaded);

        for (const auto& [name, path] : clock_images) {
            if (path[0] == '\0' || loaded_clock_images.contains(name)) {
                continue;
            }
            auto asset_path = zelda64::get_asset_path(path);
            if (std::filesystem::exists(asset_path)) {
                recompui::queue_image_from_bytes_file(name, read_file_to_bytes(asset_path));
                loaded_clock_images.emplace(name);
                if (std::string_view{name} == "clock3ds_sun_hour") {
                    clock_sun_hour_loaded = true;
                }
                else if (std::string_view{name} == "clock3ds_moon_hour") {
                    clock_moon_hour_loaded = true;
                }
            }
        }

        if (!clock_sun_hour_loaded) {
            queue_generated_hour_icon("clock3ds_sun_hour", false);
            clock_sun_hour_loaded = true;
        }
        if (!clock_moon_hour_loaded) {
            queue_generated_hour_icon("clock3ds_moon_hour", true);
            clock_moon_hour_loaded = true;
        }
    }

    void create_clock_overlay_context() {
        clock_overlay_context = recompui::create_context();
        clock_overlay_context.set_captures_input(false);
        clock_overlay_context.set_captures_mouse(false);
        load_clock_overlay_images();
    }

    void create_pause_save_prompt_overlay_context() {
        pause_save_prompt_overlay_context = recompui::create_context();
        pause_save_prompt_overlay_context.set_captures_input(false);
        pause_save_prompt_overlay_context.set_captures_mouse(false);
    }

    void hide_pause_save_prompt_overlay() {
        pause_save_prompt_overlay_markup.clear();
        if (pause_save_prompt_overlay_context != recompui::ContextId::null()) {
            pause_save_prompt_overlay_context.get_document()->SetInnerRML("");
            if (is_context_shown(pause_save_prompt_overlay_context)) {
                hide_context(pause_save_prompt_overlay_context);
            }
        }
    }

    void update_pause_save_prompt_overlay() {
        if (is_context_capturing_input() || !ultramodern::is_game_started() ||
            pause_save_prompt_overlay_context == recompui::ContextId::null()) {
            hide_pause_save_prompt_overlay();
            return;
        }

        zelda64::PauseSavePromptOverlayState state = zelda64::get_pause_save_prompt_overlay_state();
        if (!state.visible || state.alpha <= 0) {
            hide_pause_save_prompt_overlay();
            return;
        }

        const int alpha = std::max(0, std::min(255, state.alpha));
        const float opacity = alpha / 255.0f;
        const bool saved = state.save_prompt_state >= 5 && state.save_prompt_state != 6;
        const bool no_selected = state.prompt_choice != 0;

        const auto append_text = [](std::ostringstream& stream, const char* text, float left, float top, float width,
                                    float font_size) {
            const auto append_layer = [&](const char* color, float x_offset, float y_offset) {
                stream << "<div style=\"position:absolute; left:50%; top:" << std::fixed << std::setprecision(1)
                       << (top + y_offset) << "dp; margin-left:" << (left + x_offset) << "dp; width:" << width
                       << "dp; text-align:center; font-size:" << font_size << "dp; color:" << color << ";\">"
                       << text << "</div>";
            };
            append_layer("rgba(0,0,0,230)", 3.0f, 3.0f);
            append_layer("white", 0.0f, 0.0f);
        };

        std::ostringstream html;
        html << "<div style=\"position:absolute; left:0; top:0; width:100%; height:100%; "
                "font-family:Chiaro; font-weight:bold; pointer-events:none; opacity:"
             << std::fixed << std::setprecision(3) << opacity << ";\">";

        if (saved) {
            append_text(html, "Save complete.", -260.0f, 454.0f, 520.0f, 54.0f);
        }
        else {
            append_text(html, "Would you like to save?", -420.0f, 380.0f, 840.0f, 54.0f);
            append_text(html, "Yes", -250.0f, 574.0f, 180.0f, 54.0f);
            append_text(html, "No", 70.0f, 574.0f, 180.0f, 54.0f);
        }

        append_text(html, "to decide", -86.0f, 918.0f, 300.0f, 42.0f);
        html << "</div>";

        std::string new_markup = html.str();
        if (new_markup != pause_save_prompt_overlay_markup) {
            pause_save_prompt_overlay_markup = std::move(new_markup);
            pause_save_prompt_overlay_context.get_document()->SetInnerRML(pause_save_prompt_overlay_markup);
        }

        if (!is_context_shown(pause_save_prompt_overlay_context)) {
            show_context(pause_save_prompt_overlay_context);
        }
    }

    void request_clock_texture_reload() {
        clock_texture_reload_requested.store(true);
    }

    void update_clock_overlay() {
        if (clock_texture_reload_requested.exchange(false)) {
            recompui::refresh_clock_texture_pack_name();
            load_clock_overlay_images();
            clock_overlay_markup.clear();
            if (clock_overlay_context != recompui::ContextId::null()) {
                clock_overlay_context.get_document()->SetInnerRML("");
            }
        }

        if (is_context_capturing_input()) {
            if (clock_overlay_context != recompui::ContextId::null() && is_context_shown(clock_overlay_context)) {
                hide_context(clock_overlay_context);
            }
            return;
        }

        const zelda64::ClockStyle clock_style = zelda64::get_clock_style();
        const bool should_show =
            ultramodern::is_game_started() &&
            clock_style != zelda64::ClockStyle::Original &&
            (clock_style != zelda64::ClockStyle::Import || zelda64::get_clock_texture_pack_loaded());

        if (!should_show) {
            if (clock_overlay_context != recompui::ContextId::null() && is_context_shown(clock_overlay_context)) {
                hide_context(clock_overlay_context);
            }
            return;
        }

        zelda64::ClockOverlayState state = zelda64::get_clock_overlay_state();
        if (!state.visible || state.alpha <= 0) {
            if (clock_overlay_context != recompui::ContextId::null() && is_context_shown(clock_overlay_context)) {
                hide_context(clock_overlay_context);
            }
            return;
        }

        if (clock_overlay_context == recompui::ContextId::null()) {
            return;
        }

        const int alpha = std::max(0, std::min(255, state.alpha));
        const float opacity = alpha / 255.0f;
        const int day = std::max(1, std::min(3, state.day));
        constexpr int SECONDS_IN_THREE_DAYS = 3 * 24 * 60 * 60;
        constexpr int SECONDS_IN_SIX_HOURS = 6 * 60 * 60;
        const int time_until_crash = std::max(0, std::min(SECONDS_IN_THREE_DAYS, state.time_until_crash_seconds));
        const int elapsed_seconds = SECONDS_IN_THREE_DAYS - time_until_crash;
        constexpr float clock_scale = 3.35f;
        constexpr float section_width = 48.0f * clock_scale;
        constexpr float section_half_width = 24.0f * clock_scale;
        constexpr float clock_width = 192.0f * clock_scale;
        constexpr float clock_height = 12.0f * clock_scale;
        constexpr float time_box_width = 48.0f * clock_scale;
        constexpr float time_box_height = 16.0f * clock_scale;
        constexpr float bar_left = 0.0f;
        constexpr float bar_top = 64.0f;
        constexpr float day_band_top = 5.5f * clock_scale;
        constexpr float day_band_height = 4.0f * clock_scale;
        constexpr float day_band_inset = 1.5f * clock_scale;
        constexpr float final_digit_size = 16.0f * clock_scale;
        constexpr float time_digit_size = 7.4f * clock_scale;
        constexpr float hour_icon_size = 12.0f * clock_scale;
        constexpr float arrow_size = 8.0f * clock_scale;
        const float arrow_offset = (static_cast<float>(elapsed_seconds) / SECONDS_IN_THREE_DAYS) * (section_width * 3.0f);
        const float counter_x = section_half_width + arrow_offset;
        const bool slow_time = state.time_speed_offset == -2;
        const auto append_img = [](std::ostringstream& stream, const char* src, float left, float top, float width, float height) {
            stream << "<img src=\"" << src << "\" style=\"position:absolute; left:" << std::fixed << std::setprecision(1) << left
                   << "dp; top:" << top << "dp; width:" << width << "dp; height:" << height << "dp;\"></img>";
        };
        const auto append_digit = [&](std::ostringstream& stream, int digit, float left, float top, float size) {
            digit = std::max(0, std::min(9, digit));
            std::string src = "clock3ds_digit" + std::to_string(digit);
            append_img(stream, src.c_str(), left, top, size, size);
        };
        std::ostringstream html;
        html << "<div style=\"position:absolute; left:50%; bottom:30dp; margin-left:" << (-clock_width / 2.0f) << "dp; "
                "width:" << clock_width << "dp; height:132dp; opacity:" << std::fixed << std::setprecision(3) << opacity << "; "
                "font-family:Chiaro; pointer-events:none;\">"
             << "<div style=\"position:absolute; left:" << bar_left << "dp; top:" << bar_top << "dp; width:" << clock_width << "dp; height:" << clock_height << "dp;\">"
             << "<div style=\"position:absolute; left:" << (section_half_width + day_band_inset) << "dp; top:" << day_band_top << "dp; width:" << (section_width - day_band_inset * 2.0f) << "dp; height:" << day_band_height << "dp; "
                "background-color:rgba(0,128,255," << (day <= 1 ? 175 : 52) << ");\"></div>"
             << "<div style=\"position:absolute; left:" << (section_half_width + section_width + day_band_inset) << "dp; top:" << day_band_top << "dp; width:" << (section_width - day_band_inset * 2.0f) << "dp; height:" << day_band_height << "dp; "
                "background-color:rgba(255,192,0," << (day == 2 ? 175 : 52) << ");\"></div>"
             << "<div style=\"position:absolute; left:" << (section_half_width + section_width * 2.0f + day_band_inset) << "dp; top:" << day_band_top << "dp; width:" << (section_width - day_band_inset * 2.0f) << "dp; height:" << day_band_height << "dp; "
                "background-color:rgba(255,64,0," << (day >= 3 ? 175 : 52) << ");\"></div>"
             << "</div>";
        append_img(html, "clock3ds_edge", 0.0f, bar_top, 72.0f * clock_scale, clock_height);
        append_img(html, "clock3ds_middle", 72.0f * clock_scale, bar_top, section_width, clock_height);
        append_img(html, "clock3ds_edge_right", (72.0f + 48.0f) * clock_scale, bar_top, 72.0f * clock_scale, clock_height);
        append_img(html, "clock3ds_arrow", counter_x - (arrow_size / 2.0f), bar_top + 4.0f * clock_scale, arrow_size, arrow_size);

        if (state.final_hours) {
            int total_seconds = std::max(0, state.time_until_crash_seconds);
            int hours = total_seconds / (60 * 60);
            int minutes = (total_seconds / 60) % 60;
            int seconds = total_seconds % 60;
            const float digits_left = (clock_width - (8.0f * final_digit_size)) / 2.0f;
            const float digits_top = 12.0f;
            append_digit(html, hours / 10, digits_left, digits_top, final_digit_size);
            append_digit(html, hours % 10, digits_left + final_digit_size * 0.85f, digits_top, final_digit_size);
            append_img(html, "clock3ds_colon", digits_left + final_digit_size * 1.65f, digits_top, final_digit_size, final_digit_size);
            append_digit(html, minutes / 10, digits_left + final_digit_size * 2.45f, digits_top, final_digit_size);
            append_digit(html, minutes % 10, digits_left + final_digit_size * 3.30f, digits_top, final_digit_size);
            append_img(html, "clock3ds_colon", digits_left + final_digit_size * 4.10f, digits_top, final_digit_size, final_digit_size);
            append_digit(html, seconds / 10, digits_left + final_digit_size * 4.90f, digits_top, final_digit_size);
            append_digit(html, seconds % 10, digits_left + final_digit_size * 5.75f, digits_top, final_digit_size);
            append_img(html, "clock3ds_final_moon", (clock_width - final_digit_size) / 2.0f, digits_top - final_digit_size * 0.85f, final_digit_size, final_digit_size);
        }
        else {
            int total_time = ((state.current_time_seconds % (24 * 60 * 60)) + (24 * 60 * 60)) % (24 * 60 * 60);
            int hours24 = total_time / (60 * 60);
            int minutes = (total_time / 60) % 60;
            int hours12 = hours24 % 12;
            if (hours12 == 0) {
                hours12 = 12;
            }
            float time_box_left = std::max(0.0f, std::min(clock_width - time_box_width, counter_x - section_half_width));
            const float time_box_top = bar_top - 8.0f * clock_scale;
            const bool night = (hours24 < 6 || hours24 >= 18);
            const char* hour_icon = night ? "clock3ds_moon_hour" : "clock3ds_sun_hour";
            append_img(html, "clock3ds_backdrop", time_box_left, time_box_top, time_box_width, time_box_height);
            if ((night && clock_moon_hour_loaded) || (!night && clock_sun_hour_loaded)) {
                append_img(html, hour_icon, time_box_left + 5.0f * clock_scale, time_box_top + 2.0f * clock_scale, hour_icon_size, hour_icon_size);
            }
            const float digit_top = time_box_top + 4.0f * clock_scale;
            const float time_digits_width = hours12 >= 10 ? time_digit_size * 3.75f : time_digit_size * 3.05f;
            const float digit_left = time_box_left + (time_box_width - time_digits_width) * 0.58f;
            if (hours12 >= 10) {
                append_digit(html, hours12 / 10, digit_left, digit_top, time_digit_size);
                append_digit(html, hours12 % 10, digit_left + time_digit_size * 0.72f, digit_top, time_digit_size);
                append_img(html, "clock3ds_colon", digit_left + time_digit_size * 1.36f, digit_top, time_digit_size, time_digit_size);
                append_digit(html, minutes / 10, digit_left + time_digit_size * 2.03f, digit_top, time_digit_size);
                append_digit(html, minutes % 10, digit_left + time_digit_size * 2.75f, digit_top, time_digit_size);
            }
            else {
                append_digit(html, hours12, digit_left, digit_top, time_digit_size);
                append_img(html, "clock3ds_colon", digit_left + time_digit_size * 0.68f, digit_top, time_digit_size, time_digit_size);
                append_digit(html, minutes / 10, digit_left + time_digit_size * 1.35f, digit_top, time_digit_size);
                append_digit(html, minutes % 10, digit_left + time_digit_size * 2.07f, digit_top, time_digit_size);
            }
            if (slow_time) {
                append_img(html, "clock3ds_slow", time_box_left + time_box_width - time_digit_size * 0.55f, time_box_top - 4.0f, time_digit_size, time_digit_size);
            }
        }

        html << "</div>";

        std::string new_markup = html.str();
        if (new_markup != clock_overlay_markup) {
            clock_overlay_markup = std::move(new_markup);
            clock_overlay_context.get_document()->SetInnerRML(clock_overlay_markup);
        }

        if (!is_context_shown(clock_overlay_context)) {
            show_context(clock_overlay_context);
        }
    }

    Rml::ElementDocument* top_input_document() {
        // Iterate backwards and stop at the first context that takes input.
        for (auto it = shown_contexts.rbegin(); it != shown_contexts.rend(); it++) {
            if (it->context.captures_input()) {
                return it->document;
            }
        }
        return nullptr;
    }

    Rml::ElementDocument* top_mouse_document() {
        // Iterate backwards and stop at the first context that takes input.
        for (auto it = shown_contexts.rbegin(); it != shown_contexts.rend(); it++) {
            if (it->context.captures_mouse()) {
                return it->document;
            }
        }
        return nullptr;
    }

    void update_contexts() {
        for (auto& context_details : shown_contexts) {
            context_details.context.open();
            context_details.context.process_updates();
            context_details.context.close();
        }
    }
};

std::unique_ptr<UIState> ui_state;
std::recursive_mutex ui_state_mutex{};

// TODO make this not be global
extern SDL_Window* window;

void recompui::get_window_size(int& width, int& height) {
    SDL_GetWindowSizeInPixels(window, &width, &height);
}

inline const std::string read_file_to_string(std::filesystem::path path) {
    std::ifstream stream = std::ifstream{path};
    std::ostringstream ss;
    ss << stream.rdbuf();
    return ss.str(); 
}

inline std::vector<char> read_file_to_bytes(std::filesystem::path path) {
    std::ifstream stream{ path, std::ios::binary };
    return std::vector<char>{
        std::istreambuf_iterator<char>{ stream },
        std::istreambuf_iterator<char>{}
    };
}

void init_hook(RT64::RenderInterface* interface, RT64::RenderDevice* device) {
    ZELDA_UI_LOG("init_hook begin interface=%p device=%p window=%p", interface, device, window);
#if defined(__linux__)
    std::locale::global(std::locale::classic());
#endif
    ui_state = std::make_unique<UIState>(window, interface, device);
    ui_state->create_menus();
    ZELDA_UI_LOG("init_hook end");
}

moodycamel::ConcurrentQueue<SDL_Event> ui_event_queue{};

void recompui::queue_event(const SDL_Event& event) {
    ui_event_queue.enqueue(event);
}

bool recompui::try_deque_event(SDL_Event& out) {
    return ui_event_queue.try_dequeue(out);
}

int cont_button_to_key(SDL_ControllerButtonEvent& button) {
    // Configurable accept button in menu
    auto menuAcceptBinding0 = recomp::get_input_binding(recomp::GameInput::ACCEPT_MENU, 0, recomp::InputDevice::Controller);
    auto menuAcceptBinding1 = recomp::get_input_binding(recomp::GameInput::ACCEPT_MENU, 1, recomp::InputDevice::Controller);
    // note - magic number: 0 is InputType::None
    if ((menuAcceptBinding0.input_type != 0 && button.button == menuAcceptBinding0.input_id) ||
        (menuAcceptBinding1.input_type != 0 && button.button == menuAcceptBinding1.input_id)) {
        return SDLK_RETURN;
    }

    // Configurable apply button in menu
    auto menuApplyBinding0 = recomp::get_input_binding(recomp::GameInput::APPLY_MENU, 0, recomp::InputDevice::Controller);
    auto menuApplyBinding1 = recomp::get_input_binding(recomp::GameInput::APPLY_MENU, 1, recomp::InputDevice::Controller);
    // note - magic number: 0 is InputType::None
    if ((menuApplyBinding0.input_type != 0 && button.button == menuApplyBinding0.input_id) ||
        (menuApplyBinding1.input_type != 0 && button.button == menuApplyBinding1.input_id)) {
        return SDLK_f;
    } 

    // Allows closing the menu
    auto menuToggleBinding0 = recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 0, recomp::InputDevice::Controller);
    auto menuToggleBinding1 = recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 1, recomp::InputDevice::Controller);
    // note - magic number: 0 is InputType::None
    if ((menuToggleBinding0.input_type != 0 && button.button == menuToggleBinding0.input_id) ||
        (menuToggleBinding1.input_type != 0 && button.button == menuToggleBinding1.input_id)) {
        return SDLK_ESCAPE;
    }

    switch (button.button) {
        case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_UP:
            return SDLK_UP;
        case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return SDLK_DOWN;
        case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return SDLK_LEFT;
        case SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return SDLK_RIGHT;
    }

    return 0;
}


int cont_axis_to_key(SDL_ControllerAxisEvent& axis, float value) {
    switch (axis.axis) {
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY:
        if (value < 0) return SDLK_UP;
        return SDLK_DOWN;
    case SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX:
        if (value >= 0) return SDLK_RIGHT;
        return SDLK_LEFT;
    }
    return 0;
}

void apply_background_input_mode() {
    static recomp::BackgroundInputMode last_input_mode = recomp::BackgroundInputMode::OptionCount;

    recomp::BackgroundInputMode cur_input_mode = recomp::get_background_input_mode();

    if (last_input_mode != cur_input_mode) {
        SDL_SetHint(
            SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
            cur_input_mode == recomp::BackgroundInputMode::On
                ? "1"
                : "0"
        );
    }
    last_input_mode = cur_input_mode;
}

bool recompui::get_cont_active() {
    return ui_state->cont_is_active;
}

void recompui::set_cont_active(bool active) {
    ui_state->cont_is_active = active;
}

void recompui::activate_mouse() {
    ui_state->update_primary_input(true, false);
    ui_state->update_focus(true, false);
}

void draw_hook(RT64::RenderCommandList* command_list, RT64::RenderFramebuffer* swap_chain_framebuffer) {
    static int draw_count = 0;
    if (draw_count < 10 || draw_count % 120 == 0) {
        ZELDA_UI_LOG("draw_hook #%d ui_state=%p framebuffer=%p", draw_count, ui_state.get(), swap_chain_framebuffer);
    }
    draw_count++;

    apply_background_input_mode();

    // Return early if the ui context has been destroyed already.
    if (!ui_state) {
        return;
    }

    // Return to the launcher if no menu is open and the game isn't started.
    if (!recompui::is_any_context_shown() && !ultramodern::is_game_started()) {
        ZELDA_UI_LOG("showing launcher from draw_hook");
        recompui::show_context(recompui::get_launcher_context_id(), "");
    }

    std::lock_guard lock{ ui_state_mutex };

    SDL_Event cur_event{};

    bool mouse_moved = false;
    bool mouse_clicked = false;
    bool non_mouse_interacted = false;
    bool cont_interacted = false;
    bool kb_interacted = false;

    bool config_was_open = recompui::is_context_shown(recompui::get_config_context_id()) || recompui::is_context_shown(recompui::get_config_sub_menu_context_id());

    using clock = std::chrono::system_clock;

    // TODO move these into a more appropriate place.
    constexpr clock::duration start_repeat_delay = std::chrono::milliseconds{500};
    constexpr clock::duration repeat_rate = std::chrono::milliseconds{50};
    static clock::time_point next_repeat_time = {};
    static int latest_controller_key_pressed = SDLK_UNKNOWN;

    while (recompui::try_deque_event(cur_event)) {
        bool context_capturing_input = recompui::is_context_capturing_input();
        bool context_capturing_mouse = recompui::is_context_capturing_mouse();

        // Handle up button events even when input is disabled to avoid missing them during binding.
        if (cur_event.type == SDL_EventType::SDL_CONTROLLERBUTTONUP) {
            int sdl_key = cont_button_to_key(cur_event.cbutton);
            if (sdl_key == latest_controller_key_pressed) {
                latest_controller_key_pressed = SDLK_UNKNOWN;
            }
        }

        if (!recomp::all_input_disabled()) {
            bool is_mouse_input = false;
            // Implement some additional behavior for specific events on top of what RmlUi normally does with them.
            switch (cur_event.type) {
            case SDL_EventType::SDL_MOUSEMOTION: {
                int *last_mouse_pos = ui_state->last_active_mouse_position;

                if (!ui_state->mouse_is_active) {
                    float xD = cur_event.motion.x - last_mouse_pos[0];
                    float yD = cur_event.motion.y - last_mouse_pos[1];
                    if (sqrt(xD * xD + yD * yD) < 100) {
                        break;
                    }
                }
                last_mouse_pos[0] = cur_event.motion.x;
                last_mouse_pos[1] = cur_event.motion.y;

                // if controller is the primary input, don't use mouse movement to allow cursor to reactivate
                if (recompui::get_cont_active()) {
                    break;
                }
            }
            // fallthrough
            case SDL_EventType::SDL_MOUSEBUTTONDOWN:
                mouse_moved = true;
                mouse_clicked = true;
                is_mouse_input = true;
                break;
                
            case SDL_EventType::SDL_MOUSEBUTTONUP:
            case SDL_EventType::SDL_MOUSEWHEEL:
                is_mouse_input = true;
                break;
                
            case SDL_EventType::SDL_CONTROLLERBUTTONDOWN: {
                int sdl_key = cont_button_to_key(cur_event.cbutton);
                if (context_capturing_input && sdl_key) {
                    ui_state->context->ProcessKeyDown(RmlSDL::ConvertKey(sdl_key), 0);
                    latest_controller_key_pressed = sdl_key;
                    next_repeat_time = clock::now() + start_repeat_delay;
                }
                non_mouse_interacted = true;
                cont_interacted = true;
                break;
            }
            case SDL_EventType::SDL_KEYDOWN:
                non_mouse_interacted = true;
                kb_interacted = true;
                if (cur_event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_F8) {
                    if (zelda64::get_debug_mode_enabled() && ui_state->debugger_initialized) {
                        Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
                    }
                }
                break;
            case SDL_EventType::SDL_USEREVENT:
                if (cur_event.user.code == SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY) {
                    ui_state->await_stick_return_y = true;
                } else if (cur_event.user.code == SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX) {
                    ui_state->await_stick_return_x = true;
                }
                break;
            case SDL_EventType::SDL_CONTROLLERAXISMOTION:
                SDL_ControllerAxisEvent* axis_event = &cur_event.caxis;
                if (axis_event->axis != SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY && axis_event->axis != SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX) {
                    break;
                }

                float axis_value = axis_event->value * (1 / 32768.0f);
                bool* await_stick_return = axis_event->axis == SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY
                        ? &ui_state->await_stick_return_y
                        : &ui_state->await_stick_return_x;
                if (fabsf(axis_value) > 0.5f) {
                    if (!*await_stick_return) {
                        *await_stick_return = true;
                        non_mouse_interacted = true;
                        int sdl_key = cont_axis_to_key(cur_event.caxis, axis_value);
                        if (context_capturing_input && sdl_key) {
                            ui_state->context->ProcessKeyDown(RmlSDL::ConvertKey(sdl_key), 0);
                            latest_controller_key_pressed = sdl_key;
                            next_repeat_time = clock::now() + start_repeat_delay;
                        }
                    }
                    non_mouse_interacted = true;
                    cont_interacted = true;
                }
                else if (*await_stick_return && fabsf(axis_value) < 0.15f) {
                    *await_stick_return = false;
                    // Stop pressing the current key if the axis that was released was the one triggering key presses.
                    int sdl_key = cont_axis_to_key(cur_event.caxis, axis_value);
                    if (sdl_key == latest_controller_key_pressed) {
                        latest_controller_key_pressed = SDLK_UNKNOWN;
                    }
                }
                break;
            }

            // Send the event to RmlUi if this type of event is being captured.
            if (is_mouse_input) {
                if (context_capturing_mouse) {
                    RmlSDL::InputEventHandler(ui_state->context, cur_event);
                }
            }
            else {
                if (context_capturing_input) {
                    RmlSDL::InputEventHandler(ui_state->context, cur_event);
                }
            }
        }

        // If the config menu isn't open and the game has been started and either the escape key or select button are pressed, open the config menu.
        if (!config_was_open && ultramodern::is_game_started()) {
            bool open_config = false;

            switch (cur_event.type) {
            case SDL_EventType::SDL_KEYDOWN:
                if (cur_event.key.keysym.scancode == SDL_Scancode::SDL_SCANCODE_ESCAPE) {
                    open_config = true;
                }
                break;
            case SDL_EventType::SDL_CONTROLLERBUTTONDOWN:
                auto menuToggleBinding0 = recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 0, recomp::InputDevice::Controller);
                auto menuToggleBinding1 = recomp::get_input_binding(recomp::GameInput::TOGGLE_MENU, 1, recomp::InputDevice::Controller);
                // note - magic number: 0 is InputType::None
                if ((menuToggleBinding0.input_type != 0 && cur_event.cbutton.button == menuToggleBinding0.input_id) ||
                    (menuToggleBinding1.input_type != 0 && cur_event.cbutton.button == menuToggleBinding1.input_id)) {
                    open_config = true;
                }
                break;
            }

            if (open_config) {
                recompui::show_context(recompui::get_config_context_id(), "");
            }
        }
    } // end dequeue event loop

    // Handle controller key repeats.
    if (latest_controller_key_pressed != SDLK_UNKNOWN) {
        clock::time_point now = clock::now();
        if (now >= next_repeat_time) {
            ui_state->context->ProcessKeyDown(RmlSDL::ConvertKey(latest_controller_key_pressed), 0);
            next_repeat_time += repeat_rate;
        }
    }

    if (cont_interacted || kb_interacted || mouse_clicked) {
        recompui::set_cont_active(cont_interacted);
    }
    recomp::config_menu_set_cont_or_kb(ui_state->cont_is_active);

    recomp::InputField scanned_field = recomp::get_scanned_input();
    if (scanned_field != recomp::InputField{}) {
        recomp::finish_scanning_input(scanned_field);
    }

    ui_state->update_primary_input(mouse_moved, non_mouse_interacted);
    ui_state->update_focus(mouse_moved, non_mouse_interacted);
    ui_state->update_clock_overlay();
    ui_state->update_pause_save_prompt_overlay();

    if (recompui::is_any_context_shown()) {
        ui_state->update_contexts();

        int width = swap_chain_framebuffer->getWidth();
        int height = swap_chain_framebuffer->getHeight();
        if (draw_count < 10 || draw_count % 120 == 0) {
            ZELDA_UI_LOG("rendering UI contexts=%d size=%dx%d", recompui::is_any_context_shown() ? 1 : 0, width, height);
        }

        // Scale the UI based on the window size with 1080 vertical resolution as the reference point.
        ui_state->context->SetDensityIndependentPixelRatio((height) / 1080.0f);

        const bool show_launcher_background = !ultramodern::is_game_started();
        ui_state->render_interface.set_launcher_background_visible(show_launcher_background);
        ui_state->render_interface.start(command_list, width, height);

        static int prev_width = 0;
        static int prev_height = 0;

        if (prev_width != width || prev_height != height) {
            ui_state->context->SetDimensions({ width, height });
        }
        prev_width = width;
        prev_height = height;

        ui_state->context->Update();
        if (show_launcher_background) {
            ui_state->render_interface.render_launcher_background();
        }
        ui_state->context->Render();
        ui_state->render_interface.end(command_list, swap_chain_framebuffer);
    }
}

void deinit_hook() {
    recompui::destroy_all_contexts();

    std::lock_guard lock {ui_state_mutex};
    if (ui_state && ui_state->debugger_initialized) {
        Rml::Debugger::Shutdown();
    }
    Rml::Shutdown();
    ui_state->unload();
    ui_state.reset();
}

void recompui::set_render_hooks() {
    RT64::SetRenderHooks(init_hook, draw_hook, deinit_hook);
}

void recompui::message_box(const char* msg) {
    std::string display_msg = msg;
#if defined(__ANDROID__)
    if (display_msg.find("Unable to find compatible graphics device") != std::string::npos) {
        if (const char* app_folder_path = std::getenv("APP_FOLDER_PATH");
            app_folder_path != nullptr && app_folder_path[0] != '\0') {
            const std::filesystem::path diagnostic_path = std::filesystem::path(app_folder_path) / "vulkan_device_error.txt";
            std::ifstream diagnostic_file(diagnostic_path);
            if (diagnostic_file) {
                std::stringstream diagnostic_stream;
                diagnostic_stream << diagnostic_file.rdbuf();
                display_msg += "\n\nDetails were written to:\n";
                display_msg += diagnostic_path.string();
                display_msg += "\n\n";
                display_msg += diagnostic_stream.str();
            }
        }
    }
#endif
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, zelda64::program_name.data(), display_msg.c_str(), nullptr);
    printf("[ERROR] %s\n", display_msg.c_str());
}

void recompui::show_context(ContextId context, std::string_view param) {
    ContextId prev_context = recompui::try_close_current_context();
    {
        std::lock_guard lock{ ui_state_mutex };

        // TODO call the context's on_show callback with the param.
        ui_state->show_context(context);
    }
    if (prev_context != ContextId::null()) {
        prev_context.open();
    }
}

void recompui::hide_context(ContextId context) {
    ContextId prev_context = recompui::try_close_current_context();
    {
        std::lock_guard lock{ ui_state_mutex };

        ui_state->hide_context(context);
    }
    if (prev_context != ContextId::null()) {
        prev_context.open();
    }
}

void recompui::hide_all_contexts() {
    std::lock_guard lock{ui_state_mutex};

    if (ui_state) {
        ui_state->hide_all_contexts();
    }
}

bool recompui::is_context_shown(ContextId context) {
    std::lock_guard lock{ui_state_mutex};

    if (!ui_state) {
        return false;
    }

    return ui_state->is_context_shown(context);
}

bool recompui::is_context_capturing_input() {
    std::lock_guard lock{ui_state_mutex};

    if (!ui_state) {
        return false;
    }

    return ui_state->is_context_capturing_input();
}

bool recompui::is_context_capturing_mouse() {
    std::lock_guard lock{ui_state_mutex};

    if (!ui_state) {
        return false;
    }

    return ui_state->is_context_capturing_mouse();
}

bool recompui::is_any_context_shown() {
    std::lock_guard lock{ui_state_mutex};

    if (!ui_state) {
        return false;
    }

    return ui_state->is_any_context_shown();
}

#if defined(__ANDROID__)
extern "C" bool zelda64_android_is_ui_capturing_touch() {
    return recompui::is_context_capturing_mouse() || !ultramodern::is_game_started();
}
#endif

Rml::ElementDocument* recompui::load_document(const std::filesystem::path& path) {
    std::lock_guard lock{ui_state_mutex};

    return ui_state->context->LoadDocument(path.string());
}

Rml::ElementDocument* recompui::create_empty_document() {
    std::lock_guard lock{ui_state_mutex};

    return ui_state->context->CreateDocument();
}

void recompui::queue_image_from_bytes_file(const std::string &src, const std::vector<char> &bytes) {
    ui_state->render_interface.queue_image_from_bytes_file(src, bytes);
}

void recompui::queue_image_from_bytes_rgba32(const std::string &src, const std::vector<char> &bytes, uint32_t width, uint32_t height) {
    ui_state->render_interface.queue_image_from_bytes_rgba32(src, bytes, width, height);
}

void recompui::release_image(const std::string &src) {
    Rml::ReleaseTexture(src);
}

void recompui::request_clock_texture_reload() {
    std::lock_guard lock{ui_state_mutex};

    if (ui_state) {
        ui_state->request_clock_texture_reload();
    }
}

void recompui::drop_files(const std::list<std::filesystem::path> &file_list) {
    // Prevent mod installation after the game has started.
    if (ultramodern::is_game_started()) {
        return;
    }

    recompui::set_config_tab(recompui::ConfigTab::Mods);
    // If the config menu isn't open, open it in the mods tab.
    if (!recompui::is_context_shown(recompui::get_config_context_id())) {
        recompui::hide_all_contexts();
        recompui::show_context(recompui::get_config_context_id(), "");
    }

    recompui::open_notification("Installing Mods", "Please Wait");
    // TODO: Needs a progress callback and a prompt for every mod that needs to be confirmed to be overwritten.
    // TODO: Run this on a background thread and use the callbacks to advance the state instead of blocking.
    ModInstaller::Result result;
    ModInstaller::start_mod_installation(file_list, nullptr, result);

    recompui::close_prompt();

    if (!result.error_messages.empty()) {
        std::string error_label = std::accumulate(result.error_messages.begin(), result.error_messages.end(), std::string{},
            [](const std::string &lhs, const std::string &rhs)
            {
                return lhs.empty() ? rhs : lhs + '\n' + rhs;
            });

        recompui::open_info_prompt("Error Installing Mods", error_label, "OK", {}, recompui::ButtonVariant::Tertiary);
        std::vector<std::string> dummy_error_messages{};
        ModInstaller::cancel_mod_installation(result, dummy_error_messages);
        return;
    }

    std::vector<ModInstaller::Confirmation> confirmations{};

    for (const ModInstaller::Installation& pending_install : result.pending_installations) {
        if (pending_install.needs_overwrite_confirmation) {
            // Get the mod details for the current mod at this file path.
            std::string old_mod_id = recomp::mods::get_mod_id_from_filename(pending_install.mod_file.filename());
            std::optional<recomp::mods::ModDetails> old_mod_details = {};

            if (!old_mod_id.empty()) {
                old_mod_details = recomp::mods::get_details_for_mod(old_mod_id);
            }

            if (old_mod_details) {
                confirmations.emplace_back(ModInstaller::Confirmation {
                    .old_display_name = old_mod_details->display_name,
                    .new_display_name = pending_install.display_name,
                    .old_mod_id = old_mod_details->mod_id,
                    .new_mod_id = pending_install.mod_id,
                    .old_version = old_mod_details->version,
                    .new_version = pending_install.mod_version
                });
            }
            else {
                confirmations.emplace_back(ModInstaller::Confirmation {
                    .old_display_name = "?",
                    .new_display_name = pending_install.display_name,
                    .old_mod_id = "",
                    .new_mod_id = pending_install.mod_id,
                    .old_version = recomp::Version{0, 0, 0, ""},
                    .new_version = pending_install.mod_version
                });
            }
        }
    }

    if (confirmations.empty()) {
        std::vector<std::string> error_messages{};
        ModInstaller::finish_mod_installation(result, error_messages);
        ContextId old_context = recompui::try_close_current_context();
        recompui::update_mod_list();
        if (old_context != ContextId::null()) {
            old_context.open();
        }
        // TODO show errors
    }
    else {
        std::string prompt_text = std::accumulate(confirmations.begin(), confirmations.end(), std::string{},
            [](const std::string &cur_text, const ModInstaller::Confirmation &confirmation)
            {
                std::string new_text{};
                if (confirmation.old_display_name == confirmation.new_display_name) {
                    new_text = confirmation.old_display_name + " (" + confirmation.old_version.to_string() + " -> " + confirmation.new_version.to_string() + ")";
                }
                else {
                    new_text =
                        confirmation.old_display_name + " (" + confirmation.old_version.to_string() + ") -> " +
                        confirmation.new_display_name + " (" + confirmation.new_version.to_string() + ")";
                }
                return cur_text.empty() ? new_text : cur_text + '\n' + new_text;
            });

        // open prompt where confirm finishes the mod installation with the overwritten files
        recompui::open_choice_prompt("Overwrite Mods?",
            prompt_text,
            "Overwrite",
            "Cancel",
            [result]() {
                std::vector<std::string> error_messages{};
                recomp::mods::close_mods();
                ModInstaller::finish_mod_installation(result, error_messages);
                ContextId old_context = recompui::try_close_current_context();
                recompui::update_mod_list();
                if (old_context != ContextId::null()) {
                    old_context.open();
                }
                // TODO show errors
            },
            [result]() {
                std::vector<std::string> error_messages{};
                ModInstaller::cancel_mod_installation(result, error_messages);
                // TODO show errors
            },
            recompui::ButtonVariant::Success,
            recompui::ButtonVariant::Error,
            true,
            ""
        );
    }
}
