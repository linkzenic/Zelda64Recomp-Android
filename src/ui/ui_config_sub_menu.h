#ifndef RECOMPUI_CONFIG_SUB_MENU_H
#define RECOMPUI_CONFIG_SUB_MENU_H

#include <span>

#include "elements/ui_button.h"
#include "elements/ui_container.h"
#include "elements/ui_label.h"
#include "elements/ui_radio.h"
#include "elements/ui_scroll_container.h"
#include "elements/ui_slider.h"
#include "elements/ui_text_input.h"

namespace recompui {

class ConfigOptionElement : public Element {
protected:
    Label *name_label = nullptr;
    std::string option_id;
    std::string name;
    std::string description;
    std::function<void(ConfigOptionElement *, bool)> hover_callback = nullptr;
    std::function<void(const std::string &, bool)> focus_callback = nullptr;

    virtual void process_event(const Event &e) override;
    std::string_view get_type_name() override { return "ConfigOptionElement"; }
public:
    ConfigOptionElement(Element *parent);
    virtual ~ConfigOptionElement();
    void set_option_id(std::string_view id);
    void set_name(std::string_view name);
    void set_description(std::string_view description);
    void set_hover_callback(std::function<void(ConfigOptionElement *, bool)> callback);
    void set_focus_callback(std::function<void(const std::string &, bool)> callback);
    virtual void set_large_touch_style(bool enabled);
    virtual void set_option_enabled(bool enabled);
    const std::string &get_description() const;
    void set_nav_auto(NavDirection dir) override { get_focus_element()->set_nav_auto(dir); }
    void set_nav_none(NavDirection dir) override { get_focus_element()->set_nav_none(dir); }
    void set_nav(NavDirection dir, Element* element) override { get_focus_element()->set_nav(dir, element); }
    void set_nav_manual(NavDirection dir, const std::string& target) override { get_focus_element()->set_nav_manual(dir, target); }
    virtual Element* get_focus_element() { return this; }
};

class ConfigOptionSlider : public ConfigOptionElement {
protected:
    Slider *slider = nullptr;
    std::function<void(const std::string &, double)> callback;

    void slider_value_changed(double v);
    std::string_view get_type_name() override { return "ConfigOptionSlider"; }
public:
    ConfigOptionSlider(Element *parent, double value, double min_value, double max_value, double step_value, bool percent, std::function<void(const std::string &, double)> callback);
    Element* get_focus_element() override { return slider; }
    void set_large_touch_style(bool enabled) override;
};

class ConfigOptionTextInput : public ConfigOptionElement {
protected:
    TextInput *text_input = nullptr;
    std::function<void(const std::string &, const std::string &)> callback;

    void text_changed(const std::string &text);
    std::string_view get_type_name() override { return "ConfigOptionTextInput"; }
public:
    ConfigOptionTextInput(Element *parent, std::string_view value, std::function<void(const std::string &, const std::string &)> callback);
    Element* get_focus_element() override { return text_input; }
    void set_numeric_constraint(size_t max_length);
    void set_large_touch_style(bool enabled) override;
};

class ConfigOptionRadio : public ConfigOptionElement {
protected:
    Radio *radio = nullptr;
    std::function<void(const std::string &, uint32_t)> callback;

    void index_changed(uint32_t index);
    std::string_view get_type_name() override { return "ConfigOptionRadio"; }
public:
    ConfigOptionRadio(Element *parent, uint32_t value, const std::vector<std::string> &options, std::function<void(const std::string &, uint32_t)> callback);
    Element* get_focus_element() override { return radio; }    
    void set_large_touch_style(bool enabled) override;
    void set_option_enabled(bool enabled) override;
};

class ConfigOptionMultiToggle : public ConfigOptionElement {
protected:
    Container *row = nullptr;
    std::vector<RadioOption *> options;
    std::vector<bool> values;
    std::function<void(const std::string &, uint32_t, bool)> callback;

    void option_pressed(uint32_t index);
    void sync_option(uint32_t index);
    std::string_view get_type_name() override { return "ConfigOptionMultiToggle"; }
public:
    ConfigOptionMultiToggle(Element *parent, const std::vector<std::string> &options, const std::vector<bool> &values,
                            std::function<void(const std::string &, uint32_t, bool)> callback);
    Element* get_focus_element() override {
        return options.empty() ? static_cast<Element*>(row) : static_cast<Element*>(options.front());
    }
    void set_vertical_nav(NavDirection dir, Element* element);
    void set_large_touch_style(bool enabled) override;
    void set_option_enabled(bool enabled) override;
};

class ConfigOptionToggle : public ConfigOptionElement {
protected:
    Button *button = nullptr;
    bool value = false;
    std::function<void(const std::string &, bool)> callback;

    void toggle_value();
    void sync_button_text();
    std::string_view get_type_name() override { return "ConfigOptionToggle"; }
public:
    ConfigOptionToggle(Element *parent, bool value, std::function<void(const std::string &, bool)> callback);
    Element* get_focus_element() override { return button; }
    void set_large_touch_style(bool enabled) override;
};

class ConfigOptionButton : public ConfigOptionElement {
protected:
    Button *button = nullptr;
    std::function<void(const std::string &)> callback;

    void button_pressed();
    std::string_view get_type_name() override { return "ConfigOptionButton"; }
public:
    ConfigOptionButton(Element *parent, std::string_view button_text, std::function<void(const std::string &)> callback);
    Element* get_focus_element() override { return button; }
    void set_large_touch_style(bool enabled) override;
};

class ConfigSubMenu : public Element {
private:
    Container *header_container = nullptr;
    Button *back_button = nullptr;
    Label *title_label = nullptr;
    Container *body_container = nullptr;
    Label *description_label = nullptr;
    Container *config_container = nullptr;
    ScrollContainer *config_scroll_container = nullptr;
    std::vector<ConfigOptionElement *> config_option_elements;
    ConfigOptionElement * description_option_element = nullptr;
    bool back_button_visible = true;
    bool large_touch_style = false;

    void back_button_pressed();
    void set_description_option_element(ConfigOptionElement *option, bool active);
    ConfigOptionElement *add_option(ConfigOptionElement *option, std::string_view id, std::string_view name, std::string_view description);
protected:
    std::string_view get_type_name() override { return "ConfigSubMenu"; }
public:
    ConfigSubMenu(Element *parent);
    virtual ~ConfigSubMenu();
    void enter(std::string_view title);
    void clear_options();
    void set_header_visible(bool visible);
    void set_back_button_visible(bool visible);
    void set_description_visible(bool visible);
    void set_large_touch_style(bool enabled);
    ConfigOptionElement *add_slider_option(std::string_view id, std::string_view name, std::string_view description, double value, double min, double max, double step, bool percent, std::function<void(const std::string &, double)> callback);
    ConfigOptionElement *add_text_option(std::string_view id, std::string_view name, std::string_view description, std::string_view value, std::function<void(const std::string &, const std::string &)> callback);
    ConfigOptionElement *add_numeric_text_option(std::string_view id, std::string_view name, std::string_view description, std::string_view value, size_t max_length, std::function<void(const std::string &, const std::string &)> callback);
    ConfigOptionElement *add_radio_option(std::string_view id, std::string_view name, std::string_view description, uint32_t value, const std::vector<std::string> &options, std::function<void(const std::string &, uint32_t)> callback);
    ConfigOptionElement *add_multi_toggle_option(std::string_view id, std::string_view name, std::string_view description, const std::vector<std::string> &options, const std::vector<bool> &values, std::function<void(const std::string &, uint32_t, bool)> callback);
    ConfigOptionElement *add_toggle_option(std::string_view id, std::string_view name, std::string_view description, bool value, std::function<void(const std::string &, bool)> callback);
    ConfigOptionElement *add_button_option(std::string_view id, std::string_view name, std::string_view description, std::string_view button_text, std::function<void(const std::string &)> callback);
    void add_section_header(std::string_view name);
};

class ElementConfigSubMenu : public Rml::Element {
public:
    ElementConfigSubMenu(const Rml::String &tag);
    virtual ~ElementConfigSubMenu();
    void set_display(bool display);
    ConfigSubMenu *get_config_sub_menu_element() const;
private:
    ConfigSubMenu *config_sub_menu;
};

}
#endif
