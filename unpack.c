
//SysIII/usr/src/cmd/unpack.c
//Compare this file to the similar file:

//Show the results in this format:

/*
 *	Huffman decompressor (new files replace old)
 *	Adapted April 1979, from a program by T.G. Szymanski, March 1978.
 *	Usage:	unpack filename...
 */

#include <stdio.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>

jmp_buf env;
struct	stat status;

#define	BLKSIZE	512
#define NAMELEN 80
#define	SUF0	'.'
#define	SUF1	'z'
#define US 037
#define RS 036
#ifdef pdp11
#define COMPATABILITY
#endif

/* variables associated with i/o */
char	filename [NAMELEN+2];
int	infile;
int	outfile;
int	inleft;
char	*inp;
char	*outp;
char	inbuff [BLKSIZE];
char	outbuff [BLKSIZE];

/* the dictionary */
long	origsize;
int	maxlev;
int	intnodes [25];
char	*tree [25];
char	characters [256];
char	*eof;

/* read in the dictionary portion and build decoding structures */
/* return 1 if successful, 0 otherwise */
getdict ()
{
	register int c, i, nchildren;
	/*
	 * check two-byte header
	 * get size of original file,
	 * get number of levels in maxlev,
	 * get number of leaves on level i in intnodes[i],
	 * set tree[i] to point to leaves for level i
	 */
	eof = &characters[0];
	inbuff[6] = 25;
	inleft = read(infile, &inbuff[0], BLKSIZE);
	if (inleft < 0) {
		printf (".z: read error");
		return (0);
	}
	if (inbuff[0] != US) goto goof;
#ifdef COMPATABILITY
	if (inbuff[1] == US) { /* oldstyle packing */
		printf (": old-style");
		if (setjmp(env)) return (0);
		expand();
		return (1);
	}
#endif
	if (inbuff[1] != RS) goto goof;

	inp = &inbuff[2];
	origsize = 0;
	for (i=0; i<4; i++) {
		origsize = origsize*256 + ((*inp++) & 0377);
	}
	maxlev = *inp++ & 0377;
	if (maxlev > 24) {
goof:		printf (".z: not in packed format");
		return (0);
	}
	for (i=1; i<=maxlev; i++)
		intnodes[i] = *inp++ & 0377;
	for (i=1; i<=maxlev; i++) {
		tree[i] = eof;
		for (c=intnodes[i]; c>0; c--) {
			if (eof >= &characters[255])
				goto goof;
			*eof++ = *inp++;
		}
	}
	*eof++ = *inp++;
	intnodes[maxlev] += 2;
	inleft -= inp-&inbuff[0];
	if (inleft < 0)
		goto goof;

	/*
	 * convert intnodes[i] to be number of
	 * internal nodes possessed by level i
	 */
	nchildren = 0;
	for (i=maxlev; i>=1; i--) {
		c = intnodes[i];
		intnodes[i] = nchildren /= 2;
		nchildren += c;
	}
	return (decode());
}

/* unpack the file */
/* return 1 if successful, 0 otherwise */
int decode ()
{
	register int bitsleft, c, i;
	int j, lev;
	char *p;
	outp = &outbuff[0];
	lev = 1;
	i = 0;
	while (1) {
		if (inleft <= 0) {
			inleft = read(infile, inp = &inbuff[0], BLKSIZE);
			if (inleft < 0) {
				printf (".z: read error");
				return (0);
			}
		}
		if (--inleft < 0) {
uggh:			printf (".z: unpacking error");
			return (0);
		}
		c = *inp++;
		bitsleft = 8;
		while (--bitsleft >= 0) {
			i *= 2;
			if (c & 0200)
				i++;
			c <<= 1;
			if ((j = i-intnodes[lev]) >= 0) {
				p = &tree[lev][j];
				if (p == eof) {
					c = outp-&outbuff[0];
					if (write(outfile, outbuff, c) != c) {
wrerr:						printf (": write error");
						return (0);
					}
					origsize -= c;
					if (origsize != 0) goto uggh;
					return(1);
				}
				*outp++ = *p;
				if (outp == &outbuff[BLKSIZE]) {
					if (write(outfile, outp = &outbuff[0], BLKSIZE) != BLKSIZE)
						goto wrerr;
					origsize -= BLKSIZE;
				}
				lev = 1;
				i = 0;
			} else
				lev++;
		}
	}
}


main(argc, argv)
int argc; char *argv[];
{       register int i, k;
	int sep;
	register char *cp;
	int fcount = 0; /* failure count */

	for (k = 1; k<argc; k++)
	{
		sep = -1;  cp = filename;
		for (i=0; i < (NAMELEN-3) && (*cp = argv[k][i]); i++)
			if (*cp++ == '/') sep = i;
		if (cp[-1]==SUF1 && cp[-2]==SUF0)
		{       argv[k][i-2] = '\0'; /* Remove suffix and try again */
			k--;
			continue;
		}

		printf ("%s: %s", argv[0], argv[k]);
		fcount++;  /* expect the worst */
		if (i >= (NAMELEN-3) || (i-sep) > 13)
		{       printf (": file name too long");
			goto done;
		}
		*cp++ = SUF0;  *cp++ = SUF1;  *cp = '\0';
		if ((infile = open(filename, 0)) == -1)
		{       printf (".z: cannot open");
			goto done;
		}

		if( stat(argv[k], &status) != -1 )
		{
			printf(": already exists");
			goto closein;
		}
		fstat (infile, &status);
		if ( status.st_nlink != 1)
			printf(".z: Warning: file has links");
		if ((outfile = creat (argv[k], status.st_mode&07777)) == -1)
		{       printf (": cannot create");
			goto closein;
		}

		chown (argv[k], status.st_uid, status.st_gid);

		if (getdict()) {		/* unpack */
			fcount--; 	/* success after all */
			printf (": unpacked");
			unlink(filename);
			utime(argv[k], &status.st_atime);/* preserve acc & mod dates */
		}
		else {
			unlink(argv[k]);
		}
		close(outfile);
closein:	close(infile);
done:		printf ("\n");
	}
	return (fcount);
}

#ifdef COMPATABILITY
/* this code is for unpacking files, which were packed using the previous algorithm */
int Tree [1024];
expand()
{       register int tp, bit, word;
	int keysize, i, *t;

	outp = outbuff;
	inp = &inbuff[2];
	inleft -= 2;
	t = Tree;
	origsize = ((long int ) (unsigned) getwd())*256*256;
	origsize += (unsigned) getwd();
	for ( keysize=getwd(); keysize--; )
	{       if ((i = (unsigned) getch()) == 0377)
			*t++ = getwd();
		else
			*t++ = i & 0377;
	}

	bit = tp = 0;
	for (;;)
	{       if (bit <= 0)
		{       word = getwd();
			bit = 16;
		}
		tp += Tree[tp + (word<0)];
		word <<= 1;  bit--;
		if (Tree[tp] == 0)
		{       putch(Tree[tp+1]);
			tp = 0;
			if ((origsize -= 1) == 0) {
				write (outfile, outbuff, outp-&outbuff[0]);
				return;
			}
		}
	}
}

getch() {
	if (inleft <= 0) {
		inleft = read (infile, inp=inbuff, BLKSIZE);
		if (inleft < 0) {
			printf (".z: read error");
			longjmp (env, 1);
		}
	}
	inleft--;
	return (*inp++ & 0377);
}

getwd() {
	register char c;
	register d;
	c = getch();
	d = getch();
	d <<= 8;
	d |= (c & 0377);
	return (d);
}

putch(c) char c; {
	register int n;
	*outp++ = c;
	if (outp == &outbuff[BLKSIZE]) {
		n = write (outfile, outp=outbuff, BLKSIZE);
		if (n < BLKSIZE) {
			printf (": write error");
			longjmp (env, 2);
		}
	}
}
#endif

