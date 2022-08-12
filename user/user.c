#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "lib/definitions.h"

int main(int argc, char** argv) {
        int res;
        int out_ioctl;
        int fd;
        char *path;
        unsigned long timeout;

        int command = 0;
        char buf[MAX_BUF_SIZE];
        int offset;

        if (argc < 2) {
                printf("Usage: sudo ./user [Device File Path]\n");
                return -1;
        }

        // open device
        path = argv[1];
        fd = open(path, O_RDWR);
        if (fd == -1) {
                printf("open error on device file %s\n", path);
                return -1;
        }

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
                case 1:
                        out_ioctl = set_high_priority(fd);
                        if (out_ioctl == -1) {
                                printf("Error high priority\n");
                        }
                        printf("Switched to high priority\n");
                        break;
                case 2:
                        out_ioctl = set_low_priority(fd);
                        if (out_ioctl == -1) {
                                printf("Error low priority\n");
                        }
                        printf("Switched to low priority\n");
                        break;
                case 3:
                        out_ioctl = set_blocking_operations(fd);
                        if (out_ioctl == -1) {
                                printf("Error blocking operations\n");
                        }
                        printf("Switched to blocking operations\n");
                        break;
                case 4:
                        out_ioctl = set_unblocking_operations(fd);
                        if (out_ioctl == -1) {
                                printf("Error non-blocking operations\n");
                        }
                        printf("Switched to non-blocking operations\n");
                        break;
                case 5:
                        printf("Inserisci il valore del timeout: ");
                        do {
                                scanf("%ld", &timeout);
                        } while (timeout < 1);
                        out_ioctl = set_timeout(fd, timeout);
                        if (out_ioctl == -1) {
                                printf("Error timeout\n");
                        }
                        printf("Update awake timeout for blocking operations\n");
                        break;
                case 6:
                        printf("Inserisci testo da scrivere: ");
                        fgets(buf, MAX_BUF_SIZE, stdin);
                        res = write(fd, buf, strlen(buf));
                        break;
                case 7:
                        printf("Inserisci quanti byte vuoi leggere: ");
                        scanf("%d", &offset);
                        res = read(fd, buf, offset);
                        break;
                case 8:
                        close(fd);
                        return 0;
                default: 
                        printf("Operazione non disponibile\n\n");
                }
        }

        return 0;
}