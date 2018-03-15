/*
 * Copyright 2014 Wind River Systems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MORSE_ENTRY {
	char  letter;
	char *code;
} morse_table[] = {
	{ 'a', "*-"   },
	{ 'b', "-***" },
	{ 'c', "-*-*" },
	{ 'd', "-**"  },
	{ 'e', "*"    },
	{ 'f', "**-*" },
	{ 'g', "--*"  },
	{ 'h', "****" },
	{ 'i', "**"   },
	{ 'j', "*---" },
	{ 'k', "-*-"  },
	{ 'l', "*-**" },
	{ 'm', "--"   },
	{ 'n', "-*"   },
	{ 'o', "---"  },
	{ 'p', "*-*-" },
	{ 'q', "--*-" },
	{ 'r', "*-*"  },
	{ 's', "***"  },
	{ 't', "-"    },
	{ 'u', "**-"  },
	{ 'v', "***-" },
	{ 'w', "*--"  },
	{ 'x', "-**-" },
	{ 'y', "-*--" },
	{ 'z', "--**" },
	{ '1', "*----"},
	{ '2', "**---"},
	{ '3', "***--"},
	{ '4', "****-"},
	{ '5', "*****"},
	{ '6', "-****"},
	{ '7', "--***"},
	{ '8', "---**"},
	{ '9', "----*"},
	{ '0', "-----"},
	{ ' ', " "    }, /* letter space */
	{ ',', "   "  }, /* word   space */
	{ '\0', NULL  }
};

char *char2morse(char c) {
	int i;

	for (i=0; NULL != morse_table[i].code ;i++) {
		if (c == morse_table[i].letter) {
			return morse_table[i].code;
		}
	}

	/* not found, so return 'q' */
	return "--*-";
}

char morse2char(char* m) {
	int i;

	for (i=0; NULL != morse_table[i].code ;i++) {
		if (0 == strcmp(m,morse_table[i].code)) {
			return morse_table[i].letter;
		}
	}

	/* not found, so return '?' */
	return '?';
}

/**************************************************************/

#define dit_min_cnt         3 /* debounce >  3/20 sec */
#define dit_max_cnt        15 /* up to    < 15/20 sec (anything longer that dit is dah) */
#define space_min_cnt       3 /* debounce */
#define char_space_min_cnt 15 /* enough time for a char space = 15/20 sec */
#define word_space_min_cnt 40 /* enough time for a word space = 40/20 sec */

#define IOBUF_MAX   1024
#define TEXTBUF_MAX   20

char inMorseBuf[ IOBUF_MAX+1];
char outMorseBuf[IOBUF_MAX+1];
char inTextBuf[  TEXTBUF_MAX+1];
char inTextStr[  TEXTBUF_MAX+1];
char outTextBuf[ TEXTBUF_MAX+1];
char outTextStr[ IOBUF_MAX+1];

static int  outMorsePtr=0;
static int  inMorsePtr =0;
static int  outTextPtr =0;
static int  inTextPtr  =0;

static char lastkey= ' ';
static int  highcnt = 0;
static int  lowcnt  = 0;

/* init the parsing structures. */
void parse_morse_init(void) {
	int i;

	strcpy(inMorseBuf ," ");
	strcpy(outMorseBuf,"");
	strcpy(outTextStr ,"");
	strcpy(inTextStr  ,"");

	for (i=0; i<(TEXTBUF_MAX+1); i++) {
		inTextBuf[ i] ='_';
		outTextBuf[i]='_';
	}
	inTextBuf[ TEXTBUF_MAX]='\0';
	outTextBuf[TEXTBUF_MAX]='\0';
}

static char parse_morse_in(void) {
	char c;

	c = morse2char(inMorseBuf);
	if (',' == c) c = ' ';

	/* printf("\nparse_morse_in|%s|%c|\n",inMorseBuf,c); */

	inMorsePtr =0;
	inMorseBuf[0] = '\0';

	/* add char */
	inTextBuf[inTextPtr] = c;
	/* add leader */
	inTextBuf[(inTextPtr+2) % TEXTBUF_MAX] = '_';
	/* next position */
	inTextPtr = (inTextPtr+1) % TEXTBUF_MAX;

	return c;
}

char scan_morse_in(void) {
	char key;
	char ret_key='\0';

	/* read the device key port */
	key = get_device_key();
	if (1 == key)
		highcnt++;
	else
		lowcnt++;

	/* parse the 1->0 transition */
	if ((1 == lastkey) && (0 == key)) {
		if (highcnt <= dit_max_cnt)
			strcat(inMorseBuf,"*");
		else
			strcat(inMorseBuf,"-");
		highcnt=0;
	}

	/* parse the 0->1 transition */
	if ((0 == lastkey) && (1 == key)) {
		lowcnt=0;
	}

	/* low enough for a char space? */
	if (lowcnt == char_space_min_cnt) {
		ret_key=parse_morse_in();
		strcpy(inMorseBuf,"");
	}

	/* low enough for a word space? */
	if (lowcnt == word_space_min_cnt) {
		strcpy(inMorseBuf,"   ");
		ret_key=parse_morse_in();
		strcpy(inMorseBuf,"");
	}

	/* save the current state for the next pass */
	lastkey = key;

	return ret_key;
}

void clear_morse_out(void) {
	strcpy(outMorseBuf,"");
	strcpy(outTextStr,"");
}

void scan_morse_out(void) {
	if ('\0' != outMorseBuf[0]) {

		/* printf("\n<%s>\n",outMorseBuf); */

		/* if output head is '|', left-shift outTextStr and outMorseBuf */
		if ('|' == outMorseBuf[0]) {
			strcpy(outTextStr,&outTextStr[1]);
			strcpy(outMorseBuf,&outMorseBuf[1]);
		}

		/* output head of outMorseBuf, then left-shift outMorseBuf */
		if ('*' == outMorseBuf[0])
			set_user_key(1);
		else
			set_user_key(0);
		strcpy(outMorseBuf,&outMorseBuf[1]);
	}
}

void parse_morse_out(char c) {
	int i;
	char outstr[10];

	/* force lower case */
	c = tolower(c);

	/* add char */
	outTextBuf[outTextPtr] = c;
	/* add leader */
	outTextBuf[(outTextPtr+2) % TEXTBUF_MAX] = '_';
	/* next position */
	outTextPtr = (outTextPtr+1) % TEXTBUF_MAX;

	strcpy(outstr,char2morse(c));
	strcat(outTextStr,outstr);
	strcat(outTextStr," ");

	for (i=0;i<strlen(outstr);i++) {
		if      ('*' == outstr[i])
			strcat(outMorseBuf,"*|");
		else if ('-' == outstr[i])
			strcat(outMorseBuf,"***|"); /* 'dah' = three 'dit' */
		else if (' ' == outstr[i])
			strcat(outMorseBuf,"   |"); /* word-space = three 'dit' */

		/* add bit space (one 'dit' length ) */
		strcat(outMorseBuf," ");
	}
	/* add char-space (3 'dit' length) */
	strcat(outMorseBuf,"  |");
}


