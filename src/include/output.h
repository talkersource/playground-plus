/*
 * Playground+ - output.h
 * Enhanced output processor by Mo McKinlay
 * ----------------------------------------------------------------------------
 */

#ifndef output_h
#define output_h

typedef enum {
  TT_VOID = 0,
  TT_LOGIN =   (1<<0),
  TT_LOGOUT =  (1<<1),
  TT_RECON =   (1<<2),
  TT_ECHO =    (1<<3),
  TT_ROOM =    (1<<4),
  TT_AUTO =    (1<<5),
  TT_FRIEND =  (1<<6),
  TT_OFRIEND = (1<<7),
  TT_MULTI =   (1<<8),
  TT_NMULTI =  (1<<9),
  TT_PIPE =    (1<<10),
  TT_TELL =    (1<<11),
  TT_SHOUT =   (1<<12),
  TT_SOCIAL =  (1<<13),
  TT_SEEECHO = (1<<14),
  TT_ITEM =    (1<<15)
} tagtype_t;

typedef struct tagentry_s tagentry_t;

struct tagentry_s {
  tagtype_t type;
  char tag;
};

/* Prototypes */

extern tagentry_t tags[];

extern tagtype_t output_checktags(player *);
extern char *output_tags(player *, tagtype_t);

extern file process_output(player *, char *);

#endif /* output_h */

