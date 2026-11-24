#ifndef RECOMPUI_SAVE_EDITOR_CONFIG_H
#define RECOMPUI_SAVE_EDITOR_CONFIG_H

#include <vector>

#include "ui_config_sub_menu.h"

namespace recompui {

class Button;
class Container;

class SaveEditorConfigPanel : public Element {
public:
    enum class LiveCategory {
        Time,
        State,
        Health,
        Equipment,
        Inventory,
        Masks,
        Quest,
        Songs,
        Dungeons,
        Count,
    };

    SaveEditorConfigPanel(Element *parent, Rml::Element *host_element);
    virtual ~SaveEditorConfigPanel();
    void refresh();
protected:
    void process_event(const Event &e) override;
    std::string_view get_type_name() override { return "SaveEditorConfigPanel"; }
private:
    enum class Mode {
        None,
        PregameSetup,
        LiveEditor,
    };

    enum class DungeonCategory {
        Woodfall,
        Snowhead,
        GreatBay,
        StoneTower,
    };

    void rebuild_if_ready();
    void populate_setup_options();
    void populate_live_editor();
    void populate_live_editor_category();
    void populate_bottle_contents_panel();
    void request_live_editor_refresh();
    void build_live_category_tabs();
    void set_live_category(LiveCategory category);
    void update_live_category_tabs();
    void set_mode(Mode new_mode);

    Container *live_category_container = nullptr;
    Container *editor_body_container = nullptr;
    ConfigSubMenu *config_sub_menu = nullptr;
    ConfigSubMenu *bottle_contents_sub_menu = nullptr;
    Rml::Element *host_element = nullptr;
    Mode mode = Mode::None;
    LiveCategory live_category = LiveCategory::Time;
    DungeonCategory dungeon_category = DungeonCategory::Woodfall;
    bool live_editor_refresh_pending = false;
    std::vector<Button *> live_category_buttons;
};

class ElementSaveEditorConfig : public Rml::Element {
public:
    ElementSaveEditorConfig(const Rml::String &tag);
    virtual ~ElementSaveEditorConfig();
private:
    SaveEditorConfigPanel *panel = nullptr;
};

void refresh_save_editor_config();

}

#endif
