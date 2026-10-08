#ifndef __CRC_H
#define __CRC_H

unsigned char calc_crc8(const unsigned char *data, const unsigned int data_len);


unsigned short usMBCRC16( unsigned char * pucFrame, unsigned short usLen );
unsigned short usSumFunction(unsigned char * pucFrame, unsigned short usLen);
unsigned int my_crc16(unsigned char * pucFrame,unsigned int start,unsigned int end);

#endif
