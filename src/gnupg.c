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


/*
 * define for strings to check for with regex
 * should allow for internationalization
 */

#define GPG_ERR_CANTWRITE	"file create error"
#define GPG_ERR_CANTOPEN 	"can't open"
#define GPG_ERR_BADPASSPHRASE	"bad passphrase"
#define GPG_ERR_NOSECRETKEY	"secret key not available"

/* end defines */

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/wait.h>

#include	<unistd.h>
#include	<time.h>
#include	<regex.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<assert.h>
#include	<pwd.h>
#include	<limits.h>

#include	<libxml/tree.h>
#include	<libxml/parser.h>

#include	"pwman.h"
#include	"ui.h"
#include	"actions.h"
#include	"gnupg.h"

#define STDOUT 0
#define STDIN 1
#define STDERR 2

static int passphrase_good = 0;
static int gnupg_hit_sigpipe = 0;

static char	*gnupg_expand_filename(char const *);

static void
gnupg_sigpipe_handler()
{
	gnupg_hit_sigpipe = 1;
}

static char *
gnupg_add_to_buf(buf, new)
	char		*buf;
	char const	*new;
{
size_t	size;

	if (new == NULL)
		return buf;

	if (buf == NULL) {
		buf = malloc(strlen(new) + 1);
		strcpy(buf, new);
		return buf;
	}
	
	size = strlen(buf) + strlen(new) + 1;
	buf = realloc(buf, size);
	strcat(buf, new);
	
	return buf;
}

static int
gnupg_str_in_buf(buf, check)
	char const	*buf, *check;
{
regex_t	reg;

	debug("str_in_buf: start checking");
	
	if ((buf == NULL) || (check == NULL))
		return 0;

	regcomp(&reg, check ,0);
	if (regexec(&reg, buf, 0, NULL , 0) == 0) {
		regfree(&reg);
		return 1;
	} else {
		regfree(&reg);
		return 0;
	}
}

static char *
gnupg_expand_filename(filename)
	char const	*filename;
{
char const	*home;
char		*ret;

	if (*filename != '~')
		return strdup(filename);

	if ((home = getenv("HOME")) == NULL) {
	struct passwd	*pw;
		if ((pw = getpwuid(getuid())) == NULL)
			return strdup(filename);
		home = pw->pw_dir;
	}

	ret = malloc(strlen(filename + 1) + strlen(home) + 1);
	sprintf(ret, "%s%s", home, filename + 1);
	return ret;
}

static int 
gnupg_exec(char *path, char *args[], FILE *stream[3])
{
	int stdin_fd[2];
	int stdout_fd[2];
	int stderr_fd[2];
	int pid, ret;

	pipe(stdin_fd);
	pipe(stdout_fd);
	pipe(stderr_fd);

	if ((path == NULL) || (args == NULL)) 
		return -1;

	pid = fork();

	/* Do the right thing with the fork */
	if (pid == 0) {
		close(stdout_fd[0]);
		close(stdin_fd[1]);
		close(stderr_fd[0]);

		dup2(stdout_fd[1], STDOUT_FILENO);
		dup2(stdin_fd[0], STDIN_FILENO);
		dup2(stderr_fd[1], STDERR_FILENO);

		ret = execv(path, args);
		if(ret)
			pw_abort("Failed to run %s, aborted with error code %d", path, errno);
		abort(); /*NOTREACHED*/
	} else {
		close(stdout_fd[1]);
		close(stdin_fd[0]);
		close(stderr_fd[1]);

		if (stream != NULL) {
			stream[STDOUT] = fdopen(stdout_fd[0], "r");
			stream[STDIN] = fdopen(stdin_fd[1], "w");
			stream[STDERR] = fdopen(stderr_fd[0], "r");
		}
		
		/* Mark us as not having had a sigpipe yet */
		gnupg_hit_sigpipe = 0;
		signal(SIGPIPE, gnupg_sigpipe_handler);

		return pid;
	}
}

static void
gnupg_exec_end(int pid, FILE *stream[3])
{
char	buf[STRING_LONG];

	/* If we hit a problem, report that */
	if (gnupg_hit_sigpipe) {
		fputs("GPG hit an error and died...\n", stderr);
		while (fgets(buf, STRING_LONG - 1, stream[STDOUT]) != NULL)
			fputs(buf, stderr);

		while (fgets(buf, STRING_LONG - 1, stream[STDERR]) != NULL)
			fputs(buf, stderr);

		/* TODO - figure out why we don't get the real error message from */
		/*  GPG displayed here like one might expect */
	}
	signal(SIGPIPE, NULL);

	/* Close up */
	debug("gnupg_exec_end : close streams");
	if (stream[0])
		fclose(stream[0]);

	if (stream[1])
		fclose(stream[1]);

	if (stream[2])
		fclose(stream[2]);

	debug("waiting for pid %d", pid);
	waitpid(pid, NULL, 0);

	/* Bail out if gpg broke */
	if (gnupg_hit_sigpipe)
		exit(1);
}

static char *
gnupg_find_recp(str)
	char const	*str;
{
char	*user, *start, *end;
int	 size;
	debug(str);

	/* Is it "<id>" ? */
	start = strstr(str,"\"");
	if(start != NULL) {
		start += 1;
		end = strstr(start,"\"");
	} else {
		/* Is it ID <id>\n? */
		start = strstr(str,"ID");
		if(start != NULL) {
			start += 3;
			end = strstr(start,"\n");
		} else {
			/* No idea */
			user = "(not sure)";
			return user;
		}
	}

	size = end - start;
	user = malloc(size+1);
	strcpy(user, start);
	debug("Recipient is %s", user);
	return user;
}

/* Returns 0 if found, -1 if not found, and -2 if found but expired */
int
gnupg_check_id(id)
	char const	*id;
{	
regex_t	 reg, expired_reg;
int	 pid;
char	 text[STRING_LONG], idstr[STRING_LONG], keyid[STRING_LONG];
char	 exp[STRING_LONG], *args[4];
FILE	*streams[3];
int	 id_is_key_id = 0, key_found = 0, key_is_expired = -1;
	
	debug("check_gnupg_id: check gnupg id\n");

	/* Check we're given nice input */
	if (strchr(id, '%')) 
		/* hmm, could be format string bug so tell get lost */
		return -1;

	/* Build our expired key matching regexp */
	snprintf(exp, STRING_LONG, "^(pub|sub):e:");
	regcomp(&expired_reg, exp, REG_EXTENDED);

	/* Is the supplied ID really a key ID? */
	/* (If it is, it's 8 chars long, and 0-9A-F) */
	snprintf(keyid, STRING_LONG, "^[0-9A-Z]{8}$");
	regcomp(&reg, keyid,REG_EXTENDED);
	if (regexec(&reg, id, 0, NULL , 0) == 0) {
		/* The supplied ID is a key ID */
		id_is_key_id = 1;
		debug("check_gnupg_id: supplied ID was a gnupg key id\n");
	} else {
		debug("check_gnupg_id: supplied ID taken to be an email address\n");
	}

	if (id_is_key_id == 1) {
		/* Match on "pub:.:SIZE:type:FULLID:DATE" */
		/* Where FULLID is 8 chars then the 8 chars we expected */
		snprintf(idstr, STRING_LONG, "^pub:.:[0-9]+:[0-9]+:[0-9a-zA-Z]{8}%s:", id);
	} else {
		/* Match on "(pub|uid) .... NAME <EMAIL ADDRESS>" */
		snprintf(idstr, STRING_LONG, "[^<]*<%s>", id);
	}

	/* Fire off GPG to find all our keys */
	args[0] = "gpg";
	args[1] = "--with-colons";
	args[2] = "--list-keys";
	args[3] = NULL;

	pid = gnupg_exec(options->gpg_path, args, streams);

	while (fgets(text, STRING_LONG, streams[STDOUT])) {
		regcomp(&reg, idstr, REG_EXTENDED);
		if (regexec(&reg, text, 0, NULL , 0) == 0) {
			/* Found the key! */
			key_found = 1;

			/* Check it isn't also expired */
			if (regexec(&expired_reg, text, 0, NULL , 0) == 0) {
				/*
				 * Only mark as expired if we haven't found another
				 * version that isn't expired.
				 */
				if (key_is_expired == -1)
					key_is_expired = 1;
			} else {
				key_is_expired = 0;
			}
		}
	}
	
	/* Tidy up */
	gnupg_exec_end(pid, streams);

	/* If we found it, return found / found+expired */
	if (key_found) {
		if (key_is_expired)
			return -2; 
		return 0; 
	}

	/* Didn't find it */
	return -1;
}

/**
 * Get a single GnuPG Recipient ID
 */
char *
gnupg_get_id()
{
	for (;;) {
	char	*id;
		id = ui_statusline_ask_str("GnuPG Recipient ID:");
		if (id[0] == 0)
			return id;

		if (gnupg_check_id(id) == 0)
			return id;

		xfree(id);
		debug("get_gnupg_id: if here is reached id is bad");
		ui_statusline_msg("Bad recipient, try again");
		getch();
	}
}

/**
 * Get multiple GnuPG Recipient IDs
 */
void
gnupg_get_ids(ids, max_id_num)
	char	**ids;
	size_t	  max_id_num;
{
InputField	*fields;
size_t		i;

	fields = xcalloc(max_id_num, sizeof(*fields));

	for (i = 0; i < max_id_num; i++) {
		fields[i].value = &ids[i]; /* String to write into comes from caller */
		fields[i].type = STRING;

		fields[i].name = xmalloc(STRING_LONG + 1); /* Needs a local string to write into */
		snprintf(fields[i].name, STRING_LONG + 1,
			 "Recipient %d: ", (int) (i + 1));
	}

	/* Prompt to edit the recipients. This will verify the IDs for us. */
	action_input_gpgid_dialog(fields, max_id_num, "Edit Recipients");

	for (i = 0; i < max_id_num; i++)
		free(fields[i].name);
	free(fields);
}

char *
gnupg_get_filename(rw)
{
char	*ret;

	assert(rw == 'r' || rw == 'w');

	ret = malloc(PATH_MAX + 1);
	ret = ui_statusline_ask_str(rw == 'r' ? "File to read from:" : "File to write to:");
	return ret;
}

const char *
gnupg_get_passphrase()
{
static char	*passphrase = NULL;

/*	passphrase == malloc(V_LONG_STR);*/
	if ((time_base >= (time(NULL) - (options->passphrase_timeout*60))) && (passphrase_good == 1))
		return passphrase;
	
	passphrase_good = 0;
	passphrase = ui_statusline_ask_passwd("Enter GnuPG passphrase (^G to Cancel):", 0x07); /* 0x07 == ^G */
	passphrase_good = 1;

	return passphrase;
}

void
gnupg_forget_passphrase()
{
	passphrase_good = 0;

	debug("forget_passphrase: passphrase forgotten");
	ui_statusline_msg("Passphrase forgotten");
}

static int 
gnupg_check_executable()
{
	FILE *streams[3];
	char *args[3];
	int pid, count, version[3];
	
	debug("check_gnupg: start");
	if(options->gpg_path == NULL){
		return -1;
	}

	args[0] = "gpg";
	args[1] = "--version";
	args[2] = NULL;
	
	pid = gnupg_exec(options->gpg_path, args, streams);

	/* this might do version checking someday if needed */
	count = fscanf(streams[STDOUT], "gpg (GnuPG) %d.%d.%d", 
			&version[0], &version[1], &version[2]);
	gnupg_exec_end(pid, streams);

	debug("exec ended");
	if(count != 3){
		ui_statusline_msg("WARNING! GnuPG Executable not found"); getch();
		return -1;
	} else {
		debug("check_gnupg: Version %d.%d.%d", version[0], version[1], version[2]);	
		return 0;
	}
}

int
gnupg_write_many(xmlDocPtr doc, char **ids, int num_ids, char const *filename)
{
FILE	*streams[3];
char	 buf[STRING_LONG];
char	*args[7 + 1 + (2 * 10)]; /* 7 initial args, 1 null, 10 recipients */
char	*err;
int	 pid, i, pos, num_valid_ids;
char	*expfile;

	debug("gnupg_write: do some checks");	
	if ((gnupg_check_executable() != 0) || !filename || (filename[0] == 0)){
		debug("gnupg_write: no gnupg or filename not set");
		return -1;
	}

	/* Ensure all IDs are valid, and we have enough */
	num_valid_ids = 0;
	for (i=0; i < num_ids; i++) {
		if (ids[i]) {
			if (gnupg_check_id(ids[i]) != 0) {
				ids[i] = gnupg_get_id();
				
				if(ids[i][0] == 0)
					return -1;
			}

			num_valid_ids++;
		}
	}
	debug("gnupg_write: writing to %d recipients", num_valid_ids);

	if (num_valid_ids == 0)
		return -1;

	debug("gnupg_write: start writing");

	expfile = gnupg_expand_filename(filename);

	err = NULL;
	args[0] = "gpg";
	args[1] = "-e";
	args[2] = "-a";
	args[3] = "--always-trust"; /* gets rid of error when moving 
				       keys from other machines */
	args[4] = "--yes";
	args[5] = "-o";
	args[6] = expfile;

	/* Add in all the recipients */
	pos = 7;
	for (i=0; i<num_ids; i++) {
		if (ids[i][0] != 0) {
			args[pos] = "-r";
			pos++;
			args[pos] = ids[i];
			pos++;
		}
	}
	args[pos] = NULL;

	for (;;) {
		/* clear err buffer */
		if (err) {
			free(err);
			err = NULL;
		}

		pid = gnupg_exec(options->gpg_path, args, streams);

#if LIBXML_VERSION >= 20423
		xmlDocFormatDump(streams[STDIN], doc, TRUE);
#else
		xmlDocDump(streams[STDIN], doc);
#endif
		close(fileno(streams[STDIN]));
		
		while (fgets(buf, STRING_LONG - 1, streams[STDERR]) != NULL)
			err = gnupg_add_to_buf(err, buf);
		gnupg_exec_end(pid, streams);
		
		debug("gnupg_write: start error checking");
		/*
		 * check for errors(no key, bad pass, no file etc etc)
		 */
		if (gnupg_str_in_buf(err, GPG_ERR_CANTWRITE)) {
			debug("gnupg_write: cannot write to %s", expfile);

			snprintf(buf, STRING_LONG, "Cannot write to %s", expfile);
			ui_statusline_msg(buf);
			getch();

			free(expfile);
			expfile = gnupg_get_filename('w');
			if (!expfile)
				return -1;

			continue;
		}
		break;
	}

	ui_statusline_msg("List saved");
	debug("gnupg_write: file write sucessful");
	
	free(expfile);
	return 0;
}

int
gnupg_write(xmlDocPtr doc, char* id, char* filename)
{
	return gnupg_write_many(doc, &id, 1, filename);
}

int
gnupg_read(filename, doc)
	char const	*filename;
	xmlDocPtr	*doc;
{
char		*args[9], *data, *err, buf[STRING_LONG], *user, *expfile;
char const	*passphrase;
FILE		*streams[3];
int		 pid;
int		 ret = 0;
	
	if (gnupg_check_executable() != 0){
		*doc = xmlNewDoc((xmlChar const *) "1.0");
		return -1;
	}
/*
	if(check_gnupg_passphrase(filename) == -1){
		return -1;
	}*/

	expfile = gnupg_expand_filename(filename);

	data = NULL;
	err = NULL;
	user = NULL;
	args[0] = "gpg";
	args[1] = "--passphrase-fd";
	args[2] = "0";
	args[3] = "--no-verbose";
	args[4] = "--batch";
	args[5] = "--output";
	args[6] = "-";
	args[7] = expfile;
	args[8] = NULL;
	
	for (;;) {
		/* clear buffers */
		if (err != NULL) {
			free(err); 
			err = NULL; 
		}

		if (data != NULL) {
			free(data);
			data = NULL;
		}
		
		passphrase = gnupg_get_passphrase();

		if (passphrase == NULL) {
			/* They hit cancel on the password prompt */
			write_options = 0;
			ret = 255;
			break;
		}

		pid = gnupg_exec(options->gpg_path, args, streams);

		fputs(passphrase, streams[STDIN]);
		fputc('\n' , streams[STDIN]);
		fclose(streams[STDIN]);
		streams[STDIN] = NULL;

		debug("gnupg_read: start reading data");
		while (fgets(buf, STRING_LONG - 1, streams[STDOUT]) != NULL)
			data = gnupg_add_to_buf(data, buf);

		while (fgets(buf, STRING_LONG - 1, streams[STDERR]) != NULL)
			err = gnupg_add_to_buf(err, buf);
	
		gnupg_exec_end(pid, streams);

		debug("gnupg_read: start error checking");
		debug(err);

		/*
		 * check for errors(no key, bad pass, no file etc etc)
		 */
		if (gnupg_str_in_buf(err, GPG_ERR_BADPASSPHRASE)) {
			debug("gnupg_read: bad passphrase");
			ui_statusline_msg("Bad passphrase, please re-enter");
			getch();

			passphrase_good = 0;
			continue;
		}

		if (gnupg_str_in_buf(err, GPG_ERR_CANTOPEN)) {
			debug("gnupg_read: cannot open %s", filename);
			snprintf(buf, STRING_LONG, "Cannot open file \"%s\"", expfile);
			ui_statusline_msg(buf);
			getch();

			ret = -1;
			break;
		}

		if (gnupg_str_in_buf(err, GPG_ERR_NOSECRETKEY)) {
			debug("gnupg_read: secret key not available!");
			user = gnupg_find_recp(err);
			snprintf(buf, STRING_LONG, "You do not have the secret key for %s", user);
			ui_statusline_msg(buf);
			getch();

			ret = 254;
			break;
		}
		break;
	}

	free(expfile);

	debug("gnupg_read: finished read");
	if (data != NULL) {
		debug("gnupg_read: data != NULL");
		*doc = xmlParseMemory(data, strlen(data));
		free(data);
	} else {
		debug("gnupg_read: data is null");
		*doc = xmlNewDoc((xmlChar const *) "1.0");
	}

	if (err != NULL)
		free(err);

	if (user != NULL)
		free(user);

	debug("gnupg_read: finished all");

	return ret;
}
