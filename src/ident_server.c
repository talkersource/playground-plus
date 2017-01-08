/*
 * ident_server.c
 * Ident server for rfc1413 lookups
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
 *       Distributed as part of Playground+ package with permission.
 */

char identserverc_rscid[] = "$Revision: 1.6 $";

#if defined(__STRICT_ANSI__)
 #include <features.h>
 #define __USE_POSIX
 #define __USE_BSD
 #define __USE_MISC 
#endif /* __STRICT_ANSI__ */

#include <unistd.h>
#if defined(HAVE_FILIOH)
 #include <sys/filio.h>
#endif /* HAVE_FILIOH */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/resource.h>

#include "include/config.h"
#include "include/player.h"
#include "include/fix.h"
#include "include/proto.h"
#include "include/ident.h"

/* Extern Function Prototypes */

/* Extern Variables */

#ifndef errno
extern int errno;
#endif

#if !defined(OSF)
 #if !defined(NetBSD) && defined(BSD) /* Check for NetBSD; prevent compilation error -- blimey */
  /* Missing BSD PROTOTYPES */
  extern int             setrlimit(int, struct rlimit *);
  extern int             getrlimit(int, struct rlimit *);
  extern int             setitimer(int, struct itimerval *, struct itimerval *);
  extern int             fclose();
  extern int             select();
  extern time_t          time(time_t *);
  extern int             connect();
  extern int             shutdown(int, int);
  extern int             socket(int, int, int);
  extern int             strftime(char *, int, char *, struct tm *);
  extern void            perror(char *);
  extern int             fflush();
  extern int             ioctl();
  extern int             fprintf();
  extern void            bzero(void *, int);
  extern void            bcopy(char *, char *, int);
  extern int             vfprintf(FILE *stream, char *format, ...);
 #endif /* BSD */
#else /* OSF */
 extern int              ioctl(int d, unsigned long request, void * arg);
#endif /* !OSF */

#if !defined(MALLOC)
 #define MALLOC(c) malloc(c)
#endif /* MALLOC */

#if !defined(FREE)
 #define FREE(c) free(c)
#endif /* FREE */


/* Local Function Prototypes */

void                     catch_sigterm(SIGTYPE);
void                     check_connections(void);
void                     check_requests(void);
void                     closedown_request(int slot);
void                     do_request(ident_request *id_req);
void                     logid(char *,...);  
void                     process_request(int slot);
void                     process_result(int slot);
void                     queue_request(ident_identifier id,
	struct sockaddr_in *local_sockadd, struct sockaddr_in *sockadd);
void                     take_off_queue(int freeslot);
void                     write_request(ident_request *id_req);

/* Local Variables */

#define BUFFER_SIZE 1024
char                     scratch[4096];
char                     req_buf[BUFFER_SIZE];
ident_request            idents_in_progress[MAX_IDENTS_IN_PROGRESS];
ident_request           *first_inq = NULL;
ident_request           *last_inq = NULL;
int                      debug = 0;
int                      status = 0;
int                      beats_per_second;
int                      req_size;

#define IDENT_STATUS_RUNNING 1
#define IDENT_STATUS_SHUTDOWN 2

int main(int argc, char *argv[])
{
	struct rlimit        rlp;
#if defined(USE_SIGACTION)
	struct sigaction     sigact;
#endif
	struct sockaddr_in   sadd;
	fd_set               fds_write;
	fd_set               fds_read;
	int                  i;
	struct timeval       timeout;
	time_t               now;

	getrlimit(RLIMIT_NOFILE, &rlp);
	rlp.rlim_cur = rlp.rlim_max;
	setrlimit(RLIMIT_NOFILE, &rlp);

	/* First things first, let the client know we're alive */
	write(1, IDENT_SERVER_CONNECT_MSG, strlen(IDENT_SERVER_CONNECT_MSG));

	/* Need this to keep in sync with the client side
	 *
	 * <start><ident_id><local_port><sin_family><s_addr><sin_port>
	 */
	req_size = sizeof(char) + sizeof(ident_identifier)
                            + sizeof(sadd.sin_addr.s_addr)
                            + sizeof(sadd.sin_port)
                            + sizeof(sadd.sin_family)
							+ sizeof(sadd.sin_addr.s_addr)
							+ sizeof(sadd.sin_port);

#if defined(DEBUG_IDENT)
	logid("ident", "Ident message size = %d", req_size);
#endif
	/* Set up signal handling */
#if defined(USE_SIGACTION)
 #if defined(HAVE_SIGEMPTYSET)
	sigemptyset(&(sigact.sa_mask));
 #else
	sigact.sa_mask = 0;
 #endif /* HAVE_SIGEMPTYSET */
 #if defined(__linux__)
	sigact.sa_restorer = (void *)NULL;
 #endif
	sigact.sa_handler = catch_sigterm;
	sigaction(SIGTERM, &sigact, (struct sigaction *)NULL);
	sigact.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigact, (struct sigaction *)NULL);
#else /* !USE_SIGACTION */
	signal(SIGTERM, catch_sigterm);
	signal(SIGPIPE, SIG_IGN);
#endif /* USE_SIGACTION */

#if defined(HAVE_BZERO)
	bzero(&idents_in_progress[0], MAX_IDENTS_IN_PROGRESS
					* sizeof(ident_request));
#else /* !HAVE_BZERO */
	memset(&idents_in_progress[0], 0,
		  MAX_IDENTS_IN_PROGRESS * sizeof(ident_request));
#endif /* HAVE_BZERO */
	for (i = 0 ; i < MAX_IDENTS_IN_PROGRESS ; i++)
	{
		idents_in_progress[i].fd = -1;
	}
	/* Now enter the main loop */
	status = IDENT_STATUS_RUNNING;
	while (status != IDENT_STATUS_SHUTDOWN)
	{
		if (1 == getppid())
		{
		/* If our parent is now PID 1 (init) the talker must have died without
		 * killing us, so we have no business still being here
		 *
		 * time to die...
		 */
			exit(0);
		}

		FD_ZERO(&fds_write);   /* These are for connection being established */
		FD_ZERO(&fds_read);    /* These are for a reply being ready */

		FD_SET(0, &fds_read);
		for (i = 0 ; i < MAX_IDENTS_IN_PROGRESS ; i++)
		{
			if (idents_in_progress[i].fd > 0)
			{
				if (idents_in_progress[i].flags & IDENT_CONNREFUSED)
				{
					process_result(i);
				} else if (!(idents_in_progress[i].flags & IDENT_CONNECTED))
				{
					FD_SET(idents_in_progress[i].fd, &fds_write);
				} else
				{
					FD_SET(idents_in_progress[i].fd, &fds_read);
				}
			} else
			{
				/* Free slot, so lets try to fill it */
				take_off_queue(i);
			}
		}

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		i = select(FD_SETSIZE, &fds_read, &fds_write, 0, &timeout);
		switch (i)
		{
		case -1:
#if defined(DEBUG_IDENT_TOO)
			 logid("ident", "ident: select failed: %s\n", strerror(errno));
#endif /* DEBUG_IDENT_TOO */
			break;
		case 0:
			break;
		default:
			for (i = 0 ; i < MAX_IDENTS_IN_PROGRESS ; i++)
			{
				if (idents_in_progress[i].fd == -1)
				{
					continue;
				}
				if (FD_ISSET(idents_in_progress[i].fd, &fds_write))
				{
					/* Has now connected, so send request */
					idents_in_progress[i].flags |= IDENT_CONNECTED;
					write_request(&idents_in_progress[i]);
				} else if (FD_ISSET(idents_in_progress[i].fd, &fds_read))
				{
					/* Reply is ready, so process it */
					idents_in_progress[i].flags |= IDENT_REPLY_READY;
					process_result(i);
				}
			}
			if (FD_ISSET(0, &fds_read))
			{
				check_requests();
			}
		}

		now = time(NULL);
		for (i = 0 ; i < MAX_IDENTS_IN_PROGRESS ; i++)
		{
			if (idents_in_progress[i].fd > 0)
			{
				if (now > (idents_in_progress[i].request_time + IDENT_TIMEOUT))
				{
					/* Request has timed out, whether on connect or reply */
					idents_in_progress[i].flags |= IDENT_TIMEDOUT;
					process_result(i);
				}
			}
		}
	}

	return 0;
}

/* Catch a SIGTERM, this means to shutdown */

void catch_sigterm(SIGTYPE)
{
	status = IDENT_STATUS_SHUTDOWN;
}

/* Check for requests from the client */

void check_requests(void)
{
	char                 msgbuf[129];
	int                  toread;
	int                  bread;
	static int           bufpos = 0;
	int                  i;

	ioctl(0, FIONREAD, &toread);
	if (toread <= 0)
	{
		return;
	}
	if (toread > 128)
	{
		toread = 128;
	}

	bread = read(0, msgbuf, toread);
	for (i = 0 ; i < bread ;)
	{
		req_buf[bufpos++] = msgbuf[i++];
		if (bufpos == req_size)
		{
			process_request(bufpos);
			bufpos = 0;
		}
	}
}

void process_request(int toread)
{
	int                  i;
	struct sockaddr_in   sockadd;
	struct sockaddr_in   local_sockadd;
	ident_identifier     ident_id;

	for (i = 0 ; i < toread ;)
	{
		switch (req_buf[i++])
		{
		case IDENT_CLIENT_SEND_REQUEST:
			memcpy(&ident_id, &req_buf[i], sizeof(ident_id));
			i += sizeof(ident_id);
			memcpy(&(local_sockadd.sin_addr.s_addr), &req_buf[i],
		 		sizeof(local_sockadd.sin_addr.s_addr));
			i += sizeof(local_sockadd.sin_addr.s_addr);
			memcpy(&(local_sockadd.sin_port), &req_buf[i],
				sizeof(local_sockadd.sin_port));
			i += sizeof(local_sockadd.sin_port);
			memcpy(&(sockadd.sin_family), &req_buf[i],
				sizeof(sockadd.sin_family));
			i += sizeof(sockadd.sin_family);
			memcpy(&(sockadd.sin_addr.s_addr), &req_buf[i],
				sizeof(sockadd.sin_addr.s_addr));
			i += sizeof(sockadd.sin_addr.s_addr);
			memcpy(&(sockadd.sin_port), &req_buf[i],
				sizeof(sockadd.sin_port));
			i += sizeof(sockadd.sin_port);

#if defined(DEBUG_IDENT_TOO)
			logid("ident",
				"Server: Id      = %d\n"
				"        Address = %08X:%d\n"
				"        To      = %08X:%d\n"
				"Bytes needed %d, Bytes to recieve %d\n",
				ident_id,
				(int) ntohl(sockadd.sin_addr.s_addr),
				ntohs(sockadd.sin_port),
				(int) ntohl(local_sockadd.sin_addr.s_addr),
				ntohs(local_sockadd.sin_port),
				i, toread);
#endif /* DEBUG_IDENT_TOO */

			queue_request(ident_id, &local_sockadd, &sockadd);
			break;
		case IDENT_CLIENT_CANCEL_REQUEST:
			memcpy(&ident_id, &req_buf[i], sizeof(ident_id));
			i += sizeof(ident_id);
			break;
		default:
#if defined(DEBUG_IDENT_TOO)
			logid("ident", "Ident_Server: Unknown message from client:"
							 " '%d'\n",
				req_buf[i]);
#endif /* DEBUG_IDENT_TOO */
			 break;
		}
	}
}


/* Queue up a request from the client and/or send it off */

void queue_request(ident_identifier id, struct sockaddr_in *local_sockadd,
	struct sockaddr_in *sockadd)
{
	ident_request       *new_id_req;

	new_id_req = (ident_request *) MALLOC(sizeof(ident_request));
	memset(new_id_req, 0, sizeof(ident_request));
	new_id_req->ident_id = id;
	memcpy(&(new_id_req->local_sockaddr), local_sockadd,
		sizeof(struct sockaddr_in));
	new_id_req->next = NULL;
	memcpy(&(new_id_req->sock_addr), sockadd, sizeof(struct sockaddr_in));
	if (!first_inq)
	{
		first_inq = new_id_req;
	} else if (!last_inq)
	{
		last_inq = new_id_req;
		first_inq->next = last_inq;
	} else
	{
		last_inq->next = new_id_req;
		last_inq = new_id_req;
	}
}

void take_off_queue(int freeslot)
{
	ident_request       *old;

	if (first_inq)
	{
		memcpy(&idents_in_progress[freeslot], first_inq, sizeof(ident_request));
		old = first_inq;
		first_inq = first_inq->next;
		FREE(old);
		do_request(&idents_in_progress[freeslot]);
	}
}

void do_request(ident_request *id_req)
{
	int                  dummy;
	int                  ret;
	struct sockaddr_in   sa;

#if defined(DEBUG_IDENT_TOO)
	logid("ident", "Server: Doing request - %d\n",
			  id_req->ident_id);
#endif /* DEBUG_IDENT_TOO */
	if (-1 == (id_req->fd = socket(PF_INET, SOCK_STREAM, 0)))
	{
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "Couldn't get new fd for request");
#endif /* DEBUG_IDENT_TOO */
		/* Erk, put on pending queue */
		queue_request(id_req->ident_id, &(id_req->local_sockaddr),
					&(id_req->sock_addr));
		return;
	}
	if (ioctl(id_req->fd, FIONBIO, (caddr_t)&dummy) < 0)
	{
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "Can't set non-blocking on request sock");
#endif /* DEBUG_IDENT_TOO */
		/* Do without? */
	}
	id_req->request_time = time(NULL);
	/* First bind to correct local address */
#if defined(__FreeBSD__)
	sa.sin_len = sizeof(sa);
#endif /* __FreeBSD__ */
	sa.sin_family = id_req->sock_addr.sin_family;
	sa.sin_port = htons(0);
	sa.sin_addr.s_addr = id_req->local_sockaddr.sin_addr.s_addr;
	ret = bind(id_req->fd, (struct sockaddr *)&sa, sizeof(sa));
	if (ret != 0)
	{
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "do_Request:bind() error \"%s\"\nAddress was: %s, errno = %d\n", strerror(errno), inet_ntoa(sa.sin_addr),
			errno);
#endif /* DEBUG_IDENT_TOO */
	}

	/* Now try and connect to remote address */
	sa.sin_addr.s_addr = id_req->sock_addr.sin_addr.s_addr;
	sa.sin_port = htons(IDENT_PORT);
	ret = connect(id_req->fd, (struct sockaddr *) &sa, sizeof(sa));
	if (ret != 0
#if defined(EINPROGRESS)
		 && errno != EINPROGRESS
#endif /* EINPROGRESS */
		)
	{
		if (errno == ECONNREFUSED)
		{
#if defined(DEBUG_IDENT_TOO)
			logid("ident", "ID %d, CONNREFUSED\n", id_req->ident_id);
#endif /* DEBUG_IDENT_TOO */
			id_req->flags |= IDENT_CONNREFUSED;
		} else
		{
#if defined(DEBUG_IDENT_TOO)
			logid("ident", "Error on connect, NOT CONNREFUSED, errno = %d",
				 errno);
#endif /* DEBUG_IDENT_TOO */
		}
#if defined(EINPROGRESS)
	} else if (errno != EINPROGRESS)
#else /* !EINPROGRESS */
	} else
#endif /* EINPROGRESS */
	{
		id_req->flags |= IDENT_CONNECTED;
		write_request(id_req);
	}
}

void write_request(ident_request *id_req)
{
#if defined(DEBUG_IDENT_TOO)
	logid("ident", "Server: write_request()...\n");
#endif
	sprintf(scratch, "%d, %d\n",
		ntohs(id_req->sock_addr.sin_port),
		ntohs(id_req->local_sockaddr.sin_port));
	if (-1 == write(id_req->fd, scratch, strlen(scratch)))
	{
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "Server: write() failed\n");
#endif
		id_req->flags |= IDENT_CONNREFUSED;
	} else
	{
		id_req->flags |= IDENT_WRITTEN;
	}

#if defined(DEBUG_IDENT_TOO)
	logid("ident", "Server: Sending request '%s'\n", scratch);
#endif /* DEBUG_IDENT_TOO */
}

/* Get a result from an identd and send it to the client in the
 * form:
 *
 * <SERVER_SEND_REPLY><ident_id><reply text>'\n'
 */

void process_result(int slot)
{
	char                 reply[BUFFER_SIZE];
	int                  bread;
	char                *s;
	char                *t = NULL;
	char                *reply_text = NULL;

	s = scratch;
	*s++ = IDENT_SERVER_SEND_REPLY;
	memcpy(s, &(idents_in_progress[slot].ident_id),
		sizeof(ident_identifier));
	s += sizeof(ident_identifier);
	strcpy(s, "<ERROR>");
	reply_text = s;
#if defined(DEBUG_IDENT_TOO)
	logid("ident", "Server: Processing result...\n");
#endif /* DEBUG_IDENT_TOO */
	if (idents_in_progress[slot].flags & IDENT_CONNREFUSED)
	{
		/* Connection was refused */
		strcpy(s, "<CONN-REFUSED>");
		t = strchr(s, '\0');
	} else if (!(idents_in_progress[slot].flags & IDENT_CONNECTED))
	{
		/* No connection was established */
		strcpy(s, "<CONN-FAILED>");
		t = strchr(s, '\0');
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "    Not Connected\n");
#endif /* DEBUG_IDENT_TOO */
	} else if (!(idents_in_progress[slot].flags & IDENT_WRITTEN))
	{
		/* Connection made but no message written */
		strcpy(s, "<DIDN'T-SEND>");
		t = strchr(s, '\0');
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "    Not Written\n");
#endif /* DEBUG_IDENT_TOO */
	} else if (!(idents_in_progress[slot].flags & IDENT_REPLY_READY))
	{
		/* Request written but didn't get reply before timeout */
		strcpy(s, "<TIMED-OUT>");
		t = strchr(s, '\0');
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "    Not Ready for Read\n");
#endif /* DEBUG_IDENT_TOO */
	} else
	{
		/* Got a reply, phew! */
		/* BUFFER_SIZE - 20 (== 1004 atm) should be plenty as RFC1413
		 * specifies that a USERID reply should not have a user id field
		 * of more than 512 octets.
		 * Additionally RFC1413 specifies that:
		 * "Clients should feel free to abort the connection if they
		 *  receive 1000 characters without receiving an <EOL>"
		 *
		 * NB: <EOL> ::= "015 012"  ; CR-LF End of Line Indicator
		 * I assume that under C this will be converted to '\n'
		 */
#if defined(HAVE_BZERO)
		bzero(reply, BUFFER_SIZE);
#else /* !HAVE_BZERO */
		memset(reply, 0, BUFFER_SIZE);
#endif /* HAVE_BZERO */
		bread = read(idents_in_progress[slot].fd, reply, BUFFER_SIZE - 20);
		reply[bread] = '\0';

		/* Make sure the reply is '\n' terminated */
		t = strchr(reply, '\n');
		if (t)
		{
			t++;
#if defined(HAVE_BZERO)
	 bzero(t, &reply[BUFFER_SIZE] - t - 1);
#else
	 memset(t, 0, &reply[BUFFER_SIZE] - t - 1);
#endif /* HAVE_BZERO */
		} else
		{
			reply[1001] = '\n';
#if defined(HAVE_BZERO)
	 bzero(&reply[1002], BUFFER_SIZE - 1002 - 1);
#else
	 memset(&reply[1002], 0, BUFFER_SIZE - 1002 - 1);
#endif /* HAVE_BZERO */
		}
#if defined(DEBUG_IDENT_TOO)
		logid("ident", "Server: Got reply '%s'\n",
			  reply);
#endif /* DEBUG_IDENT_TOO */

		if (!(t = strstr(reply, "USERID")))
		{
			/* In this case the reply MUST be in the form:
			 *
			 * <port-pair> : ERROR : <error-type>
			 *
			 * (see RFC1413, if a later one exists for the ident protocol
			 *  then this code should be updated)
			 *
			 * We will reply to our client with the '<error-type>' text
			 *
			 */
#if defined(DEBUG_IDENT)
			logid("ident_ids", "SERVER: slot %d, ident_id %d, ACTUAL reply '%s'",
				slot, idents_in_progress[slot].ident_id, reply);
#endif /* DEBUG_IDENT */
			 if (!(t = strstr(reply, "ERROR")))
			 {
			 /* Reply doesn't contain 'USERID' *or* 'ERROR' */
				 strcpy(s, "<ERROR>");
			 } else
			 {
#if defined(DEBUG_IDENT_TOO)
				 logid("ident", "Reply: '%s'", t);
#endif /* DEBUG_IDENT_TOO */
				t = strchr(t, ':');
			 /*
			  * <port-pair> : ERROR : <error-type>
			  *                     ^
			  *             t-------|
			  */
				 if (!t)
				 {
					 /* Couldn't find the colon after 'ERROR' */
					 strcpy(s, "<ERROR>");
				 } else
				 {
					 *s++ = '<';
				 }
			}
		} else
		{
			/* Got a reply of the form:
			 *
			 * <port-pair> : USERID : <opsys-field> : <user-id>
			 *               ^
			 *         t-----|
			 *
			 * We will reply to our client with the '<user-id>' text
			 *
			 */

#if defined(DEBUG_IDENT)
			logid("ident_ids", "SERVER: slot %d, ident_id %d, ACTUAL reply '%s'",
			  slot, idents_in_progress[slot].ident_id, reply);
			 logid("ident", "Reply: '%s'", t);
#endif /* DEBUG_IDENT */
			t = strchr(t, ':');
			if (!t)
			{
			/* Couldn't find the : after USERID 
			 *
			 * <port-pair> : USERID : <opsys-field> : <user-id>
			 *                      ^
			 *           t----------|
			 */
				strcpy(s, "<ERROR>");
			} else
			{
				t++;
				t = strchr(t, ':');
				if (!t)
				{
				/* Couldn't find the : after <opsys-field>
				 *
				 * <port-pair> : USERID : <opsys-field> : <user-id>
				 *                                      ^
				 *                           t----------|
				 */
					strcpy(s, "<ERROR>");
				}
			}
		}
		if (t)
		{
		/* t is now at the : before the field we want to return */
		/* Skip the : */
			t++;
		/* Skip any white space */
			while ((*t == ' ' || *t == '\t') && *t != '\0')
			{
				t++;
			}
			if (*t != '\0')
			{
			/* Found start of the desired text */
				sprintf(s, "%s", t);
				t = strchr(s, '\0');
				t--;
				/* The reply SHOULD be '\n' terminated (RFC1413) */
				while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r')
				{
					t--;
				}
				/* t now at end of text we want */
				/* Move forward to next char */
				t++;
				/* If char before s is a '<' we put it there ourselves to wrap
				 * an 'ERROR' return in <>. So we need to put the > on the
				 * end
				 */
				if (*(s - 1) == '<')
				{
					*t++ = '>';
				}
			} else
			{
				strcpy(s, "<ERROR>");
				t = 0;
			}
		}
	}
	/* Make sure t is pointing to the NULL terminator of the string */
	if (!t)
	{
		t = strchr(s, '\0');
	}
	/* A newline terminates the message */
	*t++ = '\n';
	*t = '\0';
	/* Now actually send the reply to the client */
	write(1, scratch, (t - scratch));
#if defined(DEBUG_IDENT)
	logid("ident_ids", "SERVER: slot %d, ident_id %d, reply '%s'",
		slot, idents_in_progress[slot].ident_id, reply_text);
#endif /* DEBUG_IDENT */
#if defined(DEBUG_IDENT_TOO)
	*(t + 1) = '\0';
	logid("ident", "    Sending reply '%s' of %d bytes\n", s, (t - scratch));
#endif /* DEBUG_IDENT_TOO */
	closedown_request(slot);
}

void closedown_request(int slot)
{
	shutdown(idents_in_progress[slot].fd, 2);
	close(idents_in_progress[slot].fd);
	idents_in_progress[slot].fd = -1;
}


/* log using stdarg variable length arguments */

void logid(char *str, ...)
{
	va_list              args;
	FILE                *fp;
	char                *fmt;
	struct tm           *tim;
	char                 the_time[256];
	char                 fname[21];
	time_t               current_time;

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
