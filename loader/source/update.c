/*

Nintendont (Loader) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2014  JoostinOnline

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <network.h>
#include <zlib.h>
#include <ogc/lwp_watchdog.h>
#include <sys/param.h>

#include "exi.h"
#include "FPad.h"
#include "font.h"
#include "global.h"
#include "http.h"
#include "ssl.h"
#include "menu.h"
#include "update.h"

#include "ff_utf8.h"
#include "unzip/miniunz.h"
#include "unzip/ioapi.h"

extern char launch_dir[MAXPATHLEN];

typedef struct {
	const char *url;
	const char *text;
	const char *filename;
	const u32 max_size;
} downloads_t;

typedef enum {
	DOWNLOAD_NINTENDONT = 0
} DOWNLOADS;

static const downloads_t Downloads[] = {
	{"https://github.com/project-slippi/Nintendont/releases/latest/download/debug_loader.dol", "Updating Nintendont", "boot.dol", 0x400000}, // 4MB
};

extern void changeToDefaultDrive();

static s32 Download(DOWNLOADS download_number)  {
	int line = 5;
	int ret, major = 0, minor = 0;
	char errmsg[48];
	unsigned int http_status = 0;
	u8* outbuf = NULL;
	unsigned int filesize;
	char filepath[MAXPATHLEN];

	bool dir_argument_exists = (launch_dir[0] != 0);
	const char *dir = (dir_argument_exists ? launch_dir : "/apps/Slippi Nintendont/");

	ClearScreen();
	PrintInfo();

	snprintf(filepath, sizeof(filepath), "%s%s", dir, Downloads[download_number].filename);
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, Downloads[download_number].text);
	UpdateScreen();

	line++;
	gprintf("Downloading %s to %s\r\n", Downloads[download_number].url, filepath);
	ret = net_init();
	if (ret < 0) {
		gprintf("Failed to init network\r\n");
		ret = -1;
		strcpy(errmsg, "Network initialization failed.");
		goto end;
	}
	gprintf("Network Initialized\r\n");
	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Network Initialized");
	UpdateScreen();
	ssl_init(); //only once needed
	line++;
	if (download_number == DOWNLOAD_NINTENDONT) {
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Downloading latest Slippi Nintendont");
		UpdateScreen();
		line++;
	}

	int i;
	for (i = 0; i <= 10; i++) {
		ret = http_request(Downloads[download_number].url, Downloads[download_number].max_size);
		if (ret) break;
		if (i == 10) {
			gprintf("Error making http request\r\n");
			ret = -2;
			http_get_result(&http_status, NULL, NULL);
			snprintf(errmsg, sizeof(errmsg), "HTTP request failed: %u", http_status);
			goto end;
		}
	}

	ret = http_get_result(&http_status, &outbuf, &filesize); 
	if (((int)*outbuf & 0xF0000000) == 0xF0000000) {
		ret = -3;
		snprintf(errmsg, sizeof(errmsg), "http_get_result() failed: %u", http_status);
		goto end;
	}

	PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Download Complete");
	UpdateScreen();
	line++;
	if (!dir_argument_exists) {
		gprintf("Creating new directory\r\n");
		f_mkdir_char("/apps");
		f_mkdir_char("/apps/Slippi Nintendont");
	}
	else
	{
		FIL file;
		FRESULT res = f_open_char(&file, filepath, FA_WRITE|FA_CREATE_ALWAYS);
		if (res != FR_OK) {
			gprintf("File Error\r\n");
			snprintf(errmsg, sizeof(errmsg), "Error opening '%s': %u", filepath, res);
			ret = -4;
			goto end;
		} else {
			// Reserve space in the file.
			f_expand(&file, filesize, 1);

			// Write the file.
			UINT wrote;
			f_write(&file, outbuf, filesize, &wrote);
			f_close(&file);
			FlushDevices();
			ret = 1;
		}
	}
	if (ret == 1) {
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Update Complete");
		UpdateScreen();
		line++;
	}

end:
	if (ret != 1) {
		PrintFormat(DEFAULT_SIZE, MAROON, MENU_POS_X, MENU_POS_Y + 20*line, "Update Error: %s", errmsg);
	} else {
		PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X, MENU_POS_Y + 20*line, "Restart Slippi Nintendont to complete update");
	}
	UpdateScreen();
	if (outbuf != NULL) free(outbuf);
	net_deinit();
	sleep(4);
	return ret;
}

void UpdateNintendont(void) {
	bool redraw = true;
	int selected = 0;
	u64 delay = ticks_to_millisecs(gettime()) + 500;

	while (true) {
		if (redraw) {
			PrintInfo();
			PrintButtonActions("Go Back", "Update", NULL, NULL);

			// Update menu.
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 50, MENU_POS_Y + 20*5, "Download latest Slippi Nintendont");
			PrintFormat(DEFAULT_SIZE, BLACK, MENU_POS_X + 35, MENU_POS_Y + 20*(5+selected), ARROW_RIGHT);
			redraw = false;

			// Render the screen here to prevent a blank frame
			// when returning from Download().
			UpdateScreen();
			ClearScreen();
		}

		VIDEO_WaitVSync();
		FPAD_Update();
		if (delay > ticks_to_millisecs(gettime())) continue;

		if (FPAD_OK(0)) {
			if (selected == DOWNLOAD_NINTENDONT) {
				Download(selected);
				ClearScreen();
				redraw = true;
			} else {
				break;
			}
		} else if (FPAD_Start(0)) {
			break;
		} else if (FPAD_Down(1)) {
			delay = ticks_to_millisecs(gettime()) + 150;
			selected++;
			if (selected > 3) selected = 0;
			redraw = true;
		} else if (FPAD_Up(1)) {
			delay = ticks_to_millisecs(gettime()) + 150;
			selected--;
			if (selected < 0) selected = 3;
			redraw = true;
		}
	}

	return;
}
