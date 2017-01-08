/*
 * Silvercode - channel2.c
 * Additional channel code for IChan.
 * ---------------------------------------------------------------------------
 *
 * IChan (intercom channel) is copyright (c) Richard Lawrence and Mike
 * Bourdaa. The intercom code is copyright (c) Michael Simms.
 *
 * This code may not be redistributed in whole or part IN ANY FORM without
 * the prior permission of the author who can be contacted at
 * <silver@ewtoo.org>.
 *
 *     [ This file should be included from channel.c ]
 */

/* sends a message to all people seeing the intercom channel */

void ichan_wall(char *str)
{
  player *scan;

  if (intercom_fd < 1) /* intercom is down */
    return;

  for (scan = flatlist_start; scan; scan = scan->flat_next)
  {
    if (scan->residency && scan->location && !(scan->misc_flags & NO_INTERCOM_CHANNEL))
    {
      if (scan->misc_flags & CHAN_HI)
        command_type |= HIGHLIGHT;

      sys_color_atm = SYSc; /* if you have a colour available, put it here */
      tell_player(scan, str);
      sys_color_atm = SYSc;

      if (scan->misc_flags & CHAN_HI)
        command_type &= ~HIGHLIGHT;
    }
  }
}

/* intercom channel text processing */

void i_chan(char *format,...)
{
  va_list x;
  va_start(x, format);
  any_chan(get_config_msg("intercom_chan"), ichan_wall, format, x);
  va_end(x);
}

/* intercom channel "say" command */

void iu(player * p, char *str)
{
  if (check_intercom_banished_name(p->lower_name))
  {
    tell_player(p, " You have been prevented from using the intercom.\n");
    return;
  }

  if (got_msg(p, str, "iu") && is_on_channel(p, NO_INTERCOM_CHANNEL))
  {
    if (intercom_fd < 1)
    {
      tell_player(p, " The intercom is currently down.\n");
      return;
    }
    intercom_channel_say(p, str);
    channel_say(p, str, i_chan);
  }
}

/* intercom channel "emote" command */

void ie(player * p, char *str)
{
  if (check_intercom_banished_name(p->lower_name))
  {
    tell_player(p, " You have been prevented from using the intercom.\n");
    return;
  }

  if (got_msg(p, str, "ie") && is_on_channel(p, NO_INTERCOM_CHANNEL))
  {
    if (intercom_fd < 1)
    {
      tell_player(p, " The intercom is currently down.\n");
      return;
    }
    intercom_channel_emote(p, str);
    channel_emote(p, str, i_chan);
  }
}

/* intercom channel "sing" command. Text is pre-processed and then
   sent to 'ie' */

void is(player *p, char *str)
{
  char *oldstack = stack;

  if (got_msg(p, str, "is") && is_on_channel(p, NO_INTERCOM_CHANNEL))
  {
    sprintf(stack, "sings o/~ %s^N o/~", str);
    stack = end_string(stack);
    ie(p, oldstack);
    stack = oldstack;
  }
}

/* intercom channel "think" command. Text is pre-processed and then
   sent to 'ie' */

void it(player *p, char *str)
{
  char *oldstack = stack;

  if (got_msg(p, str, "it") && is_on_channel(p, NO_INTERCOM_CHANNEL))
  {
    sprintf(stack, "thinks . o O ( %s^N )", str);
    stack = end_string(stack);
    ie(p, oldstack);
    stack = oldstack;
  }
}

/* the "iw" command */

void i_who(player * p, char *str)
{
  player *scan;   
  char *oldstack = stack;
  int flag = 0, length = 26;

  if (intercom_fd < 1)
  {
    tell_player(p, " The intercom is currently down.\n");
    return;
  }
 
  sprintf(stack, " People listening to the intercom channel (ichan):\n");
  stack = strchr(stack, 0);

  for (scan = flatlist_start; scan; scan = scan->flat_next)
#ifdef ROBOTS
    if (scan->residency && scan->location && !(scan->misc_flags & NO_INTERCOM_CHANNEL) && !(scan->residency & ROBOT_PRIV))
#else
    if (scan->residency && scan->location && !(scan->misc_flags & NO_INTERCOM_CHANNEL))
#endif
    {
      if (!flag)
      {
        sprintf(stack, "   %-30.30s : ", get_config_msg("talker_name"));
        stack = strchr(stack, 0);
      }
      if ((length + strlen(scan->name)) > 70)
      {
        sprintf(stack, "\n                                    ");
        stack = strchr(stack, 0);
        length = 36;
      }
      sprintf(stack, "%s, ", scan->name);
      stack = strchr(stack, 0);
      flag = 1;
      length += (strlen(scan->name)+2);
    }
                 
  if (flag)
  {
    stack -= 2;
    *stack++ = '.';
    *stack++ = '\n';
  }

  stack = end_string(stack);   
  tell_player(p, oldstack);
  stack = oldstack;
                 
  /* get the other talkers */
   
  intercom_ichan_who(p);
}

