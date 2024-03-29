/*                         *
 *          ntype          *
 *                         */


#define VERSION "0.12"


#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>


#define uint unsigned int
#define ulong unsigned long
typedef unsigned int psize;


typedef enum ansi_color
{
	blank   = 0,
	red     = 31,
	green   = 32,
	yellow  = 33,
	blue    = 34,
	magenta = 35,
	cyan    = 36,
	white   = 37
}
color;

typedef enum contl
{
	unstarted,
	insert,
	backspace,
	eow
}
control;

typedef struct runtime_info
{
	uint    cp;
	uint    current_word;
	uint    mistakes_total;
	ulong   start_time;
	WINDOW  *tpwin;
	WINDOW  *rtwin;
	control curcon;
}
rt_info;

typedef struct buffers
{
	char    pre[100][100];
	char    post[100];
	uint    mistake_pos[100];
}
buffs;

typedef enum gamemd
{
	gm_undefined,
	words_count,
	time_count,
	endless
}
gamemode;

typedef struct settings
{
	gamemode  gm;
	ulong   timer;
	uint    words;
	bool      stall;
	bool      stay;
}
set;

typedef struct ccolor
{
	short r;
	short g;
	short b;
}
ccol;

ccol colis[16];

char         dic   [1000][100];
size_t       dic_s = 0;
char         kmp   [30];
ccol         thm   [7];
unsigned int gamecounter = 0;


static void set_color(const color c)
{
	printf("\033[%dm", c);
}

static void msg_out(const size_t num, ...)
{
	va_list messages;
	va_start(messages, num);
	
	size_t x = 0;
	while (x++ < num)
	{
		printf("%s%s", va_arg(messages, const char *), x != num ? ": " : "\n");
	}
	
	va_end(messages);
}

static void faulter(const char *label, const color col, const char *msg, const char *typ, const psize loc)
{
	set_color(col);
	
	if (!loc)
	{
		msg_out(3, label, typ, msg);
	}
	else
	{
		char locs[16];
		sprintf(locs, "%d", loc);
		msg_out(5, label, typ, msg, "at line", locs);
	}
	
	set_color(blank);
}

static noreturn void error(const char *typ, const char *msg, const int code, const psize loc)
{
	faulter("error", red, msg, typ, loc);
	exit(code);
}

static void warn(const char *typ, const char *msg, const psize loc)
{
	faulter("warning", magenta, msg, typ, loc);
}

static void notice(const char *typ, const char *msg, const psize loc)
{
	faulter("notice", white, msg, typ, loc);
}

static void strip_newline(char *str)
{
	if (str[strlen(str) - 1] == '\n') str[strlen(str) - 1] = '\0';
}

static void rectangle(int y1, int x1, int y2, int x2)
{
	mvhline(y1, x1, 0, x2-x1);
	mvhline(y2, x1, 0, x2-x1);
	mvvline(y1, x1, 0, y2-y1);
	mvvline(y1, x2, 0, y2-y1);
	mvaddch(y1, x1, ACS_ULCORNER);
	mvaddch(y2, x1, ACS_LLCORNER);
	mvaddch(y1, x2, ACS_URCORNER);
	mvaddch(y2, x2, ACS_LRCORNER);
}

static void load_dict(const char *path)
{
	dic_s = 0;
	FILE *fp = fopen(path, "r");
	if (!fp) return;
	while(fgets(dic[dic_s], 100, fp)) strip_newline(dic[dic_s++]);
	fclose(fp);
}

static void load_kmp(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp) return;
	fgets(kmp, 30, fp);
	fclose(fp);
}

static void load_thm(const char *path)
{
	FILE *fp = fopen(path, "r");
	if (!fp) return;
	size_t x = 0;
	while (x < 7)
	{
		char col[10];
		if (!fgets(col, 10, fp)) break;
		unsigned int r, g, b;
		sscanf(col, "#%2x%2x%2x", &r, &g, &b );
		thm[x].r = r * ( 1000 / 256 );
		thm[x].g = g * ( 1000 / 256 );
		thm[x].b = b * ( 1000 / 256 );
		++x;
	}
	fclose(fp);
}

static void shift_buffers(buffs *bfs)
{
	uint ctl = 0;
	
	memmove(bfs->pre, bfs->pre + 1, sizeof(bfs->pre[0]) * 49);
	bfs->pre[49][0] =  '\0';
	
	bfs->post[ctl] = '\0';
}

static void generate_giberish(rt_info *rti, buffs *bfs, set *s, uint len)
{
	int ran;
	
	ran = (rand() % dic_s);
	strcpy(bfs->pre[len], dic[ran]);
}

static WINDOW *create_newwin(int h, int w, int sy, int sx)
{
	WINDOW *local_win;
	
	local_win = newwin(h, w, sy, sx);
	box(local_win, 0, 0);
	wbkgd(local_win, COLOR_PAIR(5));
	return local_win;
}

static void srect(int h, int w, int sy, int sx)
{
	rectangle(sy, sx, sy + h, sx + w);
}

static void update_win(int h, int w, int sy, int sx, bool pressed, bool hi)
{
	attron(COLOR_PAIR(!pressed ? 5 : hi ? 2 : 1));
	srect(h, w, sy, sx);
	attroff(COLOR_PAIR(!pressed ? 5 : hi ? 2 : 1));
}

static void init_keyboard(buffs *bfs, int len, int hei)
{
	int  hr = 4, ofs = 0, idp = 0;
	
	uint ctl = 0;
	do
	{
		srect(hei / 17, hei / 9, hei - (hei / 15) * hr,((hei / 8) * idp) + ((hei / 15) * ofs) + ((len / 2) - (hei / 15) * 10));
		++idp;
		if (ctl == 9 || ctl == 19)
		{
			--hr;
			++ofs;
			idp = 0;
		}
	}
	while (++ctl != 29);
	
	srect(hei / 17, (hei / 9) * 5, hei - (hei / 15), (len / 2) - ((hei / 15) * 4));
}


static void update_keyboard(buffs *bfs, int len, int hei, bool hi, char in, char last)
{
	int  hr = 4, ofs = 0, idp = 0;
	
	uint ctl = 0;
	do
	{
		if (kmp[ctl] == in || kmp[ctl] == last)
		{
			update_win(hei / 17, hei / 9, hei - (hei / 15) * hr,((hei / 8) * idp) + ((hei / 15) * ofs) + ((len / 2) - (hei / 15) * 10),(kmp[ctl] == in), hi);
		}
		++idp;
		if (ctl == 9 || ctl == 19)
		{
			--hr;
			++ofs;
			idp = 0;
		}
	}
	while (++ctl != 29);
	
	if (' ' == in || last == ' ')
	{
		update_win(hei / 17, (hei / 9) * 5, hei - (hei / 15), (len / 2) - ((hei / 15) * 4),(' ' == in), hi);
	}
}

static void curses_init()
{
	initscr();
	noecho();
	start_color();
	
	color_content(1, &colis[0].r, &colis[0].g, &colis[0].b);
	init_color(1, thm[0].r, thm[0].g, thm[0].b);
	color_content(2, &colis[1].r, &colis[1].g, &colis[1].b);
	init_color(2, thm[2].r, thm[2].g, thm[2].b);
	color_content(3, &colis[2].r, &colis[2].g, &colis[2].b);
	init_color(3, thm[3].r, thm[3].g, thm[3].b);
	color_content(4, &colis[3].r, &colis[3].g, &colis[3].b);
	init_color(4, thm[4].r, thm[4].g, thm[4].b);
	color_content(5, &colis[4].r, &colis[4].g, &colis[4].b);
	init_color(5, thm[5].r, thm[5].g, thm[5].b);
	color_content(6, &colis[5].r, &colis[5].g, &colis[5].b);
	init_color(6, thm[6].r, thm[6].g, thm[6].b);
	color_content(7, &colis[6].r, &colis[6].g, &colis[6].b);
	init_color(7, thm[1].r, thm[1].g, thm[1].b);
	init_pair(1, 6, 3);
	init_pair(2, 6, 4);
	init_pair(3, 1, 6);
	init_pair(4, 6, 7);
	init_pair(5, 5, 6);
}

static void deinit_curses()
{
	init_color(1, colis[0].r, colis[0].g, colis[0].b);
	init_color(2, colis[1].r, colis[1].g, colis[1].b);
	init_color(3, colis[2].r, colis[2].g, colis[2].b);
	init_color(4, colis[3].r, colis[3].g, colis[3].b);
	init_color(5, colis[4].r, colis[4].g, colis[4].b);
	init_color(6, colis[5].r, colis[5].g, colis[5].b);
	init_color(7, colis[6].r, colis[6].g, colis[6].b);
	//reset_color_pairs()
	endwin();
}

static void noreturn quit(rt_info *rti, bool print, char *custom_log)
{
	ulong time_elps;
	
	printf("\033[1 q");
	endwin();
	if (print)
	{
		if (!custom_log)
		{
			time_elps = (unsigned long) time(NULL) - rti->start_time;
			printf("%d Words in %lu Seconds! that is %.2f WPM, you made %d Mistakes\n", rti->current_word, time_elps, (double) 60 * ( (double) (rti->cp / 5) / (double) time_elps), rti->mistakes_total);
		}
		else
		{
			printf("%s", custom_log);
		}
	}
	exit(0);
}

static void print_pre(set *s, rt_info *rti, buffs *bfs, uint max, uint wln, uint wff_off)
{
	uint wcc_off = wff_off, y = 0, yw = 0;
	
	uint ctl = 0;
	wattron(rti->tpwin, COLOR_PAIR(3));
	do
	{
		if ((wcc_off + strlen(bfs->pre[ctl])) > max)
		{
			++y;
			yw = y-wln;
			wcc_off = 0;
		}
		if (y >= wln) mvwaddstr(rti->tpwin,yw + 2, wcc_off + 3,bfs->pre[ctl]);
		wcc_off += (strlen(bfs->pre[ctl]) + 1);
	}
	while (++ctl < s->words);
	wattroff(rti->tpwin, COLOR_PAIR(3));
}

static void print_post(rt_info *rti, buffs *bfs, uint wff_off, uint mistakes)
{
	uint ctl = 0, wcc_off = wff_off;
	mvwaddstr(rti->tpwin, 2, wcc_off + 3, bfs->post);
	while (ctl < mistakes)
	{
		wattron(rti->tpwin, COLOR_PAIR(2));
		mvwaddch(rti->tpwin, 2, 3 + wcc_off + bfs->mistake_pos[ctl], bfs->pre[0][bfs->mistake_pos[ctl]]);
		wattroff(rti->tpwin, COLOR_PAIR(2));
		ctl++;
	}
	wmove(rti->tpwin, 2, wcc_off + 3 + strlen(bfs->post));
}

static signed char setch(char *c)
{
	*c = (char) getch();
	return *c;
}

static void input_loop(rt_info *rti, buffs *bfs, set *s, char *c, char l, size_t wcc_off)
{
	int ti = 0;
	while (setch(c) < 1)
	{
		if (ti++ > 100)
		{
			update_keyboard(bfs, COLS, LINES, 0, 0, l);
			if (rti->curcon == eow) move(2, wcc_off + 2 + strlen(bfs->post));
			else                    move(2, wcc_off + 3 + strlen(bfs->post));
			wrefresh(rti->tpwin);
		}
		if (s->gm == time_count && rti->start_time != 0 && (ulong) time(NULL) > (rti->start_time + s->timer))
		{
			quit(rti, true, NULL);
		}
	}
}

static void init_info(rt_info *rti, int len, int hei)
{
	rti->rtwin = create_newwin(LINES - (LINES / 14) * 4, COLS, 0, 0);
}

static void update_info(rt_info *rti)
{
	return;
}

static int result(rt_info *rti)
{
	clear();
	refresh();
	WINDOW * w = newwin(10, 20, LINES / 2 - 5, COLS / 2 - 10);
	box( w, 0, 0 );
	wbkgd(w, COLOR_PAIR(5));
	unsigned long time_elps = (unsigned long) time(NULL) - rti->start_time;
	double wpm = (double) 60 * ( (double) (rti->cp / 5) / (double) time_elps);
	mvwprintw(w, 1, 7 , "Result");
	mvwprintw(w, 2, 1, "wpm:  %.2f", wpm);
	mvwprintw(w, 3, 1, "time: %ld", time_elps);
	int ch;
	while ( (ch = wgetch(w)) != '\n');
	return 1;
}

static int loop(set *s)
{
	cbreak();
	timeout(1);
	printf("\033[6 q");
	buffs *bfs,bfs_o =
	{
		.post = ""
	};
	
	bool hi=false;
	uint mistakes = 0, current_word_position = 0, wcc_off;
	uint wff_off = 0, ctl = 0;
	char c = '\0', last = '\0';
	
	rt_info *rti, rti_o =
	{
		.current_word = 0,
		.mistakes_total = 0,
		.start_time = 0,
		.curcon = unstarted,
		.cp = 0
	};
	
	bfs = &bfs_o;
	rti = &rti_o;
	
	srand((uint) time(NULL) + gamecounter++);
	curses_init();
	ctl = 0;
	do
	{
		generate_giberish(rti, bfs, s, ctl);
	}
	while (++ctl < s->words);
	
	init_keyboard(bfs, COLS, LINES);
	
	rti->tpwin = create_newwin(LINES - (LINES / 14) * 4, COLS, 0, 0);
	wbkgd(rti->tpwin, COLOR_PAIR(5));
	
	while (1)
	{
		refresh();
		
		werase(rti->tpwin);
		box(rti->tpwin, 0, 0);
		if(!s->stay) wcc_off = wff_off;
		else         wcc_off = 0;
		if (rti->curcon == unstarted) init_keyboard(bfs, COLS, LINES);
		update_keyboard(bfs, COLS, LINES, hi, c, last);
		print_pre(s, rti, bfs, (uint) COLS - 6, 0, wcc_off);
		print_post(rti, bfs, wcc_off, mistakes);
		if (rti->curcon == eow) move(2, wcc_off + 2 + strlen(bfs->post));
		else                    move(2, wcc_off + 3 + strlen(bfs->post));
		wrefresh(rti->tpwin);
		refresh();
		
		last = c;
		hi = false;
		if (s->gm == words_count && rti->current_word == s->words) return result(rti);
		input_loop(rti, bfs, s, &c, last, wcc_off);
		wcc_off += (strlen(bfs->pre[0]) + 1);
		if (rti->curcon == unstarted) rti->start_time = (ulong) time(NULL);
		if (rti->curcon != eow)       rti->curcon = insert;
		switch (c)
		{
			case 10:	/* Linebreak */
			{
				c = ' ';
				break;
			};
			case '\t':	/* Tabulator */
			{
				return 1;
			};
			case 27:	/* Escape */
			{
				return result(rti);
				//quit(rti, true, NULL)
			};
			case 127: /* Backspace */
			{
				if (rti->curcon != eow)
				{
					if (current_word_position > 0) --current_word_position;
					bfs->post[current_word_position] = '\0';
					c = '\0';
					rti->curcon = backspace;
				}
				break;
			};
			default:
				if (rti->curcon != eow && current_word_position < strlen(bfs->pre[0]))
				{
					bfs->post[current_word_position] = c;
					bfs->post[current_word_position + 1] = '\0';
				}
				else hi = true;
		}
		if (rti->curcon == backspace)
		{
			if (mistakes > 0 && bfs->mistake_pos[mistakes-1] == current_word_position) --mistakes;
		}
		else if (rti->curcon == insert)
		{
			if (current_word_position == strlen(bfs->pre[0]) - 1)
			{
				if (!strcmp(bfs->pre[0], bfs->post) || !s->stall)
				{
					current_word_position = 0;
					++rti->current_word;
					rti->curcon = eow;
					mistakes = 0;
					if (!s->stay)
					{
						wff_off += strlen(bfs->pre[0])+1;
						rti->cp += strlen(bfs->pre[0])+1;
						if(1 <= ((wff_off + strlen(bfs->pre[1]))) / ((uint) COLS - 6)) wff_off = 0;
					}
					shift_buffers(bfs);
					if (s->gm == time_count || s->gm == endless) generate_giberish(rti, bfs, s, s->words-1);
				}
				else hi = true;
			}
			if (rti->curcon != eow && current_word_position < strlen(bfs->pre[0]))
			{
				if (c != bfs->pre[0][current_word_position])
				{
					hi = true;
					bfs->mistake_pos[mistakes] = current_word_position;
					++mistakes;
					++rti->mistakes_total;
				}
				else
				{
					if (mistakes > 0 && bfs->mistake_pos[mistakes-1] == current_word_position) --mistakes;
					hi = false;
				}
				++current_word_position;
			}
		}
		else if (rti->curcon == eow && c == ' ')
		{
			hi = false;
			rti->curcon = insert;
		}
	}
}

static int menu()
{
	set s_o =
	{
		.gm = gm_undefined,
		.stall = true,
		.stay = false
	};
	clear();
	wbkgd(stdscr, COLOR_PAIR(5));
	refresh();
	load_dict("dicts/en.dic");
	WINDOW *w;
	char list[2][16] =
	{
		"Local",
		"Network"
	};
	char item[32];
	int ch = '\0';
	int i = 0;
	w = newwin(10, 20, LINES / 2 - 5, COLS / 2 - 10);
	box( w, 0, 0 );
	wbkgd(w, COLOR_PAIR(5));
	for( i=0; i<2; i++ )
	{
		if( i == 0 )
		{
			wattron(w,COLOR_PAIR(5));
			wattron( w, A_STANDOUT );
		}
		else
		{
			wattroff(w,COLOR_PAIR(5));
			wattroff( w, A_STANDOUT );
		}
		sprintf(item, "%-7s",  list[i]);
		mvwprintw( w, i+5, 7, "%s", item );
	}
	wrefresh( w );
	i = 0;
	noecho();
	keypad( w, TRUE );
	curs_set( 0 );
	while ( (ch = wgetch(w)) != 'q' && ch != '\033')
	{
		sprintf(item, "%-7s",  list[i]);
		wattron(w,COLOR_PAIR(5));
		mvwprintw( w, i+5, 7, "%s", item ) ;
		wattroff(w,COLOR_PAIR(5));
		switch( ch )
		{
			case KEY_UP:
				i--;
				i = ( i<0 ) ? 1 : i;
				break;
			case KEY_DOWN:
				i++;
				i = ( i>1 ) ? 0 : i;
				break;
			case '\n':
				s_o.gm = words_count;
				s_o.words = 25;
				return loop(&s_o);
				break;
		}
		
		wattron(w, COLOR_PAIR(5));
		wattron( w, A_STANDOUT );
		sprintf(item, "%-7s",  list[i]);
		mvwprintw( w, i+5, 7, "%s", item);
		wattroff( w, A_STANDOUT );
		wattroff(w, COLOR_PAIR(5));
	}
	delwin(w);
	deinit_curses();
	return 0;
}

typedef enum arg_mode
{
	noarg,
	nothing,
	theme
}
amode;

static noreturn void help(void)
{
	puts("Usage: tc [OPTION]\n\n" \
	     " -h, --help    -> prints this\n" \
	     " -v, --version -> prints version\n" \
	     " -t, --theme   -> set theme file\n");
	exit(0);
}

static noreturn void version(void)
{
	fputs("ntype version "VERSION"\n", stdout);
	
	exit(0);
}

static amode long_arg_parser(const char *arg)
{
	if (!strcmp("help", arg)) help();
	
	if (!strcmp("version", arg)) version();
	
	if (strcmp(arg, "--theme") == 0 || strcmp(arg, "-H") == 0)
	{
		return theme;
	}
	
	warn("invalid option", arg, 0);
	
	return nothing;
}

static amode short_arg_parser(const char *arg)
{
	psize i = 0 ;
	char invalid[1];
	while(arg[i])
	{
		switch (arg[i])
		{
			
			case 'h': help();
			
			case 'V': version();
			
			case 't': return theme;
			
			default :
				invalid[0] = arg[i];
				
				warn("invalid option", invalid, 0);
				
				return nothing;
		}
		i++;
	}
	return nothing;
}

static amode arg_parser(char *arg, const amode last)
{
	if (arg[0] != '-' || strlen(arg) < 2)
	{
		switch (last)
		{
			case theme:
				load_thm(arg);
				return nothing;
			case nothing:
			case noarg:
				return noarg;
		}
	}
	
	if (arg[1] == '-') return long_arg_parser(arg + 2);
	
	return short_arg_parser(arg + 1);
}

int main(const int argc, char **argv)
{
	amode argument = nothing;
	
	while (*++argv)
	{
		argument = arg_parser(*argv, argument);
	}
	
	if (argument != nothing && argument != noarg) error("uneven options", *--argv, 1, 0);
	
	load_thm("themes/monokai.theme");
	load_kmp("keymaps/dvorak.kmp");
	curses_init();
	while (menu());
	return 0;
}
