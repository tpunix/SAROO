
#include "main.h"

/******************************************************************************/

#define YEAR ((((__DATE__ [7] - '0') * 10 + (__DATE__ [8] - '0')) * 10 \
    + (__DATE__ [9] - '0')) * 10 + (__DATE__ [10] - '0'))

#define MONTH (__DATE__ [2] == 'n' ? (__DATE__ [1] == 'a' ? 1 : 6) \
    : __DATE__ [2] == 'b' ? 2 \
    : __DATE__ [2] == 'r' ? (__DATE__ [0] == 'M' ? 3 : 4) \
    : __DATE__ [2] == 'y' ? 5 \
    : __DATE__ [2] == 'l' ? 7 \
    : __DATE__ [2] == 'g' ? 8 \
    : __DATE__ [2] == 'p' ? 9 \
    : __DATE__ [2] == 't' ? 10 \
    : __DATE__ [2] == 'v' ? 11 : 12)

#define DAY ((__DATE__ [4] == ' ' ? 0 : __DATE__ [4] - '0') * 10 \
    + (__DATE__ [5] - '0'))


u32 get_build_date(void)
{
	u32 bcdate = 0;
	char *d = __DATE__;
	int m = MONTH;
	int h, l;


	bcdate |= (d[ 7]-'0')<<28;
	bcdate |= (d[ 8]-'0')<<24;
	bcdate |= (d[ 9]-'0')<<20;
	bcdate |= (d[10]-'0')<<16;

	h = (m>9)?      1: 0;
	l = (m>9)? (m-10): m;
	bcdate |= h<<12;
	bcdate |= l<<8;

	h = (d[4]==' ')? 0: d[4]-'0';
	l = d[5]-'0';
	bcdate |= h<<4;
	bcdate |= l<<0;

	return bcdate;
}

