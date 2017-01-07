/*
 * Playground+ - dsc.c
 * Simple little version file so we have with compile time         ~ phypor
 * ---------------------------------------------------------------------------
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "include/config.h"
#include "include/player.h"
#include "include/proto.h"
#include "include/version.h"

/* --------------------------------------------------------------------------

   This is the Playground+ version command which may only be modified
   to list additional modules installed by yourself. The credits must
   be kept the same!

   This pg_version has been modified with permission.

   -------------------------------------------------------------------------- */

void pg_version(player * p, char *str)
{
  char *oldstack = stack;
  int ot;

  pstack_mid("Playground+ Version Information");

  sprintf(stack, "\nThis talker is based on Playground Plus by Silver (Richard Lawrence),\nblimey (Geoffrey Swift) and phypor (J. Bradley Christian), a stable\nbug"
	  "fixed and improved version of Playground 96 by traP (Mike Bourdaa),\nastyanax (Chris Allegretta), Nogard (Hans Peterson) and "
	  "vallie (Valerie Kelly)\nwhich is itself based on Summink by Athanasius (Neil Peter Charley)\nwhich itself was"
	  " based on EW-Too by Burble (Simon Marsh).\n\n");
  stack = strchr(stack, 0);

  sprintf(stack, " -=*> Using Playground+ v%s base code.\n", PG_VERSION);
  stack = strchr(stack, 0);

#ifdef LINUX
#ifdef REDHAT5
  sprintf(stack, " -=*> Playground+ running on Linux (glibc2).\n");
#else
  sprintf(stack, " -=*> Playground+ running on Linux (glibc1).\n");
#endif
#elif SOLARIS
  sprintf(stack, " -=*> Playground+ running on Solaris.\n");
#elif SUNOS
  sprintf(stack, " -=*> Playground+ running on SunOS.\n");
#elif HPUX
  sprintf(stack, " -=*> Playground+ running on HP-UX.\n");
#elif ULTRIX
  sprintf(stack, " -=*> Playground+ running on Ultrix.\n");
#elif BSDISH
  sprintf(stack, " -=*> Playground+ running on *BSD.\n");
#else
  sprintf(stack, " -=*> Playground+ running on Unknown O/S.\n");
#endif
  stack = strchr(stack, 0);

  sprintf(stack, " -=*> Executable compiled %s\n", COMPILE_TIME);
  stack = strchr(stack, 0);

  ot = atoi(get_config_msg("player_timeout"));
  if (!ot)
    stack += sprintf(stack, " -=*> Player timeouts disabled.\n");
  else
    stack += sprintf(stack, " -=*> Player timeouts enabled (%d weeks).\n", ot);


#ifdef INTERCOM
  pg_intercom_version();
#endif

  dynatext_version();

  nunews_version();

#ifdef ROBOTS
  robot_version();
#endif

  sprintf(stack, " -=*> Auto reloading v1.0 (by phypor) enabled.\n");
  stack = strchr(stack, 0);


#ifdef ANTICRASH
  crashrec_version();
#endif

  if (config_flags & cfNOSPAM)
  {
    sprintf(stack, " -=*> Anti-spam code v3.0 (by Silver) enabled.\n");
    stack = strchr(stack, 0);
  }

  channel_version();

#ifdef COMMAND_STATS
  command_stats_version();
#endif

  slots_version();

#ifdef AUTOSHUTDOWN
  sprintf(stack, " -=*> Auto-shutdown code v1.0 (by Silver) enabled.\n");
  stack = strchr(stack, 0);
#endif

#ifdef LAST
  last_version();
#endif

#ifdef NEWPAGER
  sprintf(stack, " -=*> New pager code v1.1 (by Nightwolf) enabled.\n");
  stack = strchr(stack, 0);
#endif

  spodlist_version();

#ifdef SEAMLESS_REBOOT
  reboot_version();
#endif

  socials_version();

#ifdef IDENT
  ident_version();
#endif

  if (config_flags & cfNOSWEAR)
    swear_version();

#ifdef ALLOW_MULTIS
  multis_version();
#endif

  softmsg_version();

/* A warning that people are using debugging mode. This means sysops can
   slap silly people who use this mode in live usage -- Silver */

#ifdef DEBUGGING
#ifdef DEBUG_VERBOSE
  stack += sprintf(stack, "\n    -=*> WARNING! Talk server running in verbose ");
#else
  stack += sprintf(stack, "\n        -=*> WARNING! Talk server running in ");
#endif
  stack += sprintf(stack, "debugging mode! <*=-\n\n");
#endif

  stack = strchr(stack, 0);
  pstack_mid("Playground+ Homepage: http://pgplus.ewtoo.org");
  *stack++ = 0;

  if (p->location)
    pager(p, oldstack);
  else
    tell_player(p, oldstack);

  stack = oldstack;
}

/* net stats */

void netstat(player * p, char *str)
{
  char *oldstack = stack;
  FILE *fp;
  int pid1 = -1, pid2 = -1;

  pstack_mid("Network and server statistics");

  stack += sprintf(stack, "                           In       Out\n");

  stack += sprintf(stack, "Total bytes        : %8d  %8d\n"
		   "Average bytes      : %8d  %8d\n"
		   "Bytes per second   : %8d  %8d\n"
		   "Total packets      : %8d  %8d\n"
		   "Average packets    : %8d  %8d\n"
		   "Packets per second : %8d  %8d\n\n",
	      in_total, out_total, in_average, out_average, in_bps, out_bps,
		   in_pack_total, out_pack_total, in_pack_average,
		   out_pack_average, in_pps, out_pps);

  stack += sprintf(stack, "Talker name        : %s\n", get_config_msg("talker_name"));
  stack += sprintf(stack, "Site address       : %s (%s)\n", talker_alpha, talker_ip);
#ifdef INTERCOM
  stack += sprintf(stack, "Port number        : %d (talk server), %d (intercom server)\n", active_port, active_port - 1);
#else
  stack += sprintf(stack, "Port number        : %d\n", active_port);
#endif
  stack += sprintf(stack, "Base code revision : v%s\n", PG_VERSION);
  stack += sprintf(stack, "Talker root dir    : %s\n", ROOT);
  stack += sprintf(stack, "Compilation date   : %s\n", COMPILE_TIME);
  stack += sprintf(stack, "Process ID's       : ");

  /* Now we need to get the PIDS */

  fp = fopen("junk/PID", "r");
  if (fp)
  {
    fscanf(fp, "%d", &pid1);
    fclose(fp);
    stack += sprintf(stack, "%d (talk server)", pid1);
  }
  else
    stack += sprintf(stack, "unknown talk server PID");


  fp = fopen("junk/ANGEL_PID", "r");
  if (fp)
  {
    fscanf(fp, "%d", &pid2);
    fclose(fp);
    if (pid1 != -1)
      stack += sprintf(stack, ", ");
    stack += sprintf(stack, "%d (guardian angel)", pid2);
  }
  else
  {
    if (pid1 != -1)
      stack += sprintf(stack, ", ");
    stack += sprintf(stack, "unknown guardian angel PID");
  }

  stack += sprintf(stack, "\n%s", LINE);
  stack = end_string(stack);
  tell_player(p, oldstack);
  stack = oldstack;
}
