/*
 * This file is part of JS80P, a synthesizer plugin.
 * Copyright (C) 2023  Attila M. Magyar
 *
 * JS80P is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JS80P is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef JS80P__GUI__X11_HPP
#define JS80P__GUI__X11_HPP

#include <map>
#include <string>

#include <xcb/xcb.h>
#include <X11/Xlib.h>
#include <cairo/cairo-xcb.h>

#include "js80p.hpp"
#include "serializer.hpp"
#include "synth.hpp"

#include "gui/gui.hpp"


namespace JS80P
{

class Widget;


class XcbPlatform
{
    public:
        static constexpr double NANOSEC_SCALE = 1000000000.0;

        static xcb_window_t gui_platform_widget_to_xcb_window(
            GUI::PlatformWidget platform_widget
        );

        XcbPlatform();
        ~XcbPlatform();

        xcb_connection_t* get_connection();
        int get_fd() const;
        xcb_screen_t* get_screen() const;
        xcb_visualid_t get_screen_root_visual_id() const;
        xcb_visualtype_t* get_screen_root_visual() const;
        cairo_font_face_t* get_font_face(
            cairo_t* cairo,
            WidgetBase::FontWeight const weight
        );

        void register_widget(xcb_window_t window_id, Widget* widget);
        Widget* find_widget(xcb_window_t window_id) const;
        void unregister_widget(xcb_window_t window_id);

        std::string const get_save_file_name();
        std::string const get_open_file_name();

    private:
        class Pipe
        {
            public:
                Pipe();
                Pipe(Pipe const& pipe);
                ~Pipe();

                void close_read_fd();
                void close_write_fd();

                int read_fd;
                int write_fd;
                bool is_usable;
        };

        static char const* const FONT_TEST_STR;
        static char const* const FONTS[];

        static char const* const KDIALOG[];
        static char const* const KDIALOG_SAVE_ARGUMENTS[];
        static char const* const KDIALOG_OPEN_ARGUMENTS[];

        static char const* const ZENITY[];
        static char const* const ZENITY_SAVE_ARGUMENTS[];
        static char const* const ZENITY_OPEN_ARGUMENTS[];

        typedef std::map<xcb_window_t, Widget*> WindowIdToWidgetMap;

        xcb_visualtype_t* find_screen_root_visual() const;
        cairo_font_face_t* find_narrowest_font(
            cairo_t* cairo,
            cairo_font_weight_t weight
        );

        char const* find_executable(char const* const* alternatives) const;
        std::string const run_file_selector(
            char const* executable,
            char const* const* arguments
        );
        void build_argv(
            char const* executable,
            char const* const* arguments,
            std::vector<char*>& argv
        ) const;
        void build_env(std::vector<char*>& env) const;
        void run_file_selector_child_process(
            std::vector<char*> const& argv,
            std::vector<char*> const& env,
            Pipe& pipe
        ) const;
        std::string const read_file_selector_output(int read_fd);
        int wait_or_kill(pid_t const pid) const;

        WindowIdToWidgetMap widgets;
        xcb_connection_t* connection;
        xcb_screen_t* screen;
        xcb_visualtype_t* screen_root_visual;
        cairo_font_face_t* font_face_normal;
        cairo_font_face_t* font_face_bold;
        int xcb_fd;
        bool is_file_selector_open;
};


class Widget : public WidgetBase
{
    public:
        static void process_events(XcbPlatform* xcb);
        static void process_non_editing_events(XcbPlatform* xcb);

        Widget(char const* const text);
        virtual ~Widget();

        virtual GUI::Image load_image(
            GUI::PlatformData platform_data,
            char const* name
        ) override;

        virtual void delete_image(GUI::Image image) override;

        virtual void show() override;
        virtual void hide() override;

        virtual void focus() override;
        virtual void bring_to_top() override;
        virtual void redraw() override;

        virtual GUI::Image set_image(GUI::Image image) override;

    protected:
        Widget(
            char const* const text,
            int const left,
            int const top,
            int const width,
            int const height,
            Type const type
        );
        Widget(
            GUI::PlatformData platform_data,
            GUI::PlatformWidget platform_widget,
            Type const type
        );

        virtual void set_up(
            GUI::PlatformData platform_data,
            WidgetBase* parent
        ) override;

        virtual bool paint() override;

        virtual void fill_rectangle(
            int const left,
            int const top,
            int const width,
            int const height,
            GUI::Color const color
        ) override;

        virtual void draw_text(
            char const* const text,
            int const font_size_px,
            int const left,
            int const top,
            int const width,
            int const height,
            GUI::Color const color,
            GUI::Color const background,
            FontWeight const font_weight = FontWeight::NORMAL,
            int const padding = 0,
            TextAlignment const alignment = TextAlignment::CENTER
        ) override;

        virtual void draw_image(
            GUI::Image image,
            int const left,
            int const top,
            int const width,
            int const height
        ) override;

        virtual GUI::Image copy_image_region(
            GUI::Image source,
            int const left,
            int const top,
            int const width,
            int const height
        ) override;

        XcbPlatform* xcb() const;

    private:
        static constexpr xcb_timestamp_t DOUBLE_CLICK_TIME_DELTA = 500;
        static constexpr int DOUBLE_CLICK_POS_DELTA = 5;
        static constexpr double COLOR_COMPONENT_SCALE = 1.0 / 255.0;
        static constexpr unsigned int TRANSPARENT_WIDGETS = (
            Type::EXPORT_PATCH_BUTTON
            | Type::IMPORT_PATCH_BUTTON
            | Type::PARAM_EDITOR
            | Type::TAB_BODY
            | Type::TAB_SELECTOR
        );

        class Resource;

        typedef std::map<std::string, Resource> StringToImageMap;

        static StringToImageMap const IMAGES;

        class Resource
        {
            public:
                Resource(unsigned char* start, unsigned char* end);

                /*
                These should be unsigned char const* const, but
                cairo_read_func_t wants unsigned char*.
                */
                unsigned char* const start;
                unsigned char* const end;
        };

        class PNGStreamState
        {
            public:
                PNGStreamState(unsigned char* start, unsigned char* end);

                unsigned char* data;
                unsigned char* end;
        };

        static cairo_status_t read_png_stream_from_array(
            void *closure,
            unsigned char* data,
            unsigned int length
        );

        static void handle_error_event(
            XcbPlatform const* xcb,
            xcb_generic_error_t const* error
        );
        static void handle_expose_event(
            XcbPlatform const* xcb,
            xcb_expose_event_t const* event
        );
        static void handle_button_press_event(
            XcbPlatform const* xcb,
            xcb_button_press_event_t const* event
        );
        static void handle_button_release_event(
            XcbPlatform const* xcb,
            xcb_button_release_event_t const* event
        );
        static void handle_enter_notify_event(
            XcbPlatform const* xcb,
            xcb_enter_notify_event_t const* event
        );
        static void handle_motion_notify_event(
            XcbPlatform const* xcb,
            xcb_motion_notify_event_t const* event
        );
        static void handle_leave_notify_event(
            XcbPlatform const* xcb,
            xcb_leave_notify_event_t const* event
        );
        static void handle_client_message_event(
            XcbPlatform const* xcb,
            xcb_client_message_event_t const* event
        );
        static void handle_destroy_notify_event(
            XcbPlatform const* xcb,
            xcb_destroy_notify_event_t const* event
        );

        static bool is_double_click(
            Widget const* widget,
            xcb_timestamp_t const time,
            int const x,
            int const y
        );
        static bool is_modifier_active(uint16_t event_state);

        void initialize();

        xcb_connection_t* xcb_connection() const;
        xcb_window_t window_id() const;

        void update_fake_transparency();
        cairo_surface_t* find_first_parent_image();
        void destroy_fake_transparent_background();

        cairo_surface_t* cairo_surface;
        cairo_surface_t* fake_transparent_background;
        cairo_surface_t* fake_transparent_background_source;
        cairo_t* cairo;
        WidgetBase* first_parent_with_image;
        int fake_transparent_background_left;
        int fake_transparent_background_top;
        int mouse_down_x;
        int mouse_down_y;
        xcb_timestamp_t mouse_down_time;
        bool need_to_destroy_window;
        bool is_transparent;
        bool is_hidden;
};

}

#include "gui/widgets.hpp"

#endif
