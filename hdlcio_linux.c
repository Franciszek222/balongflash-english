//  Low -level procedures for working with a sequential port and HDLC


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>

#include "hdlcio.h"
#include "util.h"

unsigned int nand_cmd=0x1b400000;
unsigned int spp=0;
unsigned int pagesize=0;
unsigned int sectorsize=512;
unsigned int maxblock=0;     // The total number of flash drive blocks

char flash_mfr[30]={0};
char flash_descr[30]={0};
unsigned int oobsize=0;

static char pdev[500]; // The name of the sequential port


int siofd; // FD for working with a sequential port

struct termios sioparm;
//int siofd; //FD for working with a sequential port


//*************************************************
//*Boofer reference to the modem
//*************************************************

unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen) {


tcflush(siofd,TCIOFLUSH);  // We drop an unreasonable input buffer


write(siofd,"\x7e",1);  // We send the prefix


if (write(siofd,outcmdbuf,outlen) == 0) {   printf("\n Command write error");return 0;  }
tcdrain(siofd);  // We are waiting for the end of the block output


return 1;
}

//******************************************************************************************
//*Reception of the buffer with the answer from the modem
//*
//*Masslen -the number of bytes taken by a single block without analysis of the end of the end of 7f
//******************************************************************************************


unsigned int receive_reply(char* iobuf, int masslen) {
  
int i,iolen,escflag,incount;
unsigned char c;
unsigned int res;
unsigned char replybuf[14000];

incount=0;
if (read(siofd,&c,1) != 1) {
//  Printf ("\n there is no answer from the modem");

  return 0; // The modem did not answer or answered incorrectly

}
//if (c! = 0x7e) {
//  Printf ("\ n the first byte of the answer -not 7E: %02x", C);
//  Return 0; //modem did not answer or answered incorrectly
//}

replybuf[incount++]=c;

// Reading the array of data by a single block when processing command 03

if (masslen != 0) {
 res=read(siofd,replybuf+1,masslen-1);
 if (res != (masslen-1)) {
   printf("\nResponse from modem too short: %i bytes, expected %i bytes\n",res+1,masslen);
   dump(replybuf,res+1,0);
   return 0;
 }  
 incount+=masslen-1; // We already have a masslen byte in the buffer
// printf ("\ n ------it mass -------");
// DUMP (Replybuf, Incount, 0);

}

// we take the remaining tail of the buffer

while (read(siofd,&c,1) == 1)  {
 replybuf[incount++]=c;
// printf("\n--%02x",c);

 if (c == 0x7e) break;
}

// Transformation of the accepted buffer to remove ESC signs

escflag=0;
iolen=0;
for (i=0;i<incount;i++) { 
  c=replybuf[i];
  if ((c == 0x7e)&&(iolen != 0)) {
    iobuf[iolen++]=0x7e;
    break;
  }  
  if (c == 0x7d) {
    escflag=1;
    continue;
  }
  if (escflag == 1) { 
    c|=0x20;
    escflag=0;
  }  
  iobuf[iolen++]=c;
}  
return iolen;

}

//***********************************************************
//*Transformation of a command buffer with an Escape composure
//***********************************************************

unsigned int convert_cmdbuf(char* incmdbuf, int blen, char* outcmdbuf) {

int i,iolen,bcnt;
unsigned char cmdbuf[14096];

bcnt=blen;
memcpy(cmdbuf,incmdbuf,blen);
// We enter CRC in the end of the buffer

*((unsigned short*)(cmdbuf+bcnt))=crc16(cmdbuf,bcnt);
bcnt+=2;

// Comment of data with shielding ESC sequences

iolen=0;
outcmdbuf[iolen++]=cmdbuf[0];  // The first byte is copying without modifications

for(i=1;i<bcnt;i++) {
   switch (cmdbuf[i]) {
     case 0x7e:
       outcmdbuf[iolen++]=0x7d;
       outcmdbuf[iolen++]=0x5e;
       break;
      
     case 0x7d:
       outcmdbuf[iolen++]=0x7d;
       outcmdbuf[iolen++]=0x5d;
       break;
      
     default:
       outcmdbuf[iolen++]=cmdbuf[i];
   }
 }
outcmdbuf[iolen++]=0x7e; // The final byte

outcmdbuf[iolen]=0;
return iolen;
}

//***************************************************
//*Sending the team to the port and obtaining the result *
//***************************************************

int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf) {
  
unsigned char outcmdbuf[14096];
unsigned int  iolen;

iolen=convert_cmdbuf(incmdbuf,blen,outcmdbuf);  
if (!send_unframed_buf(outcmdbuf,iolen)) return 0; // Team transmission error

return receive_reply(iobuf,0);
}

//***************************************************
// Opening and setting up a sequential port
//***************************************************


int open_port(char* devname) {


int i,dflag=1;
char devstr[200]={0};


if (strlen(devname) != 0) strcpy(pdev,devname);   // We keep the name of the port  

else strcpy(devname,"/dev/ttyUSB0");  // If the port name was not set


// Instead of the full name of the device, it is allowed to transmit only the TTYUSB port number


// Check the name of the device for the presence of non -constituent characters

for(i=0;i<strlen(devname);i++) {
  if ((devname[i]<'0') || (devname[i]>'9')) dflag=0;
}
// If in the line -only numbers, add the prefix /dev /ttyusb


if (dflag) strcpy(devstr,"/dev/ttyUSB");

// Copy the name of the device

strcat(devstr,devname);

siofd = open(devstr, O_RDWR | O_NOCTTY |O_SYNC);
if (siofd == -1) {
  printf("\n! -Serial port %s cannot be opened\n", devname); 
  exit(0);
}
bzero(&sioparm, sizeof(sioparm)); // Preparing the Termios attribute block

sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
sioparm.c_iflag = 0;  // Inpck;

sioparm.c_oflag = 0;
sioparm.c_lflag = 0;
sioparm.c_cc[VTIME]=30; // Timeout  

sioparm.c_cc[VMIN]=0;  
tcsetattr(siofd, TCSANOW, &sioparm);

tcflush(siofd,TCIOFLUSH);  // Cleaning the output buffer


return 1;
}


//*************************************
// Port waiting time setup
//*************************************


void port_timeout(int timeout) {

bzero(&sioparm, sizeof(sioparm)); // Preparing the Termios attribute block

sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
sioparm.c_iflag = 0;  // Inpck;

sioparm.c_oflag = 0;
sioparm.c_lflag = 0;
sioparm.c_cc[VTIME]=timeout; // Timeout  

sioparm.c_cc[VMIN]=0;  
tcsetattr(siofd, TCSANOW, &sioparm);
}

//*************************************************
//*Search for a file by number in the specified catalog
//*
//*num -# file
//*Filename -Boofer for the full file name
//*ID -a variable in which the section identifier will be recorded
//*
//*return 0 -not found
//*1 -found
//*************************************************

int find_file(int num, char* dirname, char* filename,unsigned int* id, unsigned int* size) {

DIR* fdir;
FILE* in;
unsigned int pt;
struct dirent* dentry;
char fpattern[5];

sprintf(fpattern,"%02i",num); // Sample for finding a file 3 numbers

fdir=opendir(dirname);
if (fdir == 0) {
  printf("\n Directory %s cannot be opened\n",dirname);
  exit(1);
}

// The main cycle is to search for the file we need

while ((dentry=readdir(fdir)) != 0) {
  if (dentry->d_type != DT_REG) continue; // We miss everything except regular files

  if (strncmp(dentry->d_name,fpattern,2) == 0) break; // Found the desired file. More precisely, a file with the required 3 digits at the beginning of the name.

}

closedir(fdir);
// We form the full file name in the result of the result

if (dentry == 0) return 0; // Not found

strcpy(filename,dirname);
strcat(filename,"/");
// Copy the file name in the result of the result

strcat(filename,dentry->d_name);  

// 00-00000200-m3boot.bin
//Check the file name for the presence of signs '-'

if ((dentry->d_name[2] != '-') || (dentry->d_name[11] != '-')) {
  printf("\n Incorrect file name format- %s\n",dentry->d_name);
  exit(1);
}

// Check the digital field of the ID section

if (strspn(dentry->d_name+3,"0123456789AaBbCcDdEeFf") != 8) {
  printf("\n Error in section identifier - non-numeric character - %s\n",filename);
  exit(1);
}  
sscanf(dentry->d_name+3,"%8x",id);

// Check the availability and readability of the file

in=fopen(filename,"r");
if (in == 0) {
  printf("\n Error opening file %s\n",filename);
  exit(1);
}
if (fread(&pt,1,4,in) != 4) {
  printf("\n Error reading file %s\n",filename);
  exit(1);
}
  
// Check that the file is a raw image, without a title

if (pt == 0xa55aaa55) {
  printf("\nFile %s has a header - not suitable for flashing\n",filename);
  exit(1);
}


// What else can you check? I have not come up with it yet.  


//  We get the file size

fseek(in,0,SEEK_END);
*size=ftell(in);
fclose(in);

return 1;
}

//****************************************************
//*Demonial to the AT-Komanda Modem
//*  
//*CMD -team buffer
//*rbuf -Boofer for recording response
//*
//*Returns the length of the answer
//****************************************************

int atcmd(char* cmd, char* rbuf) {

int res;
char cbuf[128];

strcpy(cbuf,"AT");
strcat(cbuf,cmd);
strcat(cbuf,"\r");

port_timeout(100);
// We clean the boiler of the receiver and transmitter

tcflush(siofd,TCIOFLUSH);

// Sending the team

write(siofd,cbuf,strlen(cbuf));
usleep(100000);

// Reading the result

res=read(siofd,rbuf,200);
return res;
}
  
