//  Low -level procedures for working with a sequential port and HDLC


#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <io.h>
#include "printf.h"

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

static HANDLE hSerial;

static int read(int siofd, void* buf, unsigned int len)
{
    DWORD bytes_read = 0;

    ReadFile(hSerial, buf, len, &bytes_read, NULL);
 
    return bytes_read;
}

static int write(int siofd, void* buf, unsigned int len)
{
    DWORD bytes_written = 0;

    WriteFile(hSerial, buf, len, &bytes_written, NULL);

    return bytes_written;
}

//*************************************************
//*Boofer reference to the modem
//*************************************************

unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen) {


PurgeComm(hSerial, PURGE_RXCLEAR);

write(siofd,"\x7e",1);  // We send the prefix


if (write(siofd,outcmdbuf,outlen) == 0) {   printf("\n Command write error");return 0;  }
FlushFileBuffers(hSerial);

return 1;
}

//******************************************************************************************
//*Reception of the buffer with the answer from the modem
//*
//*Masslen -the number of bytes taken by a single block without analysis of the end of the end of 7f
//******************************************************************************************


unsigned int receive_reply(char* iobuf, int masslen) {
  
int i,iolen,escflag,bcnt,incount;
unsigned char c;
unsigned int res;
unsigned char replybuf[14000];

incount=0;
if (read(siofd,&c,1) != 1) {
//  Printf ("\ n there is no answer from the modem");

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

int i,iolen,escflag,bcnt,incount;
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

DEFINE_GUID(GUID_DEVCLASS_PORTS, 0x4D36E978, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18);

static int find_port(int* port_no, char* port_name)
{
  HDEVINFO device_info_set;
  DWORD member_index = 0;
  SP_DEVINFO_DATA device_info_data;
  DWORD reg_data_type;
  char property_buffer[256];
  DWORD required_size;
  char* p;
  int result = 1;

  device_info_set = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, 0, DIGCF_PRESENT);

  if (device_info_set == INVALID_HANDLE_VALUE)
    return result;

  while (TRUE)
  {
    ZeroMemory(&device_info_data, sizeof(SP_DEVINFO_DATA));
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiEnumDeviceInfo(device_info_set, member_index, &device_info_data))
      break;

    member_index++;

    if (!SetupDiGetDeviceRegistryPropertyA(device_info_set, &device_info_data, SPDRP_HARDWAREID,
             &reg_data_type, (PBYTE)property_buffer, sizeof(property_buffer), &required_size))
      continue;

    if (
        (
         strstr(_strupr(property_buffer), "VID_12D1&PID_1C05") != NULL &&
         strstr(_strupr(property_buffer), "&MI_02") != NULL
        ) ||
        (
         strstr(_strupr(property_buffer), "VID_12D1&PID_1442") != NULL &&
         strstr(_strupr(property_buffer), "&MI_00") != NULL
        ) ||
        (
         strstr(_strupr(property_buffer), "VID_12D1&PID_2020") != NULL
        )
       )
    {
      if (SetupDiGetDeviceRegistryPropertyA(device_info_set, &device_info_data, SPDRP_FRIENDLYNAME,
              &reg_data_type, (PBYTE)property_buffer, sizeof(property_buffer), &required_size))
      {
        p = strstr(property_buffer, " (COM");
        if (p != NULL)
        {
          *port_no = atoi(p + 5);
          strcpy(port_name, property_buffer);
          result = 0;
        }
      }
      break;
    }
  }

  SetupDiDestroyDeviceInfoList(device_info_set);

  return result;
}

//***************************************************
// Opening and setting up a sequential port
//***************************************************


int open_port(char* devname) {

DCB dcbSerialParams = {0};
COMMTIMEOUTS CommTimeouts;

char device[20] = "\\\\.\\COM";
int port_no;
char port_name[256];

if (*devname == '\0')
{
  printf("\n\nSearch for firmware port...\n");
  
  if (find_port(&port_no, port_name) == 0)
  {
    sprintf(devname, "%d", port_no);
    printf("Port: \"%s\"\n", port_name);
  }
  else
  {
    printf("Port not found!\n");
    exit(0); 
  }
    //Printf ("\n! -the sequential port is not set \n"); 
    //Exit (0); 

}

strcat(device, devname);

hSerial = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
if (hSerial == INVALID_HANDLE_VALUE)
{
   printf("\n! - COM Serial port %s does not open\n", devname); 
   exit(0); 
}

ZeroMemory(&dcbSerialParams, sizeof(dcbSerialParams));
dcbSerialParams.DCBlength=sizeof(dcbSerialParams);
dcbSerialParams.BaudRate = CBR_115200;
dcbSerialParams.ByteSize = 8;
dcbSerialParams.StopBits = ONESTOPBIT;
dcbSerialParams.Parity = NOPARITY;
dcbSerialParams.fBinary = TRUE;
dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;
if (!SetCommState(hSerial, &dcbSerialParams))
{
    CloseHandle(hSerial);
    printf("\n! - Error initializing COM port\n"); 
    exit(0); 
    //return -1;

}

CommTimeouts.ReadIntervalTimeout = 5;
CommTimeouts.ReadTotalTimeoutConstant = 30000;
CommTimeouts.ReadTotalTimeoutMultiplier = 0;
CommTimeouts.WriteTotalTimeoutConstant = 0;
CommTimeouts.WriteTotalTimeoutMultiplier = 0;
if (!SetCommTimeouts(hSerial, &CommTimeouts))
{
    CloseHandle(hSerial);
    printf("\n! - Error initializing COM port\n"); 
    exit(0); 
}

PurgeComm(hSerial, PURGE_RXCLEAR);

return 1;
}


//*************************************
// Port waiting time setup
//*************************************


void port_timeout(int timeout) {
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


char fpattern[80];
char fname[_MAX_PATH];
struct _finddata_t fileinfo;
intptr_t res;
FILE* in;
unsigned int pt;

sprintf(fpattern,"%s\\%02d*", dirname, num);
res = _findfirst(fpattern, &fileinfo);
_findclose(res);
if (res == -1)
    return 0;
if ((fileinfo.attrib & _A_SUBDIR) != 0)
    return 0;
strcpy(fname, fileinfo.name);
strcpy(filename, dirname);
strcat(filename, "\\");
strcat(filename, fname);  

// 00-00000200-m3boot.bin
//Check the file name for the presence of signs '-'

if (fname[2] != '-' || fname[11] != '-') {
  printf("\n Incorrect file name format -%s\n",fname);
  exit(1);
}

// Check the digital field of the ID section

if (strspn(fname+3,"0123456789AaBbCcDdEeFf") != 8) {
  printf("\n Error in section identifier - non-numeric character - %s\n",filename);
  exit(1);
}  
sscanf(fname+3,"%8x",id);

// Check the availability and readability of the file

in=fopen(filename,"rb");
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
  printf("\n File %s has a header - not suitable for flashing\n",filename);
  exit(1);
}

fclose(in);

*size = fileinfo.size;


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

PurgeComm(hSerial, PURGE_RXCLEAR);

// Sending the team

write(siofd,cbuf,strlen(cbuf));
Sleep(100);

// Reading the result

res=read(siofd,rbuf,200);
return res;
}
  
