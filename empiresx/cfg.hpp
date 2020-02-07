#pragma once

namespace genie {

enum class ConfigScreenMode {
	MODE_640_480,
	MODE_800_600,
	MODE_1024_768,
	MODE_FULLSCREEN,
	MODE_CUSTOM, // FIXME use this
};

constexpr unsigned lgy_screen_modes = (unsigned)ConfigScreenMode::MODE_1024_768 + 1;
constexpr unsigned screen_modes = (unsigned)ConfigScreenMode::MODE_CUSTOM + 1;

class Config final {
public:
	/** Startup configuration specified by program arguments. */
	ConfigScreenMode scrmode;
	unsigned poplimit = 50;

	static constexpr unsigned POP_MIN = 25, POP_MAX = 200;

	Config(int argc, char *argv[]);
};

}
