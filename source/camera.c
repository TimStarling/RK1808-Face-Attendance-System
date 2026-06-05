#include "camera.h"
#include <errno.h>

//定义BMP 的头数据
typedef struct                       /**** BMP file header structure ****/  
{  
    unsigned int   bfSize;           /* Size of file */  
    unsigned short bfReserved1;      /* Reserved */  
    unsigned short bfReserved2;      /* ... */  
    unsigned int   bfOffBits;        /* Offset to bitmap data */  
} MyBITMAPFILEHEADER;

typedef struct                       /**** BMP file info structure ****/  
{  
    unsigned int   biSize;           /* Size of info header */  
    int            biWidth;          /* Width of image */  
    int            biHeight;         /* Height of image */  
    unsigned short biPlanes;         /* Number of color planes */  
    unsigned short biBitCount;       /* Number of bits per pixel */  
    unsigned int   biCompression;    /* Type of compression to use */  
    unsigned int   biSizeImage;      /* Size of image data */  
    int            biXPelsPerMeter;  /* X pixels per meter */  
    int            biYPelsPerMeter;  /* Y pixels per meter */  
    unsigned int   biClrUsed;        /* Number of colors used */  
    unsigned int   biClrImportant;   /* Number of important colors */  
} MyBITMAPINFOHEADER;

//用户自定义的图像缓存BUF
struct buffer{
		void *start;  
		unsigned int length;  
	}*buffers; 
static unsigned int g_buffer_count = 0;

int creat_bmp(unsigned char *rgb,const char *bmp_file);

//rgb to BMP 
int creat_bmp(unsigned char *rgb,const char *bmp_file)
{
	char buf[480][640][3]={0};
	int width=640;
	int height=480;
	MyBITMAPFILEHEADER bfh;  
    MyBITMAPINFOHEADER bih;  
   
    unsigned short bfType=0x4d42;             
    bfh.bfReserved1 = 0;  
    bfh.bfReserved2 = 0;  
    bfh.bfSize = 2+sizeof(MyBITMAPFILEHEADER) + sizeof(MyBITMAPINFOHEADER)+width*height*3;  
    bfh.bfOffBits = 0x36;  
  
    bih.biSize = sizeof(MyBITMAPINFOHEADER);  
    bih.biWidth = width;  
    bih.biHeight = height;  
    bih.biPlanes = 1;  
    bih.biBitCount = 24;  
    bih.biCompression = 0;  
    bih.biSizeImage = 0;  
    bih.biXPelsPerMeter = 5000;  
    bih.biYPelsPerMeter = 5000;  
    bih.biClrUsed = 0;  
    bih.biClrImportant = 0;  
    FILE * file = fopen( bmp_file,"wb" );  
    if (!file)  
    {  
        printf("Could not write file\n");  
        return 0;  
    }  
    int i,j,k;
    for(i=479;i>=0;i--)
    {
    	for(j=0;j<640;j++)
    	{
    		for(k=2;k>=0;k--)
    		{
    			buf[i][j][k]=*rgb;
    			rgb++;
    		}
    	}
    }
   
    fwrite(&bfType,sizeof(bfType),1,file);  
    fwrite(&bfh,sizeof(bfh),1, file);  
    fwrite(&bih,sizeof(bih),1, file);  
  
    fwrite(buf,width*height*3,1,file);  
    fclose(file);  
}



int yuyv2rgb(int y, int u, int v)
{
     unsigned int pixel24 = 0;
     unsigned char *pixel = (unsigned char *)&pixel24;
     int r, g, b;
     static int  ruv, guv, buv;

	 // 色度
     ruv = 1596*(v-128);
	 guv = 391*(u-128) + 813*(v-128);
     buv = 2018*(u-128);
     
	// RGB
     r = (1164*(y-16) + ruv) / 1000;
     g = (1164*(y-16) - guv) / 1000;
     b = (1164*(y-16) + buv) / 1000;

     if(r > 255) r = 255;
     if(g > 255) g = 255;
     if(b > 255) b = 255;
     if(r < 0) r = 0;
     if(g < 0) g = 0;
     if(b < 0) b = 0;

     pixel[0] = r;
     pixel[1] = g;
     pixel[2] = b;

     return pixel24;
}



//YUYV to  RGB 
int yuyv2rgb0(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
     unsigned int in, out;
     int y0, u, y1, v;
     unsigned int pixel24;
     unsigned char *pixel = (unsigned char *)&pixel24;
     unsigned int size = width*height*2;

     for(in = 0, out = 0; in < size; in += 4, out += 6)
     {
		 // YUYV
          y0 = yuv[in+0];
          u  = yuv[in+1];
          y1 = yuv[in+2];
          v  = yuv[in+3];

          pixel24 = yuyv2rgb(y0, u, v); // RGB
          rgb[out+0] = pixel[0];    
          rgb[out+1] = pixel[1];
          rgb[out+2] = pixel[2];

          pixel24 = yuyv2rgb(y1, u, v);// RGB
          rgb[out+3] = pixel[0];
          rgb[out+4] = pixel[1];
          rgb[out+5] = pixel[2];

     }
     return 0;
}

//初始化摄像头
int  init_video()//返回摄像头设备文件的文件描述符
{
	//1.open device.打开摄像头设备 
	const char *video_devices[] = {
		"/dev/video6",
		"/dev/video7",
		"/dev/video0",
		"/dev/video1",
		NULL
	};
	int fd = -1;
	struct v4l2_format fmt;

	for (int i = 0; video_devices[i] != NULL; i++) {
		fd = open(video_devices[i], O_RDWR, 0);//以阻塞模式打开摄像头
		if (fd < 0) {
			printf("open %s failed errno=%d %s.\n", video_devices[i], errno, strerror(errno));
			continue;
		}

		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
		fmt.fmt.pix.width = 640;
		fmt.fmt.pix.height = 480;
		fmt.fmt.pix.field = V4L2_FIELD_NONE;
		if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
			printf("%s VIDIOC_S_FMT failed errno=%d %s.\n", video_devices[i], errno, strerror(errno));
			close(fd);
			fd = -1;
			continue;
		}

		printf("open device success: %s -> fd=%d\n", video_devices[i], fd);
		printf("VIDIOC_S_FMT sucess.\n");
		break;
	}

		if (fd < 0) {
		printf("open camera device failed.\n");
		return -1;
	}
 
	//2.show all supported format.显示所有支持的格式
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index = 0; //form number
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;//frame type  
	while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc) != -1){  
            printf("VIDIOC_ENUM_FMT success.->fmt.fmt.pix.pixelformat:%s\n",fmtdesc.description);
        fmtdesc.index ++;
    }

    //3.set or gain current frame.设置或查看当前格式
	//查看格式
	if (ioctl(fd,VIDIOC_G_FMT,&fmt) == -1) {
	   printf("VIDIOC_G_FMT failed.\n");
	   return -1;
    }
  	printf("VIDIOC_G_FMT sucess.->fmt.fmt.width is %ld\nfmt.fmt.pix.height is %ld\n\
fmt.fmt.pix.colorspace is %ld\n",fmt.fmt.pix.width,fmt.fmt.pix.height,fmt.fmt.pix.colorspace);
	//6.1 request buffers.申请缓冲区
	struct v4l2_requestbuffers req;  
	req.count = 2;//frame count.帧的个数
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;//automation or user define．自动分配还是自定义
	if ( ioctl(fd,VIDIOC_REQBUFS,&req)==-1){  
		printf("VIDIOC_REQBUFS map failed.\n");  
		close(fd);  
		exit(-1);  
	} 
	printf("VIDIOC_REQBUFS map success.\n");
	//为用户自定义的映射地址分配空间
	g_buffer_count = req.count;
	buffers = (struct buffer*)calloc (req.count, sizeof(*buffers));  //分配缓存

	  unsigned int n_buffers = 0; 
	  struct v4l2_buffer buf;   
	for(n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{  
	  
	    //查询序号为n_buffers 的缓冲区，得到其起始物理地址和大小
		memset(&buf,0,sizeof(buf)); 
		buf.index = n_buffers; 
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  
		buf.memory = V4L2_MEMORY_MMAP;  
		if(ioctl(fd,VIDIOC_QUERYBUF,&buf) == -1)
		{  
			printf("VIDIOC_QUERYBUF failed.\n");
			close(fd);  
			exit(-1);  
		} 
        printf("VIDIOC_QUERYBUF success.\n");

		
  		//memory map   把2块缓存地址映射到用户空间
		buffers[n_buffers].length = buf.length;	
		buffers[n_buffers].start = mmap(NULL,buf.length,PROT_READ|PROT_WRITE,MAP_SHARED,fd,buf.m.offset);  
		if(MAP_FAILED == buffers[n_buffers].start){  
			printf("memory map failed.\n");
			close(fd);  
			exit(-1);  
		} 
		printf("memory map success.\n"); 
		
		//Queen buffer.将缓冲帧放入队列 
		if (ioctl(fd , VIDIOC_QBUF, &buf) ==-1) {
		    printf("VIDIOC_QBUF failed.->n_buffers=%d\n", n_buffers);
		    return -1;
		}
		printf("VIDIOC_QBUF.->Frame buffer %d: address=0x%p, length=%ld\n",\
n_buffers,buffers[n_buffers].start, buffers[n_buffers].length);
	} 
	//7.使能视频设备输出视频流
	enum v4l2_buf_type type; 
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
	if (ioctl(fd,VIDIOC_STREAMON,&type) == -1) {
		printf("VIDIOC_STREAMON failed.\n");
		return -1;
	}
	printf("VIDIOC_STREAMON success.\n"); 
	
	return fd;
}
//将帧缓存画面转化为bmp图片

int save_rgb_bmp(unsigned char *rgb, const char *bmp_file)
{
	return creat_bmp(rgb, bmp_file);
}

int get_rgb_frame(int fd, unsigned char *rgb_buf, unsigned int rgb_size)
{
	if (rgb_buf == NULL || rgb_size < 640 * 480 * 3) {
		printf("invalid rgb frame buffer.\n");
		return -1;
	}

	struct v4l2_buffer buf;
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
		printf("VIDIOC_DQBUF failed.->fd=%d errno=%d %s\n", fd, errno, strerror(errno));
		return -1;
	}

	yuyv2rgb0((unsigned char *)buffers[buf.index].start, rgb_buf, 640, 480);

	if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
		printf("VIDIOC_QBUF failed.->fd=%d errno=%d %s\n", fd, errno, strerror(errno));
		return -1;
	}

	return 0;
}

int get_bmp(int fd, const char *name)
{
	unsigned char rgb_buf[640 * 480 * 3] = {0};
	if (get_rgb_frame(fd, rgb_buf, sizeof(rgb_buf)) == -1) {
		return -1;
	}

	return creat_bmp(rgb_buf, name);
}

void free_video(int fd)
{
	int i;
	for (i = 0; i < (int)g_buffer_count; i++) {
		munmap(buffers[i].start, buffers[i].length);
		printf("munmap success.\n");
	}
	free(buffers);
	buffers = NULL;
	g_buffer_count = 0;

	close(fd);
	printf("Camera test Done.\n");
}
