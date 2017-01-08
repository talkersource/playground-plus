/*  
 * Silvercode - intercom2.c
 * Additional intercom code for IChan.
 * ---------------------------------------------------------------------------  
 *   
 * IChan (intercom channel) is copyright (c) Richard Lawrence and Mike
 * Bourdaa. The intercom code is copyright (c) Michael Simms.
 * 
 * This code may not be redistributed in whole or part IN ANY FORM without
 * the prior permission of the author who can be contacted at
 * <silver@ewtoo.org>.
 * 
 *     [ This file should be included from intercom.c ]
 */

/* takes a message a user has said and sends it to the connected talkers */

static void parse_in_user_ichan_say_command(talker_list * remote_talker, char *str)
{     
  char *name, *msg = 0, *ptr, *oldstack;
  const char *method;
    
  name = str;
  if (name && *name)
  {
    msg = strchr(name, ':');
    if (msg)
      *msg++ = 0;
  }   
  if (!name || !*name || !msg || !*msg) 
  {
    log("intercom", "Invalid string in parse_in_user_ichan_say_command");
    return;  
  }
  oldstack = stack;

  ptr = (strchr(msg, 0)) - 1;
      
  switch (*ptr)
  {
    case '!':
      method = "exclaims";
      break;
    case '?':
      method = "asks";
      break;
    default:
      method = "says";
      break;   
  }
    
  sprintf(oldstack, "%s@%s %s '%s'", name, remote_talker->abbr, method, msg);
  stack = end_string(oldstack);
    
  send_to_talker("%c%s", ICHAN_MESSAGE, oldstack);
  stack = oldstack;
}

/* takes a users emote (which could also be a sing or think) and sends it
   to the connected talkers */

static void parse_in_user_ichan_emote_command(talker_list * remote_talker, char *str)
{
  char *name, *msg = 0, *oldstack;

  name = str;
  if (name && *name)
  {
    msg = strchr(name, ':');
    if (msg)
      *msg++ = 0;
  }
  if (!name || !*name || !msg || !*msg)
  {
    log("intercom", "Invalid string in parse_in_user_ichan_emote_command");
    return;
  }
  oldstack = stack;

  if (*msg == '\'')
    sprintf(oldstack, "%s@%s%s", name, remote_talker->abbr, msg);
  else
    sprintf(oldstack, "%s@%s %s", name, remote_talker->abbr, msg);

  stack = end_string(oldstack);

  send_to_talker("%c%s", ICHAN_MESSAGE, oldstack);

  stack = oldstack;
}

/* sends an action message (ie. logging on/off) to the intercom channel */

static void parse_in_user_ichan_action_command(talker_list * remote_talker, char *str)
{
  char *name, *msg = 0, *oldstack;

  name = str;
  if (name && *name)
  {
    msg = strchr(name, ':');
    if (msg)
      *msg++ = 0;
  }
  if (!name || !*name || !msg || !*msg)
  {
    log("intercom", "Invalid string in parse_in_user_ichan_action_command");
    return;
  }
  oldstack = stack;

  /* here we place any actions necessary -- please DON'T modify in order
     for them to stay consistent on every talker */

  if (!strcmp(msg, "join"))
    sprintf(stack, "++ %s@%s joins the channel ++", name, remote_talker->abbr);
  else if (!strcmp(msg, "leave"))
    sprintf(stack, "-- %s@%s leaves the channel --", name, remote_talker->abbr);
  else
  {
    sprintf(stack, "Illegal ichan action '%s' received from %s at '%s'", msg, name, remote_talker->name);
    stack = end_string(stack);
    log("intercom", oldstack);
    stack = oldstack;
    return;
  }

  stack = end_string(stack);

  send_to_talker("%c%s", ICHAN_MESSAGE, oldstack);

  stack = oldstack;
}

/* tell a person who is listening to the channel from other talkers */

static void return_ichan_who(talker_list * remote_talker, char *str)
{
  char *user_list = 0, *count_str = 0, *job_str;
  job_list *this_job; 
  char talker[100];
      
  job_str = str;
  if (job_str && *job_str) 
  {
    count_str = strchr(job_str, ':');
    if (count_str)
    {
      *count_str++ = 0;
      if (*count_str)
      {
        user_list = strchr(count_str, ':');
        if (user_list)
          *user_list++ = 0;
      }
    }
  }
  if (!user_list || !*user_list || !*count_str || !job_str || !*job_str)
  {
    log("intercom", "Bad string in return_ichan_who");
    return;
  }
  this_job = return_job(job_str);
          
  strcpy(current_name, this_job->sender);
   
  sprintf(talker, "%s (%s)", remote_talker->name, remote_talker->abbr);
  tell_personal("   %-30.30s : %s", talker, user_list);
}  

/* set up a job for the intercom channel who command "iw" */

static void do_ichan_who(talker_list * remote_talker, char *str)
{
  job_list *this_job;

  this_job = make_job_entry();
  this_job->job_ref = ATOI(str);
  strcpy(this_job->originator, remote_talker->abbr);
 
  send_to_talker("%c%ld", INTERCOM_ICHAN_WHO, this_job->job_id);
}

/* request a list of people listening to the intercom channel from other
   connected talkers */

static void request_ichan_list_global(void)
{
  talker_list *scan;
  job_list *this_job;

  this_job = make_job_entry();

  strcpy(this_job->sender, current_name);

  scan = talker_list_anchor;
  while (scan->next)
  {
    scan = scan->next;
    if (scan->fd > 0)
      tell_remote_talker(scan, "%c%ld", INTERCOM_ICHAN_WHO, this_job->job_id);
  }
}

/* intercom channel list processing */

static void reply_ichan_list_global(char *str)
{
  job_list *this_job;
  talker_list *remote_talker = 0;
  char *job_str, *msg = 0;

  job_str = str;
  if (job_str && *job_str)
  {
    msg = strchr(job_str, ':');
    if (msg)
      *msg++ = 0;
  }
  if (!job_str || !*job_str || !msg || !*msg)
  {
    log("intercom", "Invalid message sent to reply_ichan_list_global");
    return;
  }
  this_job = return_job(job_str);
  if (!this_job)
    return;
  if (this_job->originator[0])
    remote_talker = match_talker_abbr_absolute(this_job->originator);

  if (!remote_talker)
    return;

  tell_remote_talker(remote_talker, "%c%ld:%s", INTERCOM_ICHAN_LIST,
                     this_job->job_ref, msg);

  return;
}
