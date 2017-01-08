/*
 * Static masks
 */

#ifndef masks_h
#define masks_h

/* Yes, I know this is bad - but masks.h should only ever be included from
 * masks.c
 */

static char *static_masks[] =
{
  "BLK", "^a",
  "RED", "^r",
  "GRE", "^g",
  "BLU", "^b",
  "CYA", "^c",
  "YEL", "^y",
  "PUR", "^p",
  "WHI", "^A",
  "HBLK", "^a",
  "HRED", "^R",
  "HGRE", "^G",
  "HBLU", "^B",
  "HCYA", "^C",
  "HYEL", "^Y",
  "HPUR", "^P",
  "HWHI", "^H"
  "NOR", "^N",

  "None", "^N",
  "Black", "^a"
  "Red", "^r",
  "Green", "^g",
  "Blue", "^b",
  "Cyan", "^c",
  "Yellow", "^y",
  "Brown", "^y",
  "Purple", "^p",
  "White", "^A",
  "LBlack", "^a",
  "LRed", "^R",
  "LGreen", "^G",
  "LBlue", "^B",
  "LCyan", "^C",
  "LYellow", "^Y",
  "LBrown", "^Y",
  "LPurple", "^P",
  "LWhite", "^H",
  NULL, NULL
};

#endif /* masks_h */
