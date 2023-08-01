#include <stdio.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>


#define DEV_MANUFACTURER 0x0644
#define DEV_PRODUCT 0x8071

#define BUFSIZE 64
#define DUMP_LEN 16

#define PORT12 0x01
#define PORT34 0x03

#define STEREO_ENA  0x04
#define STEREO_DISA 0x01

#define LOOP_ENA    0x01
#define LOOP_DISA   0x00

int verbose_flag = 0;

char cmd3101[4] = {0x31, 0x01, 0x00, 0x00};
char cmd1801[4] = {0x18, 0x01, 0x00, 0x00};


void dump_buf(const char* buf, int len)
{

  printf("\n-----\n");

  int i = 0;

  char txt [DUMP_LEN + 1];
  int txtlen = 0;
  int line_cntr = 0;
   
  memset (txt, 0x0, DUMP_LEN + 1);
   
  for (i = 0; i < len; i++)
    {
      printf("%.2x ", (uint8_t)buf[i]);
      txt[txtlen++] = buf[i];

      if (DUMP_LEN == line_cntr)
	{
	  printf ("\t");

	  for (int j = 0; j < line_cntr; j++)
	    printf ("%c", txt[j]);
	 
	  printf("\n");
      
	  txtlen = 0;
	  line_cntr = 0;
	  memset(txt, 0x0, DUMP_LEN+1);
	}

      line_cntr++;
    }

  if (0 != txtlen)
    printf ("\t%s\n", txt);

  printf("\n-----\n");
}


int do_control_transfer(libusb_device_handle *devh, char* cmd_buf, int len)
{
  int libusb_control_transferRet;

  printf("---> do_control_transfer\n");

  //devicehandle, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout
  libusb_control_transferRet =
    libusb_control_transfer(devh,
			    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
			    0x1d,
			    0,
			    0,
			    cmd_buf,
			    (uint16_t) len,
			    500
			    );

  printf("-->do_control_transfer\n");
  dump_buf(cmd_buf, len);

  return libusb_control_transferRet;
}


int do_control_transfer_read(libusb_device_handle *devh, char* cmd_buf, int len)
{
  //read
  //devicehandle, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout
  int libusb_control_transferRet = libusb_control_transfer(devh, LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN, 0x1e, 0, 0, cmd_buf, len, 1500);
  printf("-->do_control_transfer_read\n\n");

  if (libusb_control_transferRet) dump_buf(cmd_buf, libusb_control_transferRet);

  return libusb_control_transferRet;
}


/**
 * @brief enables or disables the input port
 * @input input 1 -4
 * @ena 0 to disable, 1 to enable
 */
int input_ena (libusb_device_handle *devh, int input, int ena)
{
  int libusb_control_transferRet;


  //                                                    |input/5          |ena/8
  char conf_block [11] = {0x61, 0x02, 0x04, 0x62, 0x02, 0x03, 0x83, 0x02, 0x00, 0x00, 0x00};
  printf("---> enable_input %d, %d\n", input, ena);


  if (0 >= input || 4 < input)
    {
      printf("---> enable_input input %d is out of range\n", input);
      return -1;
    }
  
  conf_block[5] = (uint8_t) input;

  if (0 == ena)
    conf_block[8] = 0x01;

  libusb_control_transferRet = do_control_transfer(devh, conf_block, sizeof(conf_block));

  return libusb_control_transferRet;
}


/**
 * @brief enables or disables stereo on the given port pair
 * @port either PORT12 or PORT34
 * @ena 0 to disable, 1 to enable stereo
 */
int stereo_ena (libusb_device_handle *devh, int port, int ena)
{
  int libusb_control_transferRet;

  //12 ena stereo
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x01, 0x86, 0x02, 0x04, 0x00, 0x00
  //12 disa stereo
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x01, 0x86, 0x02, 0x01, 0x00, 0x00

  //34 ena stereo
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x03, 0x86, 0x02, 0x04, 0x00, 0x00"
  //34 disa stereo
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x03, 0x86, 0x02, 0x01, 0x00, 0x00

  //                                                    |input/5          |ena/8
  char conf_block [11] = {0x61, 0x02, 0x05, 0x62, 0x02, 0x01, 0x86, 0x02, 0x01, 0x00, 0x00};

  printf("---> stereo_ena %d, %d\n", port, ena);


  if (PORT12 != port &&  PORT34 != port)
    {
      printf("---> stereo_ena: port %d is out of range\n", port);
      return -1;
    }
  
  conf_block[5] = (uint8_t) port;

  if (0 == ena)
    conf_block[8] = STEREO_DISA;
  else
    conf_block[8] = STEREO_ENA;

  libusb_control_transferRet = do_control_transfer(devh, conf_block, sizeof(conf_block));
  
  return libusb_control_transferRet;
}


/**
 * @brief enables or disables mix mode on the given port pair
 * @port either PORT12 or PORT34
 * @ena 0 to disable mix and enable lineout, 1 to enable mix
 */
int out_mix_mode (libusb_device_handle *devh, int port, int ena)
{
  int libusb_control_transferRet;

  //12 ena mix                     | /5                                            | /13
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x01, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x01, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x02, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x02, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00

  //12 line out
  //0x61, 0x02, 0x03, 0x62, 0x02, 0x01, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x01, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00
  //0x61, 0x02, 0x03, 0x62, 0x02, 0x02, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x02, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00

  //l34 ena mix
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x01, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x03, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00
  //0x61, 0x02, 0x05, 0x62, 0x02, 0x02, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x04, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00

  //l34 ena line out
  //0x61, 0x02, 0x03, 0x62, 0x02, 0x03, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x03, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00
  //0x61, 0x02, 0x03, 0x62, 0x02, 0x04, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x04, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00

  char conf_block [20] = {
    0x61, 0x02, 0x05, 0x62, 0x02, 0x01, 0x41, 0x01, 0x61, 0x02, 0x01, 0x62, 0x02, 0x01, 0x42, 0x01, 0x43, 0x01, 0x00, 0x00
  };

  printf("---> out_mix_mode %d, %d\n", port, ena);

  uint8_t p2_1;
  uint8_t p2_2;
  uint8_t p5_1;
  uint8_t p5_2;
  uint8_t p13_1;
  uint8_t p13_2;
  

  switch (port)
    {
    case PORT12:
      printf("---> out_mix_mode: port12 (%d)\n", port);
      
      if (ena)
	{
	  p2_1 = 0x05;
	  p2_2 = 0x05;
	  p5_1 = 0x01;
	  p5_2 = 0x02;
	  p13_1 = 0x01;
	  p13_2 = 0x02;
	}
      else
	{
	  p2_1 = 0x03;
	  p2_2 = 0x03;
	  p5_1 = 0x01;
	  p5_2 = 0x02;
	  p13_1 = 0x01;
	  p13_2 = 0x02;
	}
      break;

    case PORT34:
      printf("---> out_mix_mode: port34 (%d)\n", port);

      if (ena)
	{
	  p2_1 = 0x05;
	  p2_2 = 0x05;
	  p5_1 = 0x01;
	  p5_2 = 0x02;
	  p13_1 = 0x03;
	  p13_2 = 0x04;
	}
      else
	{
	  p2_1 = 0x03;
	  p2_2 = 0x03;
	  p5_1 = 0x03;
	  p5_2 = 0x04;
	  p13_1 = 0x03;
	  p13_2 = 0x04;
	}
      break;

    default:
      printf("---> out_mix_mode: port (%d) is out of range\n", port);
      return -1;

    }

  conf_block[2] = (char)p2_1;
  conf_block[5] = (char)p5_1;
  conf_block[13] = (char)p13_1;

  libusb_control_transferRet = do_control_transfer(devh, conf_block, sizeof(conf_block));

  if (0 >= libusb_control_transferRet)
    return libusb_control_transferRet;

  conf_block[2] = (char)p2_2;
  conf_block[5] = (char)p5_2;
  conf_block[13] = (char)p13_2;

  libusb_control_transferRet = do_control_transfer(devh, conf_block, sizeof(conf_block));
  
  return libusb_control_transferRet;
}



/**
 * @brief enables or disables loopback
 * @ena 0 to disable, 1 to enable stereo
 */
int loop_ena (libusb_device_handle *devh, int ena)
{
  int libusb_control_transferRet;
//                                                               |1 to ena /8
//loopback ena " 0x61, 0x02, 0x06, 0x62, 0x02, 0x00, 0x81, 0x02, 0x01, 0x00, 0x00"
//loopback off " 0x61, 0x02, 0x06, 0x62, 0x02, 0x00, 0x81, 0x02, 0x00, 0x00, 0x00"

  //                                                                      |ena/8
  char conf_block [11] = {0x61, 0x02, 0x06, 0x62, 0x02, 0x00, 0x81, 0x02, 0x00, 0x00, 0x00};

  printf("---> loopback_ena %d\n", ena);

  if (0 == ena)
    conf_block[8] = LOOP_DISA;
  else
    conf_block[8] = LOOP_ENA;

  libusb_control_transferRet = do_control_transfer(devh, conf_block, sizeof(conf_block));
  
  return libusb_control_transferRet;
}



int powersave_ena (libusb_device_handle *devh, int ena)
{
  int libusb_control_transferRet;

  //power save off
  //"0x15, 0x02, 0x00, 0x00, 0x00"
  //power save on
  //"0x15, 0x02, 0x01, 0x00, 0x00"

  char conf_block [5] = {0x15, 0x02, 0x00, 0x00, 0x00};

  printf("---> powersave_ena %d\n", ena);

  if (ena)
    conf_block[2] = 0x01;
  else
    conf_block[2] = 0x00;

  libusb_control_transferRet = do_control_transfer(devh, conf_block, sizeof(conf_block));
  
  return libusb_control_transferRet;
}




int control_transfer_show_result (int res)
{
  
  switch (res)
    {
    case LIBUSB_ERROR_TIMEOUT:
      printf("timeout\n");
      printf("%s %s %u: libusb_control_transfer() failed\n", __FILE__, __func__, __LINE__);
      break;
    case LIBUSB_ERROR_PIPE:
      printf("pipe\n");
      printf("%s %s %u: libusb_control_transfer() failed\n", __FILE__, __func__, __LINE__);
      break;
    case LIBUSB_ERROR_NO_DEVICE:
      printf("no dev\n");
      printf("%s %s %u: libusb_control_transfer() failed\n", __FILE__, __func__, __LINE__);
      break;
    default:
      if (res < 0)
	{
	  printf("%s %s %u: libusb_control_transfer() failed\n", __FILE__, __func__, __LINE__);
	}
    }
}


void print_usage ()
{
    printf ("options: \n");
    printf ("--stereo-mode12, -s: enable stereo for inputs 1/2. 1 to enable, 0 to disable\n");
    printf ("--stereo-mode34, -S: enable stereo for inputs 3/4. 1 to enable, 0 to disable\n");
    printf ("--enable-input, -e: enable input. valid argument range [1-4]\n");
    printf ("--disable-input, -d: disable input. valid argument range [1-4]\n");
    printf ("--enable-loopback, -l: enable loopback\n");
    printf ("--disable-loopback, -L: disable loopback\n");
    printf ("--lineout12, -o: lineout on port 12\n");
    printf ("--lineout34, -O: lineout on port 34\n");
    printf ("--mixout12, -m: mix out on port 12\n");
    printf ("--mixout34, -M: mix out on port 34\n");
    printf ("--enable-powersave, -p: enable power saving mode\n");
    printf ("--disable-powersave, -P: disable power saving mode\n");    
    printf ("--verbose, -v: increase verbosity (doesn't accept the argument)\n");
    printf ("--help, -h: shows this help message\n");
}


int main(int argc, char* argv[])
{
  int c;
  uint8_t ok_flag;

  int opt_input_ena = 0;
  int opt_input_disa = 0;
  int opt_stereo12 = 0;
  int opt_stereo34 = 0;
  int opt_stereo_mode12 = 0;
  int opt_stereo_mode34 = 0;
  int opt_loopback_disa = 0;
  int opt_loopback_ena = 0;
  int opt_lineout12 = 0;
  int opt_lineout34 = 0;
  int opt_mixout12 = 0;
  int opt_mixout34 = 0;
  int opt_powersave_ena = 0;
  int opt_powersave_disa = 0;

   while (1)
    {
      static struct option long_options[] =
        {
          /* These options set a flag. */
          {"help", no_argument,       0, 'h'},
          {"verbose", no_argument,       0, 'v'},
          {"brief",   no_argument,       0, 'b'},
 	  {"enable-loopback",  no_argument, 0, 'l'},
	  {"disable-loopback",  no_argument, 0, 'L'},
	  {"lineout12",  no_argument, 0, 'o'},
	  {"lineout34",  no_argument, 0, 'O'},
	  {"mixout12",  no_argument, 0, 'm'},
	  {"mixout34",  no_argument, 0, 'M'},
	  {"enable-powersave",  no_argument, 0, 'p'},
	  {"disable-powersave",  no_argument, 0, 'P'},
          /* These options don't set a flag.
             We distinguish them by their indices. */
          {"stereo-mode12",     required_argument,       0, 's'},
	  {"stereo-mode34",     required_argument,       0, 'S'},
          {"enable-input",  required_argument, 0, 'e'},
          {"disable-input",  required_argument, 0, 'd'},
	  
          {0, 0, 0, 0}
        };
      
      /* getopt_long stores the option index here. */
      int option_index = 0;

      c = getopt_long (argc, argv, "hvblLoOmMspP:S:e:d:",
                       long_options, &option_index);

      /* Detect the end of the options. */
      if (c == -1)
        break;

      switch (c)
        {

        case 0:
          /* If this option set a flag, do nothing else now. */
          if (long_options[option_index].flag != 0)
            break;
          printf ("option %s", long_options[option_index].name);
          if (optarg)
            printf (" with arg %s", optarg);
          printf ("\n");
          break;

	case 'h':
	  print_usage();
	  exit(0);
	  break;

        case 'v':
          puts ("option: verbose\n");
	  verbose_flag = 1;
          break;

        case 'b':
          puts ("option: brief\n");
	  verbose_flag = 0;
          break;

        case 's':
          printf ("option stereo-mode12 with value '%s'\n", optarg);
	  opt_stereo_mode12 = atoi(optarg);
	  opt_stereo12 = 1;
          break;

        case 'S':
          printf ("option stereo-mode34 with value '%s'\n", optarg);
	  opt_stereo_mode34 = atoi(optarg);
	  opt_stereo34 = 1;
          break;

        case 'e':
          printf ("option enable-input with value '%s'\n", optarg);
	  opt_input_ena = atoi(optarg);
          break;

        case 'd':
          printf ("option disable-input with value '%s'\n", optarg);
	  opt_input_disa = atoi(optarg);
          break;

        case 'l':
          printf ("option enable-loopback\n");
	  opt_loopback_ena = 1;
          break;

        case 'L':
          printf ("option disable-loopback\n");
	  opt_loopback_disa = 1;
          break;

        case 'o':
          printf ("option lineout12\n");
	  opt_lineout12 = 1;
          break;
	  
        case 'O':
          printf ("option lineout34\n");
	  opt_lineout34 = 1;
          break;
	  
        case 'm':
          printf ("option mixout12\n");
	  opt_mixout12 = 1;
          break;
	  
        case 'M':
          printf ("option mixout34\n");
	  opt_mixout34 = 1;
          break;
	  
        case 'p':
          printf ("option enable-powersave\n");
	  opt_powersave_ena = 1;
          break;

        case 'P':
          printf ("option disable-powersave\n");
	  opt_powersave_disa = 1;
          break;

        case '?':
          /* getopt_long already printed an error message. */
          break;

        default:
          abort ();
        }
    }

     
  if (libusb_init(NULL) != 0)
    {
      printf("%s %s %u: libusb_init() failed\n", __FILE__, __func__, __LINE__);
      return -1;
    }
   
  libusb_set_debug(NULL, 4);
  libusb_device_handle* devh = libusb_open_device_with_vid_pid(NULL, DEV_MANUFACTURER, DEV_PRODUCT);

  if (devh == NULL)
    {
      printf("%s %s %u: libusb_open_device_with_vid_pid() failed: need to be root\n", __FILE__, __func__, __LINE__);
      return -1;
    }
   
  int command = 0;
  int libusb_control_transferRet = 0;


  int icmd = 0;

      
  usleep(100*1000);
      
  char buf [BUFSIZE];
  memset(buf, 0x0, BUFSIZE);

  printf("-->read\n\n");
  //devicehandle, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout
  libusb_control_transferRet =
    libusb_control_transfer(
			    devh,
			    LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
			    0x1e,
			    0,
			    0,
			    buf,
			    BUFSIZE,
			    500
			    );

  control_transfer_show_result (libusb_control_transferRet);
  
  if (libusb_control_transferRet) dump_buf(buf, libusb_control_transferRet);

  int res = 0;

  printf("****===============\n");
  if (opt_input_ena)
    {
      res = input_ena(devh, opt_input_ena, 1);
    }
  else if (opt_input_disa)
    {
      res = input_ena(devh, opt_input_disa, 0);
    }
  else if (opt_stereo12)
    {
      res = stereo_ena(devh, PORT12, opt_stereo_mode12);
    }
  else if (opt_stereo34)
    {
      res = stereo_ena(devh, PORT34, opt_stereo_mode34);
    }
  else if (opt_loopback_ena)
    {
      res = loop_ena(devh, 1);
    }
  else if (opt_loopback_disa)
    {
      res = loop_ena(devh, 0);
    }
  else if (opt_lineout12)
    {
      res = out_mix_mode(devh, PORT12, 0);
    }
  else if (opt_lineout34)
    {
      res = out_mix_mode(devh, PORT34, 0);
    }
  else if (opt_mixout12)
    {
      res = out_mix_mode(devh, PORT12, 1);
    }
  else if (opt_mixout34)
    {
      res = out_mix_mode(devh, PORT34, 1);
    }
  else if (opt_powersave_ena)
    {
      res = powersave_ena(devh, 1);
    }
  else if (opt_powersave_disa)
    {
      res = powersave_ena(devh, 0);
    }


    printf("***===============\n");

  control_transfer_show_result (res);


  usleep(500*1000);
    
  /*
  usleep(500*1000);
  
  memset(buf, 0x0, BUFSIZE);
  do_control_transfer_read(devh, buf, BUFSIZE);

  
  usleep(500*1000);
  do_control_transfer(devh, cmd3101, sizeof(cmd3101));

  memset(buf, 0x0, BUFSIZE);
  usleep(500*1000);
  do_control_transfer_read(devh, buf, BUFSIZE);

  memset(buf, 0x0, BUFSIZE);
  usleep(500*1000);
  do_control_transfer_read(devh, buf, BUFSIZE);

  usleep(500*1000);
  do_control_transfer(devh, cmd1801, sizeof(cmd1801));

  memset(buf, 0x0, BUFSIZE);
  usleep(500*1000);
  do_control_transfer_read(devh, buf, BUFSIZE);
  */

  libusb_close(devh);
  libusb_exit(NULL);
  return 0;
}
