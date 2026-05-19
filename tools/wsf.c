#define _GNU_SOURCE

#include "wsf_config.h"

#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static bool wsf_run_command(const char *cmd, char *buf, size_t len);
static bool wsf_run_command_ok(const char *cmd);

#define WSF_HYPRLAND_SCROLL_FACTOR_MAX 2.0
#define WSF_ENV_VALUE_MAX 32768

enum wsf_hyprland_scroll_apply_method {
	WSF_HYPRLAND_SCROLL_APPLY_NONE = 0,
	WSF_HYPRLAND_SCROLL_APPLY_KEYWORD,
	WSF_HYPRLAND_SCROLL_APPLY_EVAL
};

struct wsf_runtime_state {
	bool user_manager_ld_preload_present;
	bool user_manager_matches;
	bool gnome_shell_found;
	bool gnome_shell_ld_preload_present;
	bool gnome_shell_ld_preload_matches;
	bool gnome_shell_library_mapped;
	bool hyprland_found;
	bool hyprland_ld_preload_present;
	bool hyprland_ld_preload_matches;
	bool hyprland_library_mapped;
	bool hyprland_targets_present;
	bool hyprland_gestures_only_present;
	bool hyprland_gestures_present;
	bool hyprland_defer_prune_present;
	pid_t gnome_shell_pid;
	pid_t hyprland_pid;
	char user_manager_ld_preload[WSF_ENV_VALUE_MAX];
	char gnome_shell_ld_preload[WSF_ENV_VALUE_MAX];
	char hyprland_ld_preload[WSF_ENV_VALUE_MAX];
	char hyprland_targets[256];
	char hyprland_gestures_only[64];
	char hyprland_gestures[64];
	char hyprland_defer_prune[64];
};

struct wsf_hyprland_state {
	bool session_hint;
	bool hyprctl_found;
	bool running;
	bool touchpad_scroll_found;
	bool lua_eval_supported;
	double touchpad_scroll_factor;
	enum wsf_hyprland_scroll_apply_method scroll_apply_method;
	char version[256];
};

static void wsf_runtime_state_collect(
	struct wsf_runtime_state *state,
	const char *lib_path
);

static const char *wsf_home(void) {
	const char *home = getenv("HOME");

	if (home == NULL || home[0] == '\0') {
		return NULL;
	}

	return home;
}

static bool wsf_build_path(char *buf, size_t len, const char *suffix) {
	const char *home = wsf_home();
	int written = 0;

	if (home == NULL) {
		return false;
	}

	written = snprintf(buf, len, "%s/%s", home, suffix);
	if (written <= 0 || (size_t) written >= len) {
		return false;
	}

	return true;
}

static const char *wsf_path_basename(const char *path) {
	const char *slash = NULL;

	if (path == NULL) {
		return "";
	}

	slash = strrchr(path, '/');
	if (slash == NULL) {
		return path;
	}

	return slash + 1;
}

static bool wsf_paths_equivalent(const char *left, const char *right) {
	char left_real[PATH_MAX];
	char right_real[PATH_MAX];

	if (left == NULL || right == NULL || left[0] == '\0' || right[0] == '\0') {
		return false;
	}

	if (strcmp(left, right) == 0) {
		return true;
	}

	if (realpath(left, left_real) == NULL || realpath(right, right_real) == NULL) {
		return false;
	}

	return strcmp(left_real, right_real) == 0;
}

static bool wsf_preload_token_is_wsf_library(const char *token) {
	return token != NULL &&
		strcmp(wsf_path_basename(token), "libwsf_preload.so") == 0;
}

static bool wsf_preload_list_contains(const char *list, const char *lib_path) {
	char *copy = NULL;
	char *cursor = NULL;
	bool found = false;

	if (list == NULL || list[0] == '\0' || lib_path == NULL || lib_path[0] == '\0') {
		return false;
	}

	copy = strdup(list);
	if (copy == NULL) {
		return false;
	}

	cursor = copy;
	while (*cursor != '\0') {
		char *token = NULL;

		while (*cursor == ':' || isspace((unsigned char) *cursor)) {
			cursor++;
		}
		if (*cursor == '\0') {
			break;
		}

		token = cursor;
		while (*cursor != '\0' && *cursor != ':' &&
			!isspace((unsigned char) *cursor)) {
			cursor++;
		}
		if (*cursor != '\0') {
			*cursor = '\0';
			cursor++;
		}

		if (wsf_paths_equivalent(token, lib_path)) {
			found = true;
			break;
		}
	}

	free(copy);
	return found;
}

static bool wsf_preload_append_token(char *buf, size_t len, const char *token) {
	size_t used = 0;
	int written = 0;

	if (buf == NULL || len == 0 || token == NULL || token[0] == '\0') {
		return false;
	}

	used = strlen(buf);
	if (used >= len) {
		return false;
	}
	written = snprintf(
		buf + used,
		len - used,
		"%s%s",
		used > 0 ? ":" : "",
		token
	);

	return written > 0 && (size_t) written < len - used;
}

static bool wsf_preload_list_without_wsf(
	const char *list,
	char *buf,
	size_t len
) {
	char *copy = NULL;
	char *cursor = NULL;
	bool ok = true;

	if (buf == NULL || len == 0) {
		return false;
	}

	buf[0] = '\0';
	if (list == NULL || list[0] == '\0') {
		return true;
	}

	copy = strdup(list);
	if (copy == NULL) {
		return false;
	}

	cursor = copy;
	while (*cursor != '\0') {
		char *token = NULL;

		while (*cursor == ':' || isspace((unsigned char) *cursor)) {
			cursor++;
		}
		if (*cursor == '\0') {
			break;
		}

		token = cursor;
		while (*cursor != '\0' && *cursor != ':' &&
			!isspace((unsigned char) *cursor)) {
			cursor++;
		}
		if (*cursor != '\0') {
			*cursor = '\0';
			cursor++;
		}

		if (wsf_preload_token_is_wsf_library(token)) {
			continue;
		}
		if (!wsf_preload_append_token(buf, len, token)) {
			ok = false;
			break;
		}
	}

	free(copy);
	return ok;
}

static bool wsf_env_file_path(char *buf, size_t len) {
	return wsf_build_path(buf, len, ".config/environment.d/wayland-scroll-factor.conf");
}

static bool wsf_env_dir_path(char *buf, size_t len) {
	return wsf_build_path(buf, len, ".config/environment.d");
}

static bool wsf_env_file_effective_value(
	const char *env_path,
	const char *key,
	char **out_value
) {
	FILE *file = NULL;
	char *line = NULL;
	size_t line_cap = 0;
	ssize_t line_len = 0;
	size_t key_len = 0;
	bool found = false;

	if (out_value != NULL) {
		*out_value = NULL;
	}
	if (env_path == NULL || key == NULL || key[0] == '\0' || out_value == NULL) {
		return false;
	}

	file = fopen(env_path, "r");
	if (file == NULL) {
		return false;
	}

	key_len = strlen(key);
	while ((line_len = getline(&line, &line_cap, file)) >= 0) {
		char *cursor = line;
		char *value = NULL;
		char *copy = NULL;

		(void) line_len;
		while (isspace((unsigned char) *cursor)) {
			cursor++;
		}
		if (*cursor == '#' || *cursor == '\0') {
			continue;
		}
		cursor[strcspn(cursor, "\r\n")] = '\0';
		if (strncmp(cursor, key, key_len) != 0 || cursor[key_len] != '=') {
			continue;
		}

		value = cursor + key_len + 1;
		copy = strdup(value);
		if (copy == NULL) {
			free(line);
			fclose(file);
			return false;
		}

		free(*out_value);
		*out_value = copy;
		found = true;
	}

	free(line);
	fclose(file);
	return found;
}

static bool wsf_env_file_preload_matches(
	const char *env_path,
	const char *lib_path
) {
	char *effective = NULL;
	bool matches = false;

	if (!wsf_env_file_effective_value(env_path, "LD_PRELOAD", &effective)) {
		return false;
	}

	matches = wsf_preload_list_contains(effective, lib_path);
	free(effective);
	return matches;
}

static bool wsf_user_manager_reload(void) {
	return wsf_run_command_ok("systemctl --user daemon-reexec >/dev/null 2>&1");
}

static bool wsf_lib_path(char *buf, size_t len) {
	const char *override = getenv("WSF_LIB_PATH");
	int written = 0;

	if (override != NULL && override[0] != '\0') {
		written = snprintf(buf, len, "%s", override);
		return written > 0 && (size_t) written < len;
	}

#ifdef WSF_LIBDIR
	written = snprintf(buf, len, "%s/libwsf_preload.so", WSF_LIBDIR);
	if (written > 0 && (size_t) written < len) {
		return true;
	}
#endif

	return wsf_build_path(
		buf,
		len,
		".local/lib/wayland-scroll-factor/libwsf_preload.so"
	);
}

static int wsf_mkdir(const char *path) {
	if (mkdir(path, 0700) == 0) {
		return 0;
	}
	if (errno == EEXIST) {
		return 0;
	}
	return -1;
}

static bool wsf_ensure_env_dir(void) {
	char base[512];
	char env_dir[512];
	const char *home = wsf_home();
	int written = 0;

	if (home == NULL) {
		return false;
	}

	written = snprintf(base, sizeof(base), "%s/.config", home);
	if (written <= 0 || (size_t) written >= sizeof(base)) {
		return false;
	}

	if (wsf_mkdir(base) != 0) {
		return false;
	}

	if (!wsf_env_dir_path(env_dir, sizeof(env_dir))) {
		return false;
	}

	if (wsf_mkdir(env_dir) != 0) {
		return false;
	}

	return true;
}

static bool wsf_write_env_file(
	const char *env_path,
	const char *preload_value
) {
	FILE *file = NULL;

	if (env_path == NULL || preload_value == NULL || preload_value[0] == '\0') {
		return false;
	}

	file = fopen(env_path, "w");
	if (file == NULL) {
		return false;
	}

	fprintf(file, "# Generated by wsf\n");
	fprintf(file, "LD_PRELOAD=%s\n", preload_value);
	if (fclose(file) != 0) {
		return false;
	}

	return true;
}

static bool wsf_repaired_preload_value(
	const char *existing_preload,
	const char *lib_path,
	char *buf,
	size_t len
) {
	char preserved[WSF_ENV_VALUE_MAX];

	if (lib_path == NULL || lib_path[0] == '\0' || buf == NULL || len == 0) {
		return false;
	}

	buf[0] = '\0';
	preserved[0] = '\0';

	if (!wsf_preload_append_token(buf, len, lib_path)) {
		return false;
	}
	if (!wsf_preload_list_without_wsf(existing_preload, preserved, sizeof(preserved))) {
		return false;
	}
	if (preserved[0] == '\0') {
		return true;
	}

	return wsf_preload_append_token(buf, len, preserved);
}

static bool wsf_repair_preload_setup(
	const char *env_path,
	const char *lib_path,
	bool verbose,
	bool *out_wrote_env
) {
	char *effective_preload = NULL;
	char repaired_preload[WSF_ENV_VALUE_MAX];
	bool env_matches = false;
	bool wrote_env = false;
	struct wsf_runtime_state runtime;

	if (out_wrote_env != NULL) {
		*out_wrote_env = false;
	}

	env_matches = wsf_env_file_preload_matches(env_path, lib_path);
	if (!env_matches) {
		if (!wsf_ensure_env_dir()) {
			if (verbose) {
				fprintf(stderr, "repair: failed to create environment.d directory.\n");
			}
			return false;
		}

		(void) wsf_env_file_effective_value(
			env_path,
			"LD_PRELOAD",
			&effective_preload
		);
		if (!wsf_repaired_preload_value(
			effective_preload,
			lib_path,
			repaired_preload,
			sizeof(repaired_preload))) {
			free(effective_preload);
			if (verbose) {
				fprintf(stderr, "repair: failed to build repaired LD_PRELOAD value.\n");
			}
			return false;
		}
		free(effective_preload);

		if (!wsf_write_env_file(env_path, repaired_preload)) {
			if (verbose) {
				fprintf(
					stderr,
					"repair: failed to write %s: %s\n",
					env_path,
					strerror(errno)
				);
			}
			return false;
		}
		wrote_env = true;
		if (out_wrote_env != NULL) {
			*out_wrote_env = true;
		}
		if (verbose) {
			printf("repair: wrote %s\n", env_path);
		}
	} else if (verbose) {
		printf("repair: environment.d already points at the installed WSF library\n");
	}

	wsf_runtime_state_collect(&runtime, lib_path);
	if (wrote_env || !runtime.user_manager_matches) {
		if (!wsf_user_manager_reload()) {
			if (verbose) {
				printf("repair: could not reload systemd --user with daemon-reexec\n");
			}
			return false;
		}
		if (verbose) {
			printf("repair: reloaded systemd --user environment\n");
		}
	}

	return true;
}

static bool wsf_string_contains(const char *haystack, const char *needle) {
	return haystack != NULL && needle != NULL && strstr(haystack, needle) != NULL;
}

static bool wsf_hyprland_session_hint(void) {
	const char *desktop = getenv("XDG_CURRENT_DESKTOP");
	const char *current_desktop = getenv("XDG_SESSION_DESKTOP");
	const char *signature = getenv("HYPRLAND_INSTANCE_SIGNATURE");

	if (signature != NULL && signature[0] != '\0') {
		return true;
	}

	return wsf_string_contains(desktop, "Hyprland") ||
		wsf_string_contains(current_desktop, "Hyprland");
}

static bool wsf_hyprland_get_touchpad_scroll_factor(double *out_factor) {
	char raw[64];
	char *end = NULL;
	double value = 0.0;

	if (out_factor == NULL) {
		return false;
	}

	if (!wsf_run_command(
		"hyprctl getoption input:touchpad:scroll_factor 2>/dev/null | awk '/float:/ {print $2; exit}'",
		raw,
		sizeof(raw))) {
		return false;
	}

	errno = 0;
	value = strtod(raw, &end);
	if (raw == end || errno == ERANGE) {
		return false;
	}

	*out_factor = value;
	return true;
}

static bool wsf_hyprland_factor_close(double left, double right) {
	double diff = left - right;

	if (diff < 0.0) {
		diff = -diff;
	}

	return diff <= 0.0005;
}

static const char *wsf_hyprland_scroll_apply_method_name(
	enum wsf_hyprland_scroll_apply_method method
) {
	switch (method) {
	case WSF_HYPRLAND_SCROLL_APPLY_KEYWORD:
		return "keyword";
	case WSF_HYPRLAND_SCROLL_APPLY_EVAL:
		return "lua-eval";
	case WSF_HYPRLAND_SCROLL_APPLY_NONE:
	default:
		return "none";
	}
}

static bool wsf_hyprland_lua_eval_supported(void) {
	return wsf_run_command_ok(
		"out=$(hyprctl eval 'hl.config({})' 2>&1); "
		"rc=$?; "
		"case \"$out\" in "
		"*'eval is only supported'*|*'unknown request'*|*'not supported'*) exit 1;; "
		"esac; "
		"exit \"$rc\""
	);
}

static void wsf_hyprland_state_collect(struct wsf_hyprland_state *state) {
	if (state == NULL) {
		return;
	}

	memset(state, 0, sizeof(*state));
	state->session_hint = wsf_hyprland_session_hint();
	state->hyprctl_found = wsf_run_command_ok("command -v hyprctl >/dev/null 2>&1");
	state->running = wsf_run_command(
		"hyprctl version 2>/dev/null",
		state->version,
		sizeof(state->version)
	);
	if (state->running) {
		state->touchpad_scroll_found = wsf_hyprland_get_touchpad_scroll_factor(
			&state->touchpad_scroll_factor
		);
		state->lua_eval_supported = wsf_hyprland_lua_eval_supported();
		state->scroll_apply_method = state->lua_eval_supported ?
			WSF_HYPRLAND_SCROLL_APPLY_EVAL :
			WSF_HYPRLAND_SCROLL_APPLY_KEYWORD;
	}
}

static double wsf_hyprland_clamp_scroll_factor(double factor, bool *clamped) {
	if (clamped != NULL) {
		*clamped = false;
	}

	if (factor > WSF_HYPRLAND_SCROLL_FACTOR_MAX) {
		if (clamped != NULL) {
			*clamped = true;
		}
		return WSF_HYPRLAND_SCROLL_FACTOR_MAX;
	}

	return factor;
}

static bool wsf_hyprland_apply_touchpad_scroll_keyword(double factor) {
	char cmd[256];

	if (snprintf(
		cmd,
		sizeof(cmd),
		"hyprctl keyword input:touchpad:scroll_factor %.4f >/dev/null 2>&1",
		factor
	) >= (int) sizeof(cmd)) {
		return false;
	}

	return wsf_run_command_ok(cmd);
}

static bool wsf_hyprland_apply_touchpad_scroll_eval(double factor) {
	char cmd[512];

	if (snprintf(
		cmd,
		sizeof(cmd),
		"hyprctl eval 'hl.config({ input = { touchpad = { scroll_factor = %.4f } } })' >/dev/null 2>&1",
		factor
	) >= (int) sizeof(cmd)) {
		return false;
	}

	return wsf_run_command_ok(cmd);
}

static bool wsf_hyprland_apply_touchpad_scroll_with_method(
	double factor,
	enum wsf_hyprland_scroll_apply_method method
) {
	double live_factor = 0.0;
	bool command_ok = false;

	switch (method) {
	case WSF_HYPRLAND_SCROLL_APPLY_KEYWORD:
		command_ok = wsf_hyprland_apply_touchpad_scroll_keyword(factor);
		break;
	case WSF_HYPRLAND_SCROLL_APPLY_EVAL:
		command_ok = wsf_hyprland_apply_touchpad_scroll_eval(factor);
		break;
	case WSF_HYPRLAND_SCROLL_APPLY_NONE:
	default:
		return false;
	}

	if (!command_ok) {
		return false;
	}

	if (!wsf_hyprland_get_touchpad_scroll_factor(&live_factor)) {
		return false;
	}

	return wsf_hyprland_factor_close(live_factor, factor);
}

static int wsf_hyprland_apply_touchpad_scroll(double factor, bool verbose) {
	struct wsf_hyprland_state state;
	double applied_factor = 0.0;
	bool clamped = false;
	enum wsf_hyprland_scroll_apply_method preferred =
		WSF_HYPRLAND_SCROLL_APPLY_NONE;
	enum wsf_hyprland_scroll_apply_method fallback =
		WSF_HYPRLAND_SCROLL_APPLY_NONE;
	enum wsf_hyprland_scroll_apply_method used =
		WSF_HYPRLAND_SCROLL_APPLY_NONE;

	wsf_hyprland_state_collect(&state);
	if (!state.running) {
		if (verbose && state.session_hint) {
			fprintf(stderr, "hyprland native backend unavailable: hyprctl is not connected to a running Hyprland session.\n");
		}
		return 0;
	}

	applied_factor = wsf_hyprland_clamp_scroll_factor(factor, &clamped);

	preferred = state.scroll_apply_method;
	fallback = preferred == WSF_HYPRLAND_SCROLL_APPLY_EVAL ?
		WSF_HYPRLAND_SCROLL_APPLY_KEYWORD :
		WSF_HYPRLAND_SCROLL_APPLY_EVAL;

	if (wsf_hyprland_apply_touchpad_scroll_with_method(
		applied_factor,
		preferred
	)) {
		used = preferred;
	} else if (wsf_hyprland_apply_touchpad_scroll_with_method(
		applied_factor,
		fallback
	)) {
		used = fallback;
	}

	if (used == WSF_HYPRLAND_SCROLL_APPLY_NONE) {
		if (verbose) {
			fprintf(
				stderr,
				"failed to apply Hyprland input:touchpad:scroll_factor; tried keyword and lua-eval backends.\n"
			);
		}
		return -1;
	}

	if (verbose) {
		printf(
			"hyprland native scroll applied via %s: input:touchpad:scroll_factor=%.4f\n",
			wsf_hyprland_scroll_apply_method_name(used),
			applied_factor
		);
		if (clamped) {
			printf(
				"hyprland note: requested %.4f, clamped to %.4f (Hyprland native range).\n",
				factor,
				WSF_HYPRLAND_SCROLL_FACTOR_MAX
			);
		}
	}

	return 1;
}

static void wsf_print_usage(const char *prog) {
	fprintf(stderr, "Usage: %s <command> [args]\n", prog);
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "  set <factor>   Set scroll factor (%.2f-%.2f)\n",
		WSF_FACTOR_MIN,
		WSF_FACTOR_MAX
	);
	fprintf(stderr, "  set [options]  Set per-axis/gesture factors\n");
	fprintf(stderr, "    --scroll-vertical <factor>\n");
	fprintf(stderr, "    --scroll-horizontal <factor>\n");
	fprintf(stderr, "    --pinch-zoom <factor>\n");
	fprintf(stderr, "    --pinch-rotate <factor>\n");
	fprintf(stderr, "    --factor <factor>\n");
	fprintf(stderr, "  get [--json]   Print effective factors\n");
	fprintf(stderr, "  apply          Apply supported live compositor backends\n");
	fprintf(stderr, "  enable         Enable preload via environment.d\n");
	fprintf(stderr, "  repair         Repair preload setup and report remaining session work\n");
	fprintf(stderr, "  disable        Disable preload via environment.d\n");
	fprintf(stderr, "  status [--json] Show current status\n");
	fprintf(stderr, "  doctor [--json] Print diagnostics\n");
}

static bool wsf_parse_factor_arg(const char *arg, double *out_factor) {
	char *end = NULL;
	double value = 0.0;

	if (arg == NULL || out_factor == NULL) {
		return false;
	}

	while (isspace((unsigned char) *arg)) {
		arg++;
	}

	errno = 0;
	value = strtod(arg, &end);
	if (arg == end || errno == ERANGE) {
		return false;
	}

	while (isspace((unsigned char) *end)) {
		end++;
	}

	if (*end != '\0') {
		return false;
	}

	if (value < WSF_FACTOR_MIN || value > WSF_FACTOR_MAX) {
		return false;
	}

	*out_factor = value;
	return true;
}

static int wsf_cmd_set(int argc, char **argv) {
	struct wsf_config_values updates;
	bool has_updates = false;
	bool has_live_scroll_factor = false;
	bool debug = wsf_debug_enabled();
	double live_scroll_factor = WSF_FACTOR_DEFAULT;
	int i = 0;

	wsf_config_values_init(&updates);

	if (argc == 3 && argv[2][0] != '-') {
		double factor = 0.0;

		if (!wsf_parse_factor_arg(argv[2], &factor)) {
			fprintf(stderr, "Invalid factor: %s\n", argv[2]);
			return 1;
		}
		updates.factor = factor;
		updates.has_factor = true;
		has_updates = true;
		has_live_scroll_factor = true;
		live_scroll_factor = factor;
	} else {
		for (i = 2; i < argc; i++) {
			const char *arg = argv[i];
			double factor = 0.0;

			if (strcmp(arg, "--scroll-vertical") == 0) {
				if (i + 1 >= argc || !wsf_parse_factor_arg(argv[i + 1], &factor)) {
					fprintf(stderr, "Invalid scroll vertical factor.\n");
					return 1;
				}
				updates.scroll_vertical_factor = factor;
				updates.has_scroll_vertical = true;
				has_updates = true;
				has_live_scroll_factor = true;
				live_scroll_factor = factor;
				i++;
				continue;
			}
			if (strcmp(arg, "--scroll-horizontal") == 0) {
				if (i + 1 >= argc || !wsf_parse_factor_arg(argv[i + 1], &factor)) {
					fprintf(stderr, "Invalid scroll horizontal factor.\n");
					return 1;
				}
				updates.scroll_horizontal_factor = factor;
				updates.has_scroll_horizontal = true;
				has_updates = true;
				has_live_scroll_factor = true;
				live_scroll_factor = factor;
				i++;
				continue;
			}
			if (strcmp(arg, "--pinch-zoom") == 0) {
				if (i + 1 >= argc || !wsf_parse_factor_arg(argv[i + 1], &factor)) {
					fprintf(stderr, "Invalid pinch zoom factor.\n");
					return 1;
				}
				updates.pinch_zoom_factor = factor;
				updates.has_pinch_zoom = true;
				has_updates = true;
				i++;
				continue;
			}
			if (strcmp(arg, "--pinch-rotate") == 0) {
				if (i + 1 >= argc || !wsf_parse_factor_arg(argv[i + 1], &factor)) {
					fprintf(stderr, "Invalid pinch rotate factor.\n");
					return 1;
				}
				updates.pinch_rotate_factor = factor;
				updates.has_pinch_rotate = true;
				has_updates = true;
				i++;
				continue;
			}
			if (strcmp(arg, "--factor") == 0) {
				if (i + 1 >= argc || !wsf_parse_factor_arg(argv[i + 1], &factor)) {
					fprintf(stderr, "Invalid factor value.\n");
					return 1;
				}
				updates.factor = factor;
				updates.has_factor = true;
				has_updates = true;
				has_live_scroll_factor = true;
				live_scroll_factor = factor;
				i++;
				continue;
			}

			fprintf(stderr, "Unknown option for set: %s\n", arg);
			return 1;
		}
	}

	if (!has_updates) {
		fprintf(stderr, "No factors specified.\n");
		return 1;
	}

	if (wsf_config_write_updates(&updates, debug) != 0) {
		fprintf(stderr, "Failed to write config.\n");
		return 1;
	}

	printf("config updated\n");
	if (has_live_scroll_factor) {
		(void) wsf_hyprland_apply_touchpad_scroll(live_scroll_factor, true);
	}
	return 0;
}

static bool wsf_factor_differs(double a, double b) {
	double diff = a - b;

	if (diff < 0.0) {
		diff = -diff;
	}

	return diff > 0.0001;
}

static bool wsf_scroll_env_override_present(void) {
	const char *factor = getenv("WSF_FACTOR");
	const char *vertical = getenv("WSF_SCROLL_VERTICAL_FACTOR");
	const char *horizontal = getenv("WSF_SCROLL_HORIZONTAL_FACTOR");

	return (factor != NULL && factor[0] != '\0') ||
		(vertical != NULL && vertical[0] != '\0') ||
		(horizontal != NULL && horizontal[0] != '\0');
}

static int wsf_cmd_apply(void) {
	struct wsf_effective_factors factors;
	int status = wsf_effective_factors(&factors, false);
	int applied = 0;

	if (status == WSF_CONFIG_ERROR) {
		fprintf(stderr, "Failed to read config.\n");
		return 1;
	}
	if (status == WSF_CONFIG_MISSING && !wsf_scroll_env_override_present()) {
		printf("no WSF scroll config found; nothing to apply\n");
		return 0;
	}

	if (wsf_factor_differs(factors.scroll_vertical, factors.scroll_horizontal)) {
		printf(
			"hyprland note: native backend has one touchpad scroll factor; using scroll_vertical_factor=%.4f.\n",
			factors.scroll_vertical
		);
	}

	applied = wsf_hyprland_apply_touchpad_scroll(factors.scroll_vertical, true);
	if (applied < 0) {
		return 1;
	}
	if (applied > 0) {
		return 0;
	}

	printf("no live native compositor backend detected\n");
	printf("hint: on GNOME, use `wsf enable` and log out/in so the preload can load into gnome-shell.\n");
	return 0;
}

static int wsf_cmd_get(bool json) {
	struct wsf_effective_factors factors;
	int status = wsf_effective_factors(&factors, false);

	(void) status;
	if (json) {
		printf(
			"{\"scroll_vertical_factor\":%.4f,"
			"\"scroll_horizontal_factor\":%.4f,"
			"\"pinch_zoom_factor\":%.4f,"
			"\"pinch_rotate_factor\":%.4f,"
			"\"legacy_factor_used\":%s}\n",
			factors.scroll_vertical,
			factors.scroll_horizontal,
			factors.pinch_zoom,
			factors.pinch_rotate,
			factors.used_legacy_factor ? "true" : "false"
		);
		return 0;
	}

	printf("scroll_vertical_factor=%.4f\n", factors.scroll_vertical);
	printf("scroll_horizontal_factor=%.4f\n", factors.scroll_horizontal);
	printf("pinch_zoom_factor=%.4f\n", factors.pinch_zoom);
	printf("pinch_rotate_factor=%.4f\n", factors.pinch_rotate);
	return 0;
}

static int wsf_cmd_enable(void) {
	char env_path[512];
	char lib_path[512];
	const char *existing = getenv("LD_PRELOAD");
	bool manager_reloaded = false;

	if (!wsf_env_file_path(env_path, sizeof(env_path))) {
		fprintf(stderr, "Failed to resolve environment.d path.\n");
		return 1;
	}

	if (!wsf_lib_path(lib_path, sizeof(lib_path))) {
		fprintf(stderr, "Failed to resolve library path.\n");
		return 1;
	}

	if (access(lib_path, R_OK) != 0) {
		fprintf(stderr, "Library not found: %s\n", lib_path);
		fprintf(stderr,
			"Install wsf (system or user) or set WSF_LIB_PATH.\n"
		);
		return 1;
	}

	if (!wsf_ensure_env_dir()) {
		fprintf(stderr, "Failed to create environment.d directory.\n");
		return 1;
	}

	if (!wsf_write_env_file(env_path, lib_path)) {
		fprintf(stderr, "Failed to write %s: %s\n", env_path, strerror(errno));
		return 1;
	}

	manager_reloaded = wsf_user_manager_reload();

	if (existing != NULL && existing[0] != '\0') {
		fprintf(stderr,
			"Warning: LD_PRELOAD already set; environment.d will override it.\n"
			"Hint: combine manually, e.g. LD_PRELOAD=%s:$LD_PRELOAD\n",
			lib_path
		);
	}

	if (manager_reloaded) {
		printf("enabled (user manager reloaded; log out/in required)\n");
	} else {
		printf("enabled (log out/in required)\n");
		printf("hint: if it does not activate after login, run `wsf repair` and try again, or reboot.\n");
	}
	return 0;
}

static int wsf_cmd_repair(void) {
	char env_path[512];
	char lib_path[512];
	struct wsf_runtime_state runtime;
	struct wsf_hyprland_state hyprland;
	bool env_present = false;
	bool lib_present = false;
	bool wrote_env = false;

	if (!wsf_env_file_path(env_path, sizeof(env_path))) {
		fprintf(stderr, "Failed to resolve environment.d path.\n");
		return 1;
	}

	if (!wsf_lib_path(lib_path, sizeof(lib_path))) {
		fprintf(stderr, "Failed to resolve library path.\n");
		return 1;
	}

	env_present = access(env_path, F_OK) == 0;
	lib_present = access(lib_path, R_OK) == 0;
	if (!lib_present) {
		fprintf(stderr, "repair: library not found: %s\n", lib_path);
		fprintf(stderr, "repair: install wsf or set WSF_LIB_PATH, then run `wsf repair` again.\n");
		return 1;
	}

	if (!wsf_repair_preload_setup(env_path, lib_path, true, &wrote_env)) {
		return 1;
	}

	wsf_runtime_state_collect(&runtime, lib_path);
	wsf_hyprland_state_collect(&hyprland);

	if (hyprland.running) {
		int applied = wsf_cmd_apply();

		if (applied != 0) {
			return applied;
		}
		if (!runtime.hyprland_library_mapped) {
			printf("repair: Hyprland scroll was handled through the native backend.\n");
			printf("repair: restart Hyprland through wsf-hyprland only if you need pinch zoom/rotate tuning.\n");
		}
		return 0;
	}

	if (!runtime.user_manager_matches) {
		printf(
			"repair: environment file is %s, but systemd --user still does not expose the installed WSF library.\n",
			env_present ? "present" : "newly written"
		);
		printf("repair: try logging out and back in; if it still fails, reboot once.\n");
		return 1;
	}

	if (runtime.gnome_shell_found && runtime.gnome_shell_library_mapped) {
		printf("repair: GNOME Shell has WSF loaded; factor changes should reload on handled gestures.\n");
		return 0;
	}

	if (runtime.gnome_shell_found) {
		printf("repair: systemd --user is ready, but the running GNOME Shell has not loaded WSF.\n");
		printf("repair: this cannot be repaired safely inside the current GNOME Wayland session.\n");
		printf("repair: log out and back in, then run `wsf status` and look for `gnome-shell library mapped: yes`.\n");
		return 0;
	}

	printf(
		"repair: preload setup is ready for the next GNOME session%s.\n",
		wrote_env ? " after rewriting environment.d" : ""
	);
	printf("repair: log in to a GNOME Wayland session and run `wsf status` to verify activation.\n");
	return 0;
}

static int wsf_cmd_disable(void) {
	char env_path[512];
	bool manager_reloaded = false;

	if (!wsf_env_file_path(env_path, sizeof(env_path))) {
		fprintf(stderr, "Failed to resolve environment.d path.\n");
		return 1;
	}

	if (unlink(env_path) != 0) {
		if (errno == ENOENT) {
			printf("already disabled\n");
			return 0;
		}
		fprintf(stderr, "Failed to remove %s: %s\n", env_path, strerror(errno));
		return 1;
	}

	manager_reloaded = wsf_user_manager_reload();
	if (manager_reloaded) {
		printf("disabled (user manager reloaded; log out/in required)\n");
	} else {
		printf("disabled (log out/in required)\n");
	}
	return 0;
}

static void wsf_print_factor_status(int status) {
	switch (status) {
	case WSF_CONFIG_OK:
		printf("config");
		break;
	case WSF_CONFIG_MISSING:
		printf("default");
		break;
	case WSF_CONFIG_INVALID:
		printf("invalid->default");
		break;
	default:
		printf("error");
		break;
	}
}

static void wsf_print_json_string(const char *value) {
	const unsigned char *cursor = (const unsigned char *) value;

	if (value == NULL) {
		printf("null");
		return;
	}

	printf("\"");
	while (*cursor != '\0') {
		switch (*cursor) {
		case '\"':
			printf("\\\"");
			break;
		case '\\':
			printf("\\\\");
			break;
		case '\n':
			printf("\\n");
			break;
		case '\r':
			printf("\\r");
			break;
		case '\t':
			printf("\\t");
			break;
		default:
			if (*cursor < 0x20) {
				printf("\\u%04x", (unsigned int) *cursor);
			} else {
				printf("%c", *cursor);
			}
			break;
		}
		cursor++;
	}
	printf("\"");
}

static void wsf_print_hyprland_json_field(
	const struct wsf_hyprland_state *state
) {
	printf("\"hyprland\":{");
	printf("\"session_hint\":%s,", state->session_hint ? "true" : "false");
	printf("\"hyprctl_found\":%s,", state->hyprctl_found ? "true" : "false");
	printf("\"running\":%s,", state->running ? "true" : "false");
	printf("\"version\":");
	if (state->running) {
		wsf_print_json_string(state->version);
	} else {
		wsf_print_json_string(NULL);
	}
	printf(",");
	printf("\"touchpad_scroll_factor\":");
	if (state->touchpad_scroll_found) {
		printf("%.4f", state->touchpad_scroll_factor);
	} else {
		printf("null");
	}
	printf(",");
	printf("\"lua_eval_supported\":%s,", state->lua_eval_supported ? "true" : "false");
	printf("\"scroll_apply_method\":");
	wsf_print_json_string(
		wsf_hyprland_scroll_apply_method_name(state->scroll_apply_method)
	);
	printf(",");
	printf("\"single_touchpad_scroll_factor\":true");
	printf("}");
}

static void wsf_print_hyprland_status(
	const struct wsf_hyprland_state *state,
	bool always
) {
	if (!always && !state->session_hint && !state->running) {
		return;
	}

	if (state->running) {
		printf("hyprland: running (%s)\n", state->version);
	} else if (state->hyprctl_found) {
		printf("hyprland: not running (hyprctl found)\n");
	} else if (state->session_hint) {
		printf("hyprland: session hinted, but hyprctl not found\n");
	} else {
		printf("hyprland: not detected\n");
	}

	if (state->touchpad_scroll_found) {
		printf(
			"hyprland touchpad scroll_factor: %.4f\n",
			state->touchpad_scroll_factor
		);
	}

	if (state->running) {
		printf(
			"hyprland config backend: %s\n",
			state->lua_eval_supported ? "lua" : "legacy"
		);
		printf(
			"hyprland scroll apply method: %s\n",
			wsf_hyprland_scroll_apply_method_name(state->scroll_apply_method)
		);
		printf("hyprland backend: native scroll apply available (single factor for vertical + horizontal)\n");
		printf("hyprland backend: native pinch zoom/rotate scaling not exposed\n");
	}
}

static void wsf_print_hyprland_preload_json_field(
	const struct wsf_runtime_state *runtime
) {
	printf("\"hyprland_preload\":{");
	printf("\"process_found\":%s,", runtime->hyprland_found ? "true" : "false");
	printf("\"pid\":%d,", runtime->hyprland_found ? (int) runtime->hyprland_pid : -1);
	printf("\"ld_preload\":");
	if (runtime->hyprland_ld_preload_present) {
		wsf_print_json_string(runtime->hyprland_ld_preload);
	} else {
		wsf_print_json_string(NULL);
	}
	printf(",");
	printf(
		"\"preload_matches\":%s,",
		runtime->hyprland_ld_preload_matches ? "true" : "false"
	);
	printf(
		"\"library_mapped\":%s,",
		runtime->hyprland_library_mapped ? "true" : "false"
	);
	printf("\"WSF_TARGETS\":");
	if (runtime->hyprland_targets_present) {
		wsf_print_json_string(runtime->hyprland_targets);
	} else {
		wsf_print_json_string(NULL);
	}
	printf(",");
	printf("\"WSF_HYPRLAND_GESTURES_ONLY\":");
	if (runtime->hyprland_gestures_only_present) {
		wsf_print_json_string(runtime->hyprland_gestures_only);
	} else {
		wsf_print_json_string(NULL);
	}
	printf(",");
	printf("\"WSF_HYPRLAND_GESTURES\":");
	if (runtime->hyprland_gestures_present) {
		wsf_print_json_string(runtime->hyprland_gestures);
	} else {
		wsf_print_json_string(NULL);
	}
	printf(",");
	printf("\"WSF_DEFER_PRUNE_UNTIL_TARGET\":");
	if (runtime->hyprland_defer_prune_present) {
		wsf_print_json_string(runtime->hyprland_defer_prune);
	} else {
		wsf_print_json_string(NULL);
	}
	printf("}");
}

static void wsf_print_hyprland_preload_status(
	const struct wsf_runtime_state *runtime,
	bool always
) {
	if (!always && !runtime->hyprland_found) {
		return;
	}

	if (!runtime->hyprland_found) {
		printf("hyprland gesture preload: not running\n");
		return;
	}

	printf("hyprland pid: %d\n", (int) runtime->hyprland_pid);
	if (runtime->hyprland_ld_preload_present) {
		printf(
			"hyprland LD_PRELOAD: %s (%s)\n",
			runtime->hyprland_ld_preload,
			runtime->hyprland_ld_preload_matches ? "includes WSF" : "does not include WSF"
		);
	} else {
		printf("hyprland LD_PRELOAD: not set\n");
	}
	printf(
		"hyprland library mapped: %s\n",
		runtime->hyprland_library_mapped ? "yes" : "no"
	);
	if (runtime->hyprland_targets_present) {
		printf("hyprland WSF_TARGETS: %s\n", runtime->hyprland_targets);
	}
	if (runtime->hyprland_gestures_only_present) {
		printf(
			"hyprland WSF_HYPRLAND_GESTURES_ONLY: %s\n",
			runtime->hyprland_gestures_only
		);
	}
	if (runtime->hyprland_gestures_present) {
		printf(
			"hyprland WSF_HYPRLAND_GESTURES: %s\n",
			runtime->hyprland_gestures
		);
	}
	if (runtime->hyprland_defer_prune_present) {
		printf(
			"hyprland WSF_DEFER_PRUNE_UNTIL_TARGET: %s\n",
			runtime->hyprland_defer_prune
		);
	}
	printf(
		"hyprland gesture preload: %s\n",
		runtime->hyprland_library_mapped ? "active" : "inactive"
	);
}

static int wsf_cmd_status(bool json) {
	char env_path[512];
	char lib_path[512];
	const char *config_path = wsf_config_path();
	struct wsf_effective_factors factors;
	struct wsf_runtime_state runtime;
	struct wsf_hyprland_state hyprland;
	int status = wsf_effective_factors(&factors, false);
	const char *env_factor = getenv("WSF_FACTOR");
	const char *env_scroll_vertical = getenv("WSF_SCROLL_VERTICAL_FACTOR");
	const char *env_scroll_horizontal = getenv("WSF_SCROLL_HORIZONTAL_FACTOR");
	const char *env_pinch_zoom = getenv("WSF_PINCH_ZOOM_FACTOR");
	const char *env_pinch_rotate = getenv("WSF_PINCH_ROTATE_FACTOR");
	bool env_present = false;
	bool lib_present = false;

	if (!wsf_env_file_path(env_path, sizeof(env_path))) {
		fprintf(stderr, "Failed to resolve environment.d path.\n");
		return 1;
	}

	if (!wsf_lib_path(lib_path, sizeof(lib_path))) {
		fprintf(stderr, "Failed to resolve library path.\n");
		return 1;
	}

	env_present = access(env_path, F_OK) == 0;
	lib_present = access(lib_path, R_OK) == 0;
	wsf_runtime_state_collect(&runtime, lib_path);
	wsf_hyprland_state_collect(&hyprland);

	if (json) {
		bool config_present = false;

		if (config_path != NULL) {
			config_present = access(config_path, F_OK) == 0;
		}

		printf("{");
		printf("\"enabled\":%s,", env_present ? "true" : "false");
		printf("\"env_file\":");
		wsf_print_json_string(env_path);
		printf(",");
		printf("\"env_file_present\":%s,", env_present ? "true" : "false");
		printf("\"library\":");
		wsf_print_json_string(lib_path);
		printf(",");
		printf("\"library_present\":%s,", lib_present ? "true" : "false");
		printf("\"user_manager_ld_preload\":");
		if (runtime.user_manager_ld_preload_present) {
			wsf_print_json_string(runtime.user_manager_ld_preload);
		} else {
			wsf_print_json_string(NULL);
		}
		printf(",");
		printf("\"user_manager_preload_matches\":%s,", runtime.user_manager_matches ? "true" : "false");
		printf("\"gnome_shell_found\":%s,", runtime.gnome_shell_found ? "true" : "false");
		printf("\"gnome_shell_pid\":%d,", runtime.gnome_shell_found ? (int) runtime.gnome_shell_pid : -1);
		printf("\"gnome_shell_ld_preload\":");
		if (runtime.gnome_shell_ld_preload_present) {
			wsf_print_json_string(runtime.gnome_shell_ld_preload);
		} else {
			wsf_print_json_string(NULL);
		}
		printf(",");
		printf("\"gnome_shell_preload_matches\":%s,", runtime.gnome_shell_ld_preload_matches ? "true" : "false");
		printf("\"gnome_shell_library_mapped\":%s,", runtime.gnome_shell_library_mapped ? "true" : "false");
		wsf_print_hyprland_json_field(&hyprland);
		printf(",");
		wsf_print_hyprland_preload_json_field(&runtime);
		printf(",");
		printf("\"config\":");
		wsf_print_json_string(config_path);
		printf(",");
		printf("\"config_present\":%s,", config_present ? "true" : "false");
		printf("\"factors\":{");
		printf("\"scroll_vertical_factor\":%.4f,", factors.scroll_vertical);
		printf("\"scroll_horizontal_factor\":%.4f,", factors.scroll_horizontal);
		printf("\"pinch_zoom_factor\":%.4f,", factors.pinch_zoom);
		printf("\"pinch_rotate_factor\":%.4f,", factors.pinch_rotate);
		printf("\"legacy_factor_used\":%s", factors.used_legacy_factor ? "true" : "false");
		printf("}");
		printf("}\n");
		return 0;
	}

	printf("enabled: %s\n", env_present ? "yes" : "no");
	printf("env file: %s (%s)\n", env_path, env_present ? "present" : "missing");
	printf("library: %s (%s)\n", lib_path, lib_present ? "present" : "missing");
	if (runtime.user_manager_ld_preload_present) {
		printf(
			"user manager LD_PRELOAD: %s (%s)\n",
			runtime.user_manager_ld_preload,
			runtime.user_manager_matches ? "includes WSF" : "does not include WSF"
		);
	} else {
		printf("user manager LD_PRELOAD: not set\n");
	}
	if (runtime.gnome_shell_found) {
		printf("gnome-shell pid: %d\n", (int) runtime.gnome_shell_pid);
		if (runtime.gnome_shell_ld_preload_present) {
			printf(
				"gnome-shell LD_PRELOAD: %s (%s)\n",
				runtime.gnome_shell_ld_preload,
				runtime.gnome_shell_ld_preload_matches ? "includes WSF" : "does not include WSF"
			);
		} else {
			printf("gnome-shell LD_PRELOAD: not set\n");
		}
		printf(
			"gnome-shell library mapped: %s\n",
			runtime.gnome_shell_library_mapped ? "yes" : "no"
		);
	}
	wsf_print_hyprland_status(&hyprland, false);
	wsf_print_hyprland_preload_status(&runtime, false);
	if (config_path != NULL) {
		bool config_present = access(config_path, F_OK) == 0;
		printf(
			"config: %s (%s)\n",
			config_path,
			config_present ? "present" : "missing"
		);
	}
	printf("scroll_vertical_factor: %.4f (", factors.scroll_vertical);
	wsf_print_factor_status(status);
	printf(")\n");
	printf("scroll_horizontal_factor: %.4f\n", factors.scroll_horizontal);
	printf("pinch_zoom_factor: %.4f\n", factors.pinch_zoom);
	printf("pinch_rotate_factor: %.4f\n", factors.pinch_rotate);
	printf("legacy factor: %s\n", factors.used_legacy_factor ? "yes" : "no");
	if (env_factor != NULL && env_factor[0] != '\0') {
		printf("WSF_FACTOR: %s (env override)\n", env_factor);
	}
	if (env_scroll_vertical != NULL && env_scroll_vertical[0] != '\0') {
		printf("WSF_SCROLL_VERTICAL_FACTOR: %s (env override)\n", env_scroll_vertical);
	}
	if (env_scroll_horizontal != NULL && env_scroll_horizontal[0] != '\0') {
		printf("WSF_SCROLL_HORIZONTAL_FACTOR: %s (env override)\n", env_scroll_horizontal);
	}
	if (env_pinch_zoom != NULL && env_pinch_zoom[0] != '\0') {
		printf("WSF_PINCH_ZOOM_FACTOR: %s (env override)\n", env_pinch_zoom);
	}
	if (env_pinch_rotate != NULL && env_pinch_rotate[0] != '\0') {
		printf("WSF_PINCH_ROTATE_FACTOR: %s (env override)\n", env_pinch_rotate);
	}
	if (hyprland.running) {
		printf(
			"runtime scroll apply: active via Hyprland %s backend\n",
			wsf_hyprland_scroll_apply_method_name(hyprland.scroll_apply_method)
		);
		if (runtime.hyprland_library_mapped) {
			printf("runtime gesture reload: active via Hyprland gestures-only preload\n");
		} else {
			printf("runtime gesture reload: inactive; restart Hyprland through wsf-hyprland to use pinch controls\n");
			printf("hint: use `start-hyprland --path $(command -v wsf-hyprland) -- ...`, or with tuigreet use `--session-wrapper $(command -v wsf-session-wrapper)`.\n");
		}
	} else if (runtime.gnome_shell_library_mapped) {
		printf("runtime config reload: active (GNOME preload rereads factors on handled gestures)\n");
	} else {
		printf("runtime config reload: pending (GNOME Shell has not loaded WSF yet)\n");
	}
	if (hyprland.running) {
		printf("note: Hyprland scroll changes apply live; restart Hyprland only to load/unload gesture preload\n");
	} else {
		printf("note: logout/login required after preload enable/disable\n");
	}
	if (!hyprland.running && env_present && !runtime.user_manager_matches &&
		!runtime.gnome_shell_library_mapped) {
		printf("hint: run `wsf repair`, then log out/in.\n");
	}
	if (!hyprland.running && runtime.user_manager_matches && !runtime.gnome_shell_library_mapped) {
		printf("hint: GNOME Shell has not loaded WSF yet; `wsf repair` can verify setup, then log out/in.\n");
	}
	return 0;
}

static bool wsf_run_command(const char *cmd, char *buf, size_t len) {
	FILE *pipe = NULL;
	int rc = 0;

	if (buf == NULL || len == 0) {
		return false;
	}

	buf[0] = '\0';
	pipe = popen(cmd, "r");
	if (pipe == NULL) {
		return false;
	}

	if (fgets(buf, (int) len, pipe) == NULL) {
		pclose(pipe);
		return false;
	}

	rc = pclose(pipe);
	if (rc == -1 || !WIFEXITED(rc) || WEXITSTATUS(rc) != 0) {
		buf[0] = '\0';
		return false;
	}

	buf[strcspn(buf, "\n")] = '\0';
	return true;
}

static bool wsf_run_command_ok(const char *cmd) {
	int rc = system(cmd);

	return rc == 0;
}

static bool wsf_systemd_user_env_value(
	const char *key,
	char *buf,
	size_t len
) {
	FILE *pipe = NULL;
	char *line = NULL;
	size_t line_cap = 0;
	size_t key_len = 0;
	bool found = false;

	if (key == NULL || key[0] == '\0' || buf == NULL || len == 0) {
		return false;
	}

	buf[0] = '\0';
	key_len = strlen(key);
	pipe = popen("systemctl --user show-environment 2>/dev/null", "r");
	if (pipe == NULL) {
		return false;
	}

	while (getline(&line, &line_cap, pipe) >= 0) {
		line[strcspn(line, "\r\n")] = '\0';
		if (strncmp(line, key, key_len) != 0 || line[key_len] != '=') {
			continue;
		}
		if (snprintf(buf, len, "%s", line + key_len + 1) >= (int) len) {
			buf[0] = '\0';
			free(line);
			pclose(pipe);
			return false;
		}
		found = true;
	}

	free(line);
	pclose(pipe);
	return found;
}

static bool wsf_read_pid_comm(pid_t pid, char *buf, size_t len) {
	char path[64];
	FILE *file = NULL;

	if (buf == NULL || len == 0 || pid <= 0) {
		return false;
	}

	if (snprintf(path, sizeof(path), "/proc/%d/comm", (int) pid) >= (int) sizeof(path)) {
		return false;
	}

	file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	if (fgets(buf, (int) len, file) == NULL) {
		fclose(file);
		return false;
	}

	fclose(file);
	buf[strcspn(buf, "\n")] = '\0';
	return true;
}

static bool wsf_find_newest_pid_by_name(const char *name, pid_t *out_pid) {
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	pid_t best = -1;

	if (name == NULL || name[0] == '\0' || out_pid == NULL) {
		return false;
	}

	dir = opendir("/proc");
	if (dir == NULL) {
		return false;
	}

	while ((entry = readdir(dir)) != NULL) {
		char *end = NULL;
		long pid_long = 0;
		char comm[128];

		if (!isdigit((unsigned char) entry->d_name[0])) {
			continue;
		}

		pid_long = strtol(entry->d_name, &end, 10);
		if (entry->d_name[0] == '\0' || end == NULL || *end != '\0') {
			continue;
		}
		if (pid_long <= 0 || pid_long > INT_MAX) {
			continue;
		}
		if (!wsf_read_pid_comm((pid_t) pid_long, comm, sizeof(comm))) {
			continue;
		}
		if (strcmp(comm, name) != 0) {
			continue;
		}
		if ((pid_t) pid_long > best) {
			best = (pid_t) pid_long;
		}
	}

	closedir(dir);

	if (best <= 0) {
		return false;
	}

	*out_pid = best;
	return true;
}

static bool wsf_read_pid_environ_value(
	pid_t pid,
	const char *key,
	char *buf,
	size_t len
) {
	char path[64];
	FILE *file = NULL;
	char envbuf[32768];
	size_t read_len = 0;
	size_t key_len = 0;
	size_t i = 0;

	if (pid <= 0 || key == NULL || key[0] == '\0' || buf == NULL || len == 0) {
		return false;
	}

	if (snprintf(path, sizeof(path), "/proc/%d/environ", (int) pid) >= (int) sizeof(path)) {
		return false;
	}

	file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	read_len = fread(envbuf, 1, sizeof(envbuf) - 1, file);
	fclose(file);

	if (read_len == 0) {
		return false;
	}

	envbuf[read_len] = '\0';
	key_len = strlen(key);

	while (i < read_len) {
		const char *entry = envbuf + i;
		size_t entry_len = strlen(entry);

		if (entry_len > key_len &&
			strncmp(entry, key, key_len) == 0 &&
			entry[key_len] == '=') {
			if (snprintf(buf, len, "%s", entry + key_len + 1) >= (int) len) {
				return false;
			}
			return true;
		}

		i += entry_len + 1;
	}

	return false;
}

static bool wsf_pid_maps_contains(pid_t pid, const char *needle) {
	char path[64];
	FILE *file = NULL;
	char line[PATH_MAX + 128];

	if (pid <= 0 || needle == NULL || needle[0] == '\0') {
		return false;
	}

	if (snprintf(path, sizeof(path), "/proc/%d/maps", (int) pid) >= (int) sizeof(path)) {
		return false;
	}

	file = fopen(path, "r");
	if (file == NULL) {
		return false;
	}

	while (fgets(line, sizeof(line), file) != NULL) {
		if (strstr(line, needle) != NULL) {
			fclose(file);
			return true;
		}
	}

	fclose(file);
	return false;
}

static void wsf_runtime_state_collect(
	struct wsf_runtime_state *state,
	const char *lib_path
) {
	memset(state, 0, sizeof(*state));

	if (wsf_systemd_user_env_value(
		"LD_PRELOAD",
		state->user_manager_ld_preload,
		sizeof(state->user_manager_ld_preload))) {
		state->user_manager_ld_preload_present = true;
		state->user_manager_matches = wsf_preload_list_contains(
			state->user_manager_ld_preload,
			lib_path
		);
	}

	if (wsf_find_newest_pid_by_name("gnome-shell", &state->gnome_shell_pid)) {
		state->gnome_shell_found = true;
		if (wsf_read_pid_environ_value(
			state->gnome_shell_pid,
			"LD_PRELOAD",
			state->gnome_shell_ld_preload,
			sizeof(state->gnome_shell_ld_preload))) {
			state->gnome_shell_ld_preload_present = true;
			state->gnome_shell_ld_preload_matches = wsf_preload_list_contains(
				state->gnome_shell_ld_preload,
				lib_path
			);
		}

		state->gnome_shell_library_mapped = wsf_pid_maps_contains(
			state->gnome_shell_pid,
			"libwsf_preload.so"
		);
	}

	if (wsf_find_newest_pid_by_name("Hyprland", &state->hyprland_pid)) {
		state->hyprland_found = true;
		if (wsf_read_pid_environ_value(
			state->hyprland_pid,
			"LD_PRELOAD",
			state->hyprland_ld_preload,
			sizeof(state->hyprland_ld_preload))) {
			state->hyprland_ld_preload_present = true;
			state->hyprland_ld_preload_matches = wsf_preload_list_contains(
				state->hyprland_ld_preload,
				lib_path
			);
		}
		if (wsf_read_pid_environ_value(
			state->hyprland_pid,
			"WSF_TARGETS",
			state->hyprland_targets,
			sizeof(state->hyprland_targets))) {
			state->hyprland_targets_present = true;
		}
		if (wsf_read_pid_environ_value(
			state->hyprland_pid,
			"WSF_HYPRLAND_GESTURES_ONLY",
			state->hyprland_gestures_only,
			sizeof(state->hyprland_gestures_only))) {
			state->hyprland_gestures_only_present = true;
		}
		if (wsf_read_pid_environ_value(
			state->hyprland_pid,
			"WSF_HYPRLAND_GESTURES",
			state->hyprland_gestures,
			sizeof(state->hyprland_gestures))) {
			state->hyprland_gestures_present = true;
		}
		if (wsf_read_pid_environ_value(
			state->hyprland_pid,
			"WSF_DEFER_PRUNE_UNTIL_TARGET",
			state->hyprland_defer_prune,
			sizeof(state->hyprland_defer_prune))) {
			state->hyprland_defer_prune_present = true;
		}
		state->hyprland_library_mapped = wsf_pid_maps_contains(
			state->hyprland_pid,
			"libwsf_preload.so"
		);
	}
}

static bool wsf_find_libinput_from_ldconfig(char *buf, size_t len) {
	return wsf_run_command(
		"ldconfig -p 2>/dev/null | awk '/libinput\\.so/{print $4; exit}'",
		buf,
		len
	);
}

static void *wsf_open_libinput(void) {
	const char *candidates[] = {
		"libinput.so",
		"libinput.so.10",
		"libinput.so.11",
		"libinput.so.12",
		"libinput.so.9",
		"libinput.so.8",
		NULL
	};
	const char *sonames[] = {
		"libinput.so.10",
		"libinput.so.11",
		"libinput.so.12",
		"libinput.so",
		"libinput.so.9",
		"libinput.so.8",
		NULL
	};
	void *handle = NULL;
	char libdir[256];
	char libpath[512];
	char path[512];
	int i = 0;

	for (i = 0; candidates[i] != NULL; i++) {
		handle = dlopen(candidates[i], RTLD_LAZY | RTLD_LOCAL);
		if (handle != NULL) {
			return handle;
		}
	}

	if (wsf_run_command("pkg-config --variable=libdir libinput 2>/dev/null",
		libdir,
		sizeof(libdir))) {
		for (i = 0; sonames[i] != NULL; i++) {
			int written = snprintf(path, sizeof(path), "%s/%s", libdir, sonames[i]);
			if (written <= 0 || (size_t) written >= sizeof(path)) {
				continue;
			}
			handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
			if (handle != NULL) {
				return handle;
			}
		}
	}

	if (wsf_find_libinput_from_ldconfig(libpath, sizeof(libpath))) {
		handle = dlopen(libpath, RTLD_LAZY | RTLD_LOCAL);
		if (handle != NULL) {
			return handle;
		}
	}

	return NULL;
}

static bool wsf_has_symbol(void *handle, const char *symbol) {
	const char *error = NULL;

	dlerror();
	(void) dlsym(handle, symbol);
	error = dlerror();

	return error == NULL;
}

struct wsf_symbol_status {
	bool libinput_found;
	bool scroll_value;
	bool scroll_v120;
	bool axis_value;
	bool axis_value_discrete;
	bool axis_source;
	bool base_event;
	bool event_type;
	bool gesture_scale;
	bool gesture_angle;
};

static void wsf_symbol_status(struct wsf_symbol_status *status) {
	void *handle = wsf_open_libinput();
	bool scroll_value = false;
	bool scroll_v120 = false;
	bool axis_value = false;
	bool axis_value_discrete = false;
	bool axis_source = false;
	bool base_event = false;
	bool event_type = false;
	bool gesture_scale = false;
	bool gesture_angle = false;

	status->libinput_found = handle != NULL;
	status->scroll_value = false;
	status->scroll_v120 = false;
	status->axis_value = false;
	status->axis_value_discrete = false;
	status->axis_source = false;
	status->base_event = false;
	status->event_type = false;
	status->gesture_scale = false;
	status->gesture_angle = false;

	if (handle != NULL) {
		scroll_value = wsf_has_symbol(handle, "libinput_event_pointer_get_scroll_value");
		scroll_v120 = wsf_has_symbol(handle, "libinput_event_pointer_get_scroll_value_v120");
		axis_value = wsf_has_symbol(handle, "libinput_event_pointer_get_axis_value");
		axis_value_discrete = wsf_has_symbol(
			handle,
			"libinput_event_pointer_get_axis_value_discrete"
		);
		axis_source = wsf_has_symbol(handle, "libinput_event_pointer_get_axis_source");
		base_event = wsf_has_symbol(handle, "libinput_event_pointer_get_base_event");
		event_type = wsf_has_symbol(handle, "libinput_event_get_type");
		gesture_scale = wsf_has_symbol(handle, "libinput_event_gesture_get_scale");
		gesture_angle = wsf_has_symbol(handle, "libinput_event_gesture_get_angle_delta");

		status->scroll_value = scroll_value;
		status->scroll_v120 = scroll_v120;
		status->axis_value = axis_value;
		status->axis_value_discrete = axis_value_discrete;
		status->axis_source = axis_source;
		status->base_event = base_event;
		status->event_type = event_type;
		status->gesture_scale = gesture_scale;
		status->gesture_angle = gesture_angle;

		dlclose(handle);
	}
}

static void wsf_doctor_symbols_print(const struct wsf_symbol_status *status) {
	if (!status->libinput_found) {
		printf("libinput symbols: unavailable (libinput.so not found)\n");
		printf("hint: ensure libinput is installed and reachable.\n");
		return;
	}

	printf(
		"libinput symbols: scroll_value=%s scroll_v120=%s axis_value=%s axis_discrete=%s axis_source=%s base_event=%s event_type=%s\n",
		status->scroll_value ? "yes" : "no",
		status->scroll_v120 ? "yes" : "no",
		status->axis_value ? "yes" : "no",
		status->axis_value_discrete ? "yes" : "no",
		status->axis_source ? "yes" : "no",
		status->base_event ? "yes" : "no",
		status->event_type ? "yes" : "no"
	);
	printf(
		"pinch hooks: scale=%s angle=%s\n",
		status->gesture_scale ? "yes" : "no",
		status->gesture_angle ? "yes" : "no"
	);
	printf("scroll filtering: GNOME continuous scroll fast path; v120/legacy axis source filter enabled\n");
	if (!status->axis_source) {
		printf("hint: axis source symbol missing; touchpad-only filter is inactive.\n");
	}
	if (!status->gesture_scale) {
		printf("hint: pinch zoom scaling unavailable; check libinput version.\n");
	}
}

static void wsf_print_kv(const char *key, const char *value) {
	printf("%s: %s\n", key, value != NULL ? value : "unknown");
}

static int wsf_cmd_doctor(bool json) {
	char env_path[512];
	char lib_path[512];
	char gnome[256];
	char libinput[256];
	char wsf_hyprland[512];
	char start_hyprland[512];
	const char *session = getenv("XDG_SESSION_TYPE");
	const char *desktop = getenv("XDG_CURRENT_DESKTOP");
	const char *config_path = wsf_config_path();
	struct wsf_effective_factors factors;
	int status = wsf_effective_factors(&factors, false);
	const char *env_factor = getenv("WSF_FACTOR");
	const char *env_scroll_vertical = getenv("WSF_SCROLL_VERTICAL_FACTOR");
	const char *env_scroll_horizontal = getenv("WSF_SCROLL_HORIZONTAL_FACTOR");
	const char *env_pinch_zoom = getenv("WSF_PINCH_ZOOM_FACTOR");
	const char *env_pinch_rotate = getenv("WSF_PINCH_ROTATE_FACTOR");
	const char *env_lib_path = getenv("WSF_LIB_PATH");
	const char *ld_preload = getenv("LD_PRELOAD");
	bool env_present = false;
	bool lib_present = false;
	bool config_present = false;
	struct wsf_symbol_status symbols;
	struct wsf_runtime_state runtime;
	struct wsf_hyprland_state hyprland;
	bool wsf_hyprland_found = false;
	bool start_hyprland_found = false;

	if (!wsf_env_file_path(env_path, sizeof(env_path))) {
		fprintf(stderr, "Failed to resolve environment.d path.\n");
		return 1;
	}

	if (!wsf_lib_path(lib_path, sizeof(lib_path))) {
		fprintf(stderr, "Failed to resolve library path.\n");
		return 1;
	}

	env_present = access(env_path, F_OK) == 0;
	lib_present = access(lib_path, R_OK) == 0;
	if (config_path != NULL) {
		config_present = access(config_path, F_OK) == 0;
	}

	wsf_symbol_status(&symbols);
	wsf_runtime_state_collect(&runtime, lib_path);
	wsf_hyprland_state_collect(&hyprland);
	wsf_hyprland_found = wsf_run_command(
		"command -v wsf-hyprland 2>/dev/null",
		wsf_hyprland,
		sizeof(wsf_hyprland)
	);
	start_hyprland_found = wsf_run_command(
		"command -v start-hyprland 2>/dev/null",
		start_hyprland,
		sizeof(start_hyprland)
	);

	if (json) {
		printf("{");
		printf("\"session\":");
		wsf_print_json_string(session);
		printf(",");
		printf("\"desktop\":");
		wsf_print_json_string(desktop);
		printf(",");
		printf("\"gnome_shell\":");
		if (wsf_run_command("gnome-shell --version 2>/dev/null", gnome, sizeof(gnome))) {
			wsf_print_json_string(gnome);
		} else {
			wsf_print_json_string(NULL);
		}
		printf(",");
		printf("\"libinput_version\":");
		if (wsf_run_command("libinput --version 2>/dev/null", libinput, sizeof(libinput))) {
			wsf_print_json_string(libinput);
		} else {
			wsf_print_json_string(NULL);
		}
		printf(",");
		printf("\"env_file\":");
		wsf_print_json_string(env_path);
		printf(",");
		printf("\"env_file_present\":%s,", env_present ? "true" : "false");
		printf("\"library\":");
		wsf_print_json_string(lib_path);
		printf(",");
		printf("\"library_present\":%s,", lib_present ? "true" : "false");
		printf("\"user_manager_ld_preload\":");
		if (runtime.user_manager_ld_preload_present) {
			wsf_print_json_string(runtime.user_manager_ld_preload);
		} else {
			wsf_print_json_string(NULL);
		}
		printf(",");
		printf("\"user_manager_preload_matches\":%s,", runtime.user_manager_matches ? "true" : "false");
		printf("\"gnome_shell_found\":%s,", runtime.gnome_shell_found ? "true" : "false");
		printf("\"gnome_shell_pid\":%d,", runtime.gnome_shell_found ? (int) runtime.gnome_shell_pid : -1);
		printf("\"gnome_shell_ld_preload\":");
		if (runtime.gnome_shell_ld_preload_present) {
			wsf_print_json_string(runtime.gnome_shell_ld_preload);
		} else {
			wsf_print_json_string(NULL);
		}
		printf(",");
		printf("\"gnome_shell_preload_matches\":%s,", runtime.gnome_shell_ld_preload_matches ? "true" : "false");
		printf("\"gnome_shell_library_mapped\":%s,", runtime.gnome_shell_library_mapped ? "true" : "false");
		wsf_print_hyprland_json_field(&hyprland);
		printf(",");
		wsf_print_hyprland_preload_json_field(&runtime);
		printf(",");
		printf("\"hyprland_launch\":{");
		printf("\"wsf_hyprland\":");
		wsf_print_json_string(wsf_hyprland_found ? wsf_hyprland : NULL);
		printf(",");
		printf("\"start_hyprland\":");
		wsf_print_json_string(start_hyprland_found ? start_hyprland : NULL);
		printf(",");
		printf("\"recommended_start_hyprland_path\":%s",
			(wsf_hyprland_found && start_hyprland_found) ? "true" : "false"
		);
		printf("},");
		printf("\"config\":");
		wsf_print_json_string(config_path);
		printf(",");
		printf("\"config_present\":%s,", config_present ? "true" : "false");
		printf("\"factors\":{");
		printf("\"scroll_vertical_factor\":%.4f,", factors.scroll_vertical);
		printf("\"scroll_horizontal_factor\":%.4f,", factors.scroll_horizontal);
		printf("\"pinch_zoom_factor\":%.4f,", factors.pinch_zoom);
		printf("\"pinch_rotate_factor\":%.4f,", factors.pinch_rotate);
		printf("\"legacy_factor_used\":%s", factors.used_legacy_factor ? "true" : "false");
		printf("},");
		printf("\"env_overrides\":{");
		printf("\"WSF_FACTOR\":");
		wsf_print_json_string(env_factor);
		printf(",");
		printf("\"WSF_SCROLL_VERTICAL_FACTOR\":");
		wsf_print_json_string(env_scroll_vertical);
		printf(",");
		printf("\"WSF_SCROLL_HORIZONTAL_FACTOR\":");
		wsf_print_json_string(env_scroll_horizontal);
		printf(",");
		printf("\"WSF_PINCH_ZOOM_FACTOR\":");
		wsf_print_json_string(env_pinch_zoom);
		printf(",");
		printf("\"WSF_PINCH_ROTATE_FACTOR\":");
		wsf_print_json_string(env_pinch_rotate);
		printf(",");
		printf("\"WSF_LIB_PATH\":");
		wsf_print_json_string(env_lib_path);
		printf(",");
		printf("\"LD_PRELOAD\":");
		wsf_print_json_string(ld_preload);
		printf("},");
		printf("\"symbols\":{");
		printf("\"scroll_value\":%s,", symbols.scroll_value ? "true" : "false");
		printf("\"scroll_v120\":%s,", symbols.scroll_v120 ? "true" : "false");
		printf("\"axis_value\":%s,", symbols.axis_value ? "true" : "false");
		printf("\"axis_value_discrete\":%s,", symbols.axis_value_discrete ? "true" : "false");
		printf("\"axis_source\":%s,", symbols.axis_source ? "true" : "false");
		printf("\"base_event\":%s,", symbols.base_event ? "true" : "false");
		printf("\"event_type\":%s,", symbols.event_type ? "true" : "false");
		printf("\"gesture_scale\":%s,", symbols.gesture_scale ? "true" : "false");
		printf("\"gesture_angle\":%s", symbols.gesture_angle ? "true" : "false");
		printf("},");
		printf("\"scroll_axis_filter_enabled\":%s", symbols.axis_source ? "true" : "false");
		printf("}\n");
		return 0;
	}

	wsf_print_kv("session", session);
	wsf_print_kv("desktop", desktop);
	if (wsf_run_command("gnome-shell --version 2>/dev/null", gnome, sizeof(gnome))) {
		wsf_print_kv("gnome-shell", gnome);
	} else {
		wsf_print_kv("gnome-shell", "not found");
	}
	if (wsf_run_command("libinput --version 2>/dev/null", libinput, sizeof(libinput))) {
		wsf_print_kv("libinput", libinput);
	} else {
		wsf_print_kv("libinput", "not found (install libinput-tools)" );
	}

	printf("env file: %s (%s)\n", env_path, env_present ? "present" : "missing");
	printf("library: %s (%s)\n", lib_path, lib_present ? "present" : "missing");
	if (runtime.user_manager_ld_preload_present) {
		printf(
			"user manager LD_PRELOAD: %s (%s)\n",
			runtime.user_manager_ld_preload,
			runtime.user_manager_matches ? "includes WSF" : "does not include WSF"
		);
	} else {
		printf("user manager LD_PRELOAD: not set\n");
	}
	if (runtime.gnome_shell_found) {
		printf("gnome-shell pid: %d\n", (int) runtime.gnome_shell_pid);
		if (runtime.gnome_shell_ld_preload_present) {
			printf(
				"gnome-shell LD_PRELOAD: %s (%s)\n",
				runtime.gnome_shell_ld_preload,
				runtime.gnome_shell_ld_preload_matches ? "includes WSF" : "does not include WSF"
			);
		} else {
			printf("gnome-shell LD_PRELOAD: not set\n");
		}
		printf(
			"gnome-shell library mapped: %s\n",
			runtime.gnome_shell_library_mapped ? "yes" : "no"
		);
	} else {
		printf("gnome-shell: not running\n");
	}
	wsf_print_hyprland_status(&hyprland, true);
	wsf_print_hyprland_preload_status(&runtime, true);
	if (wsf_hyprland_found) {
		printf("hyprland WSF launcher: %s\n", wsf_hyprland);
	} else {
		printf("hyprland WSF launcher: not found (install wsf-hyprland)\n");
	}
	if (start_hyprland_found) {
		printf("start-hyprland: %s\n", start_hyprland);
		printf("hyprland launch recommendation: start-hyprland --path %s -- ...\n",
			wsf_hyprland_found ? wsf_hyprland : "wsf-hyprland"
		);
	} else {
		printf("start-hyprland: not found\n");
	}
	if (config_path != NULL) {
		printf(
			"config: %s (%s)\n",
			config_path,
			config_present ? "present" : "missing"
		);
	}
	printf("scroll_vertical_factor: %.4f (", factors.scroll_vertical);
	wsf_print_factor_status(status);
	printf(")\n");
	printf("scroll_horizontal_factor: %.4f\n", factors.scroll_horizontal);
	printf("pinch_zoom_factor: %.4f\n", factors.pinch_zoom);
	printf("pinch_rotate_factor: %.4f\n", factors.pinch_rotate);
	printf("legacy factor: %s\n", factors.used_legacy_factor ? "yes" : "no");
	if (env_factor != NULL && env_factor[0] != '\0') {
		printf("WSF_FACTOR: %s (env override)\n", env_factor);
	}
	if (env_scroll_vertical != NULL && env_scroll_vertical[0] != '\0') {
		printf("WSF_SCROLL_VERTICAL_FACTOR: %s (env override)\n", env_scroll_vertical);
	}
	if (env_scroll_horizontal != NULL && env_scroll_horizontal[0] != '\0') {
		printf("WSF_SCROLL_HORIZONTAL_FACTOR: %s (env override)\n", env_scroll_horizontal);
	}
	if (env_pinch_zoom != NULL && env_pinch_zoom[0] != '\0') {
		printf("WSF_PINCH_ZOOM_FACTOR: %s (env override)\n", env_pinch_zoom);
	}
	if (env_pinch_rotate != NULL && env_pinch_rotate[0] != '\0') {
		printf("WSF_PINCH_ROTATE_FACTOR: %s (env override)\n", env_pinch_rotate);
	}
	if (env_lib_path != NULL && env_lib_path[0] != '\0') {
		printf("WSF_LIB_PATH: %s (env override)\n", env_lib_path);
	}
	if (ld_preload != NULL && ld_preload[0] != '\0') {
		printf("current process LD_PRELOAD: %s\n", ld_preload);
	}

	wsf_doctor_symbols_print(&symbols);
	if (hyprland.running) {
		printf(
			"runtime scroll apply: active via Hyprland %s backend\n",
			wsf_hyprland_scroll_apply_method_name(hyprland.scroll_apply_method)
		);
		if (runtime.hyprland_library_mapped) {
			printf("runtime gesture reload: active via Hyprland gestures-only preload\n");
		} else {
			printf("runtime gesture reload: inactive; launch Hyprland through wsf-hyprland to use pinch controls\n");
			printf("hint: `start-hyprland --path $(command -v wsf-hyprland) -- ...`\n");
			printf("hint: for tuigreet, `--remember-session` overrides `--cmd`; use `--session-wrapper $(command -v wsf-session-wrapper)` for selected/remembered sessions.\n");
		}
	} else if (runtime.gnome_shell_library_mapped) {
		printf("runtime config reload: active (GNOME preload rereads factors on handled gestures)\n");
	} else {
		printf("runtime config reload: inactive until GNOME Shell loads WSF\n");
	}
	if (!hyprland.running && env_present && !runtime.user_manager_matches &&
		!runtime.gnome_shell_library_mapped) {
		printf("hint: environment.d exists but systemd --user has not picked it up yet.\n");
		printf("hint: run `wsf repair`, then log out/in.\n");
	}
	if (!hyprland.running && runtime.user_manager_matches && !runtime.gnome_shell_library_mapped) {
		printf("hint: systemd --user is ready but GNOME Shell has not loaded WSF yet.\n");
		printf("hint: `wsf repair` can verify setup; then log out/in, or reboot once if your distro keeps the old session environment.\n");
	}
	if (runtime.gnome_shell_library_mapped &&
		!runtime.gnome_shell_ld_preload_present) {
		printf("note: WSF strips itself from child LD_PRELOAD after loading to avoid inherited-preload issues.\n");
	}
	if (hyprland.running) {
		printf("note: Hyprland scroll changes apply live; restart Hyprland only to load/unload gesture preload\n");
	} else {
		printf("note: logout/login required after preload enable/disable\n");
	}
	return 0;
}

int main(int argc, char **argv) {
	const char *cmd = NULL;
	bool json = false;
	int i = 0;

	if (argc < 2) {
		wsf_print_usage(argv[0]);
		return 1;
	}

	cmd = argv[1];
	if (strcmp(cmd, "set") == 0) {
		if (argc < 3) {
			fprintf(stderr, "Missing factor.\n");
			return 1;
		}
		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "--json") == 0) {
				fprintf(stderr, "Option --json is not valid for set.\n");
				return 1;
			}
		}
		return wsf_cmd_set(argc, argv);
	}
	if (strcmp(cmd, "get") == 0) {
		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "--json") == 0) {
				json = true;
			} else {
				fprintf(stderr, "Unknown option for get: %s\n", argv[i]);
				return 1;
			}
		}
		return wsf_cmd_get(json);
	}
	if (strcmp(cmd, "apply") == 0) {
		if (argc != 2) {
			fprintf(stderr, "Command apply does not accept arguments.\n");
			return 1;
		}
		return wsf_cmd_apply();
	}
	if (strcmp(cmd, "enable") == 0) {
		if (argc != 2) {
			fprintf(stderr, "Command enable does not accept arguments.\n");
			return 1;
		}
		return wsf_cmd_enable();
	}
	if (strcmp(cmd, "repair") == 0) {
		if (argc != 2) {
			fprintf(stderr, "Command repair does not accept arguments.\n");
			return 1;
		}
		return wsf_cmd_repair();
	}
	if (strcmp(cmd, "disable") == 0) {
		if (argc != 2) {
			fprintf(stderr, "Command disable does not accept arguments.\n");
			return 1;
		}
		return wsf_cmd_disable();
	}
	if (strcmp(cmd, "status") == 0) {
		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "--json") == 0) {
				json = true;
			} else {
				fprintf(stderr, "Unknown option for status: %s\n", argv[i]);
				return 1;
			}
		}
		return wsf_cmd_status(json);
	}
	if (strcmp(cmd, "doctor") == 0) {
		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "--json") == 0) {
				json = true;
			} else {
				fprintf(stderr, "Unknown option for doctor: %s\n", argv[i]);
				return 1;
			}
		}
		return wsf_cmd_doctor(json);
	}

	fprintf(stderr, "Unknown command: %s\n", cmd);
	wsf_print_usage(argv[0]);
	return 1;
}
