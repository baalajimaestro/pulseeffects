/*
 *  Copyright © 2017-2020 Wellington Wallace
 *
 *  This file is part of PulseEffects.
 *
 *  PulseEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  PulseEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PulseEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef EFFECTS_BASE_UI_HPP
#define EFFECTS_BASE_UI_HPP

#include <giomm/settings.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/listbox.h>
#include <gtkmm/stack.h>
#include <locale>
#include <memory>
#include <vector>
#include "app_info_ui.hpp"
#include "blocklist_settings_ui.hpp"
#include "pipe_manager.hpp"
#include "preset_type.hpp"
#include "spectrum_ui.hpp"
#include "util.hpp"

class EffectsBaseUi {
 public:
  EffectsBaseUi(const Glib::RefPtr<Gtk::Builder>& builder,
                Glib::RefPtr<Gio::Settings> refSettings,
                PipeManager* pipe_manager);
  EffectsBaseUi(const EffectsBaseUi&) = delete;
  auto operator=(const EffectsBaseUi&) -> EffectsBaseUi& = delete;
  EffectsBaseUi(const EffectsBaseUi&&) = delete;
  auto operator=(const EffectsBaseUi&&) -> EffectsBaseUi& = delete;
  virtual ~EffectsBaseUi();

  void on_app_changed(const NodeInfo& node_info);
  void on_app_removed(const NodeInfo& node_info);
  void on_new_output_level_db(const std::array<double, 2>& peak);

 protected:
  Glib::RefPtr<Gio::Settings> settings;
  Gtk::ListBox* listbox = nullptr;
  Gtk::Stack* stack = nullptr;

  Gtk::Box *apps_box = nullptr, *app_button_row = nullptr, *global_level_meter_grid = nullptr;
  Gtk::Image *app_input_icon = nullptr, *app_output_icon = nullptr, *saturation_icon = nullptr;
  Gtk::Label *global_output_level_left = nullptr, *global_output_level_right = nullptr;

  PipeManager* pm = nullptr;

  std::vector<AppInfoUi*> apps_list;
  std::vector<sigc::connection> connections;

  SpectrumUi* spectrum_ui = nullptr;

  template <typename T>
  void add_to_listbox(T p) {
    auto* row = Gtk::manage(new Gtk::ListBoxRow());
    auto* eventBox = Gtk::manage(new Gtk::EventBox());

    eventBox->add(*p->listbox_control);

    row->add(*eventBox);
    row->set_name(p->name);
    row->set_margin_bottom(6);
    row->set_margin_right(6);
    row->set_margin_left(6);

    std::vector<Gtk::TargetEntry> listTargets;

    auto entry = Gtk::TargetEntry("Gtk::ListBoxRow", Gtk::TARGET_SAME_APP, 0);

    listTargets.emplace_back(entry);

    eventBox->drag_source_set(listTargets, Gdk::MODIFIER_MASK, Gdk::ACTION_MOVE);

    eventBox->drag_dest_set(listTargets, Gtk::DEST_DEFAULT_ALL, Gdk::ACTION_MOVE);

    eventBox->signal_drag_data_get().connect(
        [=](const Glib::RefPtr<Gdk::DragContext>& context, Gtk::SelectionData& selection_data, guint info, guint time) {
          selection_data.set(selection_data.get_target(), p->name);
        });

    eventBox->signal_drag_data_received().connect([=](const Glib::RefPtr<Gdk::DragContext>& context, int x, int y,
                                                      const Gtk::SelectionData& selection_data, guint info,
                                                      guint time) {
      const int length = selection_data.get_length();

      if ((length >= 0) && (selection_data.get_format() == 8)) {
        auto src = selection_data.get_data_as_string();
        auto dst = p->name;

        if (src != dst) {
          auto order = Glib::Variant<std::vector<std::string>>();

          settings->get_value("plugins", order);

          auto vorder = order.get();

          auto r1 = std::find(std::begin(vorder), std::end(vorder), src);

          if (r1 != std::end(vorder)) {
            // for (auto v : vorder) {
            //   std::cout << v << std::endl;
            // }

            vorder.erase(r1);

            auto r2 = std::find(std::begin(vorder), std::end(vorder), dst);

            vorder.insert(r2, src);

            settings->set_string_array("plugins", vorder);

            // std::cout << "" << std::endl;

            // for (auto v : vorder) {
            //   std::cout << v << std::endl;
            // }
          }
        }
      }

      context->drag_finish(false, false, time);
    });

    eventBox->signal_drag_begin().connect([=](const Glib::RefPtr<Gdk::DragContext>& context) {
      auto w = row->get_allocated_width();
      auto h = row->get_allocated_height();

      auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);

      auto ctx = Cairo::Context::create(surface);

      auto styleContext = row->get_style_context();

      styleContext->add_class("drag-listboxrow-icon");

      gtk_widget_draw(GTK_WIDGET(row->gobj()), ctx->cobj());

      styleContext->remove_class("drag-listboxrow-icon");

      context->set_icon(surface);
    });

    listbox->add(*row);
  }

 private:
  Gtk::Box* placeholder_spectrum = nullptr;

  std::locale global_locale;

  auto on_listbox_sort(Gtk::ListBoxRow* row1, Gtk::ListBoxRow* row2) -> int;

  template <typename T>
  auto level_to_localized_string_showpos(const T& value, const int& places) -> std::string {
    std::ostringstream msg;

    msg.imbue(global_locale);
    msg.precision(places);

    msg << ((value > 0.0) ? "+" : "") << std::fixed << value;

    return msg.str();
  }
};

#endif
