#!/usr/bin/env python3

import json
import os
import shutil
import subprocess

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gdk, GLib, Gtk

APP_ID = "io.github.danielgrasso.WaylandScrollFactor"
FACTOR_MIN = 0.05
FACTOR_MAX = 5.0
DEFAULT_FACTOR = 1.0
DEBOUNCE_MS = 350
APPLY_TOAST_THROTTLE_MS = 1200
STATUS_TOAST_THROTTLE_MS = 3000

CLI_MAP = {
    "scroll_vertical": "--scroll-vertical",
    "scroll_horizontal": "--scroll-horizontal",
    "pinch_zoom": "--pinch-zoom",
    "pinch_rotate": "--pinch-rotate",
}


class WsfWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app)
        self.set_title("Wayland Scroll Factor")
        self.set_default_size(560, 640)

        self._cli_path = self._find_wsf()
        self._debounce_ids = {}
        self._loading = True
        self._last_doctor_output = ""

        self._toast_overlay = Adw.ToastOverlay()
        self.set_content(self._toast_overlay)
        self._last_apply_toast_at = 0
        self._last_status_toast_at = 0

        toolbar_view = Adw.ToolbarView()
        self._toast_overlay.set_child(toolbar_view)

        header = Adw.HeaderBar()
        header.set_show_end_title_buttons(True)
        header.set_title_widget(Adw.WindowTitle(title="Wayland Scroll Factor"))
        toolbar_view.add_top_bar(header)

        if not self._cli_path:
            status = Adw.StatusPage()
            status.set_title("wsf not found")
            status.set_description(
                "Install the CLI and ensure it is in PATH (e.g. ~/.local/bin)."
            )
            toolbar_view.set_content(status)
            self._loading = False
            return

        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        content.set_margin_top(24)
        content.set_margin_bottom(24)
        content.set_margin_start(24)
        content.set_margin_end(24)
        content.set_hexpand(True)
        content.set_halign(Gtk.Align.FILL)
        toolbar_view.set_content(content)
        self.set_size_request(360, 0)

        self._scroll_group = Adw.PreferencesGroup(title="Scroll sensitivity")
        content.append(self._scroll_group)

        self._sliders = {}
        self._label_group = Gtk.SizeGroup(mode=Gtk.SizeGroupMode.HORIZONTAL)
        self._hint_group = Gtk.SizeGroup(mode=Gtk.SizeGroupMode.HORIZONTAL)
        self._add_slider_row(
            self._scroll_group,
            "Vertical scroll",
            "scroll_vertical",
            "↕",
        )
        self._add_slider_row(
            self._scroll_group,
            "Horizontal scroll",
            "scroll_horizontal",
            "↔",
        )
        self._add_slider_row(
            self._scroll_group,
            "Pinch zoom",
            "pinch_zoom",
            "⤢",
        )
        self._add_slider_row(
            self._scroll_group,
            "Pinch rotate",
            "pinch_rotate",
            "↻",
        )

        self._system_group = Adw.PreferencesGroup(title="System integration")
        content.append(self._system_group)

        enabled_row = Adw.ActionRow(title="Enabled")
        enabled_row.set_subtitle("Applies on next login")
        self._enable_switch = Gtk.Switch()
        self._enable_switch.set_valign(Gtk.Align.CENTER)
        enabled_row.add_suffix(self._enable_switch)
        enabled_row.set_activatable_widget(self._enable_switch)
        self._system_group.add(enabled_row)
        self._enable_switch.connect("notify::active", self._on_enabled_toggled)

        self._diagnostics_group = Adw.PreferencesGroup(title="Diagnostics")
        content.append(self._diagnostics_group)

        actions_row = Adw.ActionRow()
        actions_row.set_subtitle("Collect runtime information")

        run_button = Gtk.Button(label="Run doctor")
        run_button.add_css_class("suggested-action")
        run_button.connect("clicked", self._on_run_doctor)

        copy_button = Gtk.Button(label="Copy diagnostics")
        copy_button.add_css_class("flat")
        copy_button.connect("clicked", self._on_copy_diagnostics)

        actions_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        actions_box.append(run_button)
        actions_box.append(copy_button)
        actions_row.add_suffix(actions_box)
        self._diagnostics_group.add(actions_row)

        self._diagnostics_buffer = Gtk.TextBuffer()
        diagnostics_view = Gtk.TextView(buffer=self._diagnostics_buffer)
        diagnostics_view.set_editable(False)
        diagnostics_view.set_cursor_visible(False)
        diagnostics_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        diagnostics_view.set_monospace(True)

        diagnostics_scroller = Gtk.ScrolledWindow()
        diagnostics_scroller.set_min_content_height(160)
        diagnostics_scroller.set_hexpand(True)
        diagnostics_scroller.set_child(diagnostics_view)
        self._diagnostics_group.add(diagnostics_scroller)

        self._loading = False
        self._refresh_all()

    def _add_slider_row(self, group, title, key, hint=None):
        row = Adw.PreferencesRow()

        row_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        row_box.set_hexpand(True)
        row_box.set_halign(Gtk.Align.FILL)
        row_box.set_margin_start(12)
        row_box.set_margin_end(12)
        row_box.set_margin_top(6)
        row_box.set_margin_bottom(6)

        label_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=6)
        label_box.set_hexpand(False)

        if hint:
            hint_label = Gtk.Label(label=hint)
            hint_label.set_xalign(0)
            hint_label.set_valign(Gtk.Align.CENTER)
            hint_label.add_css_class("dim-label")
            hint_label.set_width_chars(2)
            hint_label.set_halign(Gtk.Align.START)
            self._hint_group.add_widget(hint_label)
            label_box.append(hint_label)

        label = Gtk.Label(label=title)
        label.set_xalign(0)
        label.set_valign(Gtk.Align.CENTER)
        label.add_css_class("title")
        label_box.append(label)
        self._label_group.add_widget(label_box)

        adjustment = Gtk.Adjustment(
            value=DEFAULT_FACTOR,
            lower=FACTOR_MIN,
            upper=FACTOR_MAX,
            step_increment=0.01,
            page_increment=0.1,
            page_size=0.0,
        )
        scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=adjustment)
        scale.set_draw_value(False)
        scale.set_hexpand(True)
        scale.set_valign(Gtk.Align.CENTER)
        scale.set_size_request(220, -1)
        self._set_accessible_name(scale, title)
        def _mark_label(value):
            rounded = round(value)
            if abs(value - rounded) < 1e-6:
                return str(int(rounded))
            return f"{value:.2f}"

        scale.add_mark(FACTOR_MIN, Gtk.PositionType.BOTTOM, _mark_label(FACTOR_MIN))
        scale.add_mark(DEFAULT_FACTOR, Gtk.PositionType.BOTTOM, _mark_label(DEFAULT_FACTOR))
        scale.add_mark(FACTOR_MAX, Gtk.PositionType.BOTTOM, _mark_label(FACTOR_MAX))

        spin = Gtk.SpinButton(adjustment=adjustment, climb_rate=0.1, digits=2)
        spin.set_numeric(True)
        spin.set_valign(Gtk.Align.CENTER)
        spin.set_width_chars(5)
        self._set_accessible_name(spin, f"{title} value")

        reset_button = Gtk.Button(label="Reset")
        reset_button.add_css_class("flat")
        reset_button.set_valign(Gtk.Align.CENTER)
        self._set_accessible_name(reset_button, f"Reset {title}")

        box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        box.set_hexpand(True)
        box.set_halign(Gtk.Align.FILL)
        box.set_margin_start(8)
        box.set_margin_end(0)
        box.append(scale)
        box.append(spin)
        box.append(reset_button)

        row_box.append(label_box)
        row_box.append(box)
        row.set_child(row_box)
        group.add(row)

        adjustment.connect("value-changed", self._on_adjustment_changed, key)
        reset_button.connect("clicked", self._on_reset_clicked, key)

        self._sliders[key] = {
            "adjustment": adjustment,
        }

    def _set_accessible_name(self, widget, name):
        if hasattr(widget, "set_accessible_name"):
            widget.set_accessible_name(name)
            return
        if hasattr(widget, "update_property") and hasattr(Gtk, "AccessibleProperty"):
            try:
                widget.update_property([Gtk.AccessibleProperty.LABEL], [name])
                return
            except Exception:
                pass
        if hasattr(widget, "set_tooltip_text"):
            widget.set_tooltip_text(name)

    def _set_slider_value(self, key, value):
        slider = self._sliders.get(key)
        if not slider:
            return
        slider["adjustment"].set_value(value)

    def _on_reset_clicked(self, _button, key):
        self._set_slider_value(key, DEFAULT_FACTOR)

    def _on_adjustment_changed(self, adjustment, key):
        if self._loading:
            return
        value = adjustment.get_value()
        self._schedule_apply(key, value)

    def _schedule_apply(self, key, value):
        if key in self._debounce_ids:
            GLib.source_remove(self._debounce_ids[key])

        def _apply():
            self._debounce_ids.pop(key, None)
            self._apply_factor(key, value)
            return False

        self._debounce_ids[key] = GLib.timeout_add(DEBOUNCE_MS, _apply)

    def _apply_factor(self, key, value):
        flag = CLI_MAP.get(key)
        if flag is None:
            return
        result = self._run_wsf(["set", flag, f"{value:.4f}"])
        if not result:
            self._show_toast("wsf not found. Install the CLI first.")
            return
        if result.returncode == 0:
            self._show_apply_toast("Applied. Log out and back in to take effect.")
            return
        self._show_toast(result.stderr.strip() or "Failed to apply settings.")

    def _on_enabled_toggled(self, switch, _param):
        if self._loading:
            return
        cmd = "enable" if switch.get_active() else "disable"
        result = self._run_wsf([cmd])
        if not result:
            self._show_toast("wsf not found. Install the CLI first.")
            return
        if result.returncode == 0:
            self._show_apply_toast("Applied. Log out and back in to take effect.")
            return
        self._show_toast(result.stderr.strip() or "Failed to change status.")

    def _on_run_doctor(self, _button):
        result = self._run_wsf(["doctor"])
        if not result:
            self._show_toast("wsf not found. Install the CLI first.")
            return
        if result.returncode != 0:
            output = result.stderr.strip() or "wsf doctor failed."
        else:
            output = result.stdout
        self._last_doctor_output = output
        self._diagnostics_buffer.set_text(output)
        self._show_toast("Diagnostics updated.")

    def _on_copy_diagnostics(self, _button):
        if not self._last_doctor_output:
            self._show_toast("No diagnostics to copy yet.")
            return
        display = Gdk.Display.get_default()
        if not display:
            self._show_toast("Clipboard unavailable.")
            return
        clipboard = display.get_clipboard()
        clipboard.set_text(self._last_doctor_output)
        self._show_toast("Diagnostics copied to clipboard.")

    def _run_wsf(self, args):
        if not self._cli_path:
            return None
        try:
            return subprocess.run(
                [self._cli_path] + args,
                text=True,
                capture_output=True,
                check=False,
            )
        except OSError:
            return None

    def _find_wsf(self):
        path = shutil.which("wsf")
        if path:
            return path
        home = os.path.expanduser("~")
        fallback = os.path.join(home, ".local", "bin", "wsf")
        if os.path.exists(fallback) and os.access(fallback, os.X_OK):
            return fallback
        return None

    def _refresh_all(self):
        self._refresh_factors()
        self._refresh_status()

    def _refresh_factors(self):
        result = self._run_wsf(["get", "--json"])
        if not result or result.returncode != 0:
            return
        try:
            data = json.loads(result.stdout)
        except json.JSONDecodeError:
            return

        self._loading = True
        self._set_slider_value("scroll_vertical", data.get("scroll_vertical_factor", DEFAULT_FACTOR))
        self._set_slider_value("scroll_horizontal", data.get("scroll_horizontal_factor", DEFAULT_FACTOR))
        self._set_slider_value("pinch_zoom", data.get("pinch_zoom_factor", DEFAULT_FACTOR))
        self._set_slider_value("pinch_rotate", data.get("pinch_rotate_factor", DEFAULT_FACTOR))
        self._loading = False

    def _refresh_status(self):
        result = self._run_wsf(["status", "--json"])
        if not result or result.returncode != 0:
            self._show_status_error_toast("Unable to read status from wsf.")
            return
        try:
            data = json.loads(result.stdout)
        except json.JSONDecodeError:
            self._show_status_error_toast("Invalid status response from wsf.")
            return
        self._loading = True
        self._enable_switch.set_active(bool(data.get("enabled", False)))
        self._loading = False

    def _show_apply_toast(self, message):
        now_ms = GLib.get_monotonic_time() // 1000
        if now_ms - self._last_apply_toast_at < APPLY_TOAST_THROTTLE_MS:
            return
        self._last_apply_toast_at = now_ms
        self._show_toast(message)

    def _show_status_error_toast(self, message):
        if self._loading:
            return
        now_ms = GLib.get_monotonic_time() // 1000
        if now_ms - self._last_status_toast_at < STATUS_TOAST_THROTTLE_MS:
            return
        self._last_status_toast_at = now_ms
        self._show_toast(message)

    def _show_toast(self, message):
        self._toast_overlay.add_toast(Adw.Toast.new(message))


class WsfApp(Adw.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID)

    def do_activate(self):
        if not self.get_active_window():
            WsfWindow(self)
        self.get_active_window().present()


def main():
    app = WsfApp()
    return app.run(None)


if __name__ == "__main__":
    raise SystemExit(main())
