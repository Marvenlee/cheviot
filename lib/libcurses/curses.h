#ifndef _CURSES_H
#define _CURSES_H

#include <_ansi.h>

/* Lots of junk here, not packaged right. */

/*
extern char termcap[];
extern char tc[];
extern char *ttytype;
extern char *arp;
extern char *cp;

extern char *cl;
extern char *cm;
extern char *so;
extern char *se;

extern char row, col, mode;
extern char str[];
*/

void addstr (char *_s);
void clear (void);
void clrtobot (void);
void clrtoeol (void);
void endwin (void);
void fatal (char *_s);
char inch (void);
void initscr (void);
void move (int _y, int _x);
/* WRONG, next is varargs. */

void printw (char *_fmt, char *_a1, char *_a2, char *_a3,
			  char *_a4, char *_a5);
void outc (int _c);
void refresh (void);
void standend (void);
void standout (void);
void touchwin (void);

#endif /* _CURSES_H */
