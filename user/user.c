#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "lib/defines.h"

int main(int argc, char** argv) {
        int res;
        int out_ioctl;
        int fd;
        char *device_path;     
        unsigned long timeout;

        int command = 0;
        char buf[MAX_BUF_SIZE];
        int offset;

        if (argc < 2) {
                printf("Usage: sudo ./user [Device File Path]\n");
                return -1;
        }

        // open device
        device_path = argv[1];
        fd = device_open(device_path, O_RDWR);
        if (fd == -1) {
                printf("open error on device file %s\n", device_path);
                return -1;
        }
        printf("File descritor: %d", fd);
        printf("Device opened: %s", device_path);

        while (true) {
                printf("\033[2J\033[H");

                // set 50 bytes of buf to 0
                memset(buf, 0, MAX_BUF_SIZE);
                command = 0;

                printf("1. Set high priority\n");
                printf("2. Set low priority\n");
                printf("3. Set blocking operations\n");
                printf("4. Set non-blocking operations\n");
                printf("5. Set timeout\n");
                printf("6. Write\n");
                printf("7. Read\n");
                printf("8. Quit\n");

                printf("What driver operation you want to select?");
                scanf("%d", &command);
                
                switch(command) {
                case TO_HIGH_PRIORITY:
                        out_ioctl = set_high_priority(fd);
                        if (out_ioctl == -1) printf("Error high priority\n");
                        else printf("Switched to high priority\n");
                        break;
                case TO_LOW_PRIORITY:
                        out_ioctl = set_low_priority(fd);
                        if (out_ioctl == -1) printf("Error low priority\n");
                        else printf("Switched to low priority\n");
                        break;
                case BLOCKING:
                        out_ioctl = set_blocking_operations(fd);
                        if (out_ioctl == -1) printf("Error blocking operations\n");
                        else printf("Switched to blocking operations\n");
                        break;
                case UNBLOCKING:
                        out_ioctl = set_unblocking_operations(fd);
                        if (out_ioctl == -1) printf("Error non-blocking operations\n");
                        else printf("Switched to non-blocking operations\n");
                        break;
                case TIMEOUT:
                        printf("Inserisci il valore del timeout: ");
                        do {
                                scanf("%ld", &timeout);
                        } while (timeout < 1);
                        out_ioctl = set_timeout(fd, timeout);
                        if (out_ioctl == -1) printf("Error timeout\n");
                        else printf("Update awake timeout for blocking operations\n");
                        break;
                case WRITE:
                        printf("Inserisci testo da scrivere: ");
                        //fgets(buf, MAX_BUF_SIZE, stdin);
                        scanf("%s", buf);
                        res = device_write(fd, buf, strlen(buf));
                        if (res == -1) printf("Error on write operation (%s)\n", strerror(errno));
                        else printf("%d bytes are correctly written to device %s\n", res, device_path);
                        break;
                case READ:
                        printf("Inserisci quanti byte vuoi leggere: ");
                        scanf("%d", &offset);
                        res = device_read(fd, buf, offset);
                        if (res == -1) printf("Error on read operation (%s)\n", strerror(errno));
                        else printf("%d bytes are correctly read from device %s\n", res, device_path);
                        break;
                case RELEASE:
                        device_release(fd);
                        return 0;
                default: 
                        printf("Operazione non disponibile\n\n");
                }
        }

        return 0;
}