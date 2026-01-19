#define _GNU_SOURCE

#include "wsf_config.h"

#include <errno.h>
#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static bool wsf_env_file_path(char *buf, size_t len) {
	return wsf_build_path(buf, len, ".config/environment.d/wayland-scroll-factor.conf");
}

static bool wsf_env_dir_path(char *buf, size_t len) {
	return wsf_build_path(buf, len, ".config/environment.d");
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
	fprintf(stderr, "  enable         Enable preload via environment.d\n");
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
	bool debug = wsf_debug_enabled();
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
	FILE *file = NULL;
	const char *existing = getenv("LD_PRELOAD");

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

	file = fopen(env_path, "w");
	if (file == NULL) {
		fprintf(stderr, "Failed to write %s: %s\n", env_path, strerror(errno));
		return 1;
	}

	fprintf(file, "# Generated by wsf enable\n");
	fprintf(file, "LD_PRELOAD=%s\n", lib_path);
	fclose(file);

	if (existing != NULL && existing[0] != '\0') {
		fprintf(stderr,
			"Warning: LD_PRELOAD already set; environment.d will override it.\n"
			"Hint: combine manually, e.g. LD_PRELOAD=%s:$LD_PRELOAD\n",
			lib_path
		);
	}

	printf("enabled (logout/login required)\n");
	return 0;
}

static int wsf_cmd_disable(void) {
	char env_path[512];

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

	printf("disabled (logout/login required)\n");
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

static int wsf_cmd_status(bool json) {
	char env_path[512];
	char lib_path[512];
	const char *config_path = wsf_config_path();
	struct wsf_effective_factors factors;
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
	printf("note: logout/login required after enable/disable\n");
	return 0;
}

static bool wsf_run_command(const char *cmd, char *buf, size_t len) {
	FILE *pipe = NULL;

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

	pclose(pipe);
	buf[strcspn(buf, "\n")] = '\0';
	return true;
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
	printf("scroll axis source filter: enabled (finger/continuous)\n");
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
		printf("LD_PRELOAD: %s\n", ld_preload);
	}

	wsf_doctor_symbols_print(&symbols);
	printf("note: logout/login required after enable/disable\n");
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
	if (strcmp(cmd, "enable") == 0) {
		return wsf_cmd_enable();
	}
	if (strcmp(cmd, "disable") == 0) {
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
