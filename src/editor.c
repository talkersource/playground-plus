/*
 * Playground+ - editor.c
 * All the editor commands
 * ---------------------------------------------------------------------------
 */

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "include/config.h"
#include "include/player.h"
#include "include/fix.h"
#include "include/proto.h"

/* EWE */
/* this limits the number of lines in an edited item */
#define EWE_VERSION "1.0"
#define MAX_EDITOR_LINES 100
#define MAX_INCLUDE_PERCENTAGE 80
int	selectlist[MAX_EDITOR_LINES+1];
extern void help(player *, char *);
file	edit_help_file;

/* interns */

void editor_main(player *, char *);
void pager_help_pause(player *, char *);
void help_on_pager(player *);

/* save and restore important flags that the editor changes */

void save_flags(player * p)
{
  if (!p->edit_info)
    return;
  p->edit_info->flag_copy = p->flags;
  p->edit_info->sflag_copy = p->system_flags;
  p->edit_info->tflag_copy = p->tag_flags;
  p->edit_info->cflag_copy = p->custom_flags;
  p->edit_info->mflag_copy = p->misc_flags;
  p->edit_info->input_copy = p->input_to_fn;
  p->flags &= ~PROMPT;
  if (p->custom_flags & QUIET_EDIT)
    p->tag_flags |= BLOCK_TELLS | BLOCK_SHOUT;

  /* pg+ specific flags? */
  p->mode |= MAILEDIT;
  p->flags |= IN_EDITOR;

  p->input_to_fn = editor_main;
}

void restore_flags(player * p)
{
  if (!p->edit_info)
    return;
  p->flags = p->edit_info->flag_copy;
  p->system_flags = p->edit_info->sflag_copy;
  p->tag_flags = p->edit_info->tflag_copy;
  p->custom_flags = p->edit_info->cflag_copy;
  p->misc_flags = p->edit_info->mflag_copy;
  p->input_to_fn = p->edit_info->input_copy;
}



/* editor functions */

/* finish editing without changes */

void edit_quit(player * p, char *str)
{
  (*p->edit_info->quit_func) (p);
  finish_edit(p);
}


/* print out available commands */

void edit_view_commands(player * p, char *str)
{
  view_sub_commands(p, editor_list);
}


/* the dreaded pager B-)  */

int draw_page(player * p, char *text)
{
  int end_line = 0, n;
  ed_info *e;
  char *oldstack;
  float pdone;
  oldstack = stack;

  for (n = TERM_LINES + 1; n; n--, end_line++)
  {
    while (*text && *text != '\n')
      *stack++ = *text++;
    if (!*text)
      break;
    *stack++ = *text++;
  }
  *stack++ = 0;
  tell_player(p, oldstack);
  if (*text && p->edit_info)
  {
    e = p->edit_info;
    end_line += e->size;
    pdone = ((float) end_line / (float) e->max_size) * 100;
#ifndef NEWPAGER
    sprintf(oldstack,
	    "[Pager: %d-%d (%d) [%.0f%%] [RETURN]/b/t/q]  ", e->size, end_line, e->max_size, pdone);
#else
    sprintf(oldstack,
	    "[Pager: %d-%d (%d) [%.0f%%] <command>/[RETURN]/b/t/q]  ", e->size, end_line, e->max_size, pdone);
#endif
    stack = end_string(oldstack);
    p->flags &= ~PROMPT;
    do_prompt(p, oldstack);
  }
  stack = oldstack;
  return *text;
}

void quit_pager(player * p, ed_info * e)
{
  p->input_to_fn = e->input_copy;
  p->flags = e->flag_copy;
  if (e->buffer)
    FREE(e->buffer);
  FREE(e);
  p->edit_info = 0;
  if (!(p->mode))
    p->flags |= PROMPT;
  else
     do_prompt(p, "Mode Restored >");
}

void back_page(player * p, ed_info * e)
{
  char *scan;
  int n;
  scan = e->current;
  for (n = TERM_LINES + 1; n; n--)
  {
    while (scan != e->buffer && *scan != '\n')
      scan--;
    if (scan == e->buffer)
      break;
    e->size--;
    scan--;
  }
  e->current = scan;
}

void forward_page(player * p, ed_info * e)
{
  char *scan;
  int n;
  scan = e->current;
  for (n = TERM_LINES + 1; n; n--)
  {
    while (*scan && *scan != '\n')
      scan++;
    if (!*scan)
      break;
    e->size++;
    scan++;
  }
  e->current = scan;
}

#ifndef NEWPAGER

/* The old PG pager */

void pager_fn(player * p, char *str)
{
  ed_info *e;
  e = p->edit_info;
  switch (tolower(*str))
  {
    case 'b':
      back_page(p, e);
      break;
    case 'p':
      back_page(p, e);
      break;
    case 0:
      forward_page(p, e);
      break;
    case 'f':
      forward_page(p, e);
      break;
    case 'n':
      forward_page(p, e);
      break;
    case 't':
      e->current = e->buffer;
      e->size = 0;
      draw_page(p, e->current);
      break;
    case 'q':
      quit_pager(p, e);
      return;
  }
  if (!draw_page(p, e->current))
    quit_pager(p, e);
}

#else

/* Nightwolf's (Jefferey Cutting) pager code */

void pager_fn(player * p, char *str)
{
  ed_info *e;
  e = p->edit_info;
  if (strcasecmp(str, "b") && strcasecmp(str, "p") && strcasecmp(str, "n")
      && strcasecmp(str, "t") && strcasecmp(str, "q") && strlen(str) > 0)
  {
    if (!match_social(p, str))
      match_commands(p, str);
    do_prompt(p, get_config_msg("pager_prompt"));
    p->input_to_fn = pager_fn;
    return;
  }
  else
  {
    switch (tolower(*str))
    {
      case 'b':
	back_page(p, e);
	break;
      case 'p':
	back_page(p, e);
	break;
      case 0:
	forward_page(p, e);
	break;
      case 'n':
	forward_page(p, e);
	break;
      case 't':
	e->current = e->buffer;
	e->size = 0;
	break;
      case 'q':
	quit_pager(p, e);
	return;
    }
  }
  if (!draw_page(p, e->current))
    quit_pager(p, e);
}

#endif

void pager_help_pause(player * p, char *str)
{
  ed_info *e;
  e = p->edit_info;
  p->input_to_fn = pager_fn;
  if (!draw_page(p, e->current))
    quit_pager(p, e);
}


void pager(player * p, char *text)
{
  ed_info *e;
  int length = 0, lines = 0;
  char *scan;

  if (p->custom_flags & NO_PAGER)
  {
    tell_player(p, text);
    return;
  }
  if (p->edit_info)
  {
    tell_player(p, " Eeek, can't enter pager right now.\n");
#ifdef NEW_PAGER
    tell_player(p, " (try quitting the pager using the 'q' command)\n");
#endif
    return;
  }
  for (scan = text; *scan; scan++, length++)
  {
    if (*scan == '\n')
    {
      lines++;
    }
    length++;
  }
  if (lines > (TERM_LINES + 1))
  {
    e = (ed_info *) MALLOC(sizeof(ed_info));
    memset(e, 0, sizeof(ed_info));
    p->edit_info = e;
    e->buffer = (char *) MALLOC(length);
    memcpy(e->buffer, text, length);
    e->current = e->buffer;
    e->max_size = lines;
    e->size = 0;
    e->input_copy = p->input_to_fn;
    e->flag_copy = p->flags;
    p->input_to_fn = pager_fn;
    p->flags &= ~PROMPT;
  }
  draw_page(p, text);
}

/*
void editor_search_string(player * p, char *str)
{

  ed_info *e;
  char *this_line, *this_word;
  int x = 0;

  if (!*str)
  {
    tell_player(p, " Format: .s <search string>\n");
    return;
  }
  e = p->edit_info;
  this_line = e->current;

  for (this_word = e->current; *this_word && !x; this_word++)
  {
    if (*this_word == '\n')
      this_line = this_word + 1;
    if (!((isspace(*(this_word - 1)) && !(isspace(*this_word)))));
    else if (!strnomatch(str, this_word, 1))
      x = 1;
  }

  if (!x)
  {
    tell_player(p, " String not found before end of buffer reached.\n");
    return;
  }
  else
  {
    p->edit_info->current = this_line;
    edit_view_line(p, 0);
  }
}

int countquote(char *str)
{
  int cnt = 0;

  while (*str)
  {
    if (*str == '\"')
      cnt++;
    str++;
  }
  return cnt;
}

void edit_search_and_replace(player * p, char *str)
{

  ed_info *e;
  char buff[10000], search[50], replac[50], *holder, *oldstack;
  int b = 0, cnt = 0, s = 0, r = 0;

  if (countquote(str) != 4)
  {
    tell_player(p, " Format: .sr \"search\" \"replace\"\n");
    return;
  }
  for (b = 0; b < 10000; b++)
    buff[b] = 0;
  b = 0;
  e = p->edit_info;
  for (s = 0; s < 50; s++)
  {
    search[s] = 0;
    replac[s] = 0;
  }
  s = 0;
  while (*str && *str != '\"')
    str++;
  str++;
  while (*str && *str != '\"')
  {
    if (s < 50)
      search[s++] = *str++;
    else
      str++;
  }
  str++;
  while (*str && *str != '\"')
    str++;
  str++;
  while (*str && *str != '\"')
  {
    if (r < 50)
      replac[r++] = *str++;
    else
      str++;
  }

  for (holder = e->buffer; (*holder && b < 10000); holder++)
  {

    if (strnomatch(search, holder, 1))
      buff[b++] = *holder;
    else
    {
      cnt++;
      r = 0;
      s = 0;
      while (replac[r] && b < 10000)
	buff[b++] = replac[r++];
      while (search[s] && *holder && search[s] == *holder)
      {
	holder++;
	s++;
      }
      holder--;
    }
  }

 will this work?! guess so.. 
  strncpy(e->buffer, buff, e->max_size);

  if (cnt)
  {
    oldstack = stack;
    if (cnt != 1)
      sprintf(stack, " %d replacements made.\n", cnt);
    else
      strcpy(stack, " One replacement made.\n");
    stack = end_string(stack);
    tell_player(p, oldstack);
    e->current = e->buffer;
    stack = oldstack;
  }
  else
    tell_player(p, " No changes made.\n");

}
*/



/* EDITOR FUNCTIONS - NEW */


/* EWE */
/* function to unmark all lines in a players buffer */
void	ewe_unmark_all(ed_info *e)
{
  editor_line *scan;
  
  if(!e || !e->first_line)
    return;
  
  for(scan = e->first_line; scan; scan = scan->next) {
    scan->flags &= ~edlineMARKED1;
    scan->flags &= ~edlineMARKED2;
  }
}


/* EWE */
/* horrid routine which is very bad to return int according to flags on
   a player */
int	ewe_evaluate_pad(player *p)
{
   int foo=0;
   
   if(p->misc_flags & edPADBIT1)
     foo++;
   if(p->misc_flags & edPADBIT2)
     foo+=2;
   if(p->misc_flags & edPADBIT3)
     foo+=4;
   return foo;
}


/* EWE */
void	ewe_flags_to_stack(player *p)
{
   /* quick flags output */
   if(p->misc_flags & edINSERT_BEFORE)	strcpy(stack, "EWe: Ins: before");
   else					strcpy(stack, "EWe: Ins: after ");
   stack = strchr(stack, 0);
   
   if(p->misc_flags & edPARAGRAPH_MODE) {
     if(p->misc_flags & edPARAGRAPH_STYLE)	strcpy(stack, "   Para: correct");
     else				strcpy(stack, "   Para: ew2    ");
   }
   else					strcpy(stack, "   Para: off    ");
   stack = strchr(stack, 0);
   
   if(p->misc_flags & edAUTOTRUNCATE) {
     if(p->misc_flags & edAUTOTRUNCATE_ALL)	strcpy(stack, "  Trunc: all     ");
     else				strcpy(stack, "  Trunc: included");
   }
   else					strcpy(stack, "  Trunc: off     ");
   stack = strchr(stack, 0);
   
   if (p->custom_flags & QUIET_EDIT)
     strcpy(stack, "  Quiet: on\n");
   else
     strcpy(stack, "  Quiet: off\n");
   stack = strchr(stack, 0);
   
   /* next line!! */
   if(!ewe_evaluate_pad(p))
     strcpy(stack, "EWe: Padding: off");
   else
     sprintf(stack, "EWe: Padding: %-3d", ewe_evaluate_pad(p));
   stack = strchr(stack, 0);
   
   if(!(p->misc_flags & edPRETTY_OUTPUT))
     strcpy(stack, "  Pretty: off");
   else
     strcpy(stack, "  Pretty: on ");
   stack = strchr(stack, 0);
   
   if(p->misc_flags & edFORMAT_OUTPUT)
     strcpy(stack, "    Formatting: all ops       \n");
   else
     strcpy(stack, "    Formatting: navigation ops\n");
   stack = strchr(stack, 0);
      
   /* another line!! */
   if(p->edit_info->last_search[0]!=0) {
     if(p->edit_info->last_search[0]=='f')
       sprintf(stack, "EWe: Current search forwards for: %s\n", &(p->edit_info->last_search[1]));
     else
       sprintf(stack, "EWe: Current search backwards for: %s\n", &(p->edit_info->last_search[1]));
   }
   else
     strcpy(stack, "EWe: Current search: none\n");
   stack = strchr(stack, 0);
}


/* EWE */
/* calculate the percentage of included/real text
   admin should be careful of being too over the top with this setting */
int		ewe_calculate_percentage(ed_info *e)
{
  float perc=0;
  int incl=0;
  editor_line *scan;
  
  /* count included - we know the rest anyway */
   for(scan = e->first_line; scan; scan=scan->next)
     if(scan && scan->flags & edlineINCLUDED)
       incl++;
  
  /* no included text, return 0% */
  if(!incl)
    return 0;
    
  /* else there mustbe some included, if lines==incl, 100% */
  if(e->total_lines==incl)
    return 100;
    
  /* we know the total lines, work out what % of them are down to included
     test */
  perc = (float) incl / (float) e->total_lines;
  return (int) (perc * 100.0);
}


/* EWE */
/* print out some stats */
void            edit_stats(player * p, char *str)
{
   int		   incl = 0, perc = 0;
   char           *oldstack = stack;
   editor_line	  *scan;
   
   /* count included - we know the rest anyway */
   for(scan = p->edit_info->first_line; scan; scan=scan->next)
     if(scan && scan->flags & edlineINCLUDED)
       incl++;
       
   perc = ewe_calculate_percentage(p->edit_info);
   
   sprintf(stack, "EWe: Used %d line%s out of %d, %d line%s (%d%%) included text.\n",
   	p->edit_info->total_lines, numeric_s(p->edit_info->total_lines), 
   	MAX_EDITOR_LINES, incl, numeric_s(incl), perc);
   stack = strchr(stack, 0);
   ewe_flags_to_stack(p);
   *stack++=0;
   tell_player(p, oldstack);
   stack = oldstack;
}


/* EWE */
void		ewe_prompt(player *p)
{
   if(p->misc_flags & edINSERT_BEFORE)
     do_prompt(p, "Inb: ");
   else
     do_prompt(p, "Ina: ");
}


/* EWE */ 
/* slight cpu hit here, but it's the most sane way of doing this. */
void	ewe_reindex_lines(ed_info *info)
{
  int counter = 1;
  editor_line *scan;
  
  if(!info) {
    log("new_editor", "Severe fuckwittedness in ewe_reindex_lines");
    return;
  }
  
  /* yes, this one can validly happen */
  if(!info->first_line) {
    info->total_lines=0;
    return;
  }
  
  scan = info->first_line;
  scan->number = counter;
  while(scan->next) {
    scan = scan->next;
    scan->number = ++counter;
  }

  info->total_lines = counter;
  
  /* tis done */
}


/* EWE */
/* This inserts before/after current line.  
   It returns line if it worked, else 0 */
editor_line	*insert_edit_line(char *line, ed_info *info, int before)
{
  editor_line *new, *prev=0, *scan;
  /* assume that if we got here, the line is totally valid.
     create a structure... */
  new = (editor_line *) MALLOC(sizeof(editor_line));
  if(!new) {
    log("new_edit", "Failed to malloc for line");
    return 0;
  }
  /* copy text into new structure.  Yes this might truncate things.  No
     I do not care.  Someone might, and they might want to write a 
     routine to re-parse everything, but that's their problem. */
  strncpy(new->text, line, 76);
  new->flags = 0;
  
  /* link the new line in */
  if(info->first_line) { /* there is a current_line, logically.  */
    if(before) {
      for(scan=info->first_line; scan && scan!=info->current_line; prev=scan, scan=scan->next);
      /* this will never ever happen */
      if(!scan)
        handle_error("Really fucked list codein insert!");
      /* first line? */  
      if(!prev) {
        new->next = info->first_line;
        info->first_line = new;
        info->current_line = new;
      }
      else {
        new->next = scan;
        prev->next = new;
        info->current_line = new;
      }
    }
    else {
      new->next = info->current_line->next;
      info->current_line->next = new;
      info->current_line = new;
    }
  }
  else { /* there isn't */
    info->first_line = new;
    info->current_line = new;
    new->next = 0;
  }
  
  /* reindex it, we will use a call for this. */
  ewe_reindex_lines(info);
  
  /* all done, methinks */
  return new;
}


/* EWE */
/* delete the marked lines marked by 'set' */
void	ewe_delete_marked(ed_info *e, int set)
{
  editor_line *scan, *prev=0, *kill;

  /* we should have some marked line(s) now - how best to nuke them?
     maintain a previous marker, step through until we run out of
     checkable lines.  if we find a marked one, take it out of sequence
     and delete it.  if it is the current line, set current line to the
     next element if one exists, else the previous. */
  scan = e->first_line;
  while(scan) {
    if((set==1 && scan->flags & edlineMARKED1) ||
       (set==2 && scan->flags & edlineMARKED2)) {
      /* kill it */
      kill = scan;
      if(!prev) {
        e->first_line = scan->next;
        if(e->current_line == scan)
          e->current_line = e->first_line;
      }
      else if(!scan->next) {  /* far end */
        prev->next = 0;
        if(e->current_line == scan)
          e->current_line = prev;
      }
      else {  /* somewhere in the middle */
        prev->next = scan->next;
        if(e->current_line == scan)
          e->current_line = scan->next;
      }
      /* kill should be isolated now, and all ptrs fixed.  get scan to the
         next element before we do this */
      scan = scan->next;
      FREE(kill);
    }
    else {
      prev = scan;
      scan = scan->next;
    }
  }
  /* well - if we're lucky that works. */
}


/* EWE */
void		ewe_wipe_list(ed_info *e)
{
  editor_line *marker;
  /* revamped wipe of list - uses delete_marked */
  for(marker = e->first_line; marker; marker = marker->next)
    marker->flags |= edlineMARKED1;
  
  /* blow them away */
  ewe_delete_marked(e, 1);
  
  /* dont need this, do it anyway */
  ewe_unmark_all(e);
  
  /* reindex */
  ewe_reindex_lines(e);
}


/* EWE */
/* dumps a subsection of text to the stack.  We want it like this
   for formatting and use by other functions */
/* NOTE!!  This does NOT return terminated output!!! */
void	edit_copy_subsection_to_stack(player *p, int start, int end, int headers, int nav)
{
  editor_line *scan;
  int	actual_start, actual_end, trunc_start=0, trunc_end=0, definitely_padding=0;
  ed_info *e;
  
  e = p->edit_info;
  
  if(!e->total_lines) {
    strcpy(stack, "EWe: Buffer is empty.\n");
    stack = strchr(stack, 0);
    return;
  }
  
  /* sanity adjustment */
  if(start < 1)
    start = 1;
  if(end > e->total_lines)
    end = e->total_lines;
    
  /* work out the actual start and end taking into account padding */
  actual_start = start - (ewe_evaluate_pad(p));
  if(actual_start < 1) {
    actual_start = 1;
    trunc_start = 1;
  }
  actual_end = end + (ewe_evaluate_pad(p));
  if(actual_end > e->total_lines) {
    actual_end = e->total_lines;
    trunc_end = 1;
  }
  
  /* however - if if we dont want formatting, fix actual_start */
  if(!(p->misc_flags & edFORMAT_OUTPUT || nav)) {
    actual_start = start;
    actual_end = end;
  }
  
  /* initial divider line */
  if(p->misc_flags & edPRETTY_OUTPUT &&
    (p->misc_flags & edFORMAT_OUTPUT || nav))
    *stack++='\n';
    
  if(start!=actual_start || end!=actual_end)
    definitely_padding=1;
    
  /* print the header and footer if needed */
  if(headers && trunc_start &&
    (p->misc_flags & edFORMAT_OUTPUT || nav)) {
    strcpy(stack, "  # Top of buffer.\n");
    stack = strchr(stack, 0);
  }  
  
  /* display the lines - we're actually pretty sure this is working, so */
  for(scan = e->first_line; scan; scan=scan->next)
    if(scan->number >= actual_start && scan->number <= actual_end) {
      scan->flags &= ~edlineJUST_INSERTED;
      if(e->current_line == scan)
        sprintf(stack, "%3d*%s", scan->number, scan->text);
      else if(scan->number >= start && scan->number <= end && definitely_padding)
        sprintf(stack, "%3d-%s", scan->number, scan->text);
      else
        sprintf(stack, "%3d %s", scan->number, scan->text);
      stack = strchr(stack, 0);
      /* save a few cpu cycles.. */
      if(scan->number == actual_end)
        break;
    }
  
  /* do we need the footer? */
  if(headers && trunc_end &&
    (p->misc_flags & edFORMAT_OUTPUT || nav)) {
    sprintf(stack, "  # Bottom of buffer.\n");
    stack = strchr(stack, 0);
  }
}


/* EWE */
/* autotruncation and scrolling function */
int	autotruncate(player *p, int before)
{
  editor_line *killme=0;
  
  /* moron trap */
  if(!p || !(p->edit_info))
    return 0;
    
  /* they might have it turned off */
  if(!(p->misc_flags & edAUTOTRUNCATE))
    return 0;
    
  /* there might be no lines */
  if(!(p->edit_info->total_lines) || p->edit_info->total_lines < MAX_EDITOR_LINES)
    return 0;
    
  /* if the first line is brand new we can't do much about it */
  if(p->edit_info->first_line->flags & edlineJUST_INSERTED)
    return 0;
    
  /* if we're inserting before the first line, we're stuffed */
  if(p->edit_info->current_line == p->edit_info->first_line && before)
    return 0;
    
  /* if the first line is not included text and we only scroll away included,
     return */
  if(!(p->edit_info->first_line->flags & edlineINCLUDED) &&
     !(p->misc_flags & edAUTOTRUNCATE_ALL))
    return 0;
  
  /* i think it's ok now.  At this point, we can logically just blow away
     the offending line, and it's job done (tm) */
  /* note - if we want to insert after the first line, and it is to be
     truncated, then we should set the before flag before it hits the
     next line.  if we return 2, we toggle before on! */
  if(p->edit_info->first_line == p->edit_info->current_line) {
    before = 2;
  }
  
  /* do it */
  killme = p->edit_info->first_line;
  p->edit_info->first_line = killme->next;
  if(p->edit_info->current_line == killme)
    p->edit_info->current_line = p->edit_info->first_line;
  FREE(killme);
  
  ewe_reindex_lines(p->edit_info);
  
  /* return */
  if(before==2)
    return 2;
  return 1;
}


/* EWE */
/* single line grabbing - pity, this is where Mr Idiot User feeds us. */
/* NB - this seems to work but is UTTERLY HIDEOUS */
/* now calls edit_view_subsection for final output */
void	insert_line(player *p, char *input)
{
  char *end, subinput[76], tsubinput[76], *oldstack = stack;
  char *stub=" ";
  int pos=0, chop=0, consec=0, buflim=0, length=0, before=0, tresult;
  int spoofscan;
  ed_info *tmp;	/* need to dump text into a sep list first then sort it out */
  editor_line *first_added=0, *added;
  
  /* NOT DONE HERE ANYMORE if(p->edit_info->total_lines == MAX_EDITOR_LINES) {
    TELLPLAYER(p, "EWe: Sorry, editor buffer is now full (%d lines).\n", MAX_EDITOR_LINES);
    return;
  }*/
  
  /* blank tmp */
  tmp = (ed_info *) MALLOC(sizeof(ed_info));
  memset(tmp, 0, sizeof(ed_info));
  
  /* check for insert before */
  if(p->misc_flags & edINSERT_BEFORE)
    before=1;
  
  /* preprocess the input.  copy to stack 1 char at a time.  if we hit
     74 consecutive characters, insert a space first and then the character
     and reset consec count.  after first extra insert, start swallowing
     extrenious spaces.  */
  /* now changed to 71 - which gives space for paragraph formatting!! */
  end = input;
  /* if it's a newline only, convert to a single space for the purposes
     of my inferior parsing routine */
  if(!end || !*end)
    end = stub;
  while(end && *end) {
    if(*end==' ') {
      consec=0;
      *stack++=*end++;
      length++;
    }
    else {
      consec++;
      *stack++ = *end++;
      length++;
    }

    /* force a space into a line of continuous input */
    if(consec == 71) {
      *stack++=' ';
      length++;
      consec=0;
    }
  }
  
  /* trim trailing spaces.  god how I hate parsing.
     we should logically be one space past the last character on the stack
     Only do this is length>1, obviously */
  while(length>1 && *(stack-1)==' ') {
    stack--;
    length--;
    if(length>1)
      *stack = 0;
  }
  *stack++=0;
  
  /* DO THE PASSING FOR 74 CHARS THING */
  /* im shouting because the machine hung and just killed this section. */
  end = oldstack;
  
  /* blank subinput before we start */
  for(pos=0; pos<76; pos++)
    subinput[pos]=0;
  pos=0;
  
  /* paragraphing - if this is a correct style paragraph, indent first
     line by 3 characters */
  if(p->misc_flags & edPARAGRAPH_MODE && p->misc_flags & edPARAGRAPH_STYLE) {
    strcpy(subinput, "   ");
    pos = 3;
  }
  
  /* loop through stacked text */
  while(end && *end) {
    /* grab the next character */
    subinput[pos++] = *end++;
    /* check for eol */
    if(pos==74 || !*end) { /* the 75th character!!!!! */
      /* if this is a space, just chop it here
         if this is not a space, work back and chop */
         
      if(*end && *end!=' ') { /* exists, but not a space - wordwrap */
        /* work back and find the first space available.  chop there */
        while(*end!=' ') {
          end--;
          pos--;
        }
      }
      else if(pos) { /* out of characters anywhere past 1st */
        pos--;
      }
      
      /* at this point, if we are on a space, we should still be on it */
      
      /* count back eradicating trailing spaces */
      while(pos && subinput[pos]==' ') {
        subinput[pos] = 0;
        pos--;
      }

      /* we should be on the last character to use, at this point */
      /* put a newline here, and a terminator afterwards */
      /* if trimmed is first position, and a exists, but is a space, pos
         is trimmed is first position, and does not exist, pos
         else pos+1 */
      if(!(!pos && (subinput[pos]==' ' || !subinput[pos])))
        pos++;
      subinput[pos] = '\n';
      subinput[pos+1] = 0;
      
      /* if this is NOT the first line, trim leading spaces too
         modified now to cope with ew2 style leading spaces */
      if(chop) {
        if(p->misc_flags & edPARAGRAPH_MODE && !(p->misc_flags & edPARAGRAPH_STYLE)) {
          /* if this is an ew2 style non-first line */
          while(subinput[3]==' ') {
            strcpy(tsubinput, &subinput[1]);
            strcpy(subinput, tsubinput);
          }
        }
        else {
          while(subinput[0]==' ') {
            strcpy(tsubinput, &subinput[1]);
            strcpy(subinput, tsubinput);
          }
        }
      }
      
      /* validity check for person x trying to fake included text
         What we actually do is convert first > to ) */
      spoofscan = 0;
      while(subinput[spoofscan]==' ' && spoofscan<76)
        spoofscan++;
        
      /* we are at zero or first nonspace character */
      if(subinput[spoofscan]=='>') /* kill it!!!!!! :-) */
        subinput[spoofscan] = ')';
      
      /* used to do something else, but now this adds to the tmp line
         stack */
      insert_edit_line(subinput, tmp, 0);
       
        
      /* clear subinput totally!! 
         Probably a memset job, this one, but I had a few problems so did
         it in a sane way */
      for(pos=0; pos<76; pos++)
        subinput[pos] = 0;

      /* paragraphing
         Once we get here, we have logically already done a line, so */
      if(p->misc_flags & edPARAGRAPH_MODE && !(p->misc_flags & edPARAGRAPH_STYLE)) {
        strcpy(subinput, "   ");
        pos = 3;
      }
      else
        pos=0;
        
      chop=1; /* flag to say we've chopped a line now (hence not first line) */
      if(*end)
        end++; /* will this work here? */
    }
  }
  
  /* paragraphing - a quick check and fix for correct style paras when
     only one line has been entered.... */
  if(tmp->total_lines==1 && tmp->first_line->text[2] == ' ' &&
     p->misc_flags & edPARAGRAPH_MODE && p->misc_flags & edPARAGRAPH_STYLE) {
    strcpy(tsubinput, &(tmp->first_line->text[3]));
    strcpy(tmp->first_line->text, tsubinput);
  }

     
  while(!buflim && tmp && tmp->first_line) {
    tmp->current_line = tmp->first_line;
    /* we have a line.  do we have any editor space? */
    if(p->edit_info->total_lines == MAX_EDITOR_LINES) {
      tresult = autotruncate(p, before);
      if(!tresult)
        buflim = 1;
      else if(tresult==2)
        before = 1;
    }

    if(!buflim) {
      /* now add the line to the actual buffer */
      added = insert_edit_line(tmp->first_line->text, p->edit_info, before);
      added->flags |= edlineJUST_INSERTED;
      if(!first_added)
        first_added = added;
      before=0;		/* when inserting before, only do the first line */
      
      tmp->current_line->flags |= edlineMARKED1;
      ewe_delete_marked(tmp, 1);
    }
  }
  
  /* get rid of that temp list now */
  ewe_wipe_list(tmp);
  FREE(tmp);
  
  /* show what was added to the user.  pretty looks ugly, so we don't */
  stack = oldstack;
  if(first_added) 
    edit_copy_subsection_to_stack(p, first_added->number, p->edit_info->current_line->number, 1, 0);
  
  /* fix developed while testing on resort */
  *stack = 0;

  /* eep.  too many lines now? */
  if(!first_added) /* we managed to get something on */
    sprintf(stack, "EWe: Sorry, editor buffer is now full (%d lines).\n", MAX_EDITOR_LINES);
  else if(buflim)
    sprintf(stack, "EWe: Editor buffer limit reached (%d lines) - input truncated.\n", MAX_EDITOR_LINES);
    
  stack = strchr(stack, 0);  
  *stack++=0;
  tell_player(p, oldstack);
  stack = oldstack;
}


/* EWE */
/* the main editor function */
void            editor_main(player * p, char *str)
{
   if (!p->edit_info)
   {
      log("error", "Editor called with no edit_info");
      return;
   }
   if (*str == '/')
   {
      restore_flags(p);
      match_commands(p, str + 1);
      save_flags(p);
      return;
   }
   if (*str == '.')
   {
      sub_command(p, (str + 1), editor_list);
      if (p->edit_info)
	ewe_prompt(p);
      return;
   }
   insert_line(p, str);
   
   ewe_prompt(p);
}


/* EWE */
/* something to quickly display the edit buffer */
void	edit_view(player *p, char *str)
{
  char *oldstack=stack;
  
  if(!p || !p->edit_info)
    return;
  
  /* the info
     start line (1)
     end line (total_lines - all of them)
     no padding
     pretty output
     no headers */
  
  edit_copy_subsection_to_stack(p, 1, p->edit_info->total_lines, 1, 1);
  /* the above does NOT terminate, so */
  *stack++=0;
  tell_player(p, oldstack);
  stack = oldstack;
}


/* EWE */
int	mangle_existing_into_lines(player *p, char *chunk, ed_info *e, int incl)
{
  char *scan, grabbing[76];
  int ptr=0;
  editor_line *tmp;
  
  /* is there a chunk there?  I know this is obvious, but... */
  if(!chunk || !*chunk || !e)
    return 0;
  
  /* scan through it I suppose... */
  scan = chunk;
  grabbing[0]=0;
  while(*scan) {
    /* is it a newline? */
    if(*scan == '\n') {		/* add it to the editor lines list */
      grabbing[ptr]='\n';
      grabbing[ptr+1]=0;
      ptr=0;
      scan++;
      tmp = insert_edit_line(grabbing, e, 0);
      if(!tmp)
        handle_error("mangle_existing_into_lines trap 1");
      tmp->flags |= edlineINCLUDED;
      if(incl)
        tmp->flags |= edlineINCLUDED_MATTERS;
      grabbing[0]=0;
    }
    else {			/* add the character */
      grabbing[ptr++] = *scan++;
      if(ptr==75) { /* must be too long for the editor! */
        ptr--;
        /* force scan to end of line */
        while(*scan && *scan!='\n')
          scan++;
      }
    }
  }
  
  /* if there's anything left after this, we probably want that too */
  if(ptr) {
    grabbing[ptr]=0;
    tmp = insert_edit_line(grabbing, e, 0);
    if(!tmp)
      handle_error("mangle_existing_into_lines trap 2");
    tmp->flags |= edlineINCLUDED;
    if(incl)
      tmp->flags |= edlineINCLUDED_MATTERS;
  }
  
  /* now "scroll" away any lines to bring it within MAX_LINES-1 */
  if(e->total_lines >= MAX_EDITOR_LINES) {
    while(e->total_lines >= MAX_EDITOR_LINES) {
      e->first_line->flags |= edlineMARKED1;
      ewe_delete_marked(e, 1);
      e->total_lines--;
    }
    ewe_reindex_lines(e);
    return 1;
  }
  return 0;
}

   
/* EWE */
/* fire editor up */
void            start_edit(player * p, int max_size, player_func *finish_func, player_func *quit_func,
                            char *current, int incl)
{
   ed_info        *e;
   int             trunc=0;
   
   if (p->edit_info) {
      tell_player(p, " Unable to enter editor while already in edit/pager.\n");
      return;
   }
   e = (ed_info *) MALLOC(sizeof(ed_info));
   memset(e, 0, sizeof(ed_info));
   p->edit_info = e;
   e->first_line=0;
   e->current_line=0;
   e->total_lines=0;
   e->buffer = 0;
   e->current = 0;
   e->max_size = 0;
   e->finish_func = finish_func;
   e->quit_func = quit_func;
   
   /* hook into new code.. */
   trunc = mangle_existing_into_lines(p, current, e, incl);
   
   save_flags(p);
   
   /* initial editor information when we start editing. */
   tell_player(p, "EWe: Entering editor ... Use .help editor for help.\n"
                  "     /<command> for general commands, .<command> for editor commands\n"
                  "     Use '.end' to finish and keep edit.\n");
   
   edit_view(p, 0);
   if(trunc)
     tell_player(p, "EWe: Text truncated to this point...\n");
   edit_stats(p, 0);
   ewe_prompt(p);
}


/* EWE */
/* tie up any loose ends */
void            finish_edit(player * p)
{
   if (!(p->edit_info))
      return;
   restore_flags(p);

   /* to aid in conversion for other people, we now dump all the lines
      from the linked list into the buffer area, so we should really
      free that too in here */
   if(p->edit_info->buffer)
     FREE(p->edit_info->buffer);
     
   ewe_wipe_list(p->edit_info);
   
   FREE(p->edit_info);
   p->edit_info = 0;
   p->flags &= ~DONT_CHECK_PIPE;

  /* pg+ specific flags? */
  p->mode &= ~MAILEDIT;
  p->flags &= ~IN_EDITOR;
}




/* EWE */
/* finish editing with changes */
void            edit_end(player * p, char *str)
{
   editor_line *scan;
   int matters=0, perc, length=0;
   char *copy;
   /* actually we now have a hook in here to check for % of included text ;-) */
   for(scan = p->edit_info->first_line; scan; scan=scan->next) {
     length += strlen(scan->text);
     if(scan->flags & edlineINCLUDED_MATTERS) 
       matters=1;
   }
   
   if(matters) {
     perc = ewe_calculate_percentage(p->edit_info);
     if(perc > MAX_INCLUDE_PERCENTAGE) {
       TELLPLAYER(p, "EWe: Too much included text! (%d%% included, max %d%%)\n",
       		perc, MAX_INCLUDE_PERCENTAGE);
       return;
     }
   }
   
   /* copy it into the old buffer location.  heh. */
   /* allocate */
   p->edit_info->buffer = (char *) MALLOC(length+1);
   memset(p->edit_info->buffer, 0, length+1);
   /* copy */
   copy = p->edit_info->buffer;
   for(scan = p->edit_info->first_line; scan; scan = scan->next) {
     strcpy(copy, scan->text);
     copy = strchr(copy, 0);
   }
   
   p->edit_info->size = length+1;
   
   (*p->edit_info->finish_func) (p);
   finish_edit(p);
}


/* EWE */
/* view the current line */
void            edit_view_line(player * p, char *str)
{
   char           *oldstack=stack;

   /* thanks to eightball for finding this one */
   if(!p->edit_info->first_line) {
     tell_player(p, "EWe: Buffer is empty.\n");
     return;
   }
                                   
   edit_copy_subsection_to_stack(p, p->edit_info->current_line->number, p->edit_info->current_line->number, 1, 1);
   *stack++=0;
   tell_player(p, oldstack);
   stack = oldstack;
}


/* EWE */
/* move back up a line */
void            edit_back_line(player * p, char *str)
{
   editor_line *scan;
   
   if(!p->edit_info->total_lines) {
     tell_player(p, "EWe: Buffer is empty.\n");
     return;
   }
   
   if(p->edit_info->current_line == p->edit_info->first_line)
   {
      tell_player(p, "EWe: Can't go back any more, top of buffer.\n");
      return;
   }
   
   scan = p->edit_info->first_line;
   while(scan && scan->next && scan->next != p->edit_info->current_line)
     scan = scan->next;
   if(!scan || !scan->next)
     handle_error("fucked list in edit_back_line");
   
   p->edit_info->current_line = scan;

   edit_view_line(p, 0);
}


/* EWE */ 
/* move forward a line */
void            edit_forward_line(player * p, char *str)
{
   if(!p->edit_info->total_lines) {
     tell_player(p, "EWe: Buffer is empty.\n");
     return;
   }
   
   if(p->edit_info->current_line->number == p->edit_info->total_lines)
   {
      tell_player(p, "EWe: Can't go forward, bottom of buffer.\n");
      return;
   }
   
   /* move onward */
   p->edit_info->current_line = p->edit_info->current_line->next;
   
   edit_view_line(p, 0);
}




/* EWE */ 
/* move to bottom of buffer */
void            edit_goto_top(player * p, char *str)
{
   char *oldstack = stack;
   if(!p->edit_info->first_line) {
     tell_player(p, "EWe: Buffer is empty.\n");
     return;
   }
   if(p->edit_info->current_line == p->edit_info->first_line) {
     tell_player(p, "EWe: Already at top of buffer.\n");
     return;
   }
   p->edit_info->current_line = p->edit_info->first_line;
   
   /* let's actually display it... */
   
   edit_copy_subsection_to_stack(p, p->edit_info->current_line->number, p->edit_info->current_line->number, 0, 1);
   strcpy(stack, "EWe: Top of buffer.\n");
   stack = end_string(stack);
   tell_player(p, oldstack);
   stack = oldstack;
}


/* EWE */ 
/* move to top of buffer */
void            edit_goto_bottom(player * p, char *str)
{
   char *oldstack = stack;
   
   if(!p->edit_info->first_line) {
     tell_player(p, "EWe: Buffer is empty.\n");
     return;
   }
   if(!p->edit_info->current_line->next) {
     tell_player(p, "EWe: Already at bottom of buffer.\n");
     return;
   }
   p->edit_info->current_line = p->edit_info->first_line;
   while(p->edit_info->current_line && p->edit_info->current_line->next)
     p->edit_info->current_line = p->edit_info->current_line->next;
     
   edit_copy_subsection_to_stack(p, p->edit_info->current_line->number, p->edit_info->current_line->number, 0, 1);
   sprintf(stack, "EWe: Bottom of buffer.\n");
   stack = end_string(stack);
   tell_player(p, oldstack);
   stack = oldstack;
}


/* EWE */
/* go to a specific line */
void            edit_goto_line(player * p, char *str)
{
   char           *scan;
   int             line = 0;
   editor_line	  *linescan;
   
   if(!*str) {
     tell_player(p, "EWe: Format: .g <line number>\n");
     return;
   }
   
   /* thanks to eightball for finding this one */
   if(!p->edit_info->first_line) {
     tell_player(p, "EWe: Buffer is empty.\n");
     return;
   }
   
   /* you just know someone will do this. */
   for(scan=str; *scan; scan++)
     if(!isdigit(*scan)) {
       tell_player(p, "EWe: Format: .g <line number>\n");
       return;
     }
     
   line = atoi(str);
   if (line < 1)
   {
     tell_player(p, "EWe: Format: .g <line number>\n");
     return;
   }
   
   if (line > p->edit_info->total_lines) {
     TELLPLAYER(p, "EWe: No line %d to go to (total %d).\n", line, p->edit_info->total_lines);
     return;
   }
   
   /* should be ok, but we'll run a search though for safety */
   for(linescan = p->edit_info->first_line; linescan; linescan = linescan->next)
     if(linescan->number == line) {
       p->edit_info->current_line = linescan;
       edit_view_line(p, 0);
       return;
     }
   
   /* if this happens, something is really quite fucked! */
   log("new_editor", "meep!  go function failed to locate a line");
   tell_player(p, "EWe: Severe error.  Try .quit!\n");
}


/* EWE */
/* toggle whether someone goes into quiet mode on edit */
void            toggle_quiet_edit(player * p, char *str)
{
   restore_flags(p);
   if (!strcasecmp("off", str))
      p->custom_flags &= ~QUIET_EDIT;
   else if (!strcasecmp("on", str))
      p->custom_flags |= QUIET_EDIT;
   else
      p->custom_flags ^= QUIET_EDIT;

   if (p->custom_flags & QUIET_EDIT)
     tell_player(p, "EWe: You are now blocking tells and shouts while editing.\n");
   else
     tell_player(p, "EWe: You are now allowing shouts and tells while editing.\n");
  
   save_flags(p);
}


/* EWE */
/* clean the buffer completely */
void            edit_wipe(player * p, char *str)
{
   ewe_wipe_list(p->edit_info);
   tell_player(p, "EWe: Editor buffer completely wiped...\n");
}


/* EWE */
/* set padding from 0-9 */
void	edit_set_padding(player*p, char *str)
{
  int lines=-1, tlines;
  char *scan;
  
  if(!*str) {
    tell_player(p, "EWe: Format: .pad <0-7 lines>\n");
    return;
  }
  
  for(scan=str; *scan; scan++)
    if(!isdigit(*scan)) {
      tell_player(p, "EWe: Format: .pad <0-7 lines>\n");
      return;
    }
  
  lines = atoi(str);
  
  if(lines<0 || lines>7) {
    tell_player(p, "EWe: Format: .pad <0-7 lines>\n");
    return;
  }
  
  /* set it */
  restore_flags(p);
  tlines = lines;
  /* spotted by cynthiarose */
  p->misc_flags &= ~edPADBIT1;
  p->misc_flags &= ~edPADBIT2;
  p->misc_flags &= ~edPADBIT3;
  if(tlines>3) {
    p->misc_flags |= edPADBIT3;
    tlines-=4;
  }
  if(tlines>1) {
    p->misc_flags |= edPADBIT2;
    tlines-=2;
  }
  if(tlines) 
    p->misc_flags |= edPADBIT1;
  save_flags(p);
  
  
  if(!lines) {
    tell_player(p, "EWe: Padding on line reports turned off.\n");
    return;
  }
  
  TELLPLAYER(p, "EWe: Padding set to %d line%s each side.\n", lines, numeric_s(lines));
}


/* EWE */
/* turn on insert-before */
void	edit_toggle_insert(player *p, char *str)
{
   restore_flags(p);
   if (!strcasecmp("after", str))
      p->misc_flags &= ~edINSERT_BEFORE;
   else if (!strcasecmp("before", str))
      p->misc_flags |= edINSERT_BEFORE;
   else
      p->misc_flags ^= edINSERT_BEFORE;

   if (p->misc_flags & edINSERT_BEFORE)
      tell_player(p, "EWe: Insert before activated.\n");
   else
      tell_player(p, "EWe: Insert after activated.\n");
   save_flags(p);
}


/* EWE */
/* turn on insert-before */
void	edit_toggle_pretty(player *p, char *str)
{
   restore_flags(p);
   if (!strcasecmp("off", str))
      p->misc_flags &= ~edPRETTY_OUTPUT;
   else if (!strcasecmp("on", str))
      p->misc_flags |= edPRETTY_OUTPUT;
   else
      p->misc_flags ^= edPRETTY_OUTPUT;

   if (p->misc_flags & edPRETTY_OUTPUT)
      tell_player(p, "EWe: Newline before output enabled.\n");
   else
      tell_player(p, "EWe: newline before output disabled.\n");
   save_flags(p);
}


/* EWE */
/* turn on formatting for navigation only or all */
void	edit_toggle_formatting(player *p, char *str)
{
   restore_flags(p);
   if (!strcasecmp("navigation", str))
      p->misc_flags &= ~edFORMAT_OUTPUT;
   else if (!strcasecmp("all", str))
      p->misc_flags |= edFORMAT_OUTPUT;
   else
      p->misc_flags ^= edFORMAT_OUTPUT;

   if (p->misc_flags & edFORMAT_OUTPUT)
      tell_player(p, "EWe: Output formatting for all operations enabled.\n");
   else
      tell_player(p, "EWe: Output formatting for navigation operations enabled.\n");
   save_flags(p);
}


/* EWE */
/* set paragraph mode */
void	edit_toggle_paragraph(player *p, char *str)
{
   restore_flags(p);
   if(!strcasecmp("off", str)) 
      p->misc_flags &= ~edPARAGRAPH_MODE;
   else if (!strcasecmp("ew2", str)) {
      p->misc_flags |= edPARAGRAPH_MODE;
      p->misc_flags &= ~edPARAGRAPH_STYLE;
   }
   else if (!strcasecmp("correct", str)) {
      p->misc_flags |= edPARAGRAPH_MODE;
      p->misc_flags |= edPARAGRAPH_STYLE;
   }
   else 
     tell_player(p, "EWe: Format: .para <off/ew2/correct>\n");

   if (!(p->misc_flags & edPARAGRAPH_MODE))
      tell_player(p, "EWe: Paragraph mode disabled.\n");
   else if (p->misc_flags & edPARAGRAPH_STYLE)
      tell_player(p, "EWe: Correct style paragraphs enabled.\n");
   else
      tell_player(p, "EWe: EW2 style paragraphs enabled.\n");
   save_flags(p);
}


/* EWE */
/* set autotruncate mode */
void	edit_toggle_autotruncate(player *p, char *str)
{
   restore_flags(p);
   if(!strcasecmp("off", str)) 
      p->misc_flags &= ~edAUTOTRUNCATE;
   else if (!strcasecmp("included", str)) {
      p->misc_flags |= edAUTOTRUNCATE;
      p->misc_flags &= ~edAUTOTRUNCATE_ALL;
   }
   else if (!strcasecmp("all", str)) {
      p->misc_flags |= edAUTOTRUNCATE;
      p->misc_flags |= edAUTOTRUNCATE_ALL;
   }
   else
     tell_player(p, "EWe: Format: .trunc <off/included/all>\n");

   if (!(p->misc_flags & edAUTOTRUNCATE))
      tell_player(p, "EWe: Auto truncation disabled.\n");
   else if (p->misc_flags & edAUTOTRUNCATE_ALL)
      tell_player(p, "EWe: Auto truncation enabled for all text.\n");
   else
      tell_player(p, "EWe: Auto truncation enabled for included text.\n");
   save_flags(p);
}


/* EWE */
/* check format of arguments against expected format */
int	ewe_check_format(player *p, int expected, int have, char *argument)
{
  if(!have) {
    TELLPLAYER(p, "EWe: Malformed number/range: %s\n", argument);
    return 0;
  }
  /* checks for format */
  if(expected==1 && have > 1) {
    TELLPLAYER(p, "EWe: Sorry, %s is not a single number.\n", argument);
    return 0;
  }
  if(expected==2 && have==3) {
    TELLPLAYER(p, "EWe: Sorry, %s is not a single range.\n", argument);
    return 0;
  }
  return 1;
}
  
  
/* EWE */
/* function to check validity or passed arguments. 
   return:	0	malformed
   		1	single value
   		2	range
   		3	multi value/range
   */
int	ewe_range_process(char *process)
{
  int count, multir=0, has_range=0, sels, sele;
  char *scan, *oldstack = stack;
  
  if(!*process)
    return 0;
    
  /* check to see what sort of characters we have */
  /* we know the range is broken if:  , has either no number either side of it
  				      - has no number eitehr side of it */
  for(scan = process; *scan; scan++)
    if(!isdigit(*scan) && *scan!='-' && *scan!=',')
      return 0;
    else if(*scan == '-') {
      has_range = 1;
      if((scan == process || !*(scan+1)) ||
         (!isdigit(*(scan-1)) || !isdigit(*(scan+1))))
        return 0;
    }
    else if(*scan == ',') {
      multir = 1;
      if((scan == process || !*(scan+1)) ||
         (!isdigit(*(scan-1)) || !isdigit(*(scan+1))))
        return 0;
    }
  
  /* wipe the selection list */
  for(count=0; count<=MAX_EDITOR_LINES; count++)
    selectlist[count] = 0;
    
  /* parse what we have.  this could get messy.
     Might result in returns too if the user has been a spoon */
  scan = process;

  while(*scan) {
    /* grab first number to stack */
    while(*scan && isdigit(*scan))
      *stack++ = *scan++;
    *stack++=0;
    
    /* should be at a * or ,.  part should point to the start of the "number" */
    /* what is it? */
    if(*scan && *scan=='-') {
      scan++;
      sels = atoi(oldstack);
      stack = oldstack;
      while(*scan && isdigit(*scan))
        *stack++ = *scan++;
      *stack++=0;
      
      /* x-y-z?  NO */
      if(*scan && *scan=='-')
        return 0;
      if(*scan)
        scan++;
      sele = atoi(oldstack);
      stack = oldstack;
    }
    else {
      sels = sele = atoi(oldstack);
      stack = oldstack;
      if(*scan)
        scan++;
    }
    
    /* check these boundaries for validity */
    /* whilst zero is bad news, we're going to mention that elsewhere */
    if(sels>sele || sele<0 || sels<0 || sels>MAX_EDITOR_LINES || sele>MAX_EDITOR_LINES) /* duuuhh.  duuh..  du du duuuuuhhhhh. */
      return 0;
    
    /* select them - fear. */
    for(count=sels; count<=sele; count++) {
      selectlist[count] = 1;
    }
  }
  
  /* work out what type we have */
  if(multir)
    return 3;
  else if(has_range)
    return 2;
  return 1;
}


/* EWE */
/* code to parse a range and mark things as appropriate */
/* player is obvious, arguments are their args if any, and format is:
   0 - no value
   1 - single value
   2 - single value/range
   3 - multiple values/ranges
   For set two these count as optional.
   Yeah - it's just another lil parsing nightmare... 
   
   Oh - returns 0, 1
   	0 - select failed
   	1 - worked */
int	ewe_mark_lines(player *p, char *arguments, int format, int set)
{
  char *scan;
  int  actual_format = 0, count;
  editor_line *escan;
  
  /* if there are no lines, we really don't want to fuck about like this */
  /* OLD if(!p->edit_info->total_lines)
    return 0; */
    
  /* someone might be a *MORON* */
  if(!format || !arguments || !*arguments)
    return 0;
  
  /* wipe the select list */
  for(count = 0; count <= MAX_EDITOR_LINES; count++)
    selectlist[count] = 0;
    
  /* quickly strip the trailing spaces off arguments (full string) */
  scan = arguments;
  scan += strlen(arguments);
  if(scan!=arguments) {
    scan--;
    while(*scan && *scan==' ' && scan>=arguments)
      *scan--=0;
    if(!arguments || !*arguments)
      return 0;
  }
  
  /* quickly strip the trailing spaces off arguments (set 1) */
  scan = arguments;
  scan += strlen(arguments);
  if(scan!=arguments) {
    scan--;
    while(*scan && *scan==' ' && scan>=arguments)
      *scan--=0;
    if(!arguments || !*arguments)
      return 0;
  }
    
  /* process the given range */
  actual_format = ewe_range_process(arguments);
  /* check format */
  if(!ewe_check_format(p, format, actual_format, arguments))
    return 0;
    
  /* mark the lines if at all possible - scan list, then check afterwards for
     straggling unselected numbers */
  for(escan = p->edit_info->first_line; escan; escan = escan->next)
    if(selectlist[escan->number] == 1) {
      if(set==2 && escan->flags & edlineMARKED1) {
        TELLPLAYER(p, "EWe: Error - line %d was selected in your first argument!\n", escan->number);
        return 0;
      }
      if(set==1)
        escan->flags |= edlineMARKED1;
      else
        escan->flags |= edlineMARKED2;
      selectlist[escan->number] = 0;
    }

  /* check for line zero - pity to have to have it here but i want a decent message */
  if(selectlist[0] == 1) {
    TELLPLAYER(p, "EWe: Error - there is never a line 0.\n");
    return 0;
  }
  
  /* now check for straggling numbers */
  for(count=1; count<=MAX_EDITOR_LINES; count++)
    if(selectlist[count] == 1) {
      TELLPLAYER(p, "EWe: Error - you don't have a line %d (total %d!)\n", count, p->edit_info->total_lines);
      return 0;
    }
    
  return 1;
}


/* EWE */
/* move lines - a simple one to start off with, to test my new routines :-) */
void	edit_delete_line(player *p, char *str)
{
  /* not much point if there's nothing to delete */
  if(!p->edit_info->total_lines) {
    tell_player(p, "EWe: Buffer is empty.\n");
    return;
  }
  
  /* use current line? */
  if(!*str) {
    p->edit_info->current_line->flags |= edlineMARKED1;
  }
  else {  /* parse the string */
    if(!ewe_mark_lines(p, str, 3, 1)) {
      ewe_unmark_all(p->edit_info);
      return;
    }
  }
  
  ewe_delete_marked(p->edit_info, 1);
  ewe_reindex_lines(p->edit_info);
  /* if we've been sensible, this won't be required, but I can't rely on that */
  ewe_unmark_all(p->edit_info);
  tell_player(p, "EWe: Line(s) deleted.\n");
}


/* EWE */
/* show version of ewe */
void	edit_show_version(player *p, char *str)
{
  TELLPLAYER(p, "EWe: EW-editor V%s by slaine (james@zotz.demon.co.uk).\n", EWE_VERSION);
}


/* EWE */
/* move appropriate flagged lines */
void	ewe_move_marked(ed_info *e)
{
  editor_line *scan, *mstart, *mend, *mprev, *dprev;
  if(!e || !e->total_lines)
    return;
  
  for(mprev = 0, scan = e->first_line; scan && !(scan->flags & edlineMARKED1); mprev = scan, scan = scan->next);
  if(!scan) {
    log("new_editor", "ewe_move_marked broken 1");
    return;
  }
  mstart = scan;
  mend = scan;
  while(scan && scan->flags & edlineMARKED1) {
    mend = scan;
    scan = scan->next;
  }
  
  /* let's do something with the current_line pointer before we fuck it up */
  if(e->current_line->flags & edlineMARKED1) {
    if(mend->next)
      e->current_line = mend->next;
    else if(mprev)
      e->current_line = mprev;
    else {
      log("new_editor", "ewe_move_marked broken 2");
      return;
    }
  }
  
  /* we should have enough pointers to the structure now, so let's
     delink them */
  if(mprev)
    mprev->next = mend->next;
  else
    e->first_line = mend->next;
  
  /* and .. link them back in again - first locate the destination.. */
  /* actually - if eosrc < dest, move to after dest, else before */
  for(dprev = 0, scan = e->first_line; scan && !(scan->flags & edlineMARKED2); dprev = scan, scan = scan->next);
  if(mend->number < scan->number) {
    mend->next = scan->next;
    scan->next = mstart;
  }
  else {
    if(dprev)
      dprev->next = mstart;
    else
      e->first_line = mstart;
    mend->next = scan;    
  }
}


/* EWE */
/* move a block of text */
void	edit_move_lines(player *p, char *str)
{
  char *arg2;
  ed_info *e = p->edit_info;
  editor_line *scan;
  
  if(!*str) {
    tell_player(p, "EWe: Format: .move [single range] <line>\n");
    return;
  }
  
  if(e->total_lines<2) {
    tell_player(p, "EWe: No lines available to move.\n");
    return;
  }
  
  /* split arguments */
  arg2 = next_space(str);
  if(*arg2)
    *arg2++=0;
  
  if(*arg2) {
    if(!ewe_mark_lines(p, str, 2, 1)) {
      ewe_unmark_all(e);
      return;
    }
  }
  else
    e->current_line->flags |= edlineMARKED1;
    
  /* we aren't allowed to move text that has been included! */
  for(scan = e->first_line; scan; scan = scan->next)
    if(scan->flags & edlineMARKED1 && scan->flags & edlineINCLUDED
       && scan->flags & edlineINCLUDED_MATTERS)
    {
      tell_player(p, "EWe: Sorry, it is not allowed to move lines of included text in news.\n");
      ewe_unmark_all(e);
      return;
    }
  
  if(*arg2) {
    if(!ewe_mark_lines(p, arg2, 1, 2)) {
      ewe_unmark_all(e);
      return;
    }
  }
  else {
    if(!ewe_mark_lines(p, str, 1, 2)) {
      ewe_unmark_all(e);
      return;
    }
  }
      
  ewe_move_marked(e);   
  ewe_reindex_lines(e);
  /* check this */
  TELLPLAYER(p, "EWe: Line(s) moved.\n");
  ewe_unmark_all(e);
}


/* EWE */
/* replace a line.  this is a good idea, better even than a very good one
   hopefully */
void	edit_replace_lines(player *p, char *str)
{
  char *arg2;
  editor_line *prev, *scan;
  int before=0, tmp;
  
  if(!*str) {
    tell_player(p, "EWe: Format: .replace <single range> <text>\n");
    return;
  }
  
  /* pick an arg, any arg. */
  arg2 = str;
  while(*arg2 && *arg2!=' ')
    arg2++;
  if(*arg2)
    *arg2++=0;
  if(!*arg2) {
    tell_player(p, "EWe: Format: .replace <single range> <text>\n");
    return;
  } 
  
  /* let's see...  */
  if(!ewe_mark_lines(p, str, 2, 1))
    return;
    
  /* fuck their shit up, or whatever, but first - let's correct current_line
     so that it lives somewhere more sensible if lines are to be left over */
  for(prev=0, scan = p->edit_info->first_line; scan && !(scan->flags & edlineMARKED1); prev=scan, scan = scan->next);
  if(!scan) {
    log("new_editor", "fucked in replace 1");
    tell_player(p, "EWe: Serious error in replace, please report to admin.\n");
    return;
  }
  if(prev) {
    p->edit_info->current_line = prev;
  }
  else {
    while(scan && scan->flags & edlineMARKED1)
      scan = scan->next;
    if(scan) {
      p->edit_info->current_line = scan;
      before = 1;
    }
    /* else current==only, no need to remember it... */
  }
  
  ewe_delete_marked(p->edit_info, 1);
  ewe_reindex_lines(p->edit_info);
  ewe_unmark_all(p->edit_info);
  
  /* now re-add their text */
  
  tmp = p->misc_flags;
  
  if(before)
    p->misc_flags |= edINSERT_BEFORE;
  else
    p->misc_flags &= ~edINSERT_BEFORE; /* spotted by blimey */
    
  insert_line(p, arg2);
  
  p->misc_flags = tmp;
}


/* EWE */
int	ewe_find_help(char *str)
{
  char *scan;
  char *oldstack=stack;
  
  if(!edit_help_file.where || !*edit_help_file.where)
    return 0;

  scan = edit_help_file.where;
  
  if(!scan || !*scan || *scan!=':')
    return 0;
    
  /* loop while we have another file to read */
    /* skip initial colon */
    scan++;
    while(*scan) {
      if(*scan == ',' || *scan == '\n') {
        *stack++=0;
        if(!strcasecmp(oldstack, str)) { /* found it */
          /* skip past next \n (or current one) */
          while(*scan!='\n')
            scan++;
          scan++;
          stack = oldstack;
          /* should be at start of help text - dump out until we hit a
             \n: combination or nothing */
          while(*scan && *(scan+1) && !(*scan=='\n' && *(scan+1)==':'))
            *stack++=*scan++;
          *stack++='\n';
          *stack++=0;
          return 1;
        }
        else {
          /* we have not.  if this is a newline, search for next colon after
             newline, else step 1 char along */
          if(*scan=='\n') {
            while(*scan && *(scan+1) && !(*scan=='\n' && *(scan+1)==':'))
              scan++;
            if(*(scan+1) && *(scan+1)==':')
              scan++;
          }
          scan++;
        }
        stack = oldstack;
      }
      else {
        *stack++ = *scan++;
      }
    }
  stack = oldstack;
  return 0;
}


/* EWE */
/* editor specific help function.  written like this due to differences
   in code bases .. */
void	edit_help(player *p, char *str)
{
  char *oldstack = stack;
  
  if(!*str) {
    tell_player(p, "EWe: Format: .help <command>\n");
    return;
  }
  sprintf(stack, "EWe: Help %s\n", str);
  stack = strchr(stack, 0);
  if(!ewe_find_help(str)) {
    stack = oldstack;
    TELLPLAYER(p, "EWe: No help on '%s' found.\n", str);
    return;
  }
  tell_player(p, oldstack);  
  stack = oldstack;
}


/* EWE - start up for new editor */
void	ewe_init_editor(void)
{
  edit_help_file = load_file("doc/edit_help");
}


/* EWE support routine for searching functions - basically find the next in
   whatever order is required */
int	ewe_find_next(player *p, int skip_first)
{
  editor_line *scan, *prev;
  
  if(!p->edit_info->total_lines) {
    return 0;
  }
  
  if(!(p->edit_info->last_search[0])) {
    return 0;
  }
  
  scan = p->edit_info->current_line;
  
  if(p->edit_info->last_search[0]=='f') {
    if(!scan->next) {
      tell_player(p, "EWe: Cannot search forwards from end of buffer.\n");
      return 0;
    }
    /* we want to check top line if w scan from start, for example.. */
    if(skip_first)
      scan = scan->next;
    while(scan) {
      strcpy(stack, scan->text);
      lower_case(stack);
      if(strstr(stack, &(p->edit_info->last_search[1]))) {
        p->edit_info->current_line = scan;
	edit_view_line(p, 0);
	return 1;
      }
      scan = scan->next;
    }
  }
  else { /* backwards scan - hard */
    if(scan == p->edit_info->first_line) {
      tell_player(p, "EWe: Cannot search backwards from start of buffer.\n");
      return 0;
    }
    while(scan!=p->edit_info->first_line) { /* logically there MUST be a previous */
      for(prev = p->edit_info->first_line; prev && prev->next!=scan; prev = prev->next);
      if(!prev)
        handle_error("What the fuck!?!?!? (ewe_find_next)");
      scan = prev;
      strcpy(stack, scan->text);
      lower_case(stack);
      if(strstr(stack, &(p->edit_info->last_search[1]))) {
        p->edit_info->current_line = scan;
	edit_view_line(p, 0);
	return 1;
      }
    }
  }
  
  tell_player(p, "EWe: Not found.\n");
  return 0;
}


/* EWE - search for text */
void	edit_search(player *p, char *str)
{
  int direction=0;
  char *srch;
  editor_line *tmp=0;
  
  if(!p->edit_info->total_lines) {
    tell_player(p, "EWe: Buffer is empty.\n");
    return;
  }
  
  if(!*str) {
    tell_player(p, "EWe: Format: s [f/b] <text to search for>\n");
    return;
  }
  lower_case(str);
  
  /* blank any existing search info */
  memset(p->edit_info->last_search, 0, 75);
  
  /* preserve current line */
  tmp = p->edit_info->current_line;
  
  /* if there is more than one arg, it could be a b or f case - check
     this */
  srch = next_space(str);
  if(!srch || !*srch) {
    srch = str;
    /* top down in this case as well as below */
    p->edit_info->current_line = p->edit_info->first_line;
  }
  else {
    if((*str=='f' || *str=='b') && *(str+1) && *(str+1)==' ') {
      if(*str=='f')
        direction = 1;
      else
        direction = 2;
      *srch++=0;
      if(!srch || !*srch) {
        srch = str;
      }
    }
    else { /* top down search - preserve location! */
      p->edit_info->current_line = p->edit_info->first_line;
      srch = str;
    }
  }

  /* copy the info in to the player struct */
  if(direction==2) {
    p->edit_info->last_search[0] = 'b';
  }
  else {
    p->edit_info->last_search[0] = 'f';
  }
  strncpy(&(p->edit_info->last_search[1]), srch, 73);
  
  /* tell the player what we're doing, then farm it off to the normal
     search routine */
  if(direction==2)
    TELLPLAYER(p, "EWe: Searching backwards for '%s'.\n", &(p->edit_info->last_search[1]));
  else if(direction==1)
    TELLPLAYER(p, "EWe: Searching forwards for '%s'.\n", &(p->edit_info->last_search[1]));
  else
    TELLPLAYER(p, "EWe: Searching from start for '%s'.\n", &(p->edit_info->last_search[1]));
    
  /* if our search was unsiccessful, reset to old current position.. */
  if(!ewe_find_next(p, direction)) {
    if(tmp)
      p->edit_info->current_line = tmp;
  }
}


/* EWE */
void	edit_search_next(player *p, char *str)
{
  editor_line *tmp;
  
  if(!p->edit_info->total_lines) {
    tell_player(p, "EWe: Buffer is empty.\n");
    return;
  }
  
  /* we're going to try... */
  tmp = p->edit_info->current_line;
  
  /* we want to skip the first one of course. */
  if(!ewe_find_next(p, 1)) {
    if(tmp)
      p->edit_info->current_line = tmp;
  }
}


/* EWE - put version to stack as a vers message.. */
void	ewe_version(void)
{
  sprintf(stack, " -=*> EWe v%s (by slaine) installed.\n", EWE_VERSION);
  stack = strchr(stack, 0);
}
  
