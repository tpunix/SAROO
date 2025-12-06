
/*	
 * tiny xmodem receive code
 */

#include "main.h"


#define xm_getc _getc_tmout
#define xm_putc _putc


#define SOH  0x01
#define STX  0x02
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18

#define DLY_1S 1000

int tiny_xmodem_recv(u8 *dest)
{
	int pk_size, pk_num, pk_chk, check_sum, pk_sum, next_pk;
	int i, c, len;

	next_pk = 1;
	len = 0;

	while(1){
		while(1){
			c = xm_getc(DLY_1S);
			if(c>=0){
				if(c==SOH){
					pk_size = 128;
					break;
				}else if(c==STX){
					pk_size = 1024;
					break;
				}else if(c==EOT){
					goto _exit;
				}else if(c==CAN){
					goto _exit;
				}
			}
			xm_putc(NAK);
		}

		// start receive package:
		check_sum = 0;

		// package number check
		pk_num = xm_getc(DLY_1S);
		pk_chk = xm_getc(DLY_1S);
		if(pk_num+pk_chk!=0xff)
			goto reject;
		if(pk_num!=next_pk)
			goto reject;

		// receive whole package
		for (i=0;  i<pk_size; i++) {
			c = xm_getc(DLY_1S);
			if(c<0)
				goto reject;
			dest[i] = c;
			check_sum += c;
		}

		// package checksum check
		pk_sum = xm_getc(DLY_1S);
		if((check_sum&0xff)!=pk_sum)
			goto reject;

		// update
		dest += pk_size;
		len += pk_size;
		next_pk += 1;
		next_pk &= 0xff;
		xm_putc(ACK);

		continue;

	reject:
		while(xm_getc(10) >= 0);
		xm_putc(NAK);
	}

_exit:
	xm_putc(ACK);
	return len;
}

