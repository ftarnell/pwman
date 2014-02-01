/*
 *  PWMan - password management application
 *
 *  Copyright (C) 2002  Ivan Kelly <ivan@ivankelly.net>
 *  Copyright (c) 2014	Felicity Tarnell.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef PWMAN_UI_H
#define PWMAN_UI_H

#include <curses.h>
#include <signal.h>
#include <ctype.h>

#ifdef HAVE_TERMIOS_H
#       include <termios.h>
#else   
#       ifdef HAVE_LINUX_TERMIOS_H
#               include <linux/termios.h>
#       endif
#endif
#ifdef HAVE_SYS_IOCTL_H
#       include <sys/ioctl.h>
#endif

#include	"pwman.h"

#define MIN_LINES 22
#define MIN_COLS 60

#define LIST_TOP 3 
#define LIST_BOTTOM (LINES -3)
#define LIST_LINES (LIST_BOTTOM-LIST_TOP)

#define LAST_LIST_ITEM (first_list_item + LIST_LINES - 1)
#define NAMELEN (COLS/3)-1
#define NAMEPOS 2

#define HOSTLEN (COLS/3)-1
#define HOSTPOS (NAMEPOS + NAMELEN + 1)

#define USERLEN (COLS/3)-1
#define USERPOS (NAMEPOS + NAMELEN + HOSTLEN + 2)
#define hide_cursor() curs_set(0)
#define show_cursor() curs_set(1)

#define NUM_TO_CHAR(n) (n + 48)
#define CHAR_TO_NUM(n) (n - 49)

typedef enum {
	STRING,     // A regular string
	INT,        // An integer
	INFORMATION // Name only, no value, so read only
} TYPE;

typedef enum {
	PW_NULL,
	PW_ITEM,
	PW_SUBLIST,
	PW_UPLEVEL
} LIST_ITEM_TYPE;

typedef struct {
	char *name;
	void *value; // int* or char*
	int max_length;
	TYPE type;
	char *(*autogen)(char*);
} InputField;

void uilist_init(void);
void uilist_free(void);
void uilist_refresh(void);
void uilist_clear(void);
void uilist_headerline(void);
int ui_statusline_clear(void);
void ui_refresh_windows(void);

int view_pw(int i);

int ui_statusline_yes_no(char *, int);
int ui_statusline_msg(char *msg);
void ui_statusline_ask_num(char *, int *);
void ui_statusline_ask_char(char *, char *, char*);
char * ui_statusline_ask_str(char *, char *, int len);
char * ui_statusline_ask_passwd(char *, char *, int, int);
char * ui_statusline_ask_str_with_autogen(char *msg, char *input, int len, char *(*autogen)(char *), int ch);

void	uilist_up(void);
void	uilist_down(void);
LIST_ITEM_TYPE uilist_get_highlighted_type(void);
Pw * uilist_get_highlighted_item(void);
PWList *uilist_get_highlighted_sublist(void);
PWSearchResult *uilist_get_highlighted_searchresult(void);
void uilist_page_up(void);
void uilist_page_down(void);

void statusline_readonly();

int filter_apply(Pw *pw, PwFilter* fil);
void filter_alert(PwFilter* fil);
void filter_get(void);

int search_alert(PwSearch* srch);

#endif	/* !PWMAN_UI_H */
