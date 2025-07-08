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
#include "signver.h"
#include "zlib.h"

// File structure error flag

unsigned int errflag=0;

// Digital signature flag

int gflag=0;
// Firmware type flag

int dflag=0;

// Type of firmware from the file title

int dload_id=-1;

//***********************************************
//*Table of sections
//***********************************************

struct ptb_t ptable[120];
int npart=0; // The number of sections in the table





int main(int argc, char* argv[]) {

unsigned int opt;
int res;
FILE* in;
char devname[50] = "";
unsigned int  mflag=0,eflag=0,rflag=0,sflag=0,nflag=0,kflag=0,fflag=0;
unsigned char fdir[40];   // Catalog for multi -featured firmware


// The analysis of the command line

while ((opt = getopt(argc, argv, "d:hp:mersng:kf")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\nThe utility is designed to flash modems on the Balong V7 chipset\n\n\
%s [keys] <file name to download or directory name with files>\n\n\
The following keys are allowed:\n\n"
#ifndef WIN32
"-p <tty>- serial port for communication with the bootloader (default /dev/ttyUSB0)\n"
#else
"-p # - serial port number for communication with the bootloader (for example, -p8)\n"
" if the -p switch is not specified, the port is automatically detected\n"
#endif
"-n       - multifile firmware mode from the specified directory\n\
-g# - set digital signature mode\n\
-gl - parameter description\n\
-gd - disable signature autodetection\n\
-m - output firmware file map and exit\n\
-e - parse firmware file into sections without headers\n\
-s - parse firmware file into sections with headers\n\
-k - do not reboot modem after firmware is finished\n\
-r - force reboot modem without flashing sections\n\
-f - flash even if there are CRC errors in the source file\n\
-d# - set firmware type (DLOAD_ID, 0..7), -dl - list of types\n\
\n",argv[0]);
    return 0;

   case 'p':
    strcpy(devname,optarg);
    break;

   case 'm':
     mflag=1;
     break;
     
   case 'n':
     nflag=1;
     break;
     
   case 'f':
     fflag=1;
     break;
     
   case 'r':
     rflag=1;
     break;
     
   case 'k':
     kflag=1;
     break;
     
   case 'e':
     eflag=1;
     break;

   case 's':
     sflag=1;
     break;

   case 'g':
     gparm(optarg);
     break;
     
   case 'd':
     dparm(optarg);
     break;
     
   case '?':
   case ':':  
     return -1;
  }
}  
printf("\n Программа для прошивки устройств на Balong-чипсете, V3.0.%i, (c) forth32, 2015, GNU GPLv3",BUILDNO);
#ifdef WIN32
printf("\n Windows 32bit version  (c) rust3028, 2016");
#endif
printf("\n--------------------------------------------------------------------------------------------------\n");

if (eflag&sflag) {
  printf("\n The -s and -e switches are incompatible\n");
  return -1;
}  

if (kflag&rflag) {
  printf("\nThe -k and -r switches are incompatible\n");
  return -1;
}  

if (nflag&(eflag|sflag|mflag)) {
  printf("\n The -n switch is incompatible with the -s, -m and -e switches.\n");
  return -1;
}  
  

// -----Rebooting without specifying a file
//--------------------------------------------

if ((optind>=argc)&rflag) goto sio; 


// Opening the input file
//--------------------------------------------

if (optind>=argc) {
  if (nflag)
    printf("\n -Directory with files is not specified\n");
  else 
    printf("\n - No file name specified for download, use -h switch for hint\n");
  return -1;
}  

if (nflag) 
  // for -n -just copy the prefix

  strncpy(fdir,argv[optind],39);
else {
  // for single -file operations

in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\n Error opening %s",argv[optind]);
  return -1;
}
}


// Search for sections inside the file

if (!nflag) {
  findparts(in);
  show_fw_info();
}  

// Search for firmware files in the specified catalog

else findfiles(fdir);
  
//------The display mode of the firmware file card

if (mflag) show_file_map();

// Exit by errors CRC

if (!fflag && errflag) {
    printf("\n\n!Input file contains errors - quitting\n");
    return -1; 
}

//--------Firmware cutting mode

if (eflag|sflag) {
  fwsplit(sflag);
  printf("\n");
  return 0;
}

sio:
//---------The main mode-recording firmware
//--------------------------------------------


// Setting sio

open_port(devname);

// Determine the port mode and the version of the Dload-protocol


res=dloadversion();
if (res == -1) return -2;
if (res == 0) {
  printf("\n The modem is already in HDLC mode");
  goto hdlc;
}

// If necessary, send a digital signature command

if (gflag != -1) send_signver();

// We enter the HDLC mode


usleep(100000);
enter_hdlc();

// We entered HDLC
//------------------------------

hdlc:

// We get the version of the protocol and the device identifier

protocol_version();
dev_ident();


printf("\n----------------------------------------------------\n");

if ((optind>=argc)&rflag) {
  // Reloading without file specification

  restart_modem();
  exit(0);
}  

// Record the whole flash drive

flash_all();
printf("\n");

port_timeout(1);

// We leave the HDLC mode and reboot

if (rflag || !kflag) restart_modem();
// Exit from HDLC without rebooting

else leave_hdlc();
} 
