#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BEEP_STATE_PATH "/sys/devices/platform/xgpio_beep/state"

static int write_beep_state(const char *state)
{
    int fd = open(BEEP_STATE_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open beep state error");
        return -1;
    }

    if (write(fd, state, 1) != 1) {
        perror("write beep state error");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int beep_on(int frequency)
{
    (void)frequency;
    return write_beep_state("0");
}

int beep_off(void)
{
    return write_beep_state("1");
}

int main(void)
{
    int frequency;

    while (1) {
        printf("请输入蜂鸣器的频率\n");
        if (scanf("%d", &frequency) != 1) {
            printf("输入错误\n");
            return -1;
        }

        beep_on(frequency);
        usleep(1000000);
        beep_off();
    }

    return 0;
}
