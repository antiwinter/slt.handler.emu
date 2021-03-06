#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>
#include "wait-queue.h"

#define SZ 16
#define TIMEOUT 5
int acount = 0;
struct alias {
	char var[SZ];
	int num, arg;
	char str[SZ];
} aliases[SZ];

int lscount = 0, onscount = 0;
struct line {
	int argc;
	char *argv[SZ];
} lines[SZ * 2], ons[SZ];
int bin = 0, power_state = 0, stop = 0;

struct alias *get_alias(const char *var, int index)
{
	int i;
	for(i = 0; i < acount; i++) {
		if(strcmp(aliases[i].var, var) == 0) {
			if(!index) return &aliases[i];
			index--;
		}
	}
	return NULL;
}

void exec_on(const char *var)
{
	int i, find = 0;
	for(i = 0; i < onscount; i++) {
		if(strcmp(ons[i].argv[0], var) == 0 ||
			(!find && strcmp(ons[i].argv[0], "X") == 0)) {

			find = 1;
			exec_line(ons[i].argc - 1, &ons[i].argv[1]);
		}
	}
}

void escape_strcpy(char *dst, const char *src)
{
	char v[4] = {0, 0, 0, 0};

	for(; *src; src++, dst++) {
		if(*src == '\\') {
			v[0] = *++src;
			v[1] = *++src;
			v[2] = *++src;
			*dst = strtol(v, NULL, 8);
		} else
			*dst = *src;
	}

	*dst = 0;
}

int parse_line(int argc, char *argv[])
{
	static int i, start = 0;

	if(!argc) return 0;
	if(*argv[0] == '#') {
//		ui_print("comment: %s\n", argv[0]);
		return 0;
	}

	if(strcmp(argv[0], "end") == 0) {
		start = 0;
		return 0;
	}

	if(start) {
		ui_print("register actions: ");
		for(i = 0; i < argc; i++) {
			lines[lscount].argv[lines[lscount].argc] = malloc(SZ);
			escape_strcpy(lines[lscount].argv[lines[lscount].argc], argv[i]);
			lines[lscount].argc++;
			ui_print("%s ", argv[i]);
		}
		ui_print("\n");
		lscount++;
		return 0;
	}

	if(strcmp(argv[0], "set") == 0) {
		escape_strcpy(aliases[acount].var, argv[1]);
		aliases[acount].arg = 0;
		if(argc > 2)
			aliases[acount].num = atoi(argv[2]);
		if(argc > 3)
			escape_strcpy(aliases[acount].str, argv[3]);
		acount++;
		ui_print("set alias: %s: %d, %s\n", argv[1], atoi(argv[2]), (argc > 3)? argv[3]: "null");
	} else if(strcmp(argv[0], "on") == 0) {
		ons[onscount].argc = 0;
		ui_print("set on: ");
		for(i = 1; i < argc; i++) {
			ui_print("%s ", argv[i]);
			ons[onscount].argv[ons[onscount].argc] = malloc(SZ);
			escape_strcpy(ons[onscount].argv[ons[onscount].argc], argv[i]);
			ons[onscount].argc++;
		}
		ui_print("\n");
		onscount++;
	} else if(strcmp(argv[0], "start") == 0)
		start = 1;
	else
		ui_print("unrecgnized parser command: %s\n", argv[0]);

	return -1;
}

int exec_line(int argc, char *argv[])
{
	struct alias *al;
	int i;

	if(!argc) return 0;
	ui_print("do action: ");
	for(i = 0; i < argc; i++)
		ui_print("%s ", argv[i]);
	ui_print("\n");

	if(strcmp(argv[0], "load") == 0) {
		ui_animation(1);
	} else if(strcmp(argv[0], "unload") == 0) {
		ui_animation(3);
	} else if(strcmp(argv[0], "poweron") == 0) {
		power_state = 1;
	} else if(strcmp(argv[0], "poweroff") == 0) {
		power_state = 1;
	} else if(strcmp(argv[0], "putbin") == 0) {
		for(i = 0;;) {
			al = get_alias("bin", i++);
			if(al) ui_bin_update(i, al->arg);
			else break;
		}
		al = get_alias("timeout", 0);
		if(al) ui_bin_update(i, al->arg);
	} else if(strncmp(argv[0], "send", 4) == 0) {
		if(strcmp(argv[0], "send_txt") == 0) {
			io_send("\002");
			ui_print("--> <STX>\n");
		}
		for(i = 1; i < argc; i++) {
			const char *s = argv[i];
			if(strcmp(s, "power_state") == 0)
				s = power_state? "1": "NULL";
			al = get_alias(s, 0);
			ui_print("--> %s\n", al? al->str: s);
			io_send(al? al->str: s);
		}
		if(strcmp(argv[0], "send_txt") == 0) {
			io_send("\003");
			ui_print("--> <ETX>\n");
		}
	} else if(strcmp(argv[0], "wait") == 0) {
		for(i = 1; i < argc; i++)
			wait_queue_add_tail(argv[i]);

		for(;!wait_queue_empty(); ui_animation(2));
	} else if(strcmp(argv[0], "wait_timeout") == 0) {
		struct timeval a, b, d;
		int t;
		al = get_alias("timeout", 0);
		t = al? al->num: TIMEOUT;
		for(i = 1; i < argc; i++)
			wait_queue_add_tail(argv[i]);

		gettimeofday(&a, NULL);
		for(;!wait_queue_empty(); ui_animation(2)) {
			gettimeofday(&b, NULL);
			timersub(&b, &a, &d);
			if(d.tv_sec > t) {
				ui_print("timeout for waiting %s\n", wait_queue_head());
				al->arg++;
				return -1;
			}
		}
	} else if(strcmp(argv[0], "handshake") == 0) {
		char word[SZ];
		ui_print("--> %s\n", argv[1]);
		io_send(argv[1]);
		while(io_get_word(word) <= 0);
		ui_print("<-- %s\n", word);
		if(strcmp(word, argv[2]) != 0)
			return -1;
	}

	return 0;
}

void * receiver(void *args)
{
	char word[SZ];
	int i;
	struct line *on;
	struct alias *a;
	while(1) {
		if(io_get_word(word) <= 0) {
			usleep(300000);
			continue;
		}

		// queue management
		ui_print("<-- %s\n", word);

		// trigger on event
		exec_on(word);

		if(strcmp(wait_queue_head(), "bin") == 0) {
			bin = atoi(word);
			for(i = 0;; ) {
				a = get_alias("bin", i++);
				if(!a) break;

				if(a->num == bin)
					a->arg++;
			}
			wait_queue_pop();
		} else if(strcmp(word, wait_queue_head()) == 0
			|| strcmp(wait_queue_head(), "X") == 0) {
			wait_queue_pop();
		}
	}

	return NULL;
}

void * keyserver(void *args)
{
	static int ch, i = 0, c = 'a';
	while(ch = ui_input()) {
		switch(ch) {
			case 'q':
			case 'Q':
			ui_print("preparing to quit ...\n");
			stop = 1;
			break;
			case 0x60:
			ui_scroll(-5);
			break;
			case 0x61:
			ui_scroll(5);
			break;
			case ' ':
			ui_pausemsg(0);
			break;
			case 1: // Ctrl-A
			ui_pausemsg(1);
			break;
			case 3: // Ctrl-C
			ui_deinit();
			io_deinit();
			exit(1);
			break;
			default:
//			ui_print("key code: %d", ch);
;
		}
	}
}

int main()
{
	char buf[64], *v[SZ];
	struct alias *a;
	int n, i, r, skip_actions;
	FILE *f;
	pthread_t rv, ks;

	ui_init();
	ui_animation(0);

	f = fopen("commands.desc", "r");
	if(f == NULL) {
		ui_print("failed to open commands.desc\n");
		goto end;
	}

	// parse command list
	while(fgets(buf, 64, f)) {
		for(i = 0, v[i] = strtok(buf, " \n");
			 v[i]; v[++i] = strtok(0, " \n"));
		parse_line(i, v);
	}
	fclose(f);

	// init bin
	for(i = 0;; ) {
		a = get_alias("bin", i++);
		if(!a) break;
		ui_bin_add_tag(a->str);
	}
	ui_bin_add_tag("timeout");

	// init options tip
	ui_tip_update(1, "Press the corresponding key to perform that function:");
	ui_tip_update(3, "<S> Handler Speed");
	ui_tip_update(4, "<R> Reset");
	ui_tip_update(5, "<Q> Quit");

	// init io
	r = io_init();
	ptsname_r(r, buf, 64);
	ui_print("pts name is: %s (%d)\n", buf, r);

	// init wait queue
	wait_queue_init();

	// init receiver
	pthread_create(&rv, NULL, receiver, NULL);

	// init keyserver
	pthread_create(&ks, NULL, keyserver, NULL);

again:
	ui_print("\n\nnew routine:\n");
	skip_actions = 0;
	for(wait_queue_init(), i = 0;
	 i < lscount; i++) {
		if(skip_actions && (!strcmp(lines[i].argv[0], "send")
					  || !strcmp(lines[i].argv[0], "wait")))
			continue;
		if(exec_line(lines[i].argc, lines[i].argv) < 0)
			skip_actions = 1;
	}
	wait_queue_deinit();
	if(!stop) goto again;

end:
	ui_deinit();
	io_deinit();
}

