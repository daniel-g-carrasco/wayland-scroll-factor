#!/usr/bin/env python3

import json
import os
import shutil
import subprocess

import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")

from gi.repository import Adw, Gio, GLib, Gtk

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

# Stock symbolic icons only (shipped by adwaita-icon-theme): consistent
# stroke weight, and Adw.ActionRow prefixes align them on one axis for
# free — no hand-tuned label widths.
ROWS = (
    ("scroll_vertical", "Vertical scroll", "object-flip-vertical-symbolic"),
    ("scroll_horizontal", "Horizontal scroll", "object-flip-horizontal-symbolic"),
    ("pinch_zoom", "Pinch zoom", "zoom-in-symbolic"),
    ("pinch_rotate", "Pinch rotate", "object-rotate-right-symbolic"),
)


class WsfWindow(Adw.ApplicationWindow):
    def __init__(self, app):
        super().__init__(application=app)
        self.set_title("Wayland Scroll Factor")
        self.set_default_size(560, 480)
        # Below ~500px the slider rows have no room left for the title and
        # libadwaita starts wrapping it letter by letter; a realistic floor
        # keeps every row on one line (title-lines=1 guards the rest).
        self.set_size_request(500, -1)

        self._cli_path = self._find_wsf()
        self._version = self._get_version()
        self._debounce_ids = {}
        self._loading = True
        self._gnome_preload_active = False

        self._toast_overlay = Adw.ToastOverlay()
        self.set_content(self._toast_overlay)
        self._last_apply_toast_at = 0
        self._last_status_toast_at = 0

        toolbar_view = Adw.ToolbarView()
        self._toast_overlay.set_child(toolbar_view)

        header = Adw.HeaderBar()
        window_title = Adw.WindowTitle(title="Wayland Scroll Factor")
        if self._version:
            window_title.set_subtitle(f"v{self._version}")
        header.set_title_widget(window_title)

        menu = Gio.Menu()
        menu.append("About Wayland Scroll Factor", "win.about")
        menu_button = Gtk.MenuButton()
        menu_button.set_icon_name("open-menu-symbolic")
        menu_button.set_menu_model(menu)
        menu_button.set_tooltip_text("Main menu")
        menu_button.set_primary(True)
        header.pack_end(menu_button)

        about_action = Gio.SimpleAction.new("about", None)
        about_action.connect("activate", self._on_about)
        self.add_action(about_action)

        toolbar_view.add_top_bar(header)

        if not self._cli_path:
            status = Adw.StatusPage()
            status.set_icon_name("dialog-warning-symbolic")
            status.set_title("wsf Not Found")
            status.set_description(
                "Install the CLI and ensure it is in PATH (e.g. ~/.local/bin)."
            )
            toolbar_view.set_content(status)
            self._loading = False
            return

        # Adw.PreferencesPage gives the HIG layout for free: boxed-list
        # groups, content clamp, scrolling.
        page = Adw.PreferencesPage()
        toolbar_view.set_content(page)

        self._sliders = {}
        # One shared width for the scale+spin+undo cluster of every row:
        # without this each ActionRow splits its own leftover space and
        # the four sliders end up with four different lengths.
        self._suffix_group = Gtk.SizeGroup(mode=Gtk.SizeGroupMode.HORIZONTAL)
        scroll_group = Adw.PreferencesGroup(title="Scroll Sensitivity")
        scroll_group.set_description("1.00 is the system default speed")
        page.add(scroll_group)
        for key, title, icon_name in ROWS:
            scroll_group.add(self._build_factor_row(key, title, icon_name))

        system_group = Adw.PreferencesGroup(title="System Integration")
        page.add(system_group)
        self._enabled_row = self._build_switch_row(
            "GNOME Preload", "Enable or disable requires logging out and back in"
        )
        system_group.add(self._enabled_row)

        self._loading = False
        self._refresh_all()

    # ── widgets ──────────────────────────────────────────────────────

    def _build_factor_row(self, key, title, icon_name):
        row = Adw.ActionRow(title=title)
        if hasattr(row, "set_title_lines"):
            row.set_title_lines(1)
        row.add_prefix(Gtk.Image.new_from_icon_name(icon_name))

        adjustment = Gtk.Adjustment(
            value=DEFAULT_FACTOR,
            lower=FACTOR_MIN,
            upper=FACTOR_MAX,
            step_increment=0.05,
            page_increment=0.25,
            page_size=0.0,
        )

        scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=adjustment)
        scale.set_draw_value(False)
        scale.set_valign(Gtk.Align.CENTER)
        scale.set_size_request(200, -1)
        scale.add_mark(DEFAULT_FACTOR, Gtk.PositionType.BOTTOM, None)
        self._set_accessible_name(scale, title)

        spin = Gtk.SpinButton(adjustment=adjustment, climb_rate=0.05, digits=2)
        spin.set_numeric(True)
        spin.set_valign(Gtk.Align.CENTER)
        spin.set_width_chars(4)
        self._set_accessible_name(spin, f"{title} value")

        undo = Gtk.Button.new_from_icon_name("edit-undo-symbolic")
        undo.add_css_class("flat")
        undo.set_valign(Gtk.Align.CENTER)
        undo.set_tooltip_text(f"Reset to {DEFAULT_FACTOR:.2f}")
        undo.connect("clicked", self._on_reset_clicked, key)
        self._set_accessible_name(undo, f"Reset {title}")

        suffix_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        suffix_box.append(scale)
        suffix_box.append(spin)
        suffix_box.append(undo)
        self._suffix_group.add_widget(suffix_box)
        row.add_suffix(suffix_box)
        row.set_activatable(False)

        adjustment.connect("value-changed", self._on_adjustment_changed, key)
        self._sliders[key] = {"adjustment": adjustment, "undo": undo}
        self._sync_undo_visibility(key)
        return row

    def _build_switch_row(self, title, subtitle):
        if hasattr(Adw, "SwitchRow"):
            row = Adw.SwitchRow(title=title, subtitle=subtitle)
            row.connect("notify::active", self._on_enabled_toggled)
            self._enable_getter = row.get_active
            self._enable_setter = row.set_active
            self._enable_sensitive = row.set_sensitive
            return row
        # Older libadwaita (< 1.4): plain ActionRow + Switch.
        row = Adw.ActionRow(title=title, subtitle=subtitle)
        switch = Gtk.Switch(valign=Gtk.Align.CENTER)
        row.add_suffix(switch)
        row.set_activatable_widget(switch)
        switch.connect("notify::active", self._on_enabled_toggled)
        self._enable_getter = switch.get_active
        self._enable_setter = switch.set_active
        self._enable_sensitive = switch.set_sensitive
        return row

    def _set_accessible_name(self, widget, name):
        if hasattr(widget, "update_property") and hasattr(Gtk, "AccessibleProperty"):
            try:
                widget.update_property([Gtk.AccessibleProperty.LABEL], [name])
                return
            except Exception:
                pass
        widget.set_tooltip_text(name)

    # ── slider plumbing ──────────────────────────────────────────────

    def _set_slider_value(self, key, value):
        slider = self._sliders.get(key)
        if not slider:
            return
        slider["adjustment"].set_value(value)
        self._sync_undo_visibility(key)

    def _sync_undo_visibility(self, key):
        slider = self._sliders.get(key)
        if not slider:
            return
        at_default = abs(slider["adjustment"].get_value() - DEFAULT_FACTOR) < 1e-6
        slider["undo"].set_opacity(0.0 if at_default else 1.0)
        slider["undo"].set_can_target(not at_default)

    def _on_reset_clicked(self, _button, key):
        self._set_slider_value(key, DEFAULT_FACTOR)

    def _on_adjustment_changed(self, adjustment, key):
        self._sync_undo_visibility(key)
        if self._loading:
            return
        self._schedule_apply(key, adjustment.get_value())

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
            if self._gnome_preload_active:
                self._show_apply_toast("Saved. Applies from the next gesture.")
            else:
                self._show_apply_toast("Saved. Log out and back in to activate.")
            return
        self._show_toast(result.stderr.strip() or "Failed to apply settings.")

    def _on_enabled_toggled(self, *_args):
        if self._loading:
            return
        cmd = "enable" if self._enable_getter() else "disable"
        result = self._run_wsf([cmd])
        if not result:
            self._show_toast("wsf not found. Install the CLI first.")
            return
        if result.returncode == 0:
            self._show_apply_toast("Applied. Log out and back in for the change to take effect.")
            return
        self._show_toast(result.stderr.strip() or "Failed to change status.")

    # ── CLI plumbing ─────────────────────────────────────────────────

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

    def _get_version(self):
        # Single source of truth: ask the installed CLI, so the GUI can
        # never disagree with the wsf binary it drives.
        result = self._run_wsf(["version"])
        if result and result.returncode == 0 and result.stdout:
            parts = result.stdout.strip().split()
            if parts:
                return parts[-1]
        return None

    def _on_about(self, *_args):
        version = self._version or "unknown"
        kwargs = {
            "application_name": "Wayland Scroll Factor",
            "application_icon": APP_ID,
            "version": version,
            "developer_name": "Daniel Grasso",
            "website": "https://github.com/daniel-g-carrasco/wayland-scroll-factor",
            "issue_url": (
                "https://github.com/daniel-g-carrasco/"
                "wayland-scroll-factor/issues"
            ),
            "license_type": Gtk.License.MIT_X11,
        }
        # Adw.AboutDialog is the modern (libadwaita 1.5+) widget; fall back
        # to Adw.AboutWindow on older runtimes (Debian stable, etc.).
        if hasattr(Adw, "AboutDialog"):
            about = Adw.AboutDialog(**kwargs)
        else:
            about = Adw.AboutWindow(transient_for=self, **kwargs)
        # The old in-window "doctor" panel now lives where GNOME puts
        # diagnostics: About → Troubleshooting → Debugging Information,
        # with copy/save built in. Same `wsf doctor` output.
        doctor = self._run_wsf(["doctor"])
        if doctor and (doctor.stdout or doctor.stderr):
            about.set_debug_info(doctor.stdout or doctor.stderr)
            about.set_debug_info_filename("wsf-diagnostics.txt")
        if hasattr(Adw, "AboutDialog") and isinstance(about, Adw.AboutDialog):
            about.present(self)
        else:
            about.present()

    def _find_wsf(self):
        path = shutil.which("wsf")
        if path:
            return path
        home = os.path.expanduser("~")
        fallback = os.path.join(home, ".local", "bin", "wsf")
        if os.path.exists(fallback) and os.access(fallback, os.X_OK):
            return fallback
        return None

    # ── status refresh ───────────────────────────────────────────────

    def _refresh_all(self):
        result = self._run_wsf(["status", "--json"])
        if not result or result.returncode != 0:
            self._show_status_error_toast("Unable to read status from wsf.")
            return
        try:
            data = json.loads(result.stdout)
        except json.JSONDecodeError:
            self._show_status_error_toast("Invalid status response from wsf.")
            return

        factors = data.get("factors", data)
        self._gnome_preload_active = bool(data.get("gnome_shell_library_mapped", False))

        self._loading = True
        self._set_slider_value(
            "scroll_vertical", factors.get("scroll_vertical_factor", DEFAULT_FACTOR)
        )
        self._set_slider_value(
            "scroll_horizontal", factors.get("scroll_horizontal_factor", DEFAULT_FACTOR)
        )
        self._set_slider_value(
            "pinch_zoom", factors.get("pinch_zoom_factor", DEFAULT_FACTOR)
        )
        self._set_slider_value(
            "pinch_rotate", factors.get("pinch_rotate_factor", DEFAULT_FACTOR)
        )
        self._enable_setter(bool(data.get("enabled", False)))
        if self._gnome_preload_active:
            self._enabled_row.set_subtitle("Preload is active in this session")
        else:
            self._enabled_row.set_subtitle(
                "Enable or disable requires logging out and back in"
            )
        self._loading = False

    # ── toasts ───────────────────────────────────────────────────────

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
