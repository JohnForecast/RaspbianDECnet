#ifndef _VAXCRC_H
#define _VAXCRC_H

#define DAPPOLY 	0xE905 // X^16+X^15+X^13+X^7+X^4+X^2+X^1+X^0
#define DAPINICRC 	0xFFFF // -1
#define DDCMPPOLY	0xA001 // X^16+X^15+X^2+X^0
#define DDCMPINICRC	0x0000 // 0
#define XXXPOLY		0x8408 // X^16+X^12+X^5+X^0
#define XXXPINICRC	0x0000 // 0

class vaxcrc
{
private:
    unsigned short	crc;
    unsigned short	crc_table[16];
 public:
    vaxcrc(unsigned short poly,unsigned short inicrc);
    unsigned short	getcrc();
    void 		setcrc(unsigned short newcrc);
    void 		calc1shift(unsigned char *stream, int len);
    void 		calc2shift(unsigned char *stream, int len);
    void 		calc4shift(unsigned char *stream, int len);
   
};
#endif
