/**
 * Simple quick and dirty AOE demo using my own cmake-sdl2-template
 *
 * See INSTALL for instructions
 *
 * This is just a quick template to get started. Most code is quickly hacked together and could be made more consistent but I won't bother as long as it fucking works
 */

#include <cctype>
#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <cstring>

#include <iostream>
#include <stdexcept>
#include <memory>
#include <vector>
#include <stack>
#include <map>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_syswm.h>

#include "os_macros.hpp"
#include "os.hpp"
#include "base/net.hpp"
#include "engine.hpp"
#include "font.hpp"
#include "menu.hpp"
#include "base/game.hpp"
#include "audio.hpp"
#include "string.hpp"

#if windows
#pragma comment(lib, "opengl32")
#endif

namespace genie {

const SDL_Rect menu_lobby_txt_cancel[screen_modes] = {
	{405 + (585 - 405) / 2, 440 + (470 - 440) / 2, 0, 0},
	{506 + (731 - 506) / 2, 550 + (587 - 550) / 2, 0, 0},
	{648 + (936 - 648) / 2, 704 + (752 - 704) / 2, 0, 0},
	{648 + (936 - 648) / 2, 704 + (752 - 704) / 2, 0, 0},
	{648 + (936 - 648) / 2, 704 + (752 - 704) / 2, 0, 0},
};

const SDL_Rect menu_lobby_border_cancel[screen_modes] = {
	{405, 440, 585 - 405, 470 - 440},
	{506, 550, 731 - 506, 587 - 550},
	{648, 704, 936 - 648, 752 - 704},
	{648, 704, 936 - 648, 752 - 704},
	{648, 704, 936 - 648, 752 - 704},
};

const SDL_Rect menu_lobby_lbl_chat[screen_modes] = {
	{16, 295, 0, 0},
	{18, 367, 0, 0},
	{22, 469, 0, 0},
	{22, 469, 0, 0},
	{22, 469, 0, 0},
};

const SDL_Rect menu_lobby_border_chat[screen_modes] = {
	{10, 300, 410 - 10, 396 - 300},
	{12, 375, 512 - 12, 495 - 375},
	{16, 480, 656 - 16, 633 - 480},
	{16, 480, 656 - 16, 633 - 480},
	{16, 480, 656 - 16, 633 - 480},
};

const SDL_Rect menu_lobby_field_chat[screen_modes] = {
	{10, 402, 410 - 10, 425 - 402},
	{12, 502, 512 - 12, 525 - 502},
	{16, 643, 656 - 16, 666 - 643},
	{16, 643, 656 - 16, 666 - 643},
	{16, 643, 656 - 16, 666 - 643},
};

class MenuLobby final : public Menu, public ui::InteractableCallback, public ui::InputCallback {
	std::unique_ptr<Multiplayer> mp;
	bool host;
	std::deque<Text> chat;
	ui::Border *bkg_chat;
	ui::InputField *f_chat;
public:
	MenuLobby(SimpleRender &r, uint16_t port, bool host = true)
		: Menu(MenuId::multiplayer, r, eng->assets->fnt_title, host ? "Multi Player - Host" : "Multi Player - Client", SDL_Color{ 0xff, 0xff, 0xff })
		, mp(host ? (Multiplayer*)new MultiplayerHost(port) : (Multiplayer*)new MultiplayerClient(port))
	{
		Font &fnt = eng->assets->fnt_button;
		SDL_Color fg{bkg.text[0], bkg.text[1], bkg.text[2], 0xff}, bg{bkg.text[3], bkg.text[4], bkg.text[5], 0xff};
		ConfigScreenMode mode = eng->w->render().mode;

		ui_objs.emplace_back(new ui::Label(r, eng->assets->fnt_button, eng->assets->open_str(LangId::lbl_chat), menu_lobby_lbl_chat, eng->w->render().mode, pal, bkg, ui::HAlign::left, ui::VAlign::bottom, true, false));
		ui_objs.emplace_back(bkg_chat = new ui::Border(menu_lobby_border_chat, mode, pal, bkg, ui::BorderType::field, false));

		add_btn(new ui::Button(0, *this, r, fnt, eng->assets->open_str(LangId::btn_cancel), fg, bg, menu_lobby_txt_cancel, menu_lobby_border_cancel, pal, bkg, mode));

		add_field(f_chat = new ui::InputField(0, *this, ui::InputType::text, "", r, eng->assets->fnt_default, SDL_Color{0xff, 0xff, 0}, menu_lobby_field_chat, mode, pal, bkg));
		resize(mode, mode);
	}

	void idle() override {
		std::lock_guard<std::mutex> lock(mp->mut);
		while (!mp->chats.empty()) {
			chat.emplace_front(r, eng->assets->fnt_default, mp->chats.front().c_str(), SDL_Color{ 0xff, 0xff, 0 });
			mp->chats.pop();
		}
	}

	bool keyup(int ch) override {
		switch (ch) {
		case SDLK_ESCAPE:
			interacted(0);
			return true;
		}

		return Menu::keyup(ch);
	}

	void interacted(unsigned id) override {
		jukebox.sfx(SfxId::button4);

		switch (id) {
		case 0:
			nav->quit(1);
			break;
		}
	}

	bool input(unsigned id, ui::InputField &f) override {
		switch (id) {
		case 0:
			{
				//std::lock_guard<std::mutex> lock(mp->mut); // XXX consider recursive_mutex
				auto s = f.text();
				if (!s.empty()) {
					if (s == "/clear")
						chat.clear();
					else
						mp->chat(s);
				}
			}
			break;
		}
		return true;
	}

	void paint() override {
		Menu::paint();

		unsigned i = 0;
		SDL_Rect bnds(bkg_chat->bounds());

		for (auto &x : chat) {
			int y = bnds.y + bnds.h - 18 - 20 * i++;
			if (y <= bnds.y + 4)
				break;
			x.paint(r, bnds.x + 8, y);
		}
	}
};

const SDL_Rect menu_multi_lbl_name[screen_modes] = {
	{26, 78, 0, 0},
	{31, 96, 0, 0},
	{38, 123, 0, 0},
	{38, 123, 0, 0},
	{38, 123, 0, 0},
};

const SDL_Rect menu_multi_field_name[screen_modes] = {
	{26, 78 + 20, 200 - 26, 20},
	{31, 96 + 30, 250 - 31, 30},
	{38, 123 + 40, 320 - 38, 40},
	{38, 123 + 40, 320 - 38, 40},
	{38, 123 + 40, 320 - 38, 40},
};

const SDL_Rect menu_multi_lbl_port[screen_modes] = {
	{480, 78, 0, 0},
	{600, 96, 0, 0},
	{768, 123, 0, 0},
	{768, 123, 0, 0},
	{768, 123, 0, 0},
};

const SDL_Rect menu_multi_field_port[screen_modes] = {
	{480, 78 + 20, 580 - 480, 20},
	{600, 96 + 30, 725 - 600, 30},
	{768, 123 + 40, 928 - 768, 40},
	{768, 123 + 40, 928 - 768, 40},
	{768, 123 + 40, 928 - 768, 40},
};

const SDL_Rect menu_multi_lbl_ip[screen_modes] = {
	{288, 78, 0, 0},
	{360, 96, 0, 0},
	{460, 123, 0, 0},
	{460, 123, 0, 0},
	{460, 123, 0, 0},
};

const SDL_Rect menu_multi_field_ip[screen_modes] = {
	{288, 78 + 20, 448 - 288, 20},
	{360, 96 + 30, 560 - 360, 30},
	{460, 123 + 40, 716 - 460, 40},
	{460, 123 + 40, 716 - 460, 40},
	{460, 123 + 40, 716 - 460, 40},
};

const SDL_Rect menu_multi_btn_txt_host[screen_modes] = {
	{220 + (420 - 220) / 2, 440 + (470 - 440) / 2, 0, 0},
	{275 + (525 - 275) / 2, 550 + (587 - 550) / 2, 0, 0},
	{352 + (672 - 352) / 2, 704 + (752 - 704) / 2, 0, 0},
	{352 + (672 - 352) / 2, 704 + (752 - 704) / 2, 0, 0},
	{352 + (672 - 352) / 2, 704 + (752 - 704) / 2, 0, 0},
};

const SDL_Rect menu_multi_btn_border_host[screen_modes] = {
	{220, 440, 420 - 220, 470 - 440},
	{275, 550, 525 - 275, 587 - 550},
	{352, 704, 672 - 352, 752 - 704},
	{352, 704, 672 - 352, 752 - 704},
	{352, 704, 672 - 352, 752 - 704},
};

const SDL_Rect menu_multi_btn_txt_join[screen_modes] = {
	{10 + (210 - 10) / 2, 440 + (470 - 440) / 2, 0, 0},
	{12 + (262 - 12) / 2, 550 + (587 - 550) / 2, 0, 0},
	{16 + (336 - 16) / 2, 704 + (752 - 704) / 2, 0, 0},
	{16 + (336 - 16) / 2, 704 + (752 - 704) / 2, 0, 0},
	{16 + (336 - 16) / 2, 704 + (752 - 704) / 2, 0, 0},
};

const SDL_Rect menu_multi_btn_border_join[screen_modes] = {
	{10, 440, 210 - 10, 470 - 440},
	{12, 550, 262 - 12, 587 - 550},
	{16, 704, 336 - 16, 752 - 704},
	{16, 704, 336 - 16, 752 - 704},
	{16, 704, 336 - 16, 752 - 704},
};

const SDL_Rect menu_multi_btn_txt_cancel[screen_modes] = {
	{529, 456, 0, 0},
	{658, 569, 0, 0},
	{846, 729, 0, 0},
	{846, 729, 0, 0},
	{846, 729, 0, 0},
};

const SDL_Rect menu_multi_btn_border_cancel[screen_modes] = {
	{430, 440, 630 - 430, 470 - 440},
	{537, 550, 787 - 537, 587 - 550},
	{688, 704, 1008 - 688, 752 - 704},
	{688, 704, 1008 - 688, 752 - 704},
	{688, 704, 1008 - 688, 752 - 704},
};

class MenuMultiplayer final : public Menu, public ui::InteractableCallback, public ui::InputCallback {
	uint16_t port;
	std::string name;
	in_addr ip;
	ui::InputField *f_name, *f_port, *f_ip;
public:
	MenuMultiplayer(SimpleRender &r)
		// we skip the connection type menu 9611, because we don't support serial connection or microsoft game zone anyway
		: Menu(MenuId::multiplayer, r, eng->assets->fnt_title, eng->assets->open_str(LangId::title_multiplayer_servers), SDL_Color{ 0xff, 0xff, 0xff })
		, port(25659)
	{
		Font &fnt = eng->assets->fnt_button;
		SDL_Color fg{bkg.text[0], bkg.text[1], bkg.text[2], 0xff}, bg{bkg.text[3], bkg.text[4], bkg.text[5], 0xff};
		ConfigScreenMode mode = eng->w->render().mode;

		ui_objs.emplace_back(new ui::Label(r, eng->assets->fnt_button, "Name", menu_multi_lbl_name, eng->w->render().mode, pal, bkg));
		ui_objs.emplace_back(new ui::Label(r, eng->assets->fnt_button, "Port", menu_multi_lbl_port, eng->w->render().mode, pal, bkg));
		ui_objs.emplace_back(new ui::Label(r, eng->assets->fnt_button, "Address", menu_multi_lbl_ip, eng->w->render().mode, pal, bkg));

		add_btn(new ui::Button(1, *this, r, fnt, "(C) " + eng->assets->open_str(LangId::multiplayer_host), fg, bg, menu_multi_btn_txt_host, menu_multi_btn_border_host, pal, bkg, mode));
		add_btn(new ui::Button(2, *this, r, fnt, "(J) " + eng->assets->open_str(LangId::multiplayer_join), fg, bg, menu_multi_btn_txt_join, menu_multi_btn_border_join, pal, bkg, mode));
		add_btn(new ui::Button(0, *this, r, fnt, "(Q) " + eng->assets->open_str(LangId::btn_cancel), fg, bg, menu_multi_btn_txt_cancel, menu_multi_btn_border_cancel, pal, bkg, mode));

		add_field(f_port = new ui::InputField(0, *this, ui::InputType::port, std::to_string(port), r, eng->assets->fnt_default, SDL_Color{0xff, 0xff, 0xff}, menu_multi_field_port, mode, pal, bkg));
		add_field(f_name = new ui::InputField(1, *this, ui::InputType::text, "you", r, eng->assets->fnt_default, SDL_Color{0xff, 0xff, 0xff}, menu_multi_field_name, mode, pal, bkg));
		add_field(f_ip = new ui::InputField(2, *this, ui::InputType::ip, "127.0.0.1", r, eng->assets->fnt_default, SDL_Color{0xff, 0xff, 0xff}, menu_multi_field_ip, mode, pal, bkg));
	}

	bool keyup(int ch) override {
		if (Menu::keyup(ch))
			return true;

		switch (ch) {
		case 'c':
		case 'C':
			interacted(1);
			break;
		case 'j':
		case 'J':
			interacted(2);
			break;
		case 'q':
		case 'Q':
		case SDLK_ESCAPE:
			interacted(0);
			break;
		}
		return true;
	}

private:
	bool valid() {
		name = f_name->text();
		port = f_port->port();

		bool good = true;
		f_name->error = f_ip->error = f_port->error = false;

		if (!f_ip->ip(ip)) {
			good = false;
			f_ip->error = true;
		}

		if (port < 1 || port > 65535) {
			good = false;
			f_port->error = true;
		}

		if (name.empty()) {
			good = false;
			f_name->error = true;
		}

		if (!good) {
			jukebox.sfx(SfxId::error);
			return false;
		}

		return true;
	}
public:
	void interacted(unsigned id) override {
		switch (id) {
		case 0:
			jukebox.sfx(SfxId::button4);
			nav->quit(1);
			break;
		case 1:
		case 2:
			if (valid()) {
				jukebox.sfx(SfxId::button4);
				go_to(new MenuLobby(r, port, id == 1));
			}
			break;
		}
	}

	bool input(unsigned id, ui::InputField &f) {
		return false;
	}
};

const SDL_Rect menu_ext_settings_lbl_mode[screen_modes] = {
	{320, 198 - 30, 133, 13},
	{400, 248 - 40, 133, 13},
	{512, 316 - 50, 133, 13},
	{512, 316 - 50, 133, 13},
	{512, 316 - 50, 133, 13},
};

const SDL_Rect menu_start_btn_txt_start[screen_modes] = {
	{320, 198, 133, 13},
	{400, 248, 133, 13},
	{512, 316, 133, 13},
	{512, 316, 133, 13},
	{512, 316, 133, 13},
};

const SDL_Rect menu_start_btn_border_start[screen_modes] = {
	{170, 178, 470 - 170, 218 - 178},
	{212, 222, 587 - 212, 272 - 222},
	{272, 284, 752 - 272, 348 - 284},
	{272, 284, 752 - 272, 348 - 284},
	{272, 284, 752 - 272, 348 - 284},
};

const SDL_Rect menu_start_btn_txt_multi[screen_modes] = {
	{320, 249, 114, 13},
	{400, 311, 114, 13},
	{512, 396, 114, 13},
	{512, 396, 114, 13},
	{512, 396, 114, 13},
};

const SDL_Rect menu_start_btn_border_multi[screen_modes] = {
	{170, 228, 470 - 170, 268 - 228},
	{212, 285, 587 - 212, 335 - 285},
	{272, 364, 752 - 272, 428 - 364},
	{272, 364, 752 - 272, 428 - 364},
	{272, 364, 752 - 272, 428 - 364},
};

const SDL_Rect menu_start_btn_txt_help[screen_modes] = {
	{320, 299, 44, 13},
	{400, 374, 44, 13},
	{512, 478, 44, 13},
	{512, 478, 44, 13},
	{512, 478, 44, 13},
};

const SDL_Rect menu_start_btn_border_help[screen_modes] = {
	{170, 278, 470 - 170, 318 - 278},
	{212, 347, 587 - 212, 397 - 347},
	{272, 444, 752 - 272, 508 - 444},
	{272, 444, 752 - 272, 508 - 444},
	{272, 444, 752 - 272, 508 - 444},
};

const SDL_Rect menu_start_btn_txt_editor[screen_modes] = {
	{320, 350, 163, 13},
	{400, 437, 163, 13},
	{512, 558, 163, 13},
	{512, 558, 163, 13},
	{512, 558, 163, 13},
};

const SDL_Rect menu_start_btn_border_editor[screen_modes] = {
	{170, 328, 470 - 170, 368 - 328},
	{212, 410, 587 - 212, 460 - 410},
	{272, 524, 752 - 272, 588 - 524},
	{272, 524, 752 - 272, 588 - 524},
	{272, 524, 752 - 272, 588 - 524},
};

const SDL_Rect menu_start_btn_txt_quit[screen_modes] = {
	{320, 399, 37, 13},
	{400, 498, 37, 13},
	{512, 637, 37, 13},
	{512, 637, 37, 13},
	{512, 637, 37, 13},
};

const SDL_Rect menu_start_btn_border_quit[screen_modes] = {
	{170, 378, 470 - 170, 418 - 378},
	{212, 472, 587 - 212, 522 - 472},
	{272, 604, 752 - 272, 668 - 604},
	{272, 604, 752 - 272, 668 - 604},
	{272, 604, 752 - 272, 668 - 604},
};

/** Custom help and game settings menu. */
class MenuExtSettings final : public Menu, public ui::InteractableCallback {
public:
	MenuExtSettings(SimpleRender &r)
		: Menu(MenuId::selectnav, r, eng->assets->fnt_title, "Help and Global game settings", SDL_Color{0xff, 0xff,0xff}, true)
	{
		Font &fnt = eng->assets->fnt_button;
		SDL_Color fg{bkg.text[0], bkg.text[1], bkg.text[2], 0xff}, bg{bkg.text[3], bkg.text[4], bkg.text[5], 0xff};
		ConfigScreenMode mode = eng->w->render().mode;

		ui_objs.emplace_back(new ui::Label(r, eng->assets->fnt_button, "Video resolution", menu_ext_settings_lbl_mode, eng->w->render().mode, pal, bkg, ui::HAlign::center, ui::VAlign::bottom, true, true));

		add_btn(new ui::Button(1, *this, r, fnt, "(1) " + eng->assets->open_str(LangId::mode_640_480), fg, bg, menu_start_btn_txt_start, menu_start_btn_border_start, pal, bkg, mode, ui::HAlign::center, ui::VAlign::middle, true, true));
		add_btn(new ui::Button(2, *this, r, fnt, "(2) " + eng->assets->open_str(LangId::mode_800_600), fg, bg, menu_start_btn_txt_multi, menu_start_btn_border_multi, pal, bkg, mode, ui::HAlign::center, ui::VAlign::middle, true, true));
		add_btn(new ui::Button(3, *this, r, fnt, "(3) " + eng->assets->open_str(LangId::mode_1024_768), fg, bg, menu_start_btn_txt_help, menu_start_btn_border_help, pal, bkg, mode, ui::HAlign::center, ui::VAlign::middle, true, true));
		add_btn(new ui::Button(4, *this, r, fnt, "(4/F) Fullscreen", fg, bg, menu_start_btn_txt_editor, menu_start_btn_border_editor, pal, bkg, mode, ui::HAlign::center, ui::VAlign::middle, true, true));
		add_btn(new ui::Button(0, *this, r, fnt, "(Q) " + eng->assets->open_str(LangId::btn_back), fg, bg, menu_start_btn_txt_quit, menu_start_btn_border_quit, pal, bkg, mode, ui::HAlign::center, ui::VAlign::middle, true, true));

		resize(mode, mode);
	}

	bool keyup(int ch) override {
		switch (ch) {
		case '1':
			interacted(1);
			break;
		case '2':
			interacted(2);
			break;
		case '3':
			interacted(3);
			break;
		case '4':
		case 'f':
		case 'F':
			interacted(4);
			break;
		case 'q':
		case 'Q':
		case SDLK_ESCAPE:
			interacted(0);
			break;
		}
		return true;
	}

	void interacted(unsigned id) override {
		jukebox.sfx(SfxId::button4);

		switch (id) {
		case 0:
			nav->quit(1);
			break;
		case 1:
			eng->w->chmode(ConfigScreenMode::MODE_640_480);
			break;
		case 2:
			eng->w->chmode(ConfigScreenMode::MODE_800_600);
			break;
		case 3:
			eng->w->chmode(ConfigScreenMode::MODE_1024_768);
			break;
		case 4:
			eng->w->chmode(ConfigScreenMode::MODE_FULLSCREEN);
			break;
		}
	}
};

const SDL_Rect menu_start_lbl_copy3[screen_modes] = {
	{320, 480 - 10, 133, 13},
	{400, 600 - 10, 133, 13},
	{512, 800 - 10, 133, 13},
	{512, 800 - 10, 133, 13},
	{512, 800 - 10, 133, 13},
};

class MenuStart final : public Menu, public ui::InteractableCallback {
public:
	MenuStart(SimpleRender &r)
		: Menu(MenuId::start, r, eng->assets->fnt_title, eng->assets->open_str(LangId::title_main), SDL_Color{ 0xff, 0xff, 0xff })
	{
		Font &fnt = eng->assets->fnt_button;
		SDL_Color fg{bkg.text[0], bkg.text[1], bkg.text[2], 0xff}, bg{bkg.text[3], bkg.text[4], bkg.text[5], 0xff};
		ConfigScreenMode mode = eng->w->render().mode;

		// TODO add single player and scenario editor menus
		//add_btn(new ui::Button(0, *this, r, fnt, "(S) " + eng->assets->open_str(LangId::btn_singleplayer), fg, bg, menu_start_btn_txt_start, menu_start_btn_border_start, pal, bkg, mode));
		add_btn(new ui::Button(1, *this, r, fnt, "(M) " + eng->assets->open_str(LangId::btn_multiplayer), fg, bg, menu_start_btn_txt_multi, menu_start_btn_border_multi, pal, bkg, mode));
		add_btn(new ui::Button(2, *this, r, fnt, "(H) Help and settings", fg, bg, menu_start_btn_txt_help, menu_start_btn_border_help, pal, bkg, mode));
		//add_btn(new ui::Button(3, *this, r, fnt, "(E) " + eng->assets->open_str(LangId::btn_edit), fg, bg, menu_start_btn_txt_editor, menu_start_btn_border_editor, pal, bkg, mode));
		add_btn(new ui::Button(4, *this, r, fnt, "(Q) " + eng->assets->open_str(LangId::btn_exit), fg, bg, menu_start_btn_txt_quit, menu_start_btn_border_quit, pal, bkg, mode));

		ui_objs.emplace_back(new ui::Label(r, eng->assets->fnt_default, "� 1997 Microsoft & � 2016-2020 Folkert van Verseveld. Some rights reserved", menu_start_lbl_copy3, eng->w->render().mode, pal, bkg, ui::HAlign::center, ui::VAlign::bottom, true, true));

		resize(mode, mode);
		jukebox.play(MusicId::start);
	}

	bool keyup(int ch) override {
		switch (ch) {
		case 's':
		case 'S':
			interacted(0);
			break;
		case 'm':
		case 'M':
			interacted(1);
			break;
		case 'h':
		case 'H':
			interacted(2);
			break;
		case 'e':
		case 'E':
			interacted(3);
			break;
		case 'q':
		case 'Q':
		case SDLK_ESCAPE:
			interacted(4);
			break;
		}
		return true;
	}

	void interacted(unsigned id) override {
		jukebox.sfx(SfxId::button4);

		switch (id) {
		case 0:
			break;
		case 1:
			go_to(new MenuMultiplayer(r));
			break;
		case 2:
			go_to(new MenuExtSettings(r));
			break;
		case 3:
			break;
		case 4:
			nav->quit();
			break;
		}
	}

	void paint() override {
		paint_details(Menu::show_border);
	}
};

Navigator::Navigator(SimpleRender &r) : r(r), trace(), top() {
	trace.emplace_back(top = new MenuStart(r));
}

}

int main(int argc, char **argv)
{
	try {
		genie::Config cfg(argc, argv);

		std::cout << "hello " << genie::os.username << " on " << genie::os.compname << '!' << std::endl;

		genie::Engine eng(cfg);

		genie::SimpleRender &r = (genie::SimpleRender&)eng.w->render();
		genie::nav.reset(new genie::Navigator(r));

		genie::nav->mainloop();
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}

	return 0;
}
