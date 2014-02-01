/*
 *  PWMan - Password Management Software
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

#include	<stdlib.h>

#include	"pwman.h"
#include	"ui.h"

void
pw_abort(char const *fmt, ... )
{
va_list	ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputs("\n", stderr);
	exit(1);
}

void
debug(char const *fmt, ... )
{
#ifdef DEBUG
	va_list ap;
	fputs("PWMan Debug% ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputs("\n", stderr);
#endif
}

char *
trim_ws(char *str)
{
	int i;

	for(i = (strlen(str) - 1); i >= 0; i--){
		if(str[i] != ' '){
			return str;
		} else {
			str[i] = 0;
		}
	}
	ui_statusline_msg(str);
	return str;
}
