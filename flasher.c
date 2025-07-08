#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#include "getopt.h"
#include "printf.h"
#include "buildno.h"
#endif

#include "hdlcio.h"
#include "ptable.h"
#include "flasher.h"
#include "util.h"

#define true 1
#define false 0


//***************************************************
//*Storage of error code
//***************************************************
int errcode;


//***************************************************
//*Conclusion of team error code
//***************************************************
void printerr() {
  
if (errcode == -1) printf(" - Team timeout\n");
else printf(" - error code %02x\n",errcode);
}

//***************************************************
// Sending the team began the section
// 
//  Code -32 -bit section code
//  Size -the full amount of the recorded section
// 
//* result:
//  FALSE is a mistake
//  True -the team adopted by the modem
//***************************************************
int dload_start(uint32_t code,uint32_t size) {

uint32_t iolen;  
uint8_t replybuf[4096];
  
#ifndef WIN32
static struct __attribute__ ((__packed__))  {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t code;
  uint32_t size;
  uint8_t pool[3];
} cmd_dload_init =  {0x41,0,0,{0,0,0}};
#ifdef WIN32
#pragma pack(pop)
#endif


cmd_dload_init.code=htonl(code);
cmd_dload_init.size=htonl(size);
iolen=send_cmd((uint8_t*)&cmd_dload_init,sizeof(cmd_dload_init),replybuf);
errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2)) {
  if (iolen == 0) errcode=-1;
  return false;
}  
else return true;
}  

//***************************************************
// Sending the section block
// 
//  BLK -# block
//  Pimage -the address of the beginning of the image in memory
// 
//* result:
//  FALSE is a mistake
//  True -the team adopted by the modem
//***************************************************
int dload_block(uint32_t part, uint32_t blk, uint8_t* pimage) {

uint32_t res,blksize,iolen;
uint8_t replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t blk;
  uint16_t bsize;
  uint8_t data[fblock];
} cmd_dload_block;  
#ifdef WIN32
#pragma pack(pop)
#endif

blksize=fblock; // The initial value of the size of the block
res=ptable[part].hd.psize-blk*fblock;  // The size of the remaining piece to the end of the file
if (res<fblock) blksize=res;  // Correct the size of the last block

// command code
cmd_dload_block.cmd=0x42;
// Block number
cmd_dload_block.blk=htonl(blk+1);
// Block size
cmd_dload_block.bsize=htons(blksize);
// Portion of data from the image of the section
memcpy(cmd_dload_block.data,pimage+blk*fblock,blksize);
// send the block to the modem
iolen=send_cmd((uint8_t*)&cmd_dload_block,sizeof(cmd_dload_block)-fblock+blksize,replybuf); // We send the team

errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2))  {
  if (iolen == 0) errcode=-1;
  return false;
}
return true;
}

  
//***************************************************
// Completion of the section of the section
// 
//  Code -section code
//  Size -section size
// 
//* result:
//  FALSE is a mistake
//  True -the team adopted by the modem
//***************************************************
int dload_end(uint32_t code, uint32_t size) {

uint32_t iolen;
uint8_t replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t size;
  uint8_t garbage[3];
  uint32_t code;
  uint8_t garbage1[11];
} cmd_dload_end;
#ifdef WIN32
#pragma pack(pop)
#endif


cmd_dload_end.cmd=0x43;
cmd_dload_end.code=htonl(code);
cmd_dload_end.size=htonl(size);
iolen=send_cmd((uint8_t*)&cmd_dload_end,sizeof(cmd_dload_end),replybuf);
errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2)) {
  if (iolen == 0) errcode=-1;
  return false;
}  
return true;
}  



//***************************************************
//*Record to the modem of all sections from the table
//***************************************************
void flash_all() {

int32_t part;
uint32_t blk,maxblock;

printf("\n## ---- Section name ---- written");
// The main cycle of sections
for(part=0;part<npart;part++) {
printf("\n");  
//  Printf ("\ n02i %s), part, ptable [part] .pname);
 // The team began the section
 if (!dload_start(ptable[part].hd.code,ptable[part].hd.psize)) {
   printf("\r! Section title rejected %i (%s)",part,ptable[part].pname);
   printerr();
   exit(-2);
 }  
    
 maxblock=(ptable[part].hd.psize+(fblock-1))/fblock; // number of blocks in the section
 // Plugging cycle of transmission of the image of the section
 for(blk=0;blk<maxblock;blk++) {
  // The output of the percentage recorded
  printf("\r%02i  %-20s  %i%%",part,ptable[part].pname,(blk+1)*100/maxblock); 

    // We send the next block
  if (!dload_block(part,blk,ptable[part].pimage)) {
   printf("\n! Block %i of section rejected %i (%s)",blk,part,ptable[part].pname);
   printerr();
   exit(-2);
  }  
 }    

// Close the section
 if (!dload_end(ptable[part].hd.code,ptable[part].hd.psize)) {
   printf("\n! Error closing section %i (%s)",part,ptable[part].pname);
   printerr();
   exit(-2);
 }  
} // The end of the cycle by sections
}
