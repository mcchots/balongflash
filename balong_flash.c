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

// flag of the file structure error
unsigned int errflag=0;

// digital signature flag
int gflag=0;
// flag of the firmware type
int dflag=0;

// type of firmware from file header
int dload_id=-1;

//***********************************************
// * Partition table
//***********************************************
struct ptb_t ptable[120];
int npart=0 ; // number of sections in the table


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int main(int argc, char* argv[]) {

unsigned int opt;
int res;
FILE* in;
char devname[50] = "";
unsigned int  mflag=0,eflag=0,rflag=0,sflag=0,nflag=0,kflag=0,fflag=0;
unsigned char fdir[40];   // directory for multi-file firmware

// parse command line
while ((opt = getopt(argc, argv, "d:hp:mersng:kf")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n The utility is designed for flashing modems on the Balong V7 chipset\n\n\
 %s [keys] <file name to load or file directory name> \n\n\
 The following keys are valid:\n\n"
#ifndef WIN32
" -p <tty> - serial port for communication with the bootloader (default / dev / ttyUSB0)\n"
#else
" -p # - serial port number to communicate with the bootloader (for example, -p8) \n"
"   if the -p option is not specified, auto-detect the port \n"
#endif
" -n      - multifile firmware mode from the specified directory\n\
 -g#      - set the digital signature mode\n\
   -gl - parameters description\n\
   -gd - disallow signature auto-detection\n\
 -m       - output the firmware file map and exit\n\
 -e       - parse the firmware file into sections without headers\n\
 -s       - parse the firmware file into sections with headers\n\
 -k       - do not restart the modem at the end of the firmware\n\
 -r       - force reboot the modem without flashing partitions\n\
 -f       - flash even if there are CRC errors in the source file\n\
 -d#      - set firmware type (DLOAD_ID, 0..7), -dl - type list\n\
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
printf("\n Program for flashing devices on the Balong chipset, V3.0. % i , (c) forth32, 2015, GNU GPLv3",BUILDNO);
#ifdef WIN32
printf("\n Port for Windows 32bit  (c) rust3028, 2016");
#endif
printf("\n--------------------------------------------------------------------------------------------------\n");

if (eflag&sflag) {
  printf("\n The -s and -e options are incompatible\n");
  return -1;
}  

if (kflag&rflag) {
  printf("\n The -k and -r options are incompatible\n");
  return -1;
}  

if (nflag&(eflag|sflag|mflag)) {
  printf("\n The -n switch is incompatible with the -s, -m and -e switches\n");
  return -1;
}  
  

// ------ restart without specifying a file
//--------------------------------------------
if ((optind>=argc)&rflag) goto sio; 


// Open input file
//--------------------------------------------
if (optind>=argc) {
  if (nflag)
    printf("\n - File directory not specified\n");
  else 
    printf("\n - File name is not specified for download, use the -h switch for the hint\n");
  return -1;
}  

if (nflag) 
  // for -n - just copy the prefix
  strncpy(fdir,argv[optind],39);
else {
  // for single-file operations
in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\n Error opening %s",argv[optind]);
  return -1;
}
}


// Search for sections within the file
if (!nflag) {
  findparts(in);
  show_fw_info();
}  

// Search for firmware files in the specified directory
else findfiles(fdir);
  
// ------ Output mode of the firmware file card
if (mflag) show_file_map();

// exit by CRC error
if (!fflag && errflag) {
    printf("\n\n! The input file contains errors - exiting.\n");
    return -1; 
}

// ------- The mode of cutting the firmware file
if (eflag|sflag) {
  fwsplit(sflag);
  printf("\n");
  return 0;
}

sio:
// --------- Main mode - write firmware
//--------------------------------------------

// Configure SIO
open_port(devname);

// Determine the port mode and version of the dload protocol

res=dloadversion();
if (res == -1) return -2;
if (res == 0) {
  printf("\n The modem is already in HDLC mode");
  goto hdlc;
}

// If necessary, send the digital signature command
if (gflag != -1) send_signver();

// Enter HDLC mode

usleep(100000);
enter_hdlc();

// Entered HDLC
//------------------------------
hdlc:

// get the protocol version and device identifier
protocol_version();
dev_ident();


printf("\n----------------------------------------------------\n");

if ((optind>=argc)&rflag) {
  // reboot without specifying a file
  restart_modem();
  exit(0);
}  

// Write the entire flash drive
flash_all();
printf("\n");

port_timeout(1);

// exit HDLC mode and reboot
if (rflag || !kflag) restart_modem();
// exit HDLC without rebooting
else leave_hdlc();
} 
