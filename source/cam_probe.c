#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(void) {
    int fd = open("/dev/video6", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct v4l2_fmtdesc desc;
    memset(&desc, 0, sizeof(desc));
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0) {
        printf("fmt %u: %s fourcc=%c%c%c%c\n", desc.index, desc.description,
               desc.pixelformat & 0xff,
               (desc.pixelformat >> 8) & 0xff,
               (desc.pixelformat >> 16) & 0xff,
               (desc.pixelformat >> 24) & 0xff);
        desc.index++;
    }

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1) {
        printf("VIDIOC_S_FMT errno=%d %s\n", errno, strerror(errno));
        close(fd);
        return 2;
    }

    printf("S_FMT ok width=%u height=%u fourcc=%c%c%c%c size=%u\n",
           fmt.fmt.pix.width, fmt.fmt.pix.height,
           fmt.fmt.pix.pixelformat & 0xff,
           (fmt.fmt.pix.pixelformat >> 8) & 0xff,
           (fmt.fmt.pix.pixelformat >> 16) & 0xff,
           (fmt.fmt.pix.pixelformat >> 24) & 0xff,
           fmt.fmt.pix.sizeimage);
    close(fd);
    return 0;
}
