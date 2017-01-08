/*  
 * Silvercode - intercom_glue2.c
 * Additional intercom glue code for IChan.
 * ---------------------------------------------------------------------------  
 *   
 * IChan (intercom channel) is copyright (c) Richard Lawrence and Mike
 * Bourdaa. The intercom code is copyright (c) Michael Simms.
 * 
 * This code may not be redistributed in whole or part IN ANY FORM without
 * the prior permission of the author who can be contacted at
 * <silver@ewtoo.org>.
 * 
 *     [ This file should be included from intercom_glue.c ]
 */

extern void i_chan(char *);

/* send a "say" to the intercom to send to the other connected talkers */

void intercom_channel_say(player *p, char *str)
{
  send_to_intercom(p, "%c%c%s:x:x:%s", USER_COMMAND, ICHAN_SAY,
                   p->name, str);
}

/* send an "emote" to the intercom to send to the other connected talkers */

void intercom_channel_emote(player *p, char *str)
{
  send_to_intercom(p, "%c%c%s:x:x:%s", USER_COMMAND, ICHAN_EMOTE,
                   p->name, str);
}

/* send an "action" to the intercom to send to the other connected talkers
   (an action being logging on or off) */

void intercom_channel_action(player *p, char *str)
{
  if (!(p->residency))
    return;

#ifdef ROBOTS
  if (p->residency & ROBOT_PRIV)
    return;
#endif

  if (p->location || !strcasecmp(str, "login"))
    send_to_intercom(p, "%c%c%s:x:x:%s", USER_COMMAND, ICHAN_ACTION,
                     p->name, str);
}                                             

/* tells the intercom channel on this talker our message
   (v2.1) Won't allow banished names to use the ichan to send messages
   to the talker that banished them from another site (by Infinity)
*/

static void tell_intercom_channel(char *str)
{
  char *oldstack = stack;
  char  name[250] = "", *lengthname;
  int namelen = 0;

  lengthname = strchr(str,'@');
  namelen = strlen(lengthname);

  if (namelen > 0)
  {
    strncpy(name, str, (strlen(str)-namelen) );
  }

  lower_case(name);
  if (check_intercom_banished_name(name))
  {
    send_to_intercom(NULL,"%c%c:%s",REPLY_IS,NAME_BANISHED,name);
    return;
  }

  sprintf(stack, "%s^N\n", str);
  stack = end_string(oldstack);

  i_chan(oldstack);
  stack = oldstack;
}

/* send the request to the intercom to ask all other connected talkers for
   their list of people on the intercom channel -phew! */

void intercom_ichan_who(player * p)
{
  send_to_intercom(p, "%c%s:", INTERCOM_ICHAN_WHO, p->name);
  return;
}

/* reply to the request for the list of people on the intercom channel */

static void return_ppl_on_ichan(char *str)
{
  player *scan;
  int count = 0, length = 26;
  char *oldstack = stack;

  for (scan = flatlist_start; scan; scan = scan->flat_next)
  {
#ifdef ROBOTS
    if (scan->location && scan->residency && !(scan->misc_flags & NO_INTERCOM_CHANNEL) && !(scan->residency & ROBOT_PRIV))
#else
    if (scan->location && scan->residency && !(scan->misc_flags & NO_INTERCOM_CHANNEL))
#endif
    {
      if ((length + strlen(scan->name)) > 70)
      {
        sprintf(stack, "\n                                    ");
        stack = strchr(stack, 0);
        length = 36;
      }
      sprintf(stack, "%s, ", scan->name);
      stack = strchr(stack, 0);
      length += (strlen(scan->name)+2);
      count++;
    }
  }

  if (count == 0)  /* no-one on the channel */
    return;

  stack -= 2;
  *stack++ = '.';
  *stack++ = 0;

  send_to_intercom(NULL, "%c%s:%d:%s", INTERCOM_ICHAN_LIST, str, count, oldstack);
  stack = oldstack;
}

/* version information -- to be included in plists.c */

void ichan_version(void)
{
  sprintf(stack, " -=*> IChan v2.1.1 (by Silver/traP) installed.\n");
  stack = strchr(stack, 0);
}
