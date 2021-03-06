/*
 *  PWman - Password management application
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

#include	"ui.h"
#include	"pwman.h"
#include	"gnupg.h"
#include	"actions.h"

static void	unmark_entries(void);
static void	action_edit_pw(password_t *pw);
static void	_create_information_field(char const *name, InputField * field);

static int	disp_h = 15, disp_w = 60;

void
action_list_add_pw()
{
password_t     *pw;

InputField	fields[] = {
	{"Name: ",		NULL, STRING},
	{"Host: ",		NULL, STRING},
	{"User: ",		NULL, STRING},
	{"Password: ",		NULL, STRING, pwgen_ask},
	{"Launch command: ",	NULL, STRING}
};
int		i;

	pw = xcalloc(1, sizeof(*pw));
	if ((pw->name = ui_ask_str(fields[0].name, NULL)) == NULL)
		goto end;

	if ((pw->host = ui_ask_str(fields[1].name, NULL)) == NULL)
		goto end;

	if ((pw->user = ui_ask_str(fields[2].name, NULL)) == NULL)
		goto end;

	if ((pw->passwd = ui_ask_str_with_autogen(fields[3].name, NULL,
				     fields[3].autogen, CNTL('G'))) == NULL)
		goto end;

	if ((pw->launch = ui_ask_str(fields[4].name, NULL)) == NULL)
		goto end;

	fields[0].value = &pw->name;
	fields[1].value = &pw->host;
	fields[2].value = &pw->user;
	fields[3].value = &pw->passwd;
	fields[4].value = &pw->launch;

	i = action_yes_no_dialog(fields, (sizeof(fields) / sizeof(InputField)), NULL, "Add this entry");

	if (i) {
		folder_add_pw(current_pw_sublist, pw);
		ui_statusline_msg("New password added");
	} else {
		pw_delete(pw);
		ui_statusline_msg("New password cancelled");
	}

	uilist_refresh();
	return;

end:
	pw_delete(pw);
}

static void
action_edit_pw(pw)
	password_t	*pw;
{
InputField	fields[] = {
	{"Name: ",		&pw->name,	STRING},
	{"Host: ",		&pw->host,	STRING},
	{"User: ",		&pw->user,	STRING},
	{"Password: ",		&pw->passwd,	STRING, pwgen_ask},
	{"Launch command: ",	&pw->launch,	STRING}
};

	action_input_dialog(fields, (sizeof(fields) / sizeof(InputField)), "Edit password");
}

void
action_list_rename()
{
password_t     *curpw;
folder_t       *curpwl;
char           *new_name;

	new_name = malloc(STRING_MEDIUM);

	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		curpw = uilist_get_highlighted_item();
		if (curpw) {
			new_name = ui_ask_str("New name", curpw->name);
			if (strlen(new_name) > 0)
				pw_rename(curpw, new_name);
			free(new_name);
		}
		break;

	case PW_SUBLIST:
		curpwl = uilist_get_highlighted_sublist();
		if (curpwl) {
			new_name = ui_ask_str("New sublist name", curpwl->name);
			if (strlen(new_name) > 0)
				folder_rename_sublist(curpwl, new_name);
			free(new_name);
		}
		break;

	case PW_UPLEVEL:
	case PW_NULL:
		/* do nothing */
		break;
	}

	uilist_refresh();
}

void
action_edit_options()
{
InputField	fields[] = {
	{ "GnuPG path: ",			&options->gpg_path,		STRING},
	{ "GnuPG key ID: ",			&options->gpg_id,		STRING},
	{ "Password file: ",			&options->password_file,	STRING},
	{ "Passphrase timeout (in minutes): ",	&options->passphrase_timeout,	INT},
	{ "Copy command: ",			&options->copy_command,		STRING},
};

	if (options->safemode) {
		ui_statusline_msg("Options cannot be changed in safe mode");
		return;
	}

	action_input_dialog(fields, (sizeof(fields) / sizeof(InputField)), "Edit Preferences");
	write_options = TRUE;
}

void
action_input_dialog_draw_items(dialog_win, fields, num_fields, title, msg, width)
	WINDOW		*dialog_win;
	InputField	*fields;
	char const	*title, *msg;
{
int		i, h = 0;

	wclear(dialog_win);

	box(dialog_win, 0, 0);

	if (title) {
		wattron(dialog_win, A_BOLD);
		i = strlen(title);
		h += 2;
		mvwaddstr(dialog_win, h, (width - i) / 2, title);
		wattroff(dialog_win, A_BOLD);
	}

	/* loop through fields */
	for (i = 0; i < num_fields; i++) {
		h += 2;

		switch (fields[i].type) {
		case STRING:
			mvwprintw(dialog_win, h, 3,
				  "%d. %s %s", (i + 1), fields[i].name,
				  *(char **)fields[i].value ? *(char **)fields[i].value : "");
			break;

		case INT:
			mvwprintw(dialog_win, h, 3,
				  "%d. %s %d", (i + 1), fields[i].name, *((int *)fields[i].value));
			break;

		case INFORMATION:
			mvwprintw(dialog_win, h, 3,
				  "%d. %s", (i + 1), fields[i].name);
			break;
		}
	}

	wattron(dialog_win, A_BOLD);

	if (msg) {
		i = strlen(msg);
		h += 2;
		mvwaddstr(dialog_win, h, (width - i) / 2, msg);
	}

	wattroff(dialog_win, A_BOLD);

	/*
	 * do final stuff the put all together
	 */
	wrefresh(dialog_win);
}

void
action_input_dialog(fields, num_fields, title)
	InputField	*fields;
	char const	*title;
{
int		 ch, i;
WINDOW		*dialog_win;
char const	*msg = "(press 'q' to return to list)";
int		 longest = 0, width;

	/*
	 * initialize the info window
	 */
	if (title)
		disp_h = ((num_fields + 2) * 2) + 3;
	else
		disp_h = ((num_fields + 1) * 2) + 3;

	for (i = 0; i < num_fields; i++) {
	int	this;
		/*
		 * Find the longest length of any item.
		 */
		this = strlen(fields[i].name) + 3;
		if (fields[i].type == STRING) {
			if (*(char **)fields[i].value)
				this += strlen(*(char **)fields[i].value);
		} else
			this += 10;

		if (this > longest)
			longest = this;
	}

	width = longest + 6;
	if (width > COLS)
		width = COLS;
	if (width < disp_w)
		width = disp_w;

	dialog_win = newwin(disp_h, width, (LINES - disp_h) / 2, (COLS - width) / 2);
	keypad(dialog_win, TRUE);

	action_input_dialog_draw_items(dialog_win, fields, num_fields, title, msg, width);

	/*
	 * actions loop
	 */
	while ((ch = wgetch(dialog_win)) != 'q') {
		if (options->readonly) {
			statusline_readonly();
			continue;
		}

		if ((ch >= '1') && (ch <= NUM_TO_CHAR(num_fields))) {
			i = CHAR_TO_NUM(ch);

			if (fields[i].autogen != NULL) {
				*(char **)fields[i].value = ui_ask_str_with_autogen(
							     fields[i].name,
						  *(char **)fields[i].value,
					      fields[i].autogen, CNTL('G'));

			} else if (fields[i].type == STRING) {
				*(char **)fields[i].value = ui_ask_str(fields[i].name,
						 *(char **)fields[i].value);

			} else if (fields[i].type == INT) {
				*(int *)fields[i].value = ui_ask_num(fields[i].name);

			} else if (fields[i].type == INFORMATION) {
				/* Easy, do nothing! */
			}

			action_input_dialog_draw_items(dialog_win, fields,
					num_fields, title, msg, width);
		} else if (ch == 'l') {
			delwin(dialog_win);
			action_list_launch();
			break;
		}
	}

	/*
	 * clean up
	 */
	delwin(dialog_win);
	uilist_refresh();
}

void
action_input_gpgid_dialog(fields, num_fields, title)
	InputField	*fields;
	char const	*title;
{
int		 i, valid_id;
int		 ch = '1', first_time = 1;
WINDOW		*dialog_win;
char const	*msg = "(press 'q' when export recipient list is complete)";
char		 msg2[80];

	/*
	 * initialize the info window
	 */
	disp_h = ((num_fields + 2) * 2) + 3;
	dialog_win = newwin(disp_h, disp_w, (LINES - disp_h) / 2, (COLS - disp_w) / 2);
	keypad(dialog_win, TRUE);

	action_input_dialog_draw_items(dialog_win, fields, num_fields, title,
				       msg, disp_w);

	/*
	 * actions loop - ignore read only as not changing main state
	 */
	while (first_time || ((ch = wgetch(dialog_win)) != 'q')) {
		/* On first loop, drop straight into recipient 1 */
		first_time = 0;

		if ((ch >= '1') && (ch <= NUM_TO_CHAR(num_fields))) {
			i = CHAR_TO_NUM(ch);
			*(char **)fields[i].value = ui_ask_str(fields[i].name,
						 *(char **)fields[i].value);

			/* Now verify it's a valid recipient */
			if (strlen(fields[i].value)) {
				valid_id = gnupg_check_id(fields[i].value);
				if (valid_id == 0) {
					/* Good, valid id */
				} else {
					/* Invalid id. Warn and blank */
					if (valid_id == -2)
						snprintf(msg2, sizeof(msg2), "Key expired for \"%s\"",
						 *(char **)fields[i].value);
					else
						snprintf(msg2, sizeof(msg2), "Invalid recipient \"%s\"",
						 *(char **)fields[i].value);

					ui_statusline_msg(msg2);
					*(char **)fields[i].value = NULL;
				}

				/* Redraw display */
				action_input_dialog_draw_items(dialog_win,
							       fields,
							       num_fields,
							       title, msg,
							       disp_w);
			}
		}
	}

	/*
	 * clean up
	 */
	delwin(dialog_win);
	uilist_refresh();
}

int
action_yes_no_dialog(fields, num_fields, title, question)
	InputField	*fields;
	char const	*title, *question;
{
int		i;
WINDOW         *dialog_win;

	/*
	 * initialize the info window
	 */
	if (title)
		disp_h = ((num_fields + 2) * 2) + 3;
	else
		disp_h = ((num_fields + 1) * 2) + 3;

	dialog_win = newwin(disp_h, disp_w,
			    (LINES - disp_h) / 2, (COLS - disp_w) / 2);
	keypad(dialog_win, TRUE);

	action_input_dialog_draw_items(dialog_win, fields, num_fields, title,
				       NULL, disp_w);

	i = ui_ask_yes_no(question, 1);

	/*
	 * clean up
	 */
	delwin(dialog_win);
	uilist_refresh();

	return i;
}

void
action_list_add_sublist()
{
char           *name;
folder_t       *sublist, *iter;

	name = ui_ask_str("Sublist name:", NULL);
	for (iter = current_pw_sublist->sublists; iter != NULL; iter = iter->next) {
		if (strcmp(iter->name, name) == 0) {
			free(name);
			return;
		}
	}

	sublist = folder_new(name);
	free(name);

	folder_add_sublist(current_pw_sublist, sublist);
	uilist_refresh();
}

int
action_list_at_top_level()
{
	if (current_pw_sublist->parent) {
		action_list_up_one_level();
		uilist_refresh();
		return 0;
	} else {
		return 1;
	}
}

void
action_list_select_item()
{
password_t	*curpw;
folder_t	*curpwl;
search_result_t	*cursearch;

	/* Are they searching, or in normal mode? */
	if (search_results != NULL) {
		cursearch = uilist_get_highlighted_searchresult();
		curpwl = cursearch->sublist;
		curpw = cursearch->entry;

		if (curpw) {
			action_edit_pw(curpw);
		} else if (curpwl) {
			/* Quite out of searching */
			search_remove();

			/* Now display the selected sublist */
			current_pw_sublist = curpwl;
			unmark_entries();
			uilist_refresh();
		}

		return;
	}

	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		curpw = uilist_get_highlighted_item();
		if (curpw)
			action_edit_pw(curpw);
		break;

	case PW_SUBLIST:
		curpwl = uilist_get_highlighted_sublist();
		if (curpwl) {
			current_pw_sublist = curpwl;
			unmark_entries();
			uilist_refresh();
		}
		break;

	case PW_UPLEVEL:
		action_list_up_one_level();
		break;

	case PW_NULL:
		/* do nothing */
		break;
	}
}

void
action_list_delete_item()
{
password_t     *curpw;
folder_t       *curpwl;
search_result_t *cursearch;
int		i;
char		str[STRING_LONG];

	if (search_results) {
		cursearch = uilist_get_highlighted_searchresult();
		curpwl = cursearch->sublist;
		curpw = cursearch->entry;

		if (curpw) {
			snprintf(str, sizeof(str), "Really delete \"%s\"", curpw->name);
			if ((i = ui_ask_yes_no(str, 0)) != 0) {
				pw_delete(curpw);
				ui_statusline_msg("Password deleted");
			} else
				ui_statusline_msg("Password not deleted");

			search_remove();
			return;
		}

		snprintf(str, sizeof(str), "Really delete sublist \"%s\"", curpwl->name);
		if ((i = ui_ask_yes_no(str, 0)) != 0) {
			folder_delete_sublist(curpwl->parent, curpwl);
			ui_statusline_msg("Password sublist deleted");
		} else
			ui_statusline_msg("Password not deleted");

		search_remove();
		return;
	}

	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		curpw = uilist_get_highlighted_item();

		if (curpw) {
			snprintf(str, sizeof(str), "Really delete \"%s\"", curpw->name);
			i = ui_ask_yes_no(str, 0);
			if (i) {
				pw_delete(curpw);
				ui_statusline_msg("Password deleted");
			} else
				ui_statusline_msg("Password not deleted");
		}
		break;

	case PW_SUBLIST:
		curpwl = uilist_get_highlighted_sublist();

		if (curpwl) {
			snprintf(str, sizeof(str), "Really delete sublist \"%s\"", curpwl->name);
			i = ui_ask_yes_no(str, 0);
			if (i) {
				folder_delete_sublist(curpwl->parent, curpwl);
				ui_statusline_msg("Password sublist deleted");
			} else
				ui_statusline_msg("Password not deleted");
		}
		break;

	case PW_UPLEVEL:
	case PW_NULL:
		/* do nothing */
		break;
	}
	uilist_refresh();
}

void
action_list_move_item()
{
password_t     *curpw, *tmp;
folder_t       *curpwl, *list, *nextl;
int		type;

	if (search_results) {
		ui_statusline_msg("Cannot move items from a search result");
		return;
#ifdef notyet
		/*
		 * This would allow moving items from the search result
		 * list.  Disabled for now as I'm not sure it's that useful,
		 * and it may have problems with referential integrity.
		 */
	search_result_t	*sr = uilist_get_highlighted_searchresult();

		if (sr->entry) {
			ui_statusline_msg("Items can only be moved to a sublist or parent");
			return;
		}

		if (!sr->entry && sr->sublist->marked) {
			ui_statusline_msg("Cannot move a sublist to itself");
			return;
		}

		curpwl = sr->sublist;

		for (sr = search_results; sr; sr = sr->next) {
			if (sr->entry) {
				if (!sr->entry->marked)
					continue;

				folder_detach_pw(sr->sublist, sr->entry);
				folder_add_pw(curpwl, sr->entry);
				continue;
			}

			folder_detach_sublist(sr->sublist->parent, sr->sublist);
			folder_add_sublist(curpwl, sr->sublist);
			continue;
		}
		return;
#endif
	}

	type = uilist_get_highlighted_type();

	if (type != PW_SUBLIST && type != PW_UPLEVEL) {
		ui_statusline_msg("Items can only be moved to a sublist or parent");
		return;
	}

	if (type == PW_UPLEVEL)
		curpwl = current_pw_sublist->parent;
	else {
		curpwl = uilist_get_highlighted_sublist();
		if (curpwl->marked) {
			ui_statusline_msg("Cannot move a sublist to itself");
			return;
		}
	}

	PWLIST_FOREACH_SAFE(curpw, &current_pw_sublist->list, tmp) {
		if (!curpw->marked)
			continue;

		folder_detach_pw(current_pw_sublist, curpw);
		folder_add_pw(curpwl, curpw);
	}

	list = current_pw_sublist->sublists;
	while (list) {
		nextl = list->next;

		if (list->marked) {
			folder_detach_sublist(current_pw_sublist, list);
			folder_add_sublist(curpwl, list);
		}

		list = nextl;
		if (!list)
			break;
	}

	uilist_refresh();
}

void
action_list_move_item_up_level()
{
password_t     *curpw;
folder_t       *curpwl;

	/* Do nothing if searching */
	if (search_results != NULL)
		return;

	/* Do the right thing based on type */
	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		curpw = uilist_get_highlighted_item();
		if (curpw && current_pw_sublist->parent) {
			folder_detach_pw(current_pw_sublist, curpw);
			folder_add_pw(current_pw_sublist->parent, curpw);
			uilist_refresh();
		}
		break;

	case PW_SUBLIST:
		curpwl = uilist_get_highlighted_sublist();
		if (curpwl && current_pw_sublist->parent) {
			folder_detach_sublist(current_pw_sublist, curpwl);
			folder_add_sublist(current_pw_sublist->parent, curpwl);
			uilist_refresh();
		}
		break;

	case PW_UPLEVEL:
	case PW_NULL:
		/* do nothing */
		break;
	}
}

void
action_list_up_one_level()
{
	/* move up one sublist */
	if (current_pw_sublist->parent) {
		current_pw_sublist = current_pw_sublist->parent;
		unmark_entries();
		uilist_refresh();
	}
}

void
action_list_export()
{
password_t     *curpw;
folder_t       *curpwl;

	debug("list_export: enter switch");
	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		debug("list_export: is a pw");
		curpw = uilist_get_highlighted_item();
		if (curpw)
			folder_export_passwd(curpw);
		break;

	case PW_SUBLIST:
		debug("list_export: is a folder");
		curpwl = uilist_get_highlighted_sublist();
		if (curpwl)
			folder_export_list(curpwl);
		break;

	case PW_UPLEVEL:
	case PW_NULL:
		/* do nothing */
		break;
	}
}

static void
_create_information_field(name, field)
	char const	*name;
	InputField	*field;
{
	field->name = name;
	field->type = INFORMATION;
	field->value = NULL;
}

void
action_list_locate()
{
int		depth = 0, count = 0;
char           *currentName = NULL;
InputField     *fields;
password_t     *curpw = NULL;
folder_t       *curpwl = NULL;
folder_t       *parent = NULL;
search_result_t *cursearch;

	if (search_results != NULL) {
		cursearch = uilist_get_highlighted_searchresult();
		curpwl = cursearch->sublist;
		curpw = cursearch->entry;

		if (curpw)
			parent = curpwl;
		else
			parent = curpwl->parent;
	} else {
		parent = current_pw_sublist;
		switch (uilist_get_highlighted_type()) {
		case PW_ITEM:
			curpw = uilist_get_highlighted_item();
			break;

		case PW_SUBLIST:
			curpwl = uilist_get_highlighted_sublist();
			break;

		case PW_NULL:
		case PW_UPLEVEL:
			/* do nothing */
			break;
		}
	}

	if (curpw) {
		currentName = curpw->name;
		depth = 1;
	} else if (curpwl) {
		currentName = curpwl->name;
		depth = 1;
	} else
		return;

	/* Figure out how many parents we have */
	curpwl = parent;
	while (curpwl) {
		curpwl = curpwl->parent;
		depth++;
	}
	count = depth;

	/* Now grab their names */
	fields = xcalloc(depth, sizeof(InputField));
	if (currentName) {
		depth--;
		_create_information_field(currentName, &fields[depth]);
	}

	curpwl = parent;
	while (curpwl) {
		depth--;
		_create_information_field(curpwl->name, &fields[depth]);
		curpwl = curpwl->parent;
	}

	/* Have it rendered */
	action_input_dialog(fields, count, "Location of Item");

	/* All done, tidy up */
	free(fields);
}

void
action_list_launch()
{
int		i;
password_t     *curpw;
char		msg[STRING_LONG];

	if (options->safemode) {
		ui_statusline_msg("Launch not allowed in safe mode");
		return;
	}

	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		debug("list_launch: is a pw");
		curpw = uilist_get_highlighted_item();
		if (curpw) {
			if (!curpw->launch || !*curpw->launch) {
				ui_statusline_msg("Launch command not defined");
				return;
			}

			i = launch(curpw);
			snprintf(msg, sizeof(msg), "Application exited with code %d", i);
			ui_statusline_msg(msg);
		}
		break;

	case PW_SUBLIST:
	case PW_UPLEVEL:
	case PW_NULL:
		/* do nothing */
		break;
	}
}

void
action_list_read_file()
{
	folder_free_all();

	if (folder_read_file() != 0) {
		folder = folder_new("Main");
		current_pw_sublist = folder;
	}

	uilist_refresh();
}

void
action_list_move_item_up()
{
password_t     *curpw;
folder_t       *curpwl;
int		worked = 0;

	/* Do nothing if searching */
	if (search_results != NULL)
		return;

	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		curpw = uilist_get_highlighted_item();
		worked = folder_change_item_order(curpw, current_pw_sublist, 1);
		break;

	case PW_SUBLIST:
		curpwl = uilist_get_highlighted_sublist();
		worked = folder_change_list_order(curpwl, 1);
		break;

	case PW_UPLEVEL:
	case PW_NULL:
		/* do nothing */
		break;
	}

	if (worked) {
		uilist_up();
	}
}

void
action_list_copy_username()
{
password_t	*curpw;
search_result_t	*cursearch;

	if (options->safemode) {
		ui_statusline_msg("Clipboard cannot be used in safe mode");
		return;
	}

	if (search_results != NULL) {
		cursearch = uilist_get_highlighted_searchresult();
		curpw = cursearch->entry;
	} else {
		if (uilist_get_highlighted_type() != PW_ITEM)
			return;
		curpw = uilist_get_highlighted_item();
	}

	if (!curpw)
		return;
	if (copy_string(curpw->user) == 0)
		ui_statusline_msg("Username copied");
	else
		ui_statusline_msg("Failed to copy username");
}

void
action_list_copy_pw()
{
password_t	*curpw;
search_result_t	*cursearch;

	if (options->safemode) {
		ui_statusline_msg("Clipboard cannot be used in safe mode");
		return;
	}

	if (search_results != NULL) {
		cursearch = uilist_get_highlighted_searchresult();
		curpw = cursearch->entry;
	} else {
		if (uilist_get_highlighted_type() != PW_ITEM)
			return;
		curpw = uilist_get_highlighted_item();
	}

	if (!curpw)
		return;

	if (copy_string(curpw->passwd) == 0)
		ui_statusline_msg("Password copied");
	else
		ui_statusline_msg("Failed to copy password");
}

void
action_list_move_item_down()
{
password_t     *curpw;
folder_t       *curpwl;
int		worked = 0;

	/* Do nothing if searching */
	if (search_results != NULL)
		return;

	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		curpw = uilist_get_highlighted_item();
		worked = folder_change_item_order(curpw, current_pw_sublist, 0);
		break;

	case PW_SUBLIST:
		curpwl = uilist_get_highlighted_sublist();
		worked = folder_change_list_order(curpwl, 0);
		break;

	case PW_UPLEVEL:
	case PW_NULL:
		/* do nothing */
		break;
	}

	if (worked)
		uilist_down();
}

void
action_list_mark()
{
password_t	*curpw;
folder_t	*curpwl;

	if (search_results) {
	search_result_t	*sr = uilist_get_highlighted_searchresult();
		if (sr->entry)
			sr->entry->marked = !sr->entry->marked;
		else
			sr->sublist->marked = !sr->sublist->marked;
		uilist_refresh();
		return;
	}

	switch (uilist_get_highlighted_type()) {
	case PW_ITEM:
		if ((curpw = uilist_get_highlighted_item()) == NULL)
			return;
		curpw->marked = !curpw->marked;
		break;

	case PW_SUBLIST:
		if ((curpwl = uilist_get_highlighted_sublist()) == NULL)
			return;
		curpwl->marked = !curpwl->marked;
		break;

	default:
		break;
	}

	uilist_refresh();
}

void
unmark_entries()
{
password_t	*pw;
folder_t	*pwl;

	if (search_results) {
	search_result_t	*sr;
		for (sr = search_results; sr; sr = sr->next)
			if (sr->entry)
				sr->entry->marked = 0;
			else
				sr->sublist->marked = 0;
		return;
	}

	PWLIST_FOREACH(pw, &current_pw_sublist->list)
		pw->marked = 0;

	for (pwl = current_pw_sublist->sublists; pwl; pwl = pwl->next)
		pwl->marked = 0;
}
