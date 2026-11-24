#include "ui_config_sub_menu.h"

#include <cassert>
#include <string_view>

#include "recomp_ui.h"

namespace recompui {

// ConfigOptionElement


void ConfigOptionElement::process_event(const Event &e) {
    switch (e.type) {
    case EventType::Hover:
        if (hover_callback == nullptr) {
            break;
        }
        hover_callback(this, std::get<EventHover>(e.variant).active);
        break;
    case EventType::Update:
        break;
    default:
        assert(false && "Unknown event type.");
        break;
    }
}

ConfigOptionElement::ConfigOptionElement(Element *parent) : Element(parent, Events(EventType::Hover)) {
    set_attribute("class", "config-sub-menu-option");
    set_display(Display::Flex);
    set_flex_direction(FlexDirection::Column);
    set_gap(16.0f);
    set_height(100.0f);

    name_label = get_current_context().create_element<Label>(this, LabelStyle::Normal);
}

ConfigOptionElement::~ConfigOptionElement() {

}

void ConfigOptionElement::set_option_id(std::string_view id) {
    this->option_id = id;
}

void ConfigOptionElement::set_name(std::string_view name) {
    this->name = name;
    name_label->set_text(name);
}

void ConfigOptionElement::set_description(std::string_view description) {
    this->description = description;
}

void ConfigOptionElement::set_hover_callback(std::function<void(ConfigOptionElement *, bool)> callback) {
    hover_callback = callback;
}

void ConfigOptionElement::set_focus_callback(std::function<void(const std::string &, bool)> callback) {
    focus_callback = callback;
}

void ConfigOptionElement::set_large_touch_style(bool enabled) {
    if (!enabled) {
        return;
    }

    set_width(100.0f, Unit::Percent);
    set_height(128.0f);
    set_gap(20.0f);
    name_label->set_font_size(34.0f);
    name_label->set_letter_spacing(3.08f);
    name_label->set_line_height(38.0f);
    name_label->set_font_weight(700);
}

const std::string &ConfigOptionElement::get_description() const {
    return description;
}

void ConfigOptionElement::set_option_enabled(bool enabled) {
    set_enabled(enabled);
    set_opacity(enabled ? 1.0f : 0.42f);
    name_label->set_opacity(enabled ? 1.0f : 0.42f);
    name_label->set_color(enabled ? Color{ 242, 242, 242, 255 } : Color{ 160, 160, 160, 150 });
    if (Element *focus_element = get_focus_element()) {
        focus_element->set_enabled(enabled);
        focus_element->set_opacity(enabled ? 1.0f : 0.42f);
    }
}

// ConfigOptionSlider

void ConfigOptionSlider::slider_value_changed(double v) {
    callback(option_id, v);
}

ConfigOptionSlider::ConfigOptionSlider(Element *parent, double value, double min_value, double max_value, double step_value, bool percent, std::function<void(const std::string &, double)> callback) : ConfigOptionElement(parent) {
    this->callback = callback;

    slider = get_current_context().create_element<Slider>(this, percent ? SliderType::Percent : SliderType::Double);
    slider->set_max_width(380.0f);
    slider->set_min_value(min_value);
    slider->set_max_value(max_value);
    slider->set_step_value(step_value);
    slider->set_value(value);
    slider->add_value_changed_callback([this](double v){ slider_value_changed(v); });
    slider->set_focus_callback([this](bool active) {
        focus_callback(option_id, active);
    });
}

void ConfigOptionSlider::set_large_touch_style(bool enabled) {
    ConfigOptionElement::set_large_touch_style(enabled);
    if (!enabled) {
        return;
    }

    slider->set_max_width(560.0f);
    slider->set_large_touch_style();
}

// ConfigOptionTextInput

void ConfigOptionTextInput::text_changed(const std::string &text) {
    callback(option_id, text);
}

ConfigOptionTextInput::ConfigOptionTextInput(Element *parent, std::string_view value, std::function<void(const std::string &, const std::string &)> callback) : ConfigOptionElement(parent) {
    this->callback = callback;

    text_input = get_current_context().create_element<TextInput>(this);
    text_input->set_max_width(400.0f);
    text_input->set_text(value);
    text_input->add_text_changed_callback([this](const std::string &text){ text_changed(text); });
    text_input->set_focus_callback([this](bool active) {
        focus_callback(option_id, active);
    });
}

void ConfigOptionTextInput::set_numeric_constraint(size_t max_length) {
    text_input->set_numeric_only(true);
    text_input->set_max_length(max_length);
}

void ConfigOptionTextInput::set_large_touch_style(bool enabled) {
    ConfigOptionElement::set_large_touch_style(enabled);
    if (!enabled) {
        return;
    }

    text_input->set_max_width(520.0f);
    text_input->set_min_height(72.0f);
    text_input->set_font_size(28.0f);
    text_input->set_letter_spacing(2.52f);
    text_input->set_line_height(32.0f);
    text_input->set_padding_top(18.0f);
    text_input->set_padding_right(22.0f);
    text_input->set_padding_bottom(18.0f);
    text_input->set_padding_left(22.0f);
    text_input->set_border_width(1.1f);
    text_input->set_border_radius(16.0f);
}

// ConfigOptionRadio

void ConfigOptionRadio::index_changed(uint32_t index) {
    callback(option_id, index);
}

ConfigOptionRadio::ConfigOptionRadio(Element *parent, uint32_t value, const std::vector<std::string> &options, std::function<void(const std::string &, uint32_t)> callback) : ConfigOptionElement(parent) {
    this->callback = callback;

    radio = get_current_context().create_element<Radio>(this);
    radio->set_focus_callback([this](bool active) {
        focus_callback(option_id, active);
    });
    radio->add_index_changed_callback([this](uint32_t index){ index_changed(index); });
    for (std::string_view option : options) {
        radio->add_option(option);
    }

    if (value < options.size()) {
        radio->set_index(value);
    }
}

void ConfigOptionRadio::set_large_touch_style(bool enabled) {
    ConfigOptionElement::set_large_touch_style(enabled);
    if (!enabled) {
        return;
    }

    set_height(148.0f);
    radio->set_large_touch_style();
}

void ConfigOptionRadio::set_option_enabled(bool enabled) {
    ConfigOptionElement::set_option_enabled(enabled);
    radio->set_enabled(enabled);
    for (size_t index = 0; index < radio->num_options(); index++) {
        RadioOption* option = radio->get_option_element(index);
        option->set_enabled(enabled);
        option->set_opacity(enabled ? 1.0f : 0.38f);
        option->set_color(enabled ? Color{ 242, 242, 242, 255 } : Color{ 160, 160, 160, 145 });
    }
}

// ConfigOptionMultiToggle

void ConfigOptionMultiToggle::sync_option(uint32_t index) {
    if (index < options.size() && index < values.size()) {
        options[index]->set_selected_state(values[index]);
    }
}

void ConfigOptionMultiToggle::option_pressed(uint32_t index) {
    if (index >= values.size()) {
        return;
    }

    values[index] = !values[index];
    sync_option(index);
    callback(option_id, index, values[index]);
}

ConfigOptionMultiToggle::ConfigOptionMultiToggle(
    Element *parent, const std::vector<std::string> &option_names, const std::vector<bool> &option_values,
    std::function<void(const std::string &, uint32_t, bool)> callback) : ConfigOptionElement(parent) {
    this->callback = callback;
    values = option_values;
    values.resize(option_names.size(), false);

    row = get_current_context().create_element<Container>(this, FlexDirection::Row, JustifyContent::FlexStart);
    row->set_gap(14.0f);
    row->set_align_items(AlignItems::Center);

    for (std::string_view name : option_names) {
        uint32_t index = static_cast<uint32_t>(options.size());
        RadioOption *option = get_current_context().create_element<RadioOption>(row, name, index);
        option->set_pressed_callback([this](uint32_t index) { option_pressed(index); });
        option->set_focus_callback([this](bool active) {
            if (focus_callback != nullptr) {
                focus_callback(option_id, active);
            }
        });
        options.emplace_back(option);
        sync_option(index);

        if (options.size() > 1) {
            options[options.size() - 2]->set_nav(NavDirection::Right, options[options.size() - 1]);
            options[options.size() - 1]->set_nav(NavDirection::Left, options[options.size() - 2]);
        }
    }
}

void ConfigOptionMultiToggle::set_large_touch_style(bool enabled) {
    ConfigOptionElement::set_large_touch_style(enabled);
    if (!enabled) {
        return;
    }

    set_height(148.0f);
    row->set_gap(14.0f);
    row->set_align_items(AlignItems::Center);
    for (RadioOption* option : options) {
        option->set_large_touch_style();
    }
}

void ConfigOptionMultiToggle::set_vertical_nav(NavDirection dir, Element* element) {
    for (RadioOption* option : options) {
        option->set_nav(dir, element);
    }
}

void ConfigOptionMultiToggle::set_option_enabled(bool enabled) {
    ConfigOptionElement::set_option_enabled(enabled);
    row->set_enabled(enabled);
    for (size_t index = 0; index < options.size(); index++) {
        RadioOption* option = options[index];
        option->set_enabled(enabled);
        option->set_opacity(enabled ? 1.0f : 0.38f);
        option->set_color(enabled ? Color{ 242, 242, 242, 255 } : Color{ 160, 160, 160, 145 });
    }
}

// ConfigOptionToggle

void ConfigOptionToggle::sync_button_text() {
    button->set_text(value ? "On" : "Off");
}

void ConfigOptionToggle::toggle_value() {
    value = !value;
    sync_button_text();
    callback(option_id, value);
}

ConfigOptionToggle::ConfigOptionToggle(Element *parent, bool value, std::function<void(const std::string &, bool)> callback) : ConfigOptionElement(parent) {
    this->value = value;
    this->callback = callback;

    button = get_current_context().create_element<Button>(this, value ? "On" : "Off", ButtonStyle::Primary);
    button->set_min_width(220.0f);
    button->set_max_width(280.0f);
    button->set_padding_top(18.0f);
    button->set_padding_bottom(18.0f);
    button->add_pressed_callback([this]() { toggle_value(); });
}

void ConfigOptionToggle::set_large_touch_style(bool enabled) {
    ConfigOptionElement::set_large_touch_style(enabled);
    if (!enabled) {
        return;
    }

    button->set_min_width(320.0f);
    button->set_max_width(420.0f);
    button->set_min_height(78.0f);
    button->set_padding_top(22.0f);
    button->set_padding_bottom(22.0f);
    button->set_font_size(34.0f);
    button->set_letter_spacing(3.08f);
    button->set_line_height(38.0f);
    button->set_border_radius(18.0f);
}

// ConfigOptionButton

void ConfigOptionButton::button_pressed() {
    callback(option_id);
}

ConfigOptionButton::ConfigOptionButton(Element *parent, std::string_view button_text, std::function<void(const std::string &)> callback) : ConfigOptionElement(parent) {
    this->callback = callback;

    button = get_current_context().create_element<Button>(this, std::string(button_text), ButtonStyle::Primary);
    button->set_min_width(260.0f);
    button->set_max_width(460.0f);
    button->set_padding_top(18.0f);
    button->set_padding_bottom(18.0f);
    button->add_pressed_callback([this]() { button_pressed(); });
}

void ConfigOptionButton::set_large_touch_style(bool enabled) {
    ConfigOptionElement::set_large_touch_style(enabled);
    if (!enabled) {
        return;
    }

    button->set_min_width(360.0f);
    button->set_max_width(560.0f);
    button->set_min_height(78.0f);
    button->set_padding_top(22.0f);
    button->set_padding_bottom(22.0f);
    button->set_padding_left(26.0f);
    button->set_padding_right(26.0f);
    button->set_font_size(34.0f);
    button->set_letter_spacing(3.08f);
    button->set_line_height(38.0f);
    button->set_border_radius(18.0f);
}

// ConfigSubMenu

void ConfigSubMenu::back_button_pressed() {
    // Hide the config sub menu and show the config menu.
    ContextId config_context = recompui::get_config_context_id();
    ContextId sub_menu_context = recompui::get_config_sub_menu_context_id();

    recompui::hide_context(sub_menu_context);
    recompui::show_context(config_context, "");
    recompui::focus_mod_configure_button();
}

void ConfigSubMenu::set_description_option_element(ConfigOptionElement *option, bool active) {
    if (active) {
        description_option_element = option;
    }
    else if (description_option_element == option) {
        description_option_element = nullptr;
    }

    if (description_option_element == nullptr) {
        description_label->set_text("");
    }
    else {
        description_label->set_text(description_option_element->get_description());
    }
}

ConfigSubMenu::ConfigSubMenu(Element *parent) : Element(parent) {
    using namespace std::string_view_literals;

    set_attribute("class", "config-sub-menu");
    set_display(Display::Flex);
    set_flex(1, 1, 100.0f, Unit::Percent);
    set_flex_direction(FlexDirection::Column);
    set_height(100.0f, Unit::Percent);

    recompui::ContextId context = get_current_context();
    header_container = context.create_element<Container>(this, FlexDirection::Row, JustifyContent::FlexStart);
    header_container->set_flex_grow(0.0f);
    header_container->set_align_items(AlignItems::Center);
    header_container->set_padding(12.0f);
    header_container->set_gap(24.0f);

    {
        back_button = context.create_element<Button>(header_container, "Back", ButtonStyle::Secondary);
        back_button->add_pressed_callback([this](){ back_button_pressed(); });
        title_label = context.create_element<Label>(header_container, "Title", LabelStyle::Large);
    }

    body_container = context.create_element<Container>(this, FlexDirection::Row, JustifyContent::SpaceEvenly);
    body_container->set_padding(32.0f);
    {
        config_container = context.create_element<Container>(body_container, FlexDirection::Column, JustifyContent::Center);
        config_container->set_display(Display::Block);
        config_container->set_flex_basis(100.0f);
        config_container->set_align_items(AlignItems::Center);
        {
            config_scroll_container = context.create_element<ScrollContainer>(config_container, ScrollDirection::Vertical);
        }

        description_label = context.create_element<Label>(body_container, "", LabelStyle::Small);
        description_label->set_min_width(800.0f);
        description_label->set_padding_left(16.0f);
        description_label->set_padding_right(16.0f);
    }

    recompui::get_current_context().set_autofocus_element(back_button);
}

ConfigSubMenu::~ConfigSubMenu() {

}

void ConfigSubMenu::enter(std::string_view title) {
    title_label->set_text(title);
}

void ConfigSubMenu::clear_options() {
    config_scroll_container->clear_children();
    config_option_elements.clear();
    description_option_element = nullptr;
    description_label->set_text("");
}

void ConfigSubMenu::set_header_visible(bool visible) {
    header_container->set_display(visible ? Display::Flex : Display::None);
}

void ConfigSubMenu::set_back_button_visible(bool visible) {
    back_button_visible = visible;
    back_button->set_display(visible ? Display::Block : Display::None);
}

void ConfigSubMenu::set_description_visible(bool visible) {
    description_label->set_display(visible ? Display::Block : Display::None);
    body_container->set_justify_content(visible ? JustifyContent::SpaceEvenly : JustifyContent::FlexStart);
    config_container->set_align_items(visible ? AlignItems::Center : AlignItems::Stretch);
    config_container->set_flex(visible ? 0.0f : 1.0f, 1.0f, visible ? 100.0f : 100.0f, visible ? Unit::Dp : Unit::Percent);
    config_container->set_width(visible ? 100.0f : 100.0f, visible ? Unit::Dp : Unit::Percent);
}

void ConfigSubMenu::set_large_touch_style(bool enabled) {
    large_touch_style = enabled;
    for (ConfigOptionElement *option : config_option_elements) {
        option->set_large_touch_style(enabled);
    }
}

ConfigOptionElement *ConfigSubMenu::add_option(ConfigOptionElement *option, std::string_view id, std::string_view name, std::string_view description) {
    option->set_option_id(id);
    option->set_name(name);
    option->set_description(description);
    option->set_large_touch_style(large_touch_style);
    option->set_hover_callback([this](ConfigOptionElement *option, bool active){ set_description_option_element(option, active); });
    option->set_focus_callback([this, option](const std::string &id, bool active) { set_description_option_element(option, active); });
    if (config_option_elements.empty()) {
        if (back_button_visible) {
            back_button->set_nav(NavDirection::Down, option->get_focus_element());
            option->set_nav(NavDirection::Up, back_button);
        }
        else {
            option->set_nav_auto(NavDirection::Up);
        }
    }
    else {
        config_option_elements.back()->set_nav(NavDirection::Down, option->get_focus_element());
        option->set_nav(NavDirection::Up, config_option_elements.back()->get_focus_element());
        if (auto *previous_multi_toggle = dynamic_cast<ConfigOptionMultiToggle *>(config_option_elements.back())) {
            previous_multi_toggle->set_vertical_nav(NavDirection::Down, option->get_focus_element());
        }
    }

    config_option_elements.emplace_back(option);
    if (auto *multi_toggle = dynamic_cast<ConfigOptionMultiToggle *>(option)) {
        multi_toggle->set_vertical_nav(NavDirection::Up,
                                       config_option_elements.size() == 1
                                           ? static_cast<Element *>(back_button_visible ? back_button : option->get_focus_element())
                                           : config_option_elements[config_option_elements.size() - 2]->get_focus_element());
    }
    return option;
}

ConfigOptionElement *ConfigSubMenu::add_slider_option(std::string_view id, std::string_view name, std::string_view description, double value, double min, double max, double step, bool percent, std::function<void(const std::string &, double)> callback) {
    ConfigOptionSlider *option_slider = get_current_context().create_element<ConfigOptionSlider>(config_scroll_container, value, min, max, step, percent, callback);
    return add_option(option_slider, id, name, description);
}

ConfigOptionElement *ConfigSubMenu::add_text_option(std::string_view id, std::string_view name, std::string_view description, std::string_view value, std::function<void(const std::string &, const std::string &)> callback) {
    ConfigOptionTextInput *option_text_input = get_current_context().create_element<ConfigOptionTextInput>(config_scroll_container, value, callback);
    return add_option(option_text_input, id, name, description);
}

ConfigOptionElement *ConfigSubMenu::add_numeric_text_option(std::string_view id, std::string_view name, std::string_view description, std::string_view value, size_t max_length, std::function<void(const std::string &, const std::string &)> callback) {
    ConfigOptionTextInput *option_text_input = get_current_context().create_element<ConfigOptionTextInput>(config_scroll_container, value, callback);
    option_text_input->set_numeric_constraint(max_length);
    return add_option(option_text_input, id, name, description);
}

ConfigOptionElement *ConfigSubMenu::add_radio_option(std::string_view id, std::string_view name, std::string_view description, uint32_t value, const std::vector<std::string> &options, std::function<void(const std::string &, uint32_t)> callback) {
    ConfigOptionRadio *option_radio = get_current_context().create_element<ConfigOptionRadio>(config_scroll_container, value, options, callback);
    return add_option(option_radio, id, name, description);
}

ConfigOptionElement *ConfigSubMenu::add_multi_toggle_option(std::string_view id, std::string_view name, std::string_view description, const std::vector<std::string> &options, const std::vector<bool> &values, std::function<void(const std::string &, uint32_t, bool)> callback) {
    ConfigOptionMultiToggle *option_multi_toggle = get_current_context().create_element<ConfigOptionMultiToggle>(config_scroll_container, options, values, callback);
    return add_option(option_multi_toggle, id, name, description);
}

ConfigOptionElement *ConfigSubMenu::add_toggle_option(std::string_view id, std::string_view name, std::string_view description, bool value, std::function<void(const std::string &, bool)> callback) {
    ConfigOptionToggle *option_toggle = get_current_context().create_element<ConfigOptionToggle>(config_scroll_container, value, callback);
    return add_option(option_toggle, id, name, description);
}

ConfigOptionElement *ConfigSubMenu::add_button_option(std::string_view id, std::string_view name, std::string_view description, std::string_view button_text, std::function<void(const std::string &)> callback) {
    ConfigOptionButton *option_button = get_current_context().create_element<ConfigOptionButton>(config_scroll_container, button_text, callback);
    return add_option(option_button, id, name, description);
}

void ConfigSubMenu::add_section_header(std::string_view name) {
    Label *header_label = get_current_context().create_element<Label>(config_scroll_container, LabelStyle::Annotation);
    header_label->set_text(name);
    header_label->set_font_size(32.0f);
    header_label->set_letter_spacing(2.52f);
    header_label->set_line_height(32.0f);
    header_label->set_font_weight(700);
    header_label->set_height(40.0f);
    header_label->set_margin_top(config_option_elements.empty() ? 0.0f : 24.0f);
    header_label->set_margin_bottom(8.0f);
    header_label->set_color(Color{ 255, 255, 255, 255 });
}

// ElementConfigSubMenu

ElementConfigSubMenu::ElementConfigSubMenu(const Rml::String &tag) : Rml::Element(tag) {
    SetProperty(Rml::PropertyId::Display, Rml::Style::Display::Flex);
    SetProperty("width", "100%");
    SetProperty("height", "100%");

    recompui::Element this_compat(this);
    recompui::ContextId context = get_current_context();
    config_sub_menu = context.create_element<ConfigSubMenu>(&this_compat);
}

ElementConfigSubMenu::~ElementConfigSubMenu() {

}

void ElementConfigSubMenu::set_display(bool display) {
    SetProperty(Rml::PropertyId::Display, display ? Rml::Style::Display::Block : Rml::Style::Display::None);
}

ConfigSubMenu *ElementConfigSubMenu::get_config_sub_menu_element() const {
    return config_sub_menu;
}

}
