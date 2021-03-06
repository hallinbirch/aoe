/* Copyright 2018-2019 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

/**
 * Replicated installer and game launcher
 *
 * Licensed under Affero General Public License v3.0
 * Copyright Folkert van Verseveld
 *
 * Custom setup that looks like the original one.
 * Most buttons are still stubs though...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <linux/limits.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_surface.h>

#include <genie/bmp.h>
#include <genie/dbg.h>
#include <genie/def.h>
#include <genie/res.h>

#define TITLE "Age of Empires"

// Original website is dead, so use archived link
#define WEBSITE "http://web.archive.org/web/19980120120129/https://www.microsoft.com/games/empires"

#define WIDTH 640
#define HEIGHT 480

/* SDL handling */

unsigned init = 0;
SDL_Window *window;
SDL_Renderer *renderer;
TTF_Font *font;

#define BUFSZ 4096

/* Resource IDs */

#define BMP_MAIN_BKG 0xA2
#define BMP_MAIN_BTN 0xD1
#define BMP_LAUNCH_BKG 0xF1

#define STR_PLAY_GAME 0x15
#define STR_INSTALL_GAME 0x16
#define STR_RESET_GAME 0x1A
#define STR_NUKE_GAME 0x1B
#define STR_EXIT_SETUP 0x51
#define STR_OPEN_WEBSITE 0x3C
#define STR_SETUP_TITLE 0x61
#define STR_LAUNCH_GAME 0x1F

#define IMG_BTN_DISABLED 0
#define IMG_BTN_NORMAL   1
#define IMG_BTN_FOCUS    2
#define IMG_BTN_CLICKED  3

// scratch buffer
char buf[BUFSZ];

struct pe_lib lib_lang;

int load_lib_lang(void)
{
	snprintf(buf, BUFSZ, "%s/setupenu.dll", path_cdrom);
	return pe_lib_open(&lib_lang, buf);
}

Mix_Chunk *sfx_btn;

// XXX throw away surfaces?

SDL_Surface *surf_start, *surf_reset, *surf_nuke, *surf_exit, *surf_website;
SDL_Texture *tex_start, *tex_reset, *tex_nuke, *tex_exit, *tex_website;

SDL_Surface *surf_bkg, *surf_btn;
SDL_Texture *tex_bkg, *tex_btn;

/* launch menu stuff */

SDL_Surface *surf_launch_bkg;
SDL_Texture *tex_launch_bkg;

SDL_Surface *surf_setup_title, *surf_launch_game;
SDL_Texture *tex_setup_title, *tex_launch_game;

struct menu_item {
	SDL_Rect pos;
	int x, y;
	unsigned image;
	unsigned id, format;
	SDL_Surface **surf;
	SDL_Texture **tex;
} menu_items[] = {
	{{0xf1, 0x90 , 0x1b8, 0xb7 }, 197, 138, 2, STR_INSTALL_GAME, 0x10, &surf_start, &tex_start},
	{{0xf1, 0xba , 0x1b0, 0xcd }, 197, 180, 0, STR_RESET_GAME, 0, &surf_reset, &tex_reset},
	{{0xf1, 0xe6 , 0x1b1, 0xf9 }, 197, 223, 0, STR_NUKE_GAME, 0, &surf_nuke, &tex_nuke},
	{{0xf1, 0x10f, 0x1b8, 0x136}, 197, 265, 1, STR_EXIT_SETUP, 0x10, &surf_exit, &tex_exit},
	{{0xf1, 0x13a, 0x1b8, 0x161}, 197, 307, 1, STR_OPEN_WEBSITE, 0x10, &surf_website, &tex_website},
};

unsigned menu_option = 0;

void init_launch_menu(void)
{
	SDL_Color fg = {0, 0, 0, 255};

	void *data;
	size_t size;
	int ret;
	SDL_RWops *mem;

	ret = load_bitmap(&lib_lang, BMP_LAUNCH_BKG, &data, &size);
	assert(ret == 0);

	mem = SDL_RWFromMem(data, size);
	surf_launch_bkg = SDL_LoadBMP_RW(mem, 1);
	tex_launch_bkg = SDL_CreateTextureFromSurface(renderer, surf_launch_bkg);
	assert(tex_launch_bkg);

	load_string(&lib_lang, STR_SETUP_TITLE, buf, BUFSZ);
	dbgf("setup: %s\n", buf);
	surf_setup_title = TTF_RenderText_Solid(font, buf, fg);
	load_string(&lib_lang, STR_LAUNCH_GAME, buf, BUFSZ);
	dbgf("launch: %s\n", buf);
	surf_launch_game = TTF_RenderText_Solid(font, buf, fg);

	tex_setup_title = SDL_CreateTextureFromSurface(renderer, surf_setup_title);
	assert(tex_setup_title);
	tex_launch_game = SDL_CreateTextureFromSurface(renderer, surf_launch_game);
	assert(tex_launch_game);
}

void init_main_menu(void)
{
	SDL_Color fg = {0, 0, 0, 255};
	char path[PATH_MAX];

	game_installed = find_game_installation();
	if (has_wine)
		dbgs("wine detected");

	if (game_installed) {
		dbgs("windows installation detected");
		// enable reset and nuke buttons
		menu_items[1].image = 1;
		menu_items[2].image = 1;
	} else {
		// disable reset and nuke buttons
		menu_items[1].image = 0;
		menu_items[2].image = 0;
	}

	snprintf(path, PATH_MAX, "%s/game/help/button2.wav", path_cdrom);
	if (!(sfx_btn = Mix_LoadWAV(path)))
		panic("audio not found");

	// FIXME proper error handling
	load_string(&lib_lang, game_installed ? STR_PLAY_GAME : STR_INSTALL_GAME, buf, BUFSZ);
	surf_start = TTF_RenderText_Solid(font, buf, fg);
	load_string(&lib_lang, STR_RESET_GAME, buf, BUFSZ);
	surf_reset = TTF_RenderText_Solid(font, buf, fg);
	load_string(&lib_lang, STR_NUKE_GAME, buf, BUFSZ);
	surf_nuke = TTF_RenderText_Solid(font, buf, fg);
	load_string(&lib_lang, STR_EXIT_SETUP, buf, BUFSZ);
	surf_exit = TTF_RenderText_Solid(font, buf, fg);
	load_string(&lib_lang, STR_OPEN_WEBSITE, buf, BUFSZ);
	surf_website = TTF_RenderText_Solid(font, buf, fg);

	tex_start = SDL_CreateTextureFromSurface(renderer, surf_start);
	assert(tex_start);
	tex_reset = SDL_CreateTextureFromSurface(renderer, surf_reset);
	assert(tex_reset);
	tex_nuke = SDL_CreateTextureFromSurface(renderer, surf_nuke);
	assert(tex_nuke);
	tex_exit = SDL_CreateTextureFromSurface(renderer, surf_exit);
	assert(tex_exit);
	tex_website = SDL_CreateTextureFromSurface(renderer, surf_website);
	assert(tex_website);

	void *data;
	size_t size;
	int ret;
	SDL_RWops *mem;
	Uint32 colkey;

	ret = load_bitmap(&lib_lang, BMP_MAIN_BKG, &data, &size);
	assert(ret == 0);

	mem = SDL_RWFromMem(data, size);
	surf_bkg = SDL_LoadBMP_RW(mem, 1);
	tex_bkg = SDL_CreateTextureFromSurface(renderer, surf_bkg);
	assert(tex_bkg);

	ret = load_bitmap(&lib_lang, BMP_MAIN_BTN, &data, &size);
	assert(ret == 0);
	mem = SDL_RWFromMem(data, size);
	surf_btn = SDL_LoadBMP_RW(mem, 1);
	// enable transparent pixels
	dbgf("format: %X\n", SDL_PIXELTYPE(surf_btn->format->format));
	assert(surf_btn);
	colkey = SDL_MapRGB(surf_btn->format, 0xff, 0, 0xff);
	SDL_SetColorKey(surf_btn, 1, colkey);
	tex_btn = SDL_CreateTextureFromSurface(renderer, surf_btn);
	assert(tex_btn);
}

void display_launch_menu(void)
{
	SDL_Rect pos;
	pos.x = 0; pos.y = 0;
	pos.w = surf_launch_bkg->w; pos.h = surf_launch_bkg->h;
	SDL_RenderCopy(renderer, tex_launch_bkg, NULL, &pos);

	/* draw text */
	pos.w = surf_setup_title->w; pos.h = surf_setup_title->h;
	pos.x = WIDTH / 2 - pos.w / 2;
	pos.y = 100;
	SDL_RenderCopy(renderer, tex_setup_title, NULL, &pos);
	pos.w = surf_launch_game->w; pos.h = surf_launch_game->h;
	pos.x = WIDTH / 2 - pos.w / 2;
	pos.y = 222;
	SDL_RenderCopy(renderer, tex_launch_game, NULL, &pos);
}

void display_main_menu(void)
{
	SDL_Rect pos, img;
	pos.x = 0; pos.y = 0;
	pos.w = surf_bkg->w; pos.h = surf_bkg->h;
	SDL_RenderCopy(renderer, tex_bkg, NULL, &pos);

	for (unsigned i = 0; i < ARRAY_SIZE(menu_items); ++i) {
		struct menu_item *item = &menu_items[i];
		pos.x = item->x; pos.y = item->y;
		pos.w = surf_btn->w; pos.h = surf_btn->h / 4;
		img.x = 0; img.y = item->image * surf_btn->h / 4;
		img.w = surf_btn->w; img.h = surf_btn->h / 4;
		SDL_RenderCopy(renderer, tex_btn, &img, &pos);
	}

	for (unsigned i = 0; i < ARRAY_SIZE(menu_items); ++i) {
		struct menu_item *item = &menu_items[i];
		item->pos.w = (*item->surf)->w;
		item->pos.h = (*item->surf)->h;
		SDL_RenderCopy(renderer, *item->tex, NULL, &item->pos);
	}
}

static int button_down = 0;

static int main_btn_install_or_play(void)
{
	char path[PATH_MAX];

	if (!game_installed)
		// FIXME stub
		return 0;

	// TODO show launching menu
	init_launch_menu();
	display_launch_menu();
	SDL_RenderPresent(renderer);

	snprintf(path, PATH_MAX, "wine '%s/Empires.exe'", path_wine);
	return system(path) == 0 ? -1 : 0;
}

int menu_btn_click(void)
{
	int code = 1;

	Mix_PlayChannel(0, sfx_btn, 0);

	switch (menu_option) {
	case 0:
		code = main_btn_install_or_play();
		break;
	case 1:
	case 2:
		// FIXME stub
		break;
	case 3:
		return -1;
	case 4:
		system("xdg-open '" WEBSITE "'");
		break;
	}
	button_down = 0;
	menu_items[menu_option].image = 2;
	return code ? code : 1;
}

unsigned mouse_find_button(int x, int y)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(menu_items); ++i) {
		struct menu_item *item = &menu_items[i];
		if (x >= item->x && x < item->x + surf_btn->w &&
			y >= item->y && y < item->y + surf_btn->h / 4)
		{
			break;
		}
	}
	return i;
}

int mouse_move(const SDL_MouseMotionEvent *ev)
{
	unsigned old_option, index;

	old_option = menu_option;
	index = mouse_find_button(ev->x, ev->y);

	if (index < ARRAY_SIZE(menu_items)) {
		if (menu_items[index].image && !button_down)
			menu_option = index;
		else if (button_down && index == menu_option && menu_items[index].image != IMG_BTN_CLICKED) {
			menu_items[index].image = IMG_BTN_CLICKED;
			return 1;
		}
	} else if (!button_down && menu_items[menu_option].image != IMG_BTN_DISABLED && menu_items[menu_option].image != IMG_BTN_NORMAL) {
		menu_items[menu_option].image = IMG_BTN_FOCUS;
		return 1;
	}

	if (button_down)
		return 0;

	if (old_option != menu_option) {
		//dbgf("mouse_move (%d,%d): %u\n", ev->x, ev->y, menu_option);
		menu_items[old_option].image = IMG_BTN_NORMAL;
		menu_items[menu_option].image = IMG_BTN_FOCUS;
		return 1;
	}
	return 0;
}

int mouse_down(const SDL_MouseButtonEvent *ev)
{
	if (ev->button != SDL_BUTTON_LEFT)
		return 0;
	if (mouse_find_button(ev->x, ev->y) == menu_option) {
		menu_items[menu_option].image = IMG_BTN_CLICKED;
		button_down = 1;
		return 1;
	}
	return 0;
}

int mouse_up(const SDL_MouseButtonEvent *ev)
{
	unsigned index;

	if (ev->button != SDL_BUTTON_LEFT)
		return 0;
	if (mouse_find_button(ev->x, ev->y) == menu_option)
		return menu_btn_click();
	button_down = 0;

	index = mouse_find_button(ev->x, ev->y);
	if (index < ARRAY_SIZE(menu_items) && index != menu_option) {
		menu_items[menu_option].image = IMG_BTN_NORMAL;
		if (menu_items[index].image != IMG_BTN_DISABLED) {
			menu_option = index;
			menu_items[index].image = IMG_BTN_FOCUS;
		}
	} else
		menu_items[menu_option].image = IMG_BTN_FOCUS;
	return 1;
}

int keydown(const SDL_Event *ev)
{
	unsigned virt;
	unsigned old_option;

	virt = ev->key.keysym.sym;

	old_option = menu_option;

	switch (virt) {
	case SDLK_DOWN:
		if (button_down)
			break;
		do {
			menu_option = (menu_option + 1) % ARRAY_SIZE(menu_items);
		} while (menu_items[menu_option].image == IMG_BTN_DISABLED);
		break;
	case SDLK_UP:
		if (button_down)
			break;
		do {
			menu_option = (menu_option + ARRAY_SIZE(menu_items) - 1) % ARRAY_SIZE(menu_items);
		} while (menu_items[menu_option].image == IMG_BTN_DISABLED);
		break;
	case '\r':
	case '\n':
		button_down = 1;
		return menu_btn_click();
	case ' ':
		if (menu_option < ARRAY_SIZE(menu_items) && menu_items[menu_option].image != IMG_BTN_DISABLED) {
			menu_items[menu_option].image = IMG_BTN_CLICKED;
			button_down = 1;
			return 1;
		}
	}

	if (old_option != menu_option) {
		menu_items[old_option].image = IMG_BTN_NORMAL;
		menu_items[menu_option].image = IMG_BTN_FOCUS;
		return 1;
	}
	return 0;
}

int keyup(const SDL_Event *ev)
{
	unsigned virt = ev->key.keysym.sym;

	switch (virt) {
	case ' ':
		return menu_btn_click();
	}
	return 0;
}

void update_screen(void)
{
	display_main_menu();
	SDL_RenderPresent(renderer);
}

void main_event_loop(void)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	update_screen();

	SDL_Event ev;
	int code;
	while (SDL_WaitEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			return;
		case SDL_KEYUP:
			if ((code = keyup(&ev)) < 0)
				return;
			else if (code > 0)
				update_screen();
			break;
		case SDL_KEYDOWN:
			if ((code = keydown(&ev)) < 0)
				return;
			else if (code > 0)
				update_screen();
			break;
		case SDL_MOUSEMOTION:
			if (mouse_move(&ev.motion))
				update_screen();
			break;
		case SDL_MOUSEBUTTONDOWN:
			if (mouse_down(&ev.button))
				update_screen();
			break;
		case SDL_MOUSEBUTTONUP:
			if ((code = mouse_up(&ev.button)) < 0)
				return;
			else if (code > 0)
				update_screen();
			break;
		}
	}
}

int main(void)
{
	if (!find_setup_files())
		panic("Please insert or mount the game CD-ROM");
	if (load_lib_lang())
		panic("CD-ROM files are corrupt");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
		panic("Could not initialize user interface");

	if (!(window = SDL_CreateWindow(
		TITLE, SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT,
		SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN)))
	{
		panic("Could not create user interface");
	}

	dbgf("Available render drivers: %d\n", SDL_GetNumVideoDrivers());

	// Create default renderer and don't care if it is accelerated.
	if (!(renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC)))
		panic("Could not create rendering context");

	if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
		panic("Could not initialize image subsystem");
	if (TTF_Init())
		panic("Could not initialize fonts");

	snprintf(buf, BUFSZ, "%s/system/fonts/arial.ttf", path_cdrom);
	font = TTF_OpenFont(buf, 18);
	if (!font)
		panic("Could not setup font");

	if (Mix_OpenAudio(22050, AUDIO_S16LSB, 2, 1024) == -1)
		panic("Could not open audio");

	init_main_menu();
	main_event_loop();

	TTF_Quit();
	IMG_Quit();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
