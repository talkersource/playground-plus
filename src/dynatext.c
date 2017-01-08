/*
 * Playground+ - dynatext.c
 * Code for converting dynatext (and slimepit style masks) v3.0
 * ---------------------------------------------------------------------------
 *
 * Version 3.0 written by ekto
 * Version 2.0-2.4 written by Silver, Blimey and phypor.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <time.h>

#include "include/fix.h"
#include "include/config.h"
#include "include/player.h"
#include "include/proto.h"

#include "include/masks.h"

char talker_name[80];
char talker_email[80];
player *get_random_room_player(void);
player *get_random_talker_player(void);
char *the_time(int, int);
char *my_rank(player *);
char *my_partner(player *);
int number_of(int);
player *this_rand;

#define M_SFX       0
#define M_THING     1
#define M_DESCRIBE  2
#define M_THINGS    3

#define M_MAX       4

static file mask_files[M_MAX];
static char **mask_list[M_MAX];
static int mask_count[M_MAX] = { -1, -1, -1, -1 };
static char *mask_xlist[M_MAX][64];
static int mask_next[M_MAX];
static int mask_max[M_MAX];

static int show_tag = 0;

/* For <TARGET> */
extern player *targ;

/* For <OBJECTNAME> */

char *item_name = NULL;

/* For <STR> */
extern char social_str[128];

/* masks_genlist()
 *
 * Generate a list from the entries in a file.
 */

char **masks_genlist(file f)
{
  size_t c;
  char *p, *q, **l;
  
  if(!f.length || !f.where) return NULL;
  /* Count the number of entries */
  c=0;
  p=f.where;
  while(*p)
    {
      while((*p) && isspace(*p)) p++;
      if(!*p) continue;
      c++;
      q=strchr(p, '\n');
      if(!q)
	{
	  q=strchr(p, '\0');
	}
      else
	{
	  q++;
	}
      p=q;
    }
  if(!c)
    {
      return NULL;
    }
  c++;
  l=(char **)malloc(c*sizeof(char *));
  memset(l, 0, c*sizeof(char *));
  p=f.where;
  c=0;
  while(*p)
    {
      while((*p) && isspace(*p)) p++;
      if(!*p) continue;
      l[c]=p;
      c++;
      q=strchr(p, '\n');
      if(!q)
	{
	  q=strchr(p, '\0');
	}
      else
	{
	  *q=0;
	  q++;
	}
      p=q;
    }
  return l;
}

/* masks_init()
 *
 * Load sfx/thing/describe lists from disk
 */

void masks_load(int id, char *fn)
{
  mask_files[id]=load_file(fn);
  mask_list[id]=masks_genlist(mask_files[id]);
}

void masks_init(void)
{
  masks_load(M_SFX, "files/sfx");
  masks_load(M_THING, "files/things");
  masks_load(M_DESCRIBE, "files/describe");
  masks_load(M_THINGS, "files/pluralthings");
  masks_reset();
}

void masks_free(int id)
{
  free(mask_files[id].where);
  mask_files[id].where = NULL;
  mask_files[id].length = 0;
  free(mask_list[id]);
  mask_list[id] = NULL;
}

void masks_done(void)
{
  masks_free(M_SFX);
  masks_free(M_THING);
  masks_free(M_DESCRIBE);
  masks_free(M_THINGS);
  masks_reset();
}
  

/* masks_reset()
 *
 * Re-randomise the random masks.
 */

void masks_recount(int id)
{
  int c;
  
  if(!mask_list[id])
    {
      mask_count[id]=0;
    }
  else if(mask_count[id]<0)
    {
      for(c=0; mask_list[id][c]; c++);
      mask_count[id]=c;
    }
  mask_next[id]=0;
  mask_max[id]=0;
}

void masks_reset(void)
{
  social_str[0]=0;
  item_name = NULL;
  masks_recount(M_SFX);
  masks_recount(M_THING);
  masks_recount(M_DESCRIBE);
  masks_recount(M_THINGS);
}

/* a_an_thing()
 *
 * Return the given entry from thing[], prefixed by 'a' or 'an' as
 * appropriate.
 */

static char *a_an_thing(char *q)
{
  char *p;
  static char buf[128];
  
  p=q;
  if(isupper(p[0]) && !islower(p[1]))
    {
      if(strchr("AEFHILMNORSX", p[0]))
	{
	  sprintf(buf, "an %s", q);
	  return buf;
	}
    }
  if(strchr("aeiou", tolower(p[0])))
    {
      sprintf(buf, "an %s", q);
    }
  else
    {
      sprintf(buf, "a %s", q);
    }
  return buf;
}
  
char *masks_player(player *p, player *t, char *mask)
{
  static char buf[64];

  if(!strcmp(mask, "name")) return t->name;
  if(!strcmp(mask, "fullname"))
    {
      if(!t->pretitle[0] || p->custom_flags & NOEPREFIX)
	{
	  return t->name;
	}
      sprintf(buf, "%s %s", t->pretitle, t->name);
      return buf;
    }
  if(!strcmp(mask, "time")) return time_diff(t->jetlag);
  if(!strcmp(mask, "stime"))
    {
      strcpy(buf, the_time(t->jetlag, 1));
      strcat(buf, ":");
      strcat(buf, the_time(t->jetlag, 2));
      strcat(buf, ":");
      strcat(buf, the_time(t->jetlag, 3));
      return buf;
    }
  if(!strcmp(mask, "hour")) return the_time(t->jetlag, 1);
  if(!strcmp(mask, "min")) return the_time(t->jetlag, 2);
  if(!strcmp(mask, "sec")) return the_time(t->jetlag, 3);
  if(!strcmp(mask, "day")) return the_time(t->jetlag, 4);
  if(!strcmp(mask, "month")) return the_time(t->jetlag, 5);
  if(!strcmp(mask, "date")) return the_time(t->jetlag, 6);
  if(!strcmp(mask, "year")) return the_time(t->jetlag, 7);
  if(!strcmp(mask, "rank")) return my_rank(t);
  if(!strcmp(mask, "cash")) 
    {
      sprintf(buf, "%d", t->pennies);
      return buf;
    }
  if(!strcmp(mask, "gender")) return gstring_possessive(t);
  if(!strcmp(mask, "gender2")) return gstring(t);
  if(!strcmp(mask, "gender3")) return get_gender_string(t);
  if(!strcmp(mask, "hisher")) return gstring_possessive(t);
  if(!strcmp(mask, "heshe")) return gstring(t);
  if(!strcmp(mask, "himher")) return get_gender_string(t);
  if(!strcmp(mask, "partner")) return my_partner(t);
  if(!strcmp(mask, "lag"))
    {
      sprintf(buf, "%ld.%02ld", t->last_ping / 1000000,
	      (t->last_ping / 10000) % 1000000);
      return buf;
    }
  if(!strcmp(mask, "lagstr")) return ping_string(t);
  return NULL;
}

char *masks_talker(player *p, char *mask)
{
  static char buf[64];
  
  if(!strcmp(mask, "addr"))
    {
      sprintf(buf, "%s %d", talker_alpha, active_port);
      return buf;
    }
  if(!strcmp(mask, "name")) return talker_name;
  if(!strcmp(mask, "email")) return talker_email;
  if(!strcmp(mask, "pot")) 
    {
      sprintf(buf, "%d", pot);
      return buf;
    }
  if(!strcmp(mask, "online")) 
    {
      sprintf(buf, "%d", current_players);
      return buf;
    }
  if(!strcmp(mask, "staff")) 
    {
      sprintf(buf, "%d", count_su());
      return buf;
    }
  if(!strcmp(mask, "count")) 
    {
      sprintf(buf, "%d", number_of(0));
      return buf;
    }
  if(!strcmp(mask, "countstaff")) 
    {
      sprintf(buf, "%d", number_of(1));
      return buf;
    }
  return NULL;
}

char *masks_entry(int id)
{
  int c, d;
  
  if(mask_count[id]<1)
    {
      return NULL;
    }
  if(mask_next[id]<mask_max[id])
    {
      c=mask_next[id];
      mask_next[id]++;
      return mask_xlist[id][c];
    }
  if(mask_max[id]<63)
    {
      c=mask_max[id];
      d=mask_count[id];
      mask_xlist[id][c]=mask_list[id][rand()%d];
      mask_max[id]++;
      mask_next[id]=mask_max[id];
      return mask_xlist[id][c];
    }
  mask_next[id]=0;
  c=mask_next[id];
  mask_next[id]++;
  return mask_xlist[id][c];
}

char *masks_a_entry(int id)
{
  char *p;
  
  p=masks_entry(id);
  if(p)
    {
      return a_an_thing(p);
    }
  return NULL;
}

char *masks_random(int id)
{
  int c;

  if(mask_count[id]<1)
    {
      return NULL;
    }
  c=mask_count[id];
  return mask_list[id][rand()%c];
}

char *masks_reverse(player *p, char *str)
{
  static char *s = NULL;
  char *t, *q;

  if(s)
    {
      free(s);
    }
  s = (char *)malloc (strlen(str) + 1);
  if (!s)
    {
      return NULL;
    }
  q = s;
  t = strchr(str, '\0');
  while (t >= str+1)
    {
      t--;
      *q = *t;
      q++;
    }
  *q = 0;
  return s;
}

char *masks_replace(player *p, char *mask)
{
  int c;
  char *x;

  c=0;
  show_tag = 0;
  while(static_masks[c])
    {
      if(!strcmp(mask, static_masks[c]))
	{
	  return static_masks[c+1];
	}
      c+=2;
    }
  if(!(command_type & SOCIAL))
    {
      show_tag = 1;
    }
  if(!strncmp(mask, "M:", 2))
    {
      mask+=2;
      return masks_player(p, p, mask);
    }
  if(!strncmp(mask, "R:", 2))
    {
      mask+=2;
      return masks_player(p, get_random_room_player(), mask);
    }
  if(!strncmp(mask, "A:", 2))
    {
      mask+=2;
      return masks_player(p, get_random_talker_player(), mask);
    }
  if(!strncmp(mask, "T:", 2))
    {
      mask+=2;
      return masks_talker(p, mask);
    }
  if(!strncmp(mask, "REV:", 4))
    {
      mask+=4;
      return masks_reverse(p, mask);
    }
  if(current_player)
    {
      if(!strncmp(mask, "C:", 2))
	{
	  mask+=2;
	  return masks_player(p, current_player, mask);
	}
      if(!strcmp(mask, "USERNAME"))
	return current_player->name;
      if(!strcmp(mask, "USERGENDER"))
	return gstring_possessive(current_player);
      if(!strcmp(mask, "USERGENDER1"))
	return gstring_possessive(current_player);
      if(!strcmp(mask, "USERGENDER2"))
	return gstring(current_player);
      if(!strcmp(mask, "USERGENDER3"))
	return get_gender_string(current_player);
      if(!strcmp(mask, "USERGENDER4"))
	return self_string(current_player);
      if(!strcmp(mask, "USERPREFIX"))
	return current_player->pretitle;
      if(current_player->location)
	{
	  if(!strcmp(mask, "ROOMNAME"))
	    return current_player->location->name;
	  if(!strcmp(mask, "ROOMID"))
	    return current_player->location->id;
	  if(!strcmp(mask, "ROOMOWNER"))
	    return current_player->location->owner->lower_name;
	}
#ifndef ESOCIALS
      if(command_type & SOCIAL)
	{
	  if(!strcmp(mask, "STR"))
	    {
	      return social_str;
	    }
	}
      if(targ && (command_type & SOCIAL))
	{
	  if(targ!=current_player)
	    {
	      if(!strncmp(mask, "P:", 2))
		{
		  mask+=2;
		  return masks_player(p, targ, mask);
		}
	      if(!strcmp(mask, "TARGET"))
		return targ->name;
	      if(!strcmp(mask, "TPREFIX"))
		return targ->pretitle;
	      if(!strcmp(mask, "TGENDER"))
		return gstring_possessive(targ);
	      if(!strcmp(mask, "TGENDER1"))
		return gstring_possessive(targ);
	      if(!strcmp(mask, "TGENDER2"))
		return gstring(targ);
	      if(!strcmp(mask, "TGENDER3"))
		return get_gender_string(targ);
	    }
	}
#endif
    }
  if(item_name && !strcmp(mask, "OBJECTNAME"))
    return item_name;
  if(!strcmp(mask, "SFX"))
    {
      return masks_entry(M_SFX);
    }
  if(!strcmp(mask, "XSFX"))
    {
      return masks_random(M_SFX);
    }
  if(!strcmp(mask, "THING"))
    {
      return masks_entry(M_THING);
    }
  if(!strcmp(mask, "THINGS"))
    {
      return masks_entry(M_THINGS);
    }
  if(!strcmp(mask, "ATHING"))
    {
      return masks_a_entry(M_THING);
    }
  if(!strcmp(mask, "XTHING"))
    {
      return masks_random(M_THING);
    }
  if(!strcmp(mask, "DESCRIBE"))
    {
      return masks_entry(M_DESCRIBE);
    }
  if(!strcmp(mask, "ADESCRIBE"))
    {
      return masks_a_entry(M_DESCRIBE);
    }
  if(!strcmp(mask, "XDESCRIBE"))
    {
      return masks_random(M_DESCRIBE);
    }
  for(x=mask; *x; x++)
    {
      if(!isalpha(*x)) continue;
      if(!isupper(*x)) break;
    }
  if(*x) {
    return NULL;
  }
  lower_case(mask);
  return masks_player(p, p, mask);
}

char *masks_process(player *p, char *str)
{
  char *q, *t, *s, *oldstack;
  
  /* If the user muffles dynatext, just return 'str' */
  if(p->custom_flags & NO_DYNATEXT && !(command_type & SOCIAL))
    return str;
  
  /* Reset random masks */
  mask_next[M_SFX]=0;
  mask_next[M_THING]=0;
  mask_next[M_DESCRIBE]=0;
  mask_next[M_THINGS]=0;
  oldstack=stack;
  while(*str)
    {
      if(*str=='<')
	{
	  str++;
	  t=str;
	  while((*t) && *t != '>') t++;
	  if(*t)
	    {
	      q=(char *)malloc((t-str)+1);
	      strncpy(q, str, t-str);
	      q[t-str]=0;
	      t++;
	      s=masks_replace(p, q);
	      if(s)
		{
		  if((p->tag_flags & TAG_DYNATEXT) && (show_tag))
		    {
		      stack+=sprintf(stack, "{%s}", s);
		    }
		  else
		    {
		      stack+=sprintf(stack, "%s", s);
		    }
		  free(q);
		  str=t;
		  continue;
		}
	      free(q);
	    }
	  *stack='<';
	  stack++;
	}
      *stack=*str;
      stack++;
      str++;
    }
  *stack++=0;
  return oldstack;
}

/* Loads of dynatext options :o) */

/* Time stuff */

char *the_time(int diff, int type)
{
  time_t t;
  static char time_string[20];

  t = time(0) + (3600 * diff);
  switch (type)
  {
    case 1:
      strftime(time_string, 19, "%H", localtime(&t));
      break;
    case 2:
      strftime(time_string, 19, "%M", localtime(&t));
      break;
    case 3:
      strftime(time_string, 19, "%S", localtime(&t));
      break;
    case 4:
      strftime(time_string, 19, "%A", localtime(&t));
      break;
    case 5:
      strftime(time_string, 19, "%B", localtime(&t));
      break;
    case 6:
      strftime(time_string, 19, "%d", localtime(&t));
      break;
    case 7:
      strftime(time_string, 19, "%Y", localtime(&t));
      break;
  }
  return time_string;
}

/* Rank */

char *my_rank(player * p)
{
  /* This isn't a case simply becuase it seemed to ignore the ranks and
     print "newbie" regardless - bad programing I know -sigh- */
  /* tell the truth, unless you implemented an array to do it, 
     this is the best way your gonna get ... -phy */
  if (p->residency & CODER)
    return get_config_msg("coder_name");
  else if (p->residency & HCADMIN)
    return get_config_msg("hc_name");
  else if (p->residency & ADMIN)
    return get_config_msg("admin_name");
  else if (p->residency & LOWER_ADMIN)
    return get_config_msg("la_name");
  else if (p->residency & ASU)
    return get_config_msg("asu_name");
  else if (p->residency & SU)
    return get_config_msg("su_name");
  else if (p->residency & PSU)
    return get_config_msg("psu_name");
  else if (p->residency)
    return "resident";
  else
    return "newbie";
}

/* Returns the players 'other-half' */

char *my_partner(player * p)
{
  if (p->system_flags & (MARRIED | ENGAGED))
    return (p->married_to);
  else
    return "no-one";
}

player *get_random_room_player(void)
{
  player *scan;
  int c = 0;

  if (this_rand)
    return this_rand;

  if (!current_player)
    return (player *) NULL;

  if (!current_player->location)
    return current_player;

  for (scan = current_player->location->players_top; scan;
       scan = scan->room_next, c++);

  c = rand() % c;

  for (scan = current_player->location->players_top; scan && c;
       scan = scan->room_next, c--);

  this_rand = scan;
  return scan;
}

player *get_random_talker_player(void)
{
  player *scan;
  int i;

  if (this_rand)
    return this_rand;
  if (!current_player)
    return (player *) NULL;

  if (!current_players || !current_player->location)
    return current_player;
  i = (rand() % current_players);

  for (scan = flatlist_start; i; scan = scan->flat_next, i--);

  this_rand = scan;
  return scan;
}

int number_of(int only_su)
{
  saved_player *scan, **hash;
  char c;
  int i, count = 0;

  for (c = 'a'; c <= 'z'; c++)
  {
    hash = saved_hash[((int) (tolower(c)) - (int) 'a')];
    for (i = 0; i < HASH_SIZE; i++, hash++)
    {
      for (scan = *hash; scan; scan = scan->next)
      {
	if (scan->residency == BANISHD || scan->residency & ROBOT_PRIV ||
	    scan->residency & SYSTEM_ROOM)
	  continue;
	if (only_su == 1)
	{
	  if (scan->residency & (PSU | SU | ASU | LOWER_ADMIN | ADMIN | CODER | HCADMIN))
	    count++;
	}
	else
	  count++;
      }
    }
  }

  return count;
}

void dynatext_version()
{
  sprintf(stack, " -=*> Dynatext/Masks v3.0 (by ekto) enabled.\n");
  stack = strchr(stack, 0);
}

