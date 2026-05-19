-- Optional Hyprland Lua integration for Wayland Scroll Factor.
--
-- Load this from hyprland.lua if you want WSF scroll settings to be reapplied
-- after Lua config reloads:
--
--   dofile("/usr/share/wayland-scroll-factor/hyprland/wsf.lua")
--
-- Pinch zoom/rotate still require launching Hyprland through the
-- "Hyprland (WSF gestures)" session or an equivalent wsf-hyprland launcher.

local function apply_wsf()
  if type(hl) ~= "table" or type(hl.exec_cmd) ~= "function" then
    return
  end

  hl.exec_cmd("sh -lc 'command -v wsf >/dev/null 2>&1 && wsf apply >/dev/null 2>&1 || true'")
end

apply_wsf()

if type(hl) == "table" and type(hl.on) == "function" then
  hl.on("hyprland.start", function()
    apply_wsf()
  end)

  hl.on("config.reloaded", function()
    apply_wsf()
  end)
end
