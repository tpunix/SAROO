/*
 * string.c
 */

#include "main.h"

#if 0
void *memset(void *s, int v, int n)
{
    register char *p = (char *)s;

    while(1){
        if(--n<0)
            break;
        *p++ = (char)v;
    }

    return s;
}

void *memcpy(void *to, void *from, int n)
{
    register char *t = (char *)to;

    while(1){
        if(--n<0)
            break;
        *t++ = *(char*)from++;
    }

    return to;
}
#endif

int memcmp(void *dst, void *src, int n)
{
	register int i;
	register char *s = (char*)src;
	register char *d = (char*)dst;

	for(i=0; i<n; i++){
		if(*s!=*d){
			printk("memcmp mismatch at %08x:\n", i);
			printk("    dst_%08x=%02x\n", d, *d);
			printk("    src_%08x=%02x\n", s, *s);
			return *d-*s;
		}
		s++;
		d++;
	}

	return 0;
}



char *strcpy(char *dst, char *src)
{
    register char *d = dst;
    register char t;

    do{
        t = *src++;
        *d++ = t;
    }while(t);

    return dst;
}

char *strncpy(char *dst, char *src, int n)
{
    register char *d = dst;
    register char t;

    do{
        if(--n<0)
            break;
        t = *src++;
        *d++ = t;
    }while(t);

    return dst;
}

int strcmp(char *s1, char *s2)
{
    int r;
    int t;

    while(1){
        t = (int)*s1++;
        r = t - (int)*s2++;
        if(r)
            break;
        if(t==0)
            break;
    }

    return r;
}

int strncmp(char *s1, char *s2, int n)
{
    register int r = 0;
    register int t;

    while(1){
        if(--n<0)
            break;
        t = (int)*s1++;
        r = t - (int)*s2++;
        if(r)
            break;
        if(t==0)
            break;
    }

    return r;
}


char *strchr(char *s1, int ch)
{
	while(1){
		char t = *s1;
		if(t==0)
			return (char*)0;
		if(t==ch)
			return s1;
		s1++;
	}
}


int strlen(char *s)
{
    register char *p = s;

    while(*p++);

    return p-s-1;
}


unsigned int strtoul(char *str, char **endptr, int requestedbase, int *ret)
{
	unsigned int num = 0;
	int c, digit, retv;
	int base, nchars, leadingZero;

	base = 10;
	nchars = 0;
	leadingZero = 0;

	if(str==0 || *str==0){
		retv = 2;
		goto exit;
	}
	retv = 0;

	if(requestedbase)
		base = requestedbase;

	while ((c = *str) != 0) {
		if (nchars == 0 && c == '0') {
			leadingZero = 1;
			goto step;
		} else if (leadingZero && nchars == 1) {
			if (c == 'x') {
				base = 16;
				goto step;
			} else if (c == 'o') {
				base = 8;
				goto step;
			}
		}
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'a' && c <= 'z') {
			digit = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'Z') {
			digit = c - 'A' + 10;
		} else {
			retv = 3;
			break;
		}
		if (digit >= base) {
			retv = 4;
			break;
		}
		num *= base;
		num += digit;
step:
		str++;
		nchars++;
	}

exit:
	if(ret)
		*ret = retv;
	if(endptr)
		*endptr = str;

	return num;
}

