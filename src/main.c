#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "z80.h"

#define MEMORY_SIZE 0x10000
#define CR 0x0D  // Carriage Return
#define LF 0x0A  // Line Feed
#define READ 0x00
#define WRITE 0x01
#define SECTOR_SIZE 128

static uint8_t *memory = NULL;

static uint8_t fdc_drive = 0;
static uint8_t fdc_track = 0;
static uint8_t fdc_sector = 0;
static uint8_t fdc_command = 0;
static uint8_t fdc_status = 0;
static uint8_t fdc_dma_low = 0;
static uint8_t fdc_dma_high = 0;

static void disk_access() {
  static char *letters = "abcdefghijklmnop";
  static char filename[20];
  snprintf(filename, sizeof(filename), "disks/%c/DISK.IMG", letters[fdc_drive]);
  int sectors_per_track = fdc_drive > 3 ? 128 : 26;

  FILE *f = fopen(filename, fdc_command == WRITE ? "wb" : "rb");
  if (f == NULL) {
    fprintf(stderr, "error: can't open disk image '%s'.\n", filename);
    exit(1);
  }

  u_int16_t dma = (fdc_dma_high << 8) | fdc_dma_low;
  long sector = (long)fdc_track * sectors_per_track + fdc_sector - 1;
  long offset = sector * SECTOR_SIZE;
  uint8_t *sector_data = &memory[dma];
  size_t num_bytes;

  fseek(f, offset, SEEK_SET);
  if (fdc_command == WRITE) {
    num_bytes = fwrite(sector_data, 1, SECTOR_SIZE, f);
    if (num_bytes != SECTOR_SIZE) {
      printf("Bytes written is %zu instead of %d\n", num_bytes, SECTOR_SIZE);
      exit(1);
    }
  } else {
    num_bytes = fread(sector_data, 1, SECTOR_SIZE, f);
    if (num_bytes != SECTOR_SIZE) {
      printf("Bytes read is %zu instead of %d\n", num_bytes, SECTOR_SIZE);
      exit(1);
    }
  }

  fclose(f);
}

static uint8_t rb(void *userdata, uint16_t addr) {
  return ((u_int8_t *)userdata)[addr];
}

static void wb(void *userdata, uint16_t addr, uint8_t val) {
  ((u_int8_t *)userdata)[addr] = val;
}

static uint8_t in(z80 *const z, uint8_t port) {
  uint8_t value = 0xFF;  // Default value if no case matches
  struct pollfd fds[1];
  int timeout_ms = 0;  // Immediate check (non-blocking)

  (void)z;  // Unused parameter
  switch (port) {
    case 0:  // Console status

      fds[0].fd = STDIN_FILENO;
      fds[0].events = POLLIN;  // Check for readable data

      int result = poll(fds, 1, timeout_ms);

      if (result == -1) {
        perror("poll failed");
        exit(1);
      } else if (result > 0 && (fds[0].revents & POLLIN)) {
        value = 0xFF;
      } else {
        value = 0x00;  // No data available
      }
      break;

    case 1:  // Console input
      value = getchar();
      if (value == LF) {
        value = CR;  // Convert LF to CR
      }
      break;

    case 2:  // Printer status
      value = 0;
      break;

    case 3:  // Printer data
      printf("Printer data requested, but not implemented.\n");
      value = 0;
      break;

    case 5:  // AUX data
      printf("AUX data requested, but not implemented.\n");
      value = 0;
      break;

    case 0x0a:
      value = fdc_drive;
      break;

    case 0x0b:
      value = fdc_track;
      break;

    case 0x0c:
      value = fdc_sector;
      break;

    case 0x0d:
      value = fdc_command;
      break;

    case 0x0e:
      value = fdc_status;
      break;

    case 0x0f:
      value = fdc_dma_low;
      break;

    case 0x10:
      value = fdc_dma_high;
      break;

    default:
      printf("IN: port=0x%02X\n", port);
      break;
  }

  return value;
}

static void out(z80 *const z, uint8_t port, uint8_t val) {
  (void)z;  // Unused parameter
  switch (port) {
    case 0:  // Console input status - not implemented
      break;

    case 1:
      printf("%c", val);
      break;

    case 2:  // Printer status - not implemented
      break;

    case 3:  // Printer data - not implemented
      break;

    case 5:  // AUX data - not implemented
      break;

    case 0x0a:
      fdc_drive = val;
      break;

    case 0x0b:
      fdc_track = val;
      break;

    case 0x0c:
      fdc_sector = val;
      break;

    case 0x0d:
      fdc_command = val;
      disk_access();
      break;

    case 0x0e:
      fdc_status = val;
      break;

    case 0x0f:
      fdc_dma_low = val;
      break;

    case 0x10:
      fdc_dma_high = val;
      break;

    default:
      printf("OUT: port=0x%02X, value=0x%02X\n", port, val);
      break;
  }
}

int main() {
  memory = calloc(MEMORY_SIZE, 1);
  if (memory == NULL) {
    return 1;
  }

  z80 cpu;
  z80_init(&cpu);
  cpu.read_byte = rb;
  cpu.write_byte = wb;
  cpu.port_in = in;
  cpu.port_out = out;
  cpu.userdata = memory;

  // load boot sector
  disk_access();

  while (!cpu.halted) {
    // z80_debug_output(&cpu);
    z80_step(&cpu);
  }

  printf("CPU halted.\n");

  return 0;
}