/* gencfile.c -- generate C sourse code with the content of a file
 *
 * Copyright (C) 2020 SINTEF
 *
 * Distributed under terms of the MIT license.
 */

/*
 * Usage: gencfile FUNNAME OUTFILE INFILES...
 *
 * Args
 *   FUNNAME - name of C function that returns the content of INFILE
 *   OUTFILE - generated C source file
 *   INFILES - concatenated input files who's content will be encoded
 */

/* Get rid of warnings about using functions from the standard C library
   instead of Microsoft-specific functions in MSVS */
#if defined WIN32 || defined _WIN32 || defined __WIN32__
# pragma warning(disable: 4996)
#endif

#include <stdlib.h>
#include <stdio.h>


int main(int argc, char *argv[])
{
  FILE *fout, *fin;
  char *progname="gencfile", *funname, *outfile;
  int c, i, n=0;
  int addsep=1;  /* whether to add separator after each input file */
  int addnul=1;  /* whether to add terminating NUL */

  if (argc < 4) {
    fprintf(stderr, "Usage: gencfile FUNNAME OUTFILE INFILES...\n");
    exit(1);
  }
  funname = argv[1];
  outfile = argv[2];

  if (!(fout = fopen(outfile, "w"))) {
    perror(progname);
    exit(1);
  }

  fprintf(fout, "/* Generated by %s - do not edit! */\n", progname);
  fprintf(fout, "\n");
  fprintf(fout, "static char content[] = {\n");
  for (i=3; i<argc; i++) {
    char *infile = argv[i];
    if (!(fin = fopen(infile, "r"))) {
      perror(progname);
      exit(1);
    }
    fprintf(fout, "  /* content of \"%s\" */", infile);
    while ((c = getc(fin)) != EOF) {
      if ((n++) % 8 == 0) fprintf(fout, "\n ");
      fprintf(fout, " 0x%02x,", (unsigned int)c);
    }
    fclose(fin);
    if (addsep) {
      fprintf(fout, "\n");
      fprintf(fout, "  0x0a, 0x0a,  /* separator */\n");
    }
  }
  if (addnul)
    fprintf(fout, "  0x00  /* terminating NUL */\n");
  fprintf(fout, "};\n");
  fprintf(fout, "\n");
  fprintf(fout, "const char *%s(void) {\n", funname);
  fprintf(fout, "  return content;\n");
  fprintf(fout, "}\n");
  fclose(fout);

  return 0;
}
