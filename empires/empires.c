/* Copyright 2018 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

/**
 * Replicated Age of Empires shell
 *
 * Licensed under Affero General Public License v3.0
 * Copyright by Folkert van Verseveld.
 */

#include <stdbool.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "../setup/dbg.h"
#include "../setup/def.h"
#include "../setup/res.h"

#include "gfx.h"
#include "fs.h"
#include "ui.h"

#define TITLE "Age of Empires"

/* SDL handling */

unsigned init = 0;
SDL_Window *window;
SDL_Renderer *renderer;

struct pe_lib lib_lang;

#define BUFSZ 4096

int load_lib_lang(void)
{
	char buf[BUFSZ];
	fs_game_path(buf, BUFSZ, "language.dll");
	return pe_lib_open(&lib_lang, buf);
}

bool update_screen(void)
{
	if (!display())
		return false;
	SDL_RenderPresent(renderer);

	return true;
}

void main_event_loop(void)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);

	if (!update_screen())
		return;

	SDL_Event ev;
	while (SDL_WaitEvent(&ev)) {
		switch (ev.type) {
		case SDL_QUIT:
			return;
		case SDL_KEYDOWN:
			if (keydown(&ev.key) && !update_screen())
				return;
			break;
		case SDL_KEYUP:
			if (keyup(&ev.key) && !update_screen())
				return;
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

	game_installed = find_wine_installation();
	if (has_wine)
		dbgs("wine detected");
	dbgf("game installed: %s\n", game_installed ? "yes" : "no");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
		panic("Could not initialize user interface");

	if (!(window = SDL_CreateWindow(
		TITLE, SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT,
		SDL_WINDOW_SHOWN)))
	{
		panic("Could not create user interface");
	}

	dbgf("Available render drivers: %d\n", SDL_GetNumVideoDrivers());

	// Create default renderer and don't care if it is accelerated.
	if (!(renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC)))
		panic("Could not create rendering context");

	gfx_init();
	ui_init();

	main_event_loop();

	ui_free();
	gfx_free();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
