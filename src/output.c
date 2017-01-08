/* 
 * Playground+ - output.c 
 * Enhanced output processor by Mo McKinlay
 * ----------------------------------------------------------------------------
 *
 * Replaces the two versions of process_output supplied in PG+
 * Tweeks to basecode to show social tagging by Silver
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "include/config.h"
#include "include/player.h"
#include "include/fix.h"
#include "include/proto.h"
#include "include/output.h"

/* tags[]
 *
 * This array lists the tag characters which should be used.
 * If 0 is specified, a tag won't be output.
 * Place the tags in the order that they should appear on output.
 * (For example, TT_ECHO preceeding TT_ROOM).
 */

tagentry_t tags[] = {
  { TT_LOGIN,   ']' }, /* Logins */
  { TT_LOGOUT,  '[' }, /* Logouts */
  { TT_RECON,   '%' }, /* Reconnections */
  { TT_ECHO,    '+' }, /* Echos */
  { TT_ITEM,    '^' }, /* Items */
  { TT_ROOM,    '-' }, /* Room messages */
  { TT_AUTO,    '#' }, /* Automessages */
  { TT_FRIEND,  '*' }, /* Old-style friend tells */
  { TT_OFRIEND, '=' }, /* Old-style friend tells to others */
  { TT_MULTI,   '&' }, /* Old-style multi-tells */
  { TT_NMULTI,  '*' }, /* New-style multi-tells */
  { TT_SOCIAL,  '~' }, /* Socials */
  { TT_PIPE,    '>' }, /* Pipes */
  { TT_TELL,    '>' }, /* Tells */
  { TT_SHOUT,   '!' }, /* Shouts */
  { TT_VOID,    0 },
};

/* output_checktags()
 *
 * Build a tagtype_t set based on the current global flags and player's
 * tag_flags settings.
 */

tagtype_t output_checktags(player *p) {
  tagtype_t t;
  
  if(p==current_player) return TT_VOID;
  t=TT_VOID;
  /* Tag logins/logouts/reconnects/echos */
  if((command_type&LOGIN_TAG) && (p->tag_flags & TAG_LOGINS)) {
    t|=TT_LOGIN;
  } else if((command_type&LOGOUT_TAG) && (p->tag_flags & TAG_LOGINS)) {
    t|=TT_LOGOUT;
  } else if((command_type&RECON_TAG) && (p->tag_flags & TAG_LOGINS)) {
    t|=TT_RECON;
  } else if((command_type&ECHO_COM) && (p->tag_flags & TAG_ECHO)) {
    t|=TT_ECHO;
  };
#ifdef ALLOW_MULTIS
  if (!(command_type & MULTI_COM)) {
#endif
    if(((command_type&PERSONAL) || (p->flags&TAGGED)) && 
       (p!=current_player)) {
      if((sys_flags&FRIEND_TAG) && (p->tag_flags&TAG_PERSONAL)) {
	t|=TT_FRIEND;
      } else if((sys_flags&OFRIEND_TAG) && (p->tag_flags&TAG_PERSONAL)) {
	t|=TT_OFRIEND;
      } else if((sys_flags&REPLY_TAG) && (p->tag_flags&TAG_PERSONAL)) {
	t|=TT_MULTI;
      } else if(command_type & PERSONAL) {
	t|=TT_TELL;
      } else {
	t|=TT_PIPE;
      };
    };
#ifdef ALLOW_MULTIS
  } else {
    t|=TT_NMULTI;
  };
#endif
  if(sys_flags&ITEM_TAG) {
    if(p->tag_flags & TAG_ITEMS) {
      t|=TT_ITEM;
    } else if(p->tag_flags & TAG_ROOM) {
      t|=TT_ROOM;
    };
  };
  if((command_type&SOCIAL) && (p->tag_flags & TAG_ROOM)) {
    t|=TT_SOCIAL;
  } else 
    if(((command_type&EVERYONE) || 
	     (sys_flags&EVERYONE_TAG)) &&
	    (p->tag_flags&TAG_SHOUT)) {
    t|=TT_SHOUT;
  } else if((command_type&AUTO) && (p->tag_flags & TAG_AUTOS)) {
    t|=TT_AUTO;
  } else if(((command_type&ROOM) || (sys_flags&ROOM_TAG)) && 
	    (p->tag_flags & TAG_ROOM) && 
	    (p!=current_player)) {
    t|=TT_ROOM;
  };
  if((command_type&ECHO_COM) && (p->tag_flags&SEEECHO)) {
    t|=TT_SEEECHO;
  };
  return t;
}

/* output_tags()
 *
 * Append appropriate tags to the stack.
 */

char *output_tags(player *p, tagtype_t t) {
  char *oldstack;
  int c;
  
  oldstack=stack;
  for(c=0; tags[c].type!=TT_VOID; c++) {
    if((t & tags[c].type) && (tags[c].tag)) {
      *stack++=tags[c].tag;
      p->column++;
    };
  };
  if(t & TT_SEEECHO) {
    *stack++='[';
    strcpy(stack, current_player->name);
    while(*stack) {
      stack++;
      p->column++;
    };
    *stack++=']';
    p->column+=2;
  };
  if(oldstack!=stack) {
    /* Put a space between the tag and the rest of the text */
    *stack++=' ';
    p->column++;
  };
  return stack;
}

file process_output(player *p, char *str) {
  file o;
  int x, l, xstart;
  char *wstart, *sstart, *t;
  int scat;
  o.where=stack;  
  output_tags(p, output_checktags(p));
  scat=sys_color_atm;
  x=p->column;
  /*
   * If HIGHLIGHT is set, turn it on here.
   */
  if((p->term) && (command_type&HIGHLIGHT)) {
    strcpy(stack, terms[(p->term - 1)].bold);  
    stack=strchr(stack, 0);
  };
  if((p->term) && (p->misc_flags & SYSTEM_COLOR)) {
    strcpy(stack, getcolor(p, p->colorset[sys_color_atm]));
    stack=strchr(stack, 0);
  };
  wstart=NULL;
  sstart=NULL;
  xstart=0;
  while(*str) {
    if(*str=='^' && (*(str+1)!='^')) {
      if(wstart==str) {
	wstart+=2;
	sstart+=2;
      };
      str++;
      if((p->term) && !(p->misc_flags & NOCOLOR)) {
	if(*str=='N' && 
	   !((command_type&HIGHLIGHT) && (sys_color_atm==SYSsc))) {
	  if(p->colorset[sys_color_atm]=='N') {
	    strcpy(stack, terms[(p->term - 1)].off);  
	  } else {
	    strcpy(stack, getcolor(p, p->colorset[sys_color_atm]));
	  };
	} else {
	  strcpy(stack, getcolor(p, *str));
	};
	stack=strchr(stack, 0); 
      };
      str++;
    } else if(*str=='\n') {
      if(!(*(str+1))) {
	/* Turn colour off before the CRLF */
	if((p->term) && 
	   (((sys_color_atm!=SYSsc) && (p->misc_flags&SYSTEM_COLOR)) ||
	   ((command_type & HIGHLIGHT) && !(p->misc_flags&NOCOLOR)))) {
	  strcpy(stack, getcolor(p, '^'));
	  stack=strchr(stack, 0);
	  sys_color_atm=SYSsc;
	};	
      };
      *stack++='\r';
      *stack++='\n';
      x=0;
      str++;
    } else {
      if(*str=='^') {
	str++;
      };
      if(!isalnum(*str)) {
	wstart=NULL;
	sstart=NULL;
	xstart=0;
      };
      if((x>=p->term_width) && (p->term_width!=0)) {
	/* This word will take us over the threshold */
	if(!wstart) {
	  /* No current word, just wrap now */
	  strcpy(stack, "\r\n   ");
	  stack=strchr(stack, 0);
	  x=4;
	  *stack=*str;
	  stack++;
	  str++;
	} else {
	  /* Find out the length of the word. If it's less
	   * than p->word_wrap, we cycle backwards, otherwise
	   * we just split the word and carry on
	   */
	  l=0;
	  for(t=wstart; t!=str; t++) l++;
	  if(l<p->word_wrap) {
	    /* Cycle backwards */
	    str=wstart;
	    stack=sstart;
	    strcpy(stack, "\r\n   ");
	    stack=strchr(stack, 0);
	    x=3;
	  } else {
	    /* Split the word up */
	    strcpy(stack, "\r\n   ");
	    stack=strchr(stack, 0);
	    if(isalnum(*str)) {
	      x=4;
	      wstart=str;
	      sstart=str;
	      xstart=x;
	      *stack=*str;
	      stack++;
	      str++;
	    } else {
	      x=3;
	      str++;
	      wstart=str;
	      sstart=str;
	      xstart=x;
	    };
	  };
	};
      } else {
	if(!isspace(*str) && !wstart) {
	  /* Start of a new word */
	  wstart=str;
	  sstart=stack;
	  xstart=x;
	};
	*stack=*str;
	x++;
	stack++;
	str++;
      };
    };
  };
  /* Turn colour off again */
  if((p->term) && (p->misc_flags&SYSTEM_COLOR)) {
    strcpy(stack, getcolor(p, '^'));
    stack=strchr(stack, 0);
  };
  if((p->term) && (command_type & HIGHLIGHT)) {
    strcpy(stack, terms[(p->term - 1)].off);  
    stack=strchr(stack, 0);
  };
  *stack=0;
  o.length = ((int) stack - (int) o.where) * sizeof(char);
  sys_color_atm=scat;
  p->column = x;
  return o;
}
