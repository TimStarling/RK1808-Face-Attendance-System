#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <linux/input.h>
#include <pthread.h>
#include <linux/videodev2.h>
#include  <sys/ioctl.h>
#include  <unistd.h>
#include  <fcntl.h>
#include  <sys/mman.h>
#include  <string.h>

int  init_video();

int  get_bmp(int fd,const char * name);

int get_rgb_frame(int fd, unsigned char *rgb_buf, unsigned int rgb_size);

int save_rgb_bmp(unsigned char *rgb, const char *bmp_file);

void free_video(int fd);
