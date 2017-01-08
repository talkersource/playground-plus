
/*
 * ident_client.c
 * Ident client for rfc1413 lookups
 * ---------------------------------------------------------------------------
 *
 * The majority of the code contained herein is Copyright 1995/1996 by
 * Neil Peter Charley.
 * Portions of the code (notably that for Solaris 2.x and BSD compatibility)
 * are Copyright 1996 James William Hawtin. Thanks also goes to James
 * for pointing out ways to make the code more robust.
 * ALL code contained herein is covered by one or the other of the above
 * Copyright's.
 *
 *     Distributed as part of Playground+ package with permission.
 */

char identclientc_rcsid[] = "$Revision: 1.5 $";

#if defined(__STRICT_ANSI__)
 #include <features.h>
 #define __USE_POSIX
 #define __USE_BSD
 #define __USE_MISC 
#endif /* __STRICT_ANSI__ */

#include <stdlib.h>
#include <stdio.h>
#if !defined(LOTHLORIEN)
 #include <stdarg.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(HAVE_FILIOH)
 #include <sys/filio.h>
#endif /* HAVE_FILIOH */
#if !defined(OSF)
 #include <sys/wait.h>
#endif /* !OSF */

#if defined(NETBSD) || defined(BSDISH)
 #include <sys/socket.h>
#endif

#include "include/config.h"
#include "include/player.h"
#include "include/fix.h"
#include "include/proto.h"

#if defined(BSD3) || defined(BSDISH)
  #include <sys/un.h>
#else
  #ifdef LINUX
    #include "include/un.h"
  #else
    #ifdef NETBSD
      #include <sys/un.h>
    #else     
      #include <un.h>
    #endif
  #endif
#endif

#ifdef IDENT

#include "include/ident.h"


#if defined(ANSI)
 extern int kill __P ((__pid_t __pid, int __sig));
 extern void bzero __P ((__ptr_t __s, size_t __n));
#endif

/* This is an UGLY hack, 'cos the gcc setup on rabbit precludes the
 * #include'ing of both <sys/wait.h> and <sys/time.h>
 * The lines below (inside the #if defined) are from <sys/wait.h> on rabbit
 */
#if defined(OSF)
/* WNOHANG causes the wait to not hang if there are no stopped or terminated */
#define WNOHANG         0x1     /* dont hang in wait                     */
#endif /* OSF */

/* Extern functions */

extern void log(char *, char *);

/* Extern variables */

#ifndef errno
extern int		 errno;
#endif
#if !defined(LOTHLORIEN)
extern player		*flatlist_start;
#endif /* !LOTHLORIEN */

/* Local functions */

void			 setup_itimer(void);
void			 ident_process_reply(int msg_size);

/* Local Variables */

#define BUFFER_SIZE 2048      /* Significantly bigger than the
										 * equivalent in ident_server.c
					 */
int			 ident_toclient_fds[2];
int			 ident_toserver_fds[2];
int			 ident_server_pid = 0;
ident_identifier	 ident_id = 0;
char			 ident_buf_input[BUFFER_SIZE];
char			 ident_buf_output[BUFFER_SIZE];
char			 reply_buf[BUFFER_SIZE];

/*
 * Start up the ident server
 */

int init_ident_server(void)
{
	int ret;
#if defined(FIONBIO)
	int dummy;
#endif /* FIONBIO */
	fd_set fds;
	struct timeval timeout;
        char namebuffer[256];

        sprintf(namebuffer, "-=> %s <=- Ident server",
          get_config_msg("talker_name"));

	if (-1 == pipe(ident_toclient_fds))
	{
		switch (errno)
		{
			case EMFILE:
				log("boot", "init_ident_server: Too many fd's in use by"
		 		" process");
				exit(1);
			case ENFILE:
				log("boot", "init_ident_server: Too many fd's in use in"
		 		" system");
				exit(1);
			case EFAULT:
				log("boot", "init_ident_server: ident_toclient_fds invalid!");
				exit(1);
		}
	}
	if (-1 == pipe(ident_toserver_fds))
	{
		switch (errno)
		{
			case EMFILE:
				log("boot", "init_ident_server: Too many fd's in use by"
		 		" process");
				exit(1);
			case ENFILE:
				log("boot", "init_ident_server: Too many fd's in use in"
		 		" system");
				exit(1);
			case EFAULT:
				log("boot", "init_ident_server: ident_toserver_fds invalid!");
				exit(1);
		}
	}

	ret = fork();
	switch (ret)
	{
		case -1: /* Error */
			log("boot", "init_ident_server couldn't fork!");
			exit(1);
		case 0:   /* Child */
			close(IDENT_CLIENT_READ);
			close(IDENT_CLIENT_WRITE);
			close(0);
			dup(IDENT_SERVER_READ);
			close(IDENT_SERVER_READ);
			close(1);
			dup(IDENT_SERVER_WRITE);
			close(IDENT_SERVER_WRITE);
			execlp("bin/ident_server", namebuffer, 0);
			log("boot", "init_ident_server failed to exec ident_server");
			exit(1);
		default: /* Parent */
			ident_server_pid = ret;
			close(IDENT_SERVER_READ);
			close(IDENT_SERVER_WRITE);
			IDENT_SERVER_READ = IDENT_SERVER_WRITE = -1;
#if defined(FIONBIO)
	 if (ioctl(IDENT_CLIENT_READ, FIONBIO, &dummy) < 0)
	 {
		log("error", "Ack! Can't set non-blocking to ident read.");
	 }
	 if (ioctl(IDENT_CLIENT_WRITE, FIONBIO, &dummy) < 0)
	 {
		log("error", "Ack! Can't set non-blocking to ident write.");
	 }
#endif /* FIONBIO */
	}

	FD_ZERO(&fds);
	FD_SET(IDENT_CLIENT_READ, &fds);
	timeout.tv_sec = 15;
	timeout.tv_usec = 0;
	while (-1 == (ret = select(FD_SETSIZE, &fds, 0, 0, &timeout)))
	{
		if (errno == EINTR || errno == EAGAIN)
		{
			continue;
		}
		log("boot", "init_ident_server: Timed out waiting for server"
				  " connect");
		kill_ident_server();
		return 0;
	}

	ioctl(IDENT_CLIENT_READ, FIONREAD, &ret);
	while (ret != strlen(IDENT_SERVER_CONNECT_MSG))
	{
		sleep(1);
		ioctl(IDENT_CLIENT_READ, FIONREAD, &ret);
	}
	ret = read(IDENT_CLIENT_READ, ident_buf_input, ret);
	ident_buf_input[ret] = '\0';
	if (strcmp(ident_buf_input, IDENT_SERVER_CONNECT_MSG))
	{
		fprintf(stderr, "From Ident: '%s'\n", ident_buf_input);
		log("boot", "init_ident_server: Bad connect from server, killing");
		kill_ident_server();
		return 0;
	}
	log("boot", "Ident Server Up and Running");

	setup_itimer();
	return 1;
}

/* Shutdown the ident server */

void kill_ident_server(void)
{
	int status;

	close(IDENT_CLIENT_READ);
	close(IDENT_CLIENT_WRITE);
	IDENT_CLIENT_READ = -1;
	IDENT_CLIENT_WRITE = -1;
	kill(ident_server_pid, SIGTERM);
	waitpid(-1, &status, WNOHANG);
}

void send_ident_request(player *p, struct sockaddr_in *sadd)
{
	char *s;
	int bwritten;
	struct sockaddr_in sname;
#if defined(__GLIBC__) || (__GLIBC__ >= 2)
	socklen_t l;
#else
	int l;
#endif

	l = sizeof(struct sockaddr_in);
	getsockname(p->fd, (struct sockaddr *)&sname, &l);
#if defined(DEBUG_IDENT_TOO)
fprintf(stderr, "send_ident_request: sockname = '%s'\n",
	inet_ntoa(sname.sin_addr));
#endif /* DEBUG_IDENT_TOO */

	s = ident_buf_output;
	*s++ = IDENT_CLIENT_SEND_REQUEST;
	memcpy(s, &ident_id, sizeof(ident_id));
	s += sizeof(ident_id);
	memcpy(s, &(sname.sin_addr.s_addr), sizeof(sname.sin_addr.s_addr));
	s += sizeof(sname.sin_addr.s_addr);
	memcpy(s, &(sname.sin_port), sizeof(sname.sin_port));
	s += sizeof(sname.sin_port);
	memcpy(s, &(sadd->sin_family), sizeof(sadd->sin_family));
	s += sizeof(sadd->sin_family);
	memcpy(s, &(sadd->sin_addr.s_addr), sizeof(sadd->sin_addr.s_addr));
	s += sizeof(sadd->sin_addr.s_addr);
	memcpy(s, &(sadd->sin_port), sizeof(sadd->sin_port));
	s += sizeof(sadd->sin_port);
	bwritten = write(IDENT_CLIENT_WRITE, ident_buf_output,
						  (s - ident_buf_output));
	p->ident_id = ident_id++;
	if (bwritten < (s - ident_buf_output))
	{
		log("ident", "Client failed to write request, killing and restarting"
					" Server");
		kill_ident_server();
		/* FIXME: Maybe test for presence of PID file? */
		sleep(3);
		init_ident_server();
		bwritten = write(IDENT_CLIENT_WRITE, ident_buf_output,
							  (s - ident_buf_output));
		if (bwritten < (s - ident_buf_output))
		{
			log("ident", "Restart failed");
		}
	} else
	{
#if defined(DEBUG_IDENT)
		stdarg_log("ident_ids", "Player '%s', fd %d, ident_id %d",
			 p->name[0] ? p->name : "<NOT-ENTERED>",
	  p->fd,
	  p->ident_id);
#endif /* DEBUG_IDENT */
	}
	
#if defined(DEBUG_IDENT)
	fprintf(stderr, "Bytes Written %d, Should have sent %d\n",
		bwritten, (s - ident_buf_output)); 
	fprintf(stderr, "Client: %08X:%d\n",
			  (int) ntohl(sadd->sin_addr.s_addr),
			  ntohs(sadd->sin_port));
	fflush(stderr);
#endif
}

void read_ident_reply(void)
{
	int bread;
	int toread;
	int i;
	static int bufpos = 0;

	ioctl(IDENT_CLIENT_READ, FIONREAD, &toread);
	if (toread <= 0)
	{
		return;
	}
	bread = read(IDENT_CLIENT_READ, ident_buf_input, BUFFER_SIZE - 20);
	ident_buf_input[bread] = '\0';

	for (i = 0 ; i < bread ; )
	{
		reply_buf[bufpos++] = ident_buf_input[i++];
		if ((bufpos > (sizeof(char) + sizeof(ident_identifier)))
			 && (reply_buf[bufpos - 1] == '\n'))
		{
	 ident_process_reply(bufpos);
			bufpos = 0;
#if defined(HAVE_BZERO)
	 bzero(reply_buf, BUFFER_SIZE);
#else
	 memset(reply_buf, 0, BUFFER_SIZE);
#endif /* HAVE_BZERO */
		}
	}
}

void ident_process_reply(int msg_size)
{
	char *s, *t;
	char reply[MAX_REMOTE_USER + 1];
	int i;
	ident_identifier id;
#if defined(LOTHLORIEN)
	connection *c;
	#define FORC(c) for (c = firstc ; c ; c = c->next)
#else /* !LOTHLORIEN */
	player *c;
	#define FORC(c) for (c = flatlist_start ; c ; c = c->flat_next)
#endif /* LOTHLORIEN */

	for (i = 0 ; i < msg_size ;)
	{
		switch (reply_buf[i++])
		{
			case IDENT_SERVER_SEND_REPLY:
				memcpy(&id, &reply_buf[i], sizeof(ident_identifier));
				i += sizeof(ident_identifier);
#if defined(DEBUG_IDENT)
		 stdarg_log("ident_ids", "Got reply for ident_id %d", 
			  id);
#endif /* DEBUG_IDENT */
				FORC(c)
				{
					if (c->ident_id == id)
					{
#if defined(DEBUG_IDENT)
		  stdarg_log("ident_ids", "Matched ident_id %d to Player '%s',"
		  		   " fd %d",
				id,
				c->name[0] ? c->name : "<NOT-ENTERED>",
				c->fd);
#endif /* DEBUG_IDENT */
						break;
					}
				}
#if defined(DEBUG_IDENT)
		 fprintf(stderr, "Ident Client: Got reply '%s'\n", &reply_buf[i]);
#endif /* DEBUG_IDENT */
				s = strchr(&reply_buf[i], '\n');
				if (s)
				{
					*s++ = '\0';
				} else
				{
			 s = strchr(reply_buf, '\0');
			 *s++ = '\n';
			 *s = '\0';
				}
		 if (c)
		 {
				  /* *sigh*, some git might set up an identd that sends crap
					* stuff back, so we need to filter the reply
					* RFC1413 does state that the 'result' should consist
					* of printable characters:

						  "Returned user identifiers are expected to be printable
								 in the character set indicated."

					* Default character set is US-ASCII, and we're not catering
					* for anything else, at least not yet.
					*/
#if defined(LOTHLORIEN)
			 FREE(c->remote_user);
			 c->remote_user = CALLOC(1, strlen(&reply_buf[i]) + 1);
			 s = &reply_buf[i];
			 t = c->remote_user;
			 while (*s)
#else 
			 s = &reply_buf[i];
			 t = reply;
			 while (*s && t < &reply[MAX_REMOTE_USER])
#endif /* LOTHLORIEN */
			 {
						if (isprint(*s))
						{
								*t++ = *s++;
						} else
						{
								s++;
						}
			 }
			 *t = '\0';
#if !defined(LOTHLORIEN)
					strncpy(c->remote_user, reply, MAX_REMOTE_USER);
#endif /* !LOTHLORIEN */
#if defined(DEBUG_IDENT)
			 stdarg_log("ident_ids", "Write ident_id %d, Reply '%s' to player '%s'"
			 		        " fd %d",
			id,
			c->remote_user,
			c->name[0] ? c->name : "<NOT-ENTERED>",
			c->fd);
#endif /* DEBUG_IDENT */
				} else
				{
					/* Can only assume connection dropped from here and we still
					 * somehow got a reply, throw it away
					 */
#if defined(DEBUG_IDENT)
			 stdarg_log("ident_ids", "Threw away response for ident_id %d",
				  id);
#endif /* DEBUG_IDENT */
				}
		 while (reply_buf[i] != '\n')
		 {
			 i++;
		 }
				break;
			default:
#if defined(DEBUG_IDENT_TOO)
		 stdarg_log("ident", "Bad reply from server '%d'", reply_buf[i]);
#endif /* DEBUG_IDENT_TOO */
				i++;
		}
	}
}


/* log using stdarg variable length arguments */

void stdarg_log(char *str, ...)
{
	va_list args;
	FILE *fp;
	char *fmt;
	struct tm *tim;
	char the_time[256];
	char fname[21];
	time_t current_time;

	va_start(args, str);

	current_time = time(0);
	tim = localtime(&current_time);
	strftime((char *)&the_time, 255, "%H:%M:%S %Z - %d/%m/%Y - ", tim);
	sprintf(fname, "logs/%s.log", str);

	fp = fopen(fname, "a");
	if (!fp)
	{
		fprintf(stderr, "Eek! Can't open file to log: %s\n", fname);
		return;
	}
	fprintf(fp, "%s", the_time);
	fmt = va_arg(args, char *);
	vfprintf(fp, fmt, args);
	va_end(args);
	fprintf(fp, "\n");
	fclose(fp);
}

void ident_version(void)
{
  sprintf(stack, " -=*> Ident server v1.10 (by Athanasius and Oolon) enabled.\n");
  stack = strchr(stack, 0);
}
#endif
