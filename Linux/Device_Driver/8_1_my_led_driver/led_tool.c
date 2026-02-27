#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_PATH "/dev/my_led"

int main(int argc, char *argv[]) {
  int fd;
  char val;

  if (argc < 2) {
    printf("Usage: %s [on|off|status|blink <count>]\n", argv[0]);
    return -1;
  }

  // Open for Read and Write (O_RDWR)
  fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("Error opening device");
    return -1;
  }

  if (strcmp(argv[1], "on") == 0) {
    val = '1';
    write(fd, &val, 1);
    printf("LED set to ON\n");

  } else if (strcmp(argv[1], "off") == 0) {
    val = '0';
    write(fd, &val, 1);
    printf("LED set to OFF\n");

  } else if (strcmp(argv[1], "status") == 0) {
    char status_val;
    if (read(fd, &status_val, 1) > 0) {
      printf("Current LED Status: %s\n", (status_val == '1') ? "ON" : "OFF");
    } else {
      printf("Failed to read status.\n");
    }

  } else if (strcmp(argv[1], "blink") == 0) {
    int count = (argc == 3) ? atoi(argv[2]) : 5;
    for (int i = 0; i < count; i++) {
      val = '1'; write(fd, &val, 1); usleep(250000);
      val = '0'; write(fd, &val, 1); usleep(250000);
    }
    printf("Blink test complete.\n");
  }

  close(fd);
  return 0;
}
