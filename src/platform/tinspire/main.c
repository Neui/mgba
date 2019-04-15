
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gba/video.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/gb/gb.h>
#endif
#include "feature/gui/gui-runner.h"
#include <mgba-util/gui.h>
#include <mgba-util/gui/file-select.h>
#include <mgba-util/gui/font.h>
#include <mgba-util/gui/menu.h>
#include <mgba-util/memory.h>

#include <assert.h>

#include "common.h"
#include "lcd.h"
#include "libndls.h"
#include "keys.h"
#include "draw.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(left, current, right) MAX(left, MIN(current, right))

#define _TINSPIRE_INPUT 0x0039ECAB

struct {
	int x, y;
	touchpad_info_t tpinfo;
	touchpad_report_t prevReport;
	touchpad_report_t report; /* Hack, see _pollCursor */
	tpad_arrow_t holding;
	int timer; /* How long cursor stays onscreen */
	float sensitivity;
} cursor = {
	/* x and y is set at the initialization correctly */
	.tpinfo = (touchpad_info_t){0},
	.prevReport = (touchpad_report_t){0},
	.report = (touchpad_report_t){0},
	.holding = TPAD_ARROW_NONE,
	.timer = 0,
	.sensitivity = 0.75f
};

struct {
	struct GUIFont* font;
	struct LCDInfo lcd;
	struct ImageBuf screen; /* Image that gets drawn to and displayed */
	struct ImageBuf game; /* Image produced by the emulator */
	struct ImageBuf gameReal; /* Like above, but with 'real' size */
	struct ImageBuf gamePost; /* For fading */
	unsigned fit_width, fit_height; /* Pre-calculated size for SM_AR_FIT */
	bool frameLimiter;
	enum ScreenMode {
		SM_PA,
		SM_AR_FIT,
		SM_STRETCH,
		SM_MAX
	} screenMode;
} video = {
	.font = NULL,
	.screen = EMPTY_IMAGEBUF,
	.game = EMPTY_IMAGEBUF,
	.gameReal = EMPTY_IMAGEBUF,
	.gamePost = EMPTY_IMAGEBUF,
	.frameLimiter = true,
	.screenMode = SM_PA
};
struct ImageBuf *screenBuffer = &video.screen; /* For gui-font.c */

uint32_t *romBuffer = NULL;
size_t romBufferSize = 0;
/* runner is global so _cleanup can Deinit it if needed */
struct mGUIRunner runner = {.params.basePath = NULL};

mLOG_DEFINE_CATEGORY(GUI_TINSPIRE, "TI Nspire", "gui.TIN");

static bool _allocateRomBuffer() {
	static const size_t values[] = {
		SIZE_CART0,
		SIZE_CART0 / 2,
		SIZE_CART0 / 4,
		SIZE_CART0 / 8,
		0
	};
	for (const size_t *i = values; *i; i++) {
		if ((romBuffer = malloc(romBufferSize = *i))) {
			return true;
		}
	}
	return false;
}

static void _cleanup(void) {
	/* The calculator runs an RTOS and doesn't seem to clear the memory
	 * (and probably other stuff) owned by a application, so this function
	 * tries to clear all memory, for example when exiting or aborting. */
	if (runner.params.basePath) {
		mGUIDeinit(&runner);
	}
	free(video.gameReal.data);
	free(video.gamePost.data);
	free(video.screen.data);
	GUIFontDestroy(video.font);
	free(romBuffer);
	lcd_init(SCR_TYPE_INVALID);
}

void AbortCleanly(char *title, char *msg) {
	if (!title) {
		title = "Internal Error";
	}
	if (!msg) {
		msg = "An internal error occured. The program might've not "
			"completely cleanly and could've leaked memory. "
			"If you expirence any out of memory issues, try restarting "
			"your device.";
	}
	_cleanup();
	show_msgbox(title, msg);
	abort();
}


static int _batteryState(void) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _batteryState()");
	// TODO: Properly implement _batteryState when impl. is available
	return BATTERY_NOT_PRESENT;
}

static enum GUICursorState _pollCursor(unsigned int *x, unsigned int *y) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _pollCursor(%p, %p)", x, y);
	touchpad_report_t report;
	if (touchpad_scan(&report)) {
		mLOG(GUI_TINSPIRE, DEBUG, "Report returned wrongly!");
		return GUI_CURSOR_NOT_PRESENT;
	}
	
	/* HACK: In _pollInput it seems isKeyPressed() with a touchpad key it
	 * resets the velocity. Now we store the state read when _pollInput is
	 * called and use that if the report we got has the velocities be 0. */
	if (!(report.y_velocity || report.x_velocity)) {
		report = cursor.report;
	}
	
	if (report.pressed && cursor.holding == TPAD_ARROW_NONE) {
		cursor.holding = report.arrow;
	}
	
	if (report.contact // implied when report.pressed
			&& (cursor.holding == TPAD_ARROW_NONE
				|| cursor.holding == TPAD_ARROW_CLICK)
			) {
		
#if 0
		/* A workaround I tried because of the above report thing;
		 * This tries to match the xy_velocity values, however then
		 * touching oppositting corners it'll have large values rather
		 * than being 0. */
		int velx = (((float)cursor.prevReport.x - report.x) / cursor.tpinfo.width) * video.lcd.width * 2;
		int vely = (((float)cursor.prevReport.y - report.y) / cursor.tpinfo.height) * video.lcd.height * 2;
#endif
		
		cursor.x += cursor.sensitivity * (signed char)report.x_velocity;
		cursor.y -= cursor.sensitivity * (signed char)report.y_velocity;
		
		cursor.x = CLAMP(0, cursor.x, (int)video.lcd.width);
		cursor.y = CLAMP(0, cursor.y, (int)video.lcd.height);
		
		assert(x != NULL);
		assert(y != NULL);
		*x = cursor.x;
		*y = cursor.y;
		
		cursor.prevReport = report;
		
		if (report.x_velocity || report.y_velocity) {
			cursor.timer = 60 * 10;
		}

		if (cursor.holding == TPAD_ARROW_CLICK) {
			return GUI_CURSOR_DOWN;
		}
		return GUI_CURSOR_UP;
	} else if (cursor.timer > 0) {
		cursor.timer--;
		*x = cursor.x;
		*y = cursor.y;
		if (!report.contact) {
				cursor.holding = TPAD_ARROW_NONE;
		} else if (report.arrow != TPAD_ARROW_NONE
				&& report.arrow != TPAD_ARROW_CLICK) {
			cursor.timer = 0;
		}
		return GUI_CURSOR_UP;
	}
	
	cursor.holding = TPAD_ARROW_NONE;
	return GUI_CURSOR_NOT_PRESENT;
}

static uint32_t _pollInput(const struct mInputMap *map) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _pollInput(%p, %p)", map);
	uint32_t keys = 0;
	
	if (is_touchpad) {
		touchpad_scan(&cursor.report);
	}
	
	for (enum GUIInput i = 0; i < GUI_INPUT_MAX; i++) {
		/* iskeyPressed is a macro that uses the & address-of operator, that
		 * need an lvalue. Because of that, we need this variable. */
		enum tinspire_key key = mInputQueryBinding(map, _TINSPIRE_INPUT, i);
		keys |= TINKeyPressed(key, &cursor.report) << i;
	}
	
	return keys;
}

static uint16_t _pollGameInput(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _pollGameInput(%p)", runner);
	uint16_t keys = 0;
	for (enum GBAKey i = 0; i < GBA_KEY_MAX; i++) {
		/* iskeyPressed is a macro that uses the & address-of operator, that
		 * need an lvalue. Because of that, we need this variable. */
		enum tinspire_key key = mInputQueryBinding(&runner->core->inputMap, _TINSPIRE_INPUT, i);
		keys |= TINKeyPressed(key, &cursor.report) << i;
	}
	return keys;
}


static void _drawStart(void) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _drawStart()");
	DrawClear(&video.screen, 0);
}

static void _prepareForFrame(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _prepareForFrame(%p)", runner);
	unsigned width, height;
	runner->core->desiredVideoDimensions(runner->core, &width, &height);
	if (width != video.game.w || height != video.game.h) {
		mLOG(GUI_TINSPIRE, INFO, "Mid-Game resolution change: %ux%u -> %ux%u",
				video.game.w, video.game.h, width, height);
		
		/* Initially it seems the use an bigger resoultion and then shrink
		 * afterwards, so re-using the same memory should be fine */
		assert(width * height <= video.gameReal.w * video.gameReal.h);
		
		video.game.w = video.gamePost.w = width;
		video.game.h = video.gamePost.h = height;
		
		float fit_ratio = MIN((float)video.screen.w / video.game.w,
				(float)video.screen.h / video.game.h);
		video.fit_width = fit_ratio * video.game.w;
		video.fit_height = fit_ratio * video.game.h;
		
		runner->core->setVideoBuffer(runner->core, video.game.data, width);
	}
}

static void _drawFrame(struct mGUIRunner* runner, bool faded) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _drawFrame(%p, %d)", runner, faded);
	UNUSED(runner);
	struct ImageBuf *vb = &video.game;
	if (faded) {
		memcpy(video.gamePost.data, video.game.data,
				sizeof(video.game.data[0]) * video.game.w * video.game.h);
		DrawFade(&video.gamePost);
		vb = &video.gamePost;
	}
	
	if (video.screenMode == SM_PA) {
		DrawOn(&video.screen, vb,
				video.screen.w / 2 - video.game.w / 2,
				video.screen.h / 2 - video.game.h / 2);
	} else if (video.screenMode == SM_AR_FIT) {
		DrawOnResized(&video.screen, vb,
				video.screen.w / 2 - video.fit_width / 2,
				video.screen.h / 2 - video.fit_height / 2,
				video.fit_width, video.fit_height);
	} else if (video.screenMode == SM_STRETCH) {
		DrawOnResized(&video.screen, vb,
				0, 0,
				video.lcd.width, video.lcd.height);
	}
}

static void _drawEnd(void) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _drawEnd()");
	LCDConvertAndBlit(video.lcd, &video.screen, (char*)video.screen.data);
	if (video.frameLimiter) {
		msleep(1000 / 61); // Account for some overhead
	} else {
		idle();
	}
}

static void _drawScreenshot(struct mGUIRunner* runner, const color_t* pixels, unsigned width, unsigned height, bool faded) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _drawScreenshot(%p, %p, %u, %u, %i)", runner, pixels, width, height, faded);
	struct ImageBuf pre = video.game;
	video.game.data = (color_t*)pixels;
	video.game.w = width;
	video.game.h = height;
	_drawFrame(runner, faded);
	video.game = pre;
}

static void _guiPrepare(void) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _guiPrepare()");
	// Do nothing.
}

static void _guiFinish(void) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _guiFinish()");
	// Do nothing.
}

static void _setFrameLimiter(struct mGUIRunner* runner, bool limit) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _setFrameLimiter(%p, %i)", runner, limit);
	UNUSED(runner);
	video.frameLimiter = limit;
}

static void _incrementScreenMode(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _incrementScreenMode(%p)", runner);
	UNUSED(runner);
	video.screenMode = (video.screenMode + 1) % SM_MAX;
	mCoreConfigSetUIntValue(&runner->config, "screenMode", video.screenMode);
}


static void _gameLoaded(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _gameLoaded(%p)", runner);
	UNUSED(runner);
	video.frameLimiter = true;
}

static void _gameUnloaded(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _gameUnloaded(%p)", runner);
	UNUSED(runner);
}

static void _gameUnpaused(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _gameUnpaused(%p)", runner);
	/* Note: Called by _setup to set config values */
	unsigned mode;
	if (mCoreConfigGetUIntValue(&runner->config, "screenMode", &mode)) {
		video.screenMode = mode % SM_MAX;
	}
}

static void _gamePaused(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _gamePaused(%p)", runner);
	UNUSED(runner);
}

static void _setup(struct mGUIRunner* runner) {
	//mLOG(GUI_TINSPIRE, DEBUG, "Called _setup(%p)", runner);

	struct mInputMap *map = &runner->core->inputMap;
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_UP, GBA_KEY_UP);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_DOWN, GBA_KEY_DOWN);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_LEFT, GBA_KEY_LEFT);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_RIGHT, GBA_KEY_RIGHT);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_SHIFT, GBA_KEY_B);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_VAR, GBA_KEY_A);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_CTRL, GBA_KEY_SELECT);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_DEL, GBA_KEY_START);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_TAB, GBA_KEY_L);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_MENU, GBA_KEY_R);
	
	unsigned width, height;
	runner->core->desiredVideoDimensions(runner->core, &width, &height);
	
	assert(width != 0 && height != 0);
	
	if (width != video.game.w || height != video.game.h) {
		mLOG(GUI_TINSPIRE, INFO, "Game resolution change: %ux%u -> %ux%u",
				video.game.w, video.game.h, width, height);
		
		video.game.w = video.gamePost.w = width;
		video.game.h = video.gamePost.h = height;
		
		void *game = realloc(video.game.data, DrawMemSize(&video.game));
		if (!game) {
			AbortCleanly(NULL, NULL);
		}
		video.game.data = game;
		DrawClear(&video.game, 0);
		
		void *gamePost = realloc(video.gamePost.data, DrawMemSize(&video.gamePost));
		if (!gamePost) {
			AbortCleanly(NULL, NULL);
		}
		video.gamePost.data = gamePost;
		
		float fit_ratio = MIN((float)video.screen.w / video.game.w,
				(float)video.screen.h / video.game.h);
		video.fit_width = fit_ratio * video.game.w;
		video.fit_height = fit_ratio * video.game.h;
	}
	runner->core->setVideoBuffer(runner->core, video.game.data, width);
	video.gameReal = video.game;
	
	_gameUnpaused(runner); /* Setup config values */
	
	video.frameLimiter = true;
}

static void _dummyLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	return;
}
static struct mLogger _dummyLogger = {
	.log = _dummyLog
};

int main(int argc, char **argv) {
	scr_type_t lcdtype = lcd_type();
	if (lcdtype == SCR_TYPE_INVALID) {
		show_msgbox("Invalid LCD Type", "Invalid LCD type was returned!?\n"
				"Maybe try an different version of Ndless.");
		return 1;
	}
	if (lcdtype >= SCR_TYPE_COUNT) {
		char buf[128];
		sprintf(buf, "Unknown LCD type detected: %d\n"
				"Try updating this software, if available, otherwise "
				"submit an issue.",
				(int)lcdtype);
		show_msgbox("Unknown LCD Type", buf);
		return 1;
	}
	video.lcd = GetLCDInfo(lcdtype);
	
	video.screen.w = video.lcd.width;
	video.screen.h = video.lcd.height;
	video.screen.data = malloc(DrawMemSize(&video.screen));
	if (!video.screen.data) {
		show_msgbox("Out of memory error", "Not enough memory for a screen buffer!");
		return 1;
	}
	
	video.font = GUIFontCreate();
	if (!video.font) {
		show_msgbox("Internal error", "Couldn't create GUI resources");
		_cleanup();
		return 1;
	}
	
	mLogSetDefaultLogger(&_dummyLogger);
	
	/* Logging isn't properly setup at this point, but it should still
	 * print to stdout, so it's OK. */
	mLOG(GUI_TINSPIRE, INFO, "Screen %ix%i with rgb%i%i%i (g%i), type=%i",
			video.lcd.width, video.lcd.height,
			video.lcd.redBBP, video.lcd.greenBBP, video.lcd.blueBBP,
			video.lcd.grayBBP, (int)video.lcd.type);
	
	touchpad_info_t *info = touchpad_getinfo();
	if (info) {
		mLOG(GUI_TINSPIRE, INFO, "Touchpad detected: %ix%i",
				(int)info->width, (int)info->height);
		cursor.tpinfo = *info;
		cursor.x = video.lcd.width / 2;
		cursor.y = video.lcd.height / 2;
	} else {
		mLOG(GUI_TINSPIRE, INFO, "Touchpad NOT detected");
	}
	
	cfg_register_fileext("gba", argv[0]);
	cfg_register_fileext("gbc", argv[0]);
	cfg_register_fileext("gb", argv[0]);
	mLOG(GUI_TINSPIRE, INFO, "Registered file extensions");
	
	runner = (struct mGUIRunner){
		.params = {
			video.lcd.width, video.lcd.height,
			video.font, get_documents_dir(),
			_drawStart, _drawEnd,
			_pollInput, (info ? _pollCursor : NULL),
			_batteryState,
			_guiPrepare, _guiFinish,
		},
		.keySources = (struct GUIInputKeys[]) {
			{
				.name = "TI Nspire Input",
				.id = _TINSPIRE_INPUT,
				.keyNames = (const char*[]) {
#define X(name, display_string) display_string,
					X_TINSPIRE_KEYS
#undef X
				},
				.nKeys = TIN_KEY__LAST
			},
			{ .id = 0 }
		},
		.configExtra = (struct GUIMenuItem[]) {
			{
				.title = "Screen mode",
				.data = "screenMode",
				.submenu = 0,
				.state = SM_PA,
				.validStates = (const char*[]) {
					"Pixel-Accurate",
					"Aspect-Ratio Fit",
					"Stretched",
				},
				.nStates = 3
			}
		},
		.nConfigExtra = 1,
		.setup = _setup,
		.teardown = 0,
		.gameLoaded = _gameLoaded,
		.gameUnloaded = _gameUnloaded,
		.prepareForFrame = _prepareForFrame,
		.drawFrame = _drawFrame,
		.drawScreenshot = _drawScreenshot,
		.paused = _gamePaused,
		.unpaused = _gameUnpaused,
		.incrementScreenMode = _incrementScreenMode,
		.setFrameLimiter = _setFrameLimiter,
		.pollGameInput = _pollGameInput,
		.running = 0
	};

	mGUIInit(&runner, "tinspire");
	//mLogSetDefaultLogger(NULL); // TODO: Temporary log filter bypass
	
	/* Most stuff is now allocated, so let's try to get memory for roms */
	if (!_allocateRomBuffer()) {
		mLOG(GUI_TINSPIRE, ERROR, "Couldn't allocate buffer for rom!");
		show_msgbox("Internal error", "Not enough memory to hold a rom.");
		_cleanup();
		return 1;
	} else {
		mLOG(GUI_TINSPIRE, INFO, "Allocated %u bytes for rom", (unsigned)romBufferSize);
	}

	struct mInputMap *map = &runner.params.keyMap;
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_DEL, GUI_INPUT_BACK);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_ENTER, GUI_INPUT_SELECT);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_PLUS, mGUI_INPUT_INCREASE_BRIGHTNESS);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_MINUS, mGUI_INPUT_DECREASE_BRIGHTNESS);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_MULTIPLY, mGUI_INPUT_SCREEN_MODE);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_UP, GUI_INPUT_UP);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_DOWN, GUI_INPUT_DOWN);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_LEFT, GUI_INPUT_LEFT);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_RIGHT, GUI_INPUT_RIGHT);
	mInputBindKey(map, _TINSPIRE_INPUT, TIN_KEY_ESC, GUI_INPUT_CANCEL);

	lcd_init(lcdtype);
	/* Run ROM directly if it was opened with one */
	if (argc > 2) {
		if (runner.keySources) {
			for (size_t i = 0; runner.keySources[i].id; ++i) {
				mInputMapLoad(&runner.params.keyMap, runner.keySources[i].id, mCoreConfigGetInput(&runner.config));
			}
		}
		mLOG(GUI_TINSPIRE, INFO, "Running game at startup: %s", argv[0]);
		mGUIRun(&runner, argv[1]);
	}
	
	mGUIRunloop(&runner);
	_cleanup(); /* Already executes mGUIDeinit */
	refresh_osscr(); /* Refreshes file browser */
	return 0;
}
