#define _GNU_SOURCE

#include "wsf_config.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void wsf_debug_log(bool debug, const char *fmt, ...) {
	if (!debug) {
		return;
	}

	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "wsf: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

bool wsf_debug_enabled(void) {
	const char *env = getenv("WSF_DEBUG");

	return env != NULL && env[0] == '1';
}

static const char *wsf_home(void) {
	const char *home = getenv("HOME");

	if (home == NULL || home[0] == '\0') {
		return NULL;
	}

	return home;
}

const char *wsf_config_path(void) {
	static char path[PATH_MAX];
	const char *home = wsf_home();
	int written = 0;

	if (home == NULL) {
		return NULL;
	}

	written = snprintf(
		path,
		sizeof(path),
		"%s/.config/wayland-scroll-factor/config",
		home
	);
	if (written <= 0 || (size_t) written >= sizeof(path)) {
		return NULL;
	}

	return path;
}

static bool wsf_parse_factor_str(const char *input, double *out_factor) {
	char *end = NULL;
	double value = 0.0;
	locale_t c_locale = (locale_t) 0;

	if (input == NULL || out_factor == NULL) {
		return false;
	}

	while (isspace((unsigned char) *input)) {
		input++;
	}

	errno = 0;
	c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t) 0);
	if (c_locale != (locale_t) 0) {
		value = strtod_l(input, &end, c_locale);
		freelocale(c_locale);
	} else {
		value = strtod(input, &end);
	}
	if (input == end || errno == ERANGE) {
		return false;
	}

	while (isspace((unsigned char) *end)) {
		end++;
	}

	if (*end != '\0' && *end != '\n' && *end != '#') {
		return false;
	}

	*out_factor = value;
	return true;
}

static bool wsf_factor_in_range(double factor) {
	return factor >= WSF_FACTOR_MIN && factor <= WSF_FACTOR_MAX;
}

static int wsf_write_factor(FILE *file, const char *key, double factor) {
	int whole = 0;
	int frac = 0;

	if (file == NULL || key == NULL) {
		return -1;
	}

	whole = (int) factor;
	frac = (int) (((factor - (double) whole) * 10000.0) + 0.5);
	if (frac >= 10000) {
		whole++;
		frac -= 10000;
	}

	return fprintf(file, "%s=%d.%04d\n", key, whole, frac);
}

void wsf_config_values_init(struct wsf_config_values *values) {
	values->factor = WSF_FACTOR_DEFAULT;
	values->scroll_vertical_factor = WSF_FACTOR_DEFAULT;
	values->scroll_horizontal_factor = WSF_FACTOR_DEFAULT;
	values->pinch_zoom_factor = WSF_FACTOR_DEFAULT;
	values->pinch_rotate_factor = WSF_FACTOR_DEFAULT;
	values->has_factor = false;
	values->has_scroll_vertical = false;
	values->has_scroll_horizontal = false;
	values->has_pinch_zoom = false;
	values->has_pinch_rotate = false;
}

static char *wsf_trim(char *str) {
	char *end = NULL;

	while (isspace((unsigned char) *str)) {
		str++;
	}
	if (*str == '\0') {
		return str;
	}

	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char) *end)) {
		*end = '\0';
		end--;
	}

	return str;
}

int wsf_config_read(struct wsf_config_values *out_values, bool debug) {
	FILE *file = NULL;
	/*
	 * Fixed-size line buffer instead of getline: this parser runs inside
	 * gnome-shell (init + the live-reload tick). A corrupted or hostile
	 * config with one gigantic line would make getline malloc the whole
	 * line, spiking the compositor's memory. A valid line is "key=value"
	 * with keys < 40 chars and short float values, so 256 bytes is ample;
	 * over-long lines are drained and rejected.
	 */
	char line[256];
	struct stat st;
	bool found = false;
	bool invalid = false;
	const char *path = wsf_config_path();

	if (out_values == NULL) {
		return WSF_CONFIG_ERROR;
	}

	wsf_config_values_init(out_values);

	if (path == NULL) {
		wsf_debug_log(debug, "config path not available (HOME missing?)");
		return WSF_CONFIG_ERROR;
	}

	file = fopen(path, "r");
	if (file == NULL) {
		if (errno == ENOENT) {
			return WSF_CONFIG_MISSING;
		}
		wsf_debug_log(debug, "failed to open config: %s", strerror(errno));
		return WSF_CONFIG_ERROR;
	}

	/* Refuse implausibly large config files outright (64 KiB ceiling). */
	if (fstat(fileno(file), &st) == 0 && st.st_size > 65536) {
		wsf_debug_log(debug, "config file too large (%lld bytes); ignoring",
			(long long) st.st_size);
		fclose(file);
		return WSF_CONFIG_INVALID;
	}

	while (fgets(line, (int) sizeof(line), file) != NULL) {
		char *cursor = line;

		/* Drain the rest of an over-long line so the tail isn't parsed
		 * as a separate bogus line. */
		if (strchr(line, '\n') == NULL && !feof(file)) {
			int ch;
			while ((ch = fgetc(file)) != '\n' && ch != EOF) {
				/* discard */
			}
			invalid = true;
			continue;
		}
		char *eq = NULL;
		char *key = NULL;
		char *value = NULL;
		double factor = 0.0;

		while (isspace((unsigned char) *cursor)) {
			cursor++;
		}

		if (*cursor == '#' || *cursor == '\0' || *cursor == '\n') {
			continue;
		}

		eq = strchr(cursor, '=');
		if (eq == NULL) {
			continue;
		}

		*eq = '\0';
		key = wsf_trim(cursor);
		value = wsf_trim(eq + 1);

		if (strcmp(key, "factor") == 0) {
			if (!wsf_parse_factor_str(value, &factor) ||
				!wsf_factor_in_range(factor)) {
				invalid = true;
				continue;
			}
			out_values->factor = factor;
			out_values->has_factor = true;
			found = true;
			continue;
		}
		if (strcmp(key, "scroll_vertical_factor") == 0) {
			if (!wsf_parse_factor_str(value, &factor) ||
				!wsf_factor_in_range(factor)) {
				invalid = true;
				continue;
			}
			out_values->scroll_vertical_factor = factor;
			out_values->has_scroll_vertical = true;
			found = true;
			continue;
		}
		if (strcmp(key, "scroll_horizontal_factor") == 0) {
			if (!wsf_parse_factor_str(value, &factor) ||
				!wsf_factor_in_range(factor)) {
				invalid = true;
				continue;
			}
			out_values->scroll_horizontal_factor = factor;
			out_values->has_scroll_horizontal = true;
			found = true;
			continue;
		}
		if (strcmp(key, "pinch_zoom_factor") == 0) {
			if (!wsf_parse_factor_str(value, &factor) ||
				!wsf_factor_in_range(factor)) {
				invalid = true;
				continue;
			}
			out_values->pinch_zoom_factor = factor;
			out_values->has_pinch_zoom = true;
			found = true;
			continue;
		}
		if (strcmp(key, "pinch_rotate_factor") == 0) {
			if (!wsf_parse_factor_str(value, &factor) ||
				!wsf_factor_in_range(factor)) {
				invalid = true;
				continue;
			}
			out_values->pinch_rotate_factor = factor;
			out_values->has_pinch_rotate = true;
			found = true;
			continue;
		}
	}

	fclose(file);

	if (invalid) {
		wsf_debug_log(debug, "invalid config value; using defaults for that key");
		return WSF_CONFIG_INVALID;
	}

	if (!found) {
		return WSF_CONFIG_MISSING;
	}

	return WSF_CONFIG_OK;
}

static bool wsf_env_factor(const char *name, double *out_factor, bool debug) {
	const char *env = getenv(name);

	if (env == NULL || env[0] == '\0') {
		return false;
	}
	if (wsf_parse_factor_str(env, out_factor) &&
		wsf_factor_in_range(*out_factor)) {
		return true;
	}

	wsf_debug_log(debug, "invalid %s override; ignoring", name);
	return false;
}

int wsf_effective_factors(struct wsf_effective_factors *out_factors, bool debug) {
	struct wsf_config_values cfg;
	double base_factor = WSF_FACTOR_DEFAULT;
	double env_factor = WSF_FACTOR_DEFAULT;
	int status = WSF_CONFIG_OK;

	if (out_factors == NULL) {
		return WSF_CONFIG_ERROR;
	}

	out_factors->scroll_vertical = WSF_FACTOR_DEFAULT;
	out_factors->scroll_horizontal = WSF_FACTOR_DEFAULT;
	out_factors->pinch_zoom = WSF_FACTOR_DEFAULT;
	out_factors->pinch_rotate = WSF_FACTOR_DEFAULT;
	out_factors->used_legacy_factor = false;

	status = wsf_config_read(&cfg, debug);
	if (status == WSF_CONFIG_ERROR) {
		return status;
	}

	if (cfg.has_factor) {
		base_factor = cfg.factor;
		out_factors->used_legacy_factor = true;
	}

	out_factors->scroll_vertical = cfg.has_scroll_vertical ?
		cfg.scroll_vertical_factor : base_factor;
	out_factors->scroll_horizontal = cfg.has_scroll_horizontal ?
		cfg.scroll_horizontal_factor : base_factor;
	out_factors->pinch_zoom = cfg.has_pinch_zoom ?
		cfg.pinch_zoom_factor : WSF_FACTOR_DEFAULT;
	out_factors->pinch_rotate = cfg.has_pinch_rotate ?
		cfg.pinch_rotate_factor : WSF_FACTOR_DEFAULT;

	if (wsf_env_factor("WSF_FACTOR", &env_factor, debug)) {
		out_factors->scroll_vertical = env_factor;
		out_factors->scroll_horizontal = env_factor;
		out_factors->used_legacy_factor = true;
	}
	if (wsf_env_factor("WSF_SCROLL_VERTICAL_FACTOR", &env_factor, debug)) {
		out_factors->scroll_vertical = env_factor;
	}
	if (wsf_env_factor("WSF_SCROLL_HORIZONTAL_FACTOR", &env_factor, debug)) {
		out_factors->scroll_horizontal = env_factor;
	}
	if (wsf_env_factor("WSF_PINCH_ZOOM_FACTOR", &env_factor, debug)) {
		out_factors->pinch_zoom = env_factor;
	}
	if (wsf_env_factor("WSF_PINCH_ROTATE_FACTOR", &env_factor, debug)) {
		out_factors->pinch_rotate = env_factor;
	}

	return status;
}

static int wsf_mkdir(const char *path, bool debug) {
	if (mkdir(path, 0700) == 0) {
		return 0;
	}
	if (errno == EEXIST) {
		return 0;
	}
	wsf_debug_log(debug, "failed to create directory %s: %s", path, strerror(errno));
	return -1;
}

int wsf_config_write(double factor, bool debug) {
	struct wsf_config_values updates;

	if (!wsf_factor_in_range(factor)) {
		wsf_debug_log(debug, "factor out of range for config write");
		return -1;
	}

	wsf_config_values_init(&updates);
	updates.factor = factor;
	updates.has_factor = true;

	return wsf_config_write_updates(&updates, debug);
}

static int wsf_config_write_all(
	const struct wsf_config_values *values,
	bool debug
) {
	const char *home = wsf_home();
	const char *path = wsf_config_path();
	char config_dir[PATH_MAX];
	char base_dir[PATH_MAX];
	char tmp_path[PATH_MAX];
	FILE *file = NULL;
	int fd = -1;
	int written = 0;

	if (home == NULL || path == NULL) {
		wsf_debug_log(debug, "cannot resolve HOME for config write");
		return -1;
	}

	written = snprintf(base_dir, sizeof(base_dir), "%s/.config", home);
	if (written <= 0 || (size_t) written >= sizeof(base_dir)) {
		return -1;
	}

	written = snprintf(
		config_dir,
		sizeof(config_dir),
		"%s/.config/wayland-scroll-factor",
		home
	);
	if (written <= 0 || (size_t) written >= sizeof(config_dir)) {
		return -1;
	}

	if (wsf_mkdir(base_dir, debug) != 0) {
		return -1;
	}
	if (wsf_mkdir(config_dir, debug) != 0) {
		return -1;
	}

	written = snprintf(
		tmp_path,
		sizeof(tmp_path),
		"%s/.config/wayland-scroll-factor/config.tmp.XXXXXX",
		home
	);
	if (written <= 0 || (size_t) written >= sizeof(tmp_path)) {
		return -1;
	}

	fd = mkstemp(tmp_path);
	if (fd < 0) {
		wsf_debug_log(debug, "failed to create temp config: %s", strerror(errno));
		return -1;
	}

	file = fdopen(fd, "w");
	if (file == NULL) {
		close(fd);
		unlink(tmp_path);
		wsf_debug_log(debug, "failed to write config: %s", strerror(errno));
		return -1;
	}

	if (values->has_factor) {
		if (wsf_write_factor(file, "factor", values->factor) < 0) {
			goto write_failed;
		}
	}
	if (values->has_scroll_vertical) {
		if (wsf_write_factor(file, "scroll_vertical_factor", values->scroll_vertical_factor) < 0) {
			goto write_failed;
		}
	}
	if (values->has_scroll_horizontal) {
		if (wsf_write_factor(file, "scroll_horizontal_factor", values->scroll_horizontal_factor) < 0) {
			goto write_failed;
		}
	}
	if (values->has_pinch_zoom) {
		if (wsf_write_factor(file, "pinch_zoom_factor", values->pinch_zoom_factor) < 0) {
			goto write_failed;
		}
	}
	if (values->has_pinch_rotate) {
		if (wsf_write_factor(file, "pinch_rotate_factor", values->pinch_rotate_factor) < 0) {
			goto write_failed;
		}
	}

	if (fflush(file) != 0) {
		wsf_debug_log(debug, "failed to flush config: %s", strerror(errno));
		fclose(file);
		unlink(tmp_path);
		return -1;
	}
	if (fsync(fd) != 0) {
		wsf_debug_log(debug, "failed to sync config: %s", strerror(errno));
		fclose(file);
		unlink(tmp_path);
		return -1;
	}
	if (fclose(file) != 0) {
		wsf_debug_log(debug, "failed to close config: %s", strerror(errno));
		unlink(tmp_path);
		return -1;
	}
	if (rename(tmp_path, path) != 0) {
		wsf_debug_log(debug, "failed to replace config: %s", strerror(errno));
		unlink(tmp_path);
		return -1;
	}

	return 0;

write_failed:
	wsf_debug_log(debug, "failed to write config: %s", strerror(errno));
	fclose(file);
	unlink(tmp_path);
	return -1;
}

int wsf_config_write_updates(
	const struct wsf_config_values *updates,
	bool debug
) {
	struct wsf_config_values values;
	int status = WSF_CONFIG_OK;

	if (updates == NULL) {
		return -1;
	}

	wsf_config_values_init(&values);
	status = wsf_config_read(&values, debug);
	if (status == WSF_CONFIG_ERROR) {
		wsf_config_values_init(&values);
	}

	if (updates->has_factor) {
		if (!wsf_factor_in_range(updates->factor)) {
			return -1;
		}
		values.factor = updates->factor;
		values.has_factor = true;
	}
	if (updates->has_scroll_vertical) {
		if (!wsf_factor_in_range(updates->scroll_vertical_factor)) {
			return -1;
		}
		values.scroll_vertical_factor = updates->scroll_vertical_factor;
		values.has_scroll_vertical = true;
	}
	if (updates->has_scroll_horizontal) {
		if (!wsf_factor_in_range(updates->scroll_horizontal_factor)) {
			return -1;
		}
		values.scroll_horizontal_factor = updates->scroll_horizontal_factor;
		values.has_scroll_horizontal = true;
	}
	if (updates->has_pinch_zoom) {
		if (!wsf_factor_in_range(updates->pinch_zoom_factor)) {
			return -1;
		}
		values.pinch_zoom_factor = updates->pinch_zoom_factor;
		values.has_pinch_zoom = true;
	}
	if (updates->has_pinch_rotate) {
		if (!wsf_factor_in_range(updates->pinch_rotate_factor)) {
			return -1;
		}
		values.pinch_rotate_factor = updates->pinch_rotate_factor;
		values.has_pinch_rotate = true;
	}

	return wsf_config_write_all(&values, debug);
}
