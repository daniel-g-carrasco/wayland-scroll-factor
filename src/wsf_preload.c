#define _GNU_SOURCE

#include <ctype.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __has_include
#if __has_include(<libinput.h>)
#include <libinput.h>
#define WSF_HAVE_LIBINPUT_HEADERS 1
#endif
#endif

#include "wsf_config.h"
#include "wsf_proc.h"

struct libinput_event;
struct libinput_event_pointer;
struct libinput_event_gesture;

#if defined(WSF_HAVE_LIBINPUT_HEADERS)
typedef enum libinput_pointer_axis wsf_axis_t;
typedef enum libinput_pointer_axis_source wsf_axis_source_t;
typedef enum libinput_event_type wsf_event_type_t;
#else
typedef int wsf_axis_t;
typedef int wsf_axis_source_t;
typedef int wsf_event_type_t;
#endif

typedef double (*wsf_scroll_value_fn)(struct libinput_event_pointer *, wsf_axis_t);
typedef wsf_axis_source_t (*wsf_axis_source_fn)(struct libinput_event_pointer *);
typedef double (*wsf_gesture_value_fn)(struct libinput_event_gesture *);
typedef struct libinput_event *(*wsf_base_event_fn)(struct libinput_event_pointer *);
typedef wsf_event_type_t (*wsf_event_type_fn)(struct libinput_event *);

#if defined(WSF_HAVE_LIBINPUT_HEADERS) && defined(LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)
#define WSF_AXIS_SCROLL_VERTICAL LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL
#define WSF_AXIS_SCROLL_HORIZONTAL LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL
#else
#define WSF_AXIS_SCROLL_VERTICAL 0
#define WSF_AXIS_SCROLL_HORIZONTAL 1
#endif

#if defined(WSF_HAVE_LIBINPUT_HEADERS) && defined(LIBINPUT_POINTER_AXIS_SOURCE_WHEEL)
#define WSF_AXIS_SOURCE_WHEEL LIBINPUT_POINTER_AXIS_SOURCE_WHEEL
#define WSF_AXIS_SOURCE_FINGER LIBINPUT_POINTER_AXIS_SOURCE_FINGER
#define WSF_AXIS_SOURCE_CONTINUOUS LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS
#define WSF_AXIS_SOURCE_WHEEL_TILT LIBINPUT_POINTER_AXIS_SOURCE_WHEEL_TILT
#else
#define WSF_AXIS_SOURCE_WHEEL 1
#define WSF_AXIS_SOURCE_FINGER 2
#define WSF_AXIS_SOURCE_CONTINUOUS 3
#define WSF_AXIS_SOURCE_WHEEL_TILT 4
#endif

#if defined(WSF_HAVE_LIBINPUT_HEADERS) && defined(LIBINPUT_EVENT_POINTER_SCROLL_FINGER)
#define WSF_EVENT_POINTER_AXIS LIBINPUT_EVENT_POINTER_AXIS
#define WSF_EVENT_POINTER_SCROLL_WHEEL LIBINPUT_EVENT_POINTER_SCROLL_WHEEL
#define WSF_EVENT_POINTER_SCROLL_FINGER LIBINPUT_EVENT_POINTER_SCROLL_FINGER
#define WSF_EVENT_POINTER_SCROLL_CONTINUOUS LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS
#else
#define WSF_EVENT_POINTER_AXIS 403
#define WSF_EVENT_POINTER_SCROLL_WHEEL 404
#define WSF_EVENT_POINTER_SCROLL_FINGER 405
#define WSF_EVENT_POINTER_SCROLL_CONTINUOUS 406
#endif

static wsf_scroll_value_fn wsf_real_scroll_value = NULL;
static wsf_scroll_value_fn wsf_real_scroll_value_v120 = NULL;
static wsf_scroll_value_fn wsf_real_axis_value = NULL;
static wsf_scroll_value_fn wsf_real_axis_value_discrete = NULL;
static wsf_axis_source_fn wsf_real_axis_source = NULL;
static wsf_gesture_value_fn wsf_real_gesture_scale = NULL;
static wsf_gesture_value_fn wsf_real_gesture_angle_delta = NULL;
static wsf_base_event_fn wsf_real_base_event = NULL;
static wsf_event_type_fn wsf_real_event_type = NULL;

static void wsf_init_internal(void);

static bool wsf_debug = false;
static bool wsf_active = false;
static double wsf_scroll_vertical_factor = WSF_FACTOR_DEFAULT;
static double wsf_scroll_horizontal_factor = WSF_FACTOR_DEFAULT;
static double wsf_pinch_zoom_factor = WSF_FACTOR_DEFAULT;
static double wsf_pinch_rotate_factor = WSF_FACTOR_DEFAULT;
static bool wsf_init_done = false;
static bool wsf_config_seen = false;
static bool wsf_config_present = false;
static long long wsf_last_config_check_ms = 0;
static struct timespec wsf_config_mtime = {0};
static bool wsf_logged_missing_scroll = false;
static bool wsf_logged_missing_scroll_v120 = false;
static bool wsf_logged_missing_axis_value = false;
static bool wsf_logged_missing_axis_value_discrete = false;
static bool wsf_logged_missing_axis_source = false;
static bool wsf_logged_missing_gesture_scale = false;
static bool wsf_logged_missing_gesture_angle = false;

static void wsf_debug_log(const char *fmt, ...) {
	if (!wsf_debug) {
		return;
	}

	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "wsf: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

static long long wsf_monotonic_ms(void) {
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}

	return (long long) ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static bool wsf_timespec_equal(struct timespec a, struct timespec b) {
	return a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec;
}

static const char *wsf_basename(const char *path) {
	const char *slash = strrchr(path, '/');

	if (slash == NULL) {
		return path;
	}

	return slash + 1;
}

static bool wsf_is_self_preload_entry(const char *entry, const char *self_path) {
	if (entry == NULL || entry[0] == '\0') {
		return false;
	}

	if (self_path != NULL && self_path[0] != '\0' && strcmp(entry, self_path) == 0) {
		return true;
	}

	return strcmp(wsf_basename(entry), "libwsf_preload.so") == 0;
}

static void wsf_prune_ld_preload_env(void) {
	Dl_info info;
	const char *preload = getenv("LD_PRELOAD");
	char *copy = NULL;
	char *filtered = NULL;
	char *cursor = NULL;
	size_t filtered_len = 0;
	bool changed = false;

	if (preload == NULL || preload[0] == '\0') {
		return;
	}

	if (dladdr((void *) wsf_init_internal, &info) == 0 ||
		info.dli_fname == NULL || info.dli_fname[0] == '\0') {
		return;
	}

	copy = strdup(preload);
	filtered = calloc(strlen(preload) + 1, 1);
	if (copy == NULL || filtered == NULL) {
		free(copy);
		free(filtered);
		return;
	}

	cursor = copy;
	while (*cursor != '\0') {
		char *token = NULL;
		size_t token_len = 0;

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

		if (wsf_is_self_preload_entry(token, info.dli_fname)) {
			changed = true;
			continue;
		}

		token_len = strlen(token);
		if (token_len == 0) {
			continue;
		}

		if (filtered_len != 0) {
			filtered[filtered_len++] = ' ';
		}
		memcpy(filtered + filtered_len, token, token_len);
		filtered_len += token_len;
		filtered[filtered_len] = '\0';
	}

	if (changed) {
		if (filtered_len == 0) {
			unsetenv("LD_PRELOAD");
			wsf_debug_log("removed WSF from LD_PRELOAD for child processes");
		} else {
			setenv("LD_PRELOAD", filtered, 1);
			wsf_debug_log(
				"pruned WSF from LD_PRELOAD for child processes: %s",
				filtered
			);
		}
	}

	free(copy);
	free(filtered);
}

static bool wsf_config_snapshot(struct timespec *mtime, bool *present) {
	const char *path = wsf_config_path();
	struct stat st;

	if (mtime == NULL || present == NULL) {
		return false;
	}

	mtime->tv_sec = 0;
	mtime->tv_nsec = 0;
	*present = false;

	if (path == NULL || path[0] == '\0') {
		return false;
	}

	if (stat(path, &st) != 0) {
		return false;
	}

	*present = true;
	*mtime = st.st_mtim;
	return true;
}

static void wsf_apply_effective_factors(bool log_change) {
	struct wsf_effective_factors factors;

	if (wsf_effective_factors(&factors, wsf_debug) == WSF_CONFIG_ERROR) {
		factors.scroll_vertical = WSF_FACTOR_DEFAULT;
		factors.scroll_horizontal = WSF_FACTOR_DEFAULT;
		factors.pinch_zoom = WSF_FACTOR_DEFAULT;
		factors.pinch_rotate = WSF_FACTOR_DEFAULT;
	}

	wsf_scroll_vertical_factor = factors.scroll_vertical;
	wsf_scroll_horizontal_factor = factors.scroll_horizontal;
	wsf_pinch_zoom_factor = factors.pinch_zoom;
	wsf_pinch_rotate_factor = factors.pinch_rotate;

	if (log_change) {
		wsf_debug_log(
			"reloaded factors: scroll_vertical=%.4f scroll_horizontal=%.4f pinch_zoom=%.4f pinch_rotate=%.4f",
			wsf_scroll_vertical_factor,
			wsf_scroll_horizontal_factor,
			wsf_pinch_zoom_factor,
			wsf_pinch_rotate_factor
		);
	}
}

static void wsf_reload_factors_if_needed(void) {
	struct timespec mtime = {0};
	long long now_ms = 0;
	bool present = false;
	bool changed = false;

	if (!wsf_active) {
		return;
	}

	now_ms = wsf_monotonic_ms();
	if (wsf_last_config_check_ms != 0 &&
		now_ms - wsf_last_config_check_ms < 250) {
		return;
	}
	wsf_last_config_check_ms = now_ms;

	(void) wsf_config_snapshot(&mtime, &present);

	if (!wsf_config_seen) {
		wsf_config_seen = true;
		wsf_config_present = present;
		wsf_config_mtime = mtime;
		return;
	}

	changed = present != wsf_config_present;
	if (!changed && present) {
		changed = !wsf_timespec_equal(mtime, wsf_config_mtime);
	}

	if (!changed) {
		return;
	}

	wsf_config_present = present;
	wsf_config_mtime = mtime;
	wsf_apply_effective_factors(true);
}

static void *wsf_load_symbol(const char *name) {
	const char *error = NULL;
	void *symbol = NULL;

	dlerror();
	symbol = dlsym(RTLD_NEXT, name);
	error = dlerror();

	if (error != NULL) {
		wsf_debug_log("symbol %s not found: %s", name, error);
		return NULL;
	}

	return symbol;
}

static void wsf_init_internal(void) {
	struct timespec config_mtime = {0};
	bool config_present = false;
	char proc_name[128] = "unknown";

	if (wsf_init_done) {
		return;
	}

	wsf_debug = wsf_debug_enabled();
	wsf_apply_effective_factors(false);
	wsf_active = wsf_proc_is_target("gnome-shell");
	wsf_real_scroll_value =
		(wsf_scroll_value_fn) wsf_load_symbol(
			"libinput_event_pointer_get_scroll_value"
		);
	wsf_real_scroll_value_v120 =
		(wsf_scroll_value_fn) wsf_load_symbol(
			"libinput_event_pointer_get_scroll_value_v120"
		);
	wsf_real_axis_value =
		(wsf_scroll_value_fn) wsf_load_symbol(
			"libinput_event_pointer_get_axis_value"
		);
	wsf_real_axis_value_discrete =
		(wsf_scroll_value_fn) wsf_load_symbol(
			"libinput_event_pointer_get_axis_value_discrete"
		);
	wsf_real_axis_source =
		(wsf_axis_source_fn) wsf_load_symbol(
			"libinput_event_pointer_get_axis_source"
		);
	wsf_real_base_event =
		(wsf_base_event_fn) wsf_load_symbol(
			"libinput_event_pointer_get_base_event"
		);
	wsf_real_event_type =
		(wsf_event_type_fn) wsf_load_symbol(
			"libinput_event_get_type"
		);
	wsf_real_gesture_scale =
		(wsf_gesture_value_fn) wsf_load_symbol(
			"libinput_event_gesture_get_scale"
		);
	wsf_real_gesture_angle_delta =
		(wsf_gesture_value_fn) wsf_load_symbol(
			"libinput_event_gesture_get_angle_delta"
		);

	wsf_init_done = true;
	wsf_prune_ld_preload_env();
	(void) wsf_config_snapshot(&config_mtime, &config_present);
	wsf_config_seen = true;
	wsf_config_present = config_present;
	wsf_config_mtime = config_mtime;
	wsf_last_config_check_ms = wsf_monotonic_ms();

	if (wsf_proc_name(proc_name, sizeof(proc_name))) {
		wsf_debug_log(
			"init: process=%s active=%s scroll_vertical=%.4f scroll=%s v120=%s",
			proc_name,
			wsf_active ? "yes" : "no",
			wsf_scroll_vertical_factor,
			wsf_real_scroll_value ? "yes" : "no",
			wsf_real_scroll_value_v120 ? "yes" : "no"
		);
		wsf_debug_log(
			"init: scroll_vertical=%.4f scroll_horizontal=%.4f",
			wsf_scroll_vertical_factor,
			wsf_scroll_horizontal_factor
		);
		wsf_debug_log(
			"init: axis_value=%s axis_discrete=%s",
			wsf_real_axis_value ? "yes" : "no",
			wsf_real_axis_value_discrete ? "yes" : "no"
		);
		wsf_debug_log(
			"init: event_type=%s base_event=%s",
			wsf_real_event_type ? "yes" : "no",
			wsf_real_base_event ? "yes" : "no"
		);
		wsf_debug_log(
			"init: axis_source=%s",
			wsf_real_axis_source ? "yes" : "no"
		);
		wsf_debug_log(
			"init: gesture_scale=%s gesture_angle=%s pinch_zoom=%.4f pinch_rotate=%.4f",
			wsf_real_gesture_scale ? "yes" : "no",
			wsf_real_gesture_angle_delta ? "yes" : "no",
			wsf_pinch_zoom_factor,
			wsf_pinch_rotate_factor
		);
	} else {
		wsf_debug_log(
			"init: process=unknown active=%s scroll_vertical=%.4f scroll=%s v120=%s",
			wsf_active ? "yes" : "no",
			wsf_scroll_vertical_factor,
			wsf_real_scroll_value ? "yes" : "no",
			wsf_real_scroll_value_v120 ? "yes" : "no"
		);
		wsf_debug_log(
			"init: scroll_vertical=%.4f scroll_horizontal=%.4f",
			wsf_scroll_vertical_factor,
			wsf_scroll_horizontal_factor
		);
		wsf_debug_log(
			"init: axis_value=%s axis_discrete=%s",
			wsf_real_axis_value ? "yes" : "no",
			wsf_real_axis_value_discrete ? "yes" : "no"
		);
		wsf_debug_log(
			"init: event_type=%s base_event=%s",
			wsf_real_event_type ? "yes" : "no",
			wsf_real_base_event ? "yes" : "no"
		);
		wsf_debug_log(
			"init: axis_source=%s",
			wsf_real_axis_source ? "yes" : "no"
		);
		wsf_debug_log(
			"init: gesture_scale=%s gesture_angle=%s pinch_zoom=%.4f pinch_rotate=%.4f",
			wsf_real_gesture_scale ? "yes" : "no",
			wsf_real_gesture_angle_delta ? "yes" : "no",
			wsf_pinch_zoom_factor,
			wsf_pinch_rotate_factor
		);
	}
}

__attribute__((constructor)) static void wsf_init(void) {
	wsf_init_internal();
}

static void wsf_ensure_init(void) {
	if (!wsf_init_done) {
		wsf_init_internal();
	}
}

static double wsf_scroll_factor_for_axis(wsf_axis_t axis) {
	if (axis == WSF_AXIS_SCROLL_HORIZONTAL) {
		return wsf_scroll_horizontal_factor;
	}

	return wsf_scroll_vertical_factor;
}

static bool wsf_should_scale_scroll(
	struct libinput_event_pointer *event,
	double factor
) {
	wsf_axis_source_t source = 0;
	int type = 0;
	struct libinput_event *base = NULL;

	if (!wsf_active || factor == 1.0) {
		return false;
	}

	if (wsf_real_base_event == NULL) {
		wsf_real_base_event =
			(wsf_base_event_fn) wsf_load_symbol(
				"libinput_event_pointer_get_base_event"
			);
	}
	if (wsf_real_event_type == NULL) {
		wsf_real_event_type =
			(wsf_event_type_fn) wsf_load_symbol(
				"libinput_event_get_type"
			);
	}

	if (wsf_real_base_event != NULL && wsf_real_event_type != NULL) {
		base = wsf_real_base_event(event);
		if (base != NULL) {
			type = wsf_real_event_type(base);
			if (type == WSF_EVENT_POINTER_SCROLL_WHEEL) {
				return false;
			}
			if (type == WSF_EVENT_POINTER_SCROLL_FINGER ||
				type == WSF_EVENT_POINTER_SCROLL_CONTINUOUS) {
				return true;
			}
			if (type != WSF_EVENT_POINTER_AXIS) {
				return false;
			}
		}
	}

	if (wsf_real_axis_source == NULL) {
		wsf_real_axis_source =
			(wsf_axis_source_fn) wsf_load_symbol(
				"libinput_event_pointer_get_axis_source"
			);
	}

	if (wsf_real_axis_source == NULL) {
		if (wsf_debug && !wsf_logged_missing_axis_source) {
			wsf_debug_log("axis_source symbol missing; scroll scaling disabled");
			wsf_logged_missing_axis_source = true;
		}
		return false;
	}

	source = wsf_real_axis_source(event);
	if (source == WSF_AXIS_SOURCE_FINGER ||
		source == WSF_AXIS_SOURCE_CONTINUOUS) {
		return true;
	}

	return false;
}

double libinput_event_pointer_get_axis_value(
	struct libinput_event_pointer *event,
	wsf_axis_t axis
) {
	double value = 0.0;
	double factor = 0.0;

	wsf_ensure_init();
	wsf_reload_factors_if_needed();

	if (wsf_real_axis_value == NULL) {
		wsf_real_axis_value =
			(wsf_scroll_value_fn) wsf_load_symbol(
				"libinput_event_pointer_get_axis_value"
			);
	}

	if (wsf_real_axis_value == NULL) {
		if (wsf_debug && !wsf_logged_missing_axis_value) {
			wsf_debug_log("axis_value symbol missing; returning 0");
			wsf_logged_missing_axis_value = true;
		}
		return 0.0;
	}

	value = wsf_real_axis_value(event, axis);
	factor = wsf_scroll_factor_for_axis(axis);
	if (!wsf_should_scale_scroll(event, factor)) {
		return value;
	}

	return value * factor;
}

double libinput_event_pointer_get_axis_value_discrete(
	struct libinput_event_pointer *event,
	wsf_axis_t axis
) {
	double value = 0.0;
	double factor = 0.0;

	wsf_ensure_init();
	wsf_reload_factors_if_needed();

	if (wsf_real_axis_value_discrete == NULL) {
		wsf_real_axis_value_discrete =
			(wsf_scroll_value_fn) wsf_load_symbol(
				"libinput_event_pointer_get_axis_value_discrete"
			);
	}

	if (wsf_real_axis_value_discrete == NULL) {
		if (wsf_debug && !wsf_logged_missing_axis_value_discrete) {
			wsf_debug_log("axis_value_discrete symbol missing; returning 0");
			wsf_logged_missing_axis_value_discrete = true;
		}
		return 0.0;
	}

	value = wsf_real_axis_value_discrete(event, axis);
	factor = wsf_scroll_factor_for_axis(axis);
	if (!wsf_should_scale_scroll(event, factor)) {
		return value;
	}

	return value * factor;
}

double libinput_event_pointer_get_scroll_value(
	struct libinput_event_pointer *event,
	wsf_axis_t axis
) {
	double value = 0.0;
	double factor = 0.0;

	wsf_ensure_init();
	wsf_reload_factors_if_needed();

	if (wsf_real_scroll_value == NULL) {
		wsf_real_scroll_value =
			(wsf_scroll_value_fn) wsf_load_symbol(
				"libinput_event_pointer_get_scroll_value"
			);
	}

	if (wsf_real_scroll_value == NULL) {
		if (wsf_debug && !wsf_logged_missing_scroll) {
			wsf_debug_log("scroll_value symbol missing; returning 0");
			wsf_logged_missing_scroll = true;
		}
		return 0.0;
	}

	value = wsf_real_scroll_value(event, axis);
	factor = wsf_scroll_factor_for_axis(axis);
	if (!wsf_should_scale_scroll(event, factor)) {
		return value;
	}

	return value * factor;
}

double libinput_event_pointer_get_scroll_value_v120(
	struct libinput_event_pointer *event,
	wsf_axis_t axis
) {
	double value = 0.0;
	double factor = 0.0;

	wsf_ensure_init();
	wsf_reload_factors_if_needed();

	if (wsf_real_scroll_value_v120 == NULL) {
		wsf_real_scroll_value_v120 =
			(wsf_scroll_value_fn) wsf_load_symbol(
				"libinput_event_pointer_get_scroll_value_v120"
			);
	}

	if (wsf_real_scroll_value_v120 == NULL) {
		if (wsf_debug && !wsf_logged_missing_scroll_v120) {
			wsf_debug_log("scroll_value_v120 symbol missing; returning 0");
			wsf_logged_missing_scroll_v120 = true;
		}
		return 0.0;
	}

	value = wsf_real_scroll_value_v120(event, axis);
	factor = wsf_scroll_factor_for_axis(axis);
	if (!wsf_should_scale_scroll(event, factor)) {
		return value;
	}

	return value * factor;
}

double libinput_event_gesture_get_scale(struct libinput_event_gesture *event) {
	double scale = 1.0;

	wsf_ensure_init();
	wsf_reload_factors_if_needed();

	if (wsf_real_gesture_scale == NULL) {
		wsf_real_gesture_scale =
			(wsf_gesture_value_fn) wsf_load_symbol(
				"libinput_event_gesture_get_scale"
			);
	}

	if (wsf_real_gesture_scale == NULL) {
		if (wsf_debug && !wsf_logged_missing_gesture_scale) {
			wsf_debug_log("gesture scale symbol missing; returning 1.0");
			wsf_logged_missing_gesture_scale = true;
		}
		return 1.0;
	}

	scale = wsf_real_gesture_scale(event);
	if (!wsf_active || wsf_pinch_zoom_factor == 1.0) {
		return scale;
	}

	return 1.0 + (scale - 1.0) * wsf_pinch_zoom_factor;
}

double libinput_event_gesture_get_angle_delta(struct libinput_event_gesture *event) {
	double delta = 0.0;

	wsf_ensure_init();
	wsf_reload_factors_if_needed();

	if (wsf_real_gesture_angle_delta == NULL) {
		wsf_real_gesture_angle_delta =
			(wsf_gesture_value_fn) wsf_load_symbol(
				"libinput_event_gesture_get_angle_delta"
			);
	}

	if (wsf_real_gesture_angle_delta == NULL) {
		if (wsf_debug && !wsf_logged_missing_gesture_angle) {
			wsf_debug_log("gesture angle symbol missing; returning 0");
			wsf_logged_missing_gesture_angle = true;
		}
		return 0.0;
	}

	delta = wsf_real_gesture_angle_delta(event);
	if (!wsf_active || wsf_pinch_rotate_factor == 1.0) {
		return delta;
	}

	return delta * wsf_pinch_rotate_factor;
}
