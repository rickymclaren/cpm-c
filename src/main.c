#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include "z80.h"

#define MEMORY_SIZE 0x10000
#define CR 0x0D // Carriage Return
#define LF 0x0A // Line Feed

static uint8_t *memory = NULL;

static uint8_t fdc_drive;
static uint8_t fdc_track;
static uint8_t fdc_sector;
static uint8_t fdc_command;
static uint8_t fdc_status;
static uint8_t fdc_dma_low;
static uint8_t fdc_dma_high;

static char *diskImage(uint8_t drive)
{
  static char *letters = "abcdefghijklmnop";
  static char image[20];
  snprintf(image, sizeof(image), "disks/%c/DISK.IMG", letters[drive]);
  return image;
}

static uint8_t rb(void *userdata, uint16_t addr)
{
  return memory[addr];
}

static void wb(void *userdata, uint16_t addr, uint8_t val)
{
  memory[addr] = val;
}

static uint8_t in(z80 *const z, uint8_t port)
{
  uint8_t value = 0xFF; // Default value if no case matches

  switch (port)
  {
  case 0: // Console status
    struct pollfd fds[1];
    int timeout_ms = 0; // Immediate check (non-blocking)

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN; // Check for readable data

    int result = poll(fds, 1, timeout_ms);

    if (result == -1)
    {
      perror("poll failed");
      exit(1);
    }
    else if (result > 0 && (fds[0].revents & POLLIN))
    {
      value = 0xFF;
    }
    else
    {
      value = 0x00; // No data available
    }
    break;

  case 1: // Console input
    value = getchar();
    if (value == LF)
    {
      value = CR; // Convert LF to CR
    }
    break;

  case 2: // Printer status
    value = 0;
    break;

  case 3: // Printer data
    printf("Printer data requested, but not implemented.\n");
    value = 0;
    break;

  case 5: // Printer data
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

static void out(z80 *const z, uint8_t port, uint8_t val)
{
  switch (port)
  {
  case 0: // Console input status - not implemented
    break;

  case 1:
    printf("%c", val);
    break;

  case 2: // Printer status - not implemented
    break;

  case 3: // Printer data - not implemented
    break;

  case 5: // AUX data - not implemented
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
    int sectors_per_track = 26;
    if (fdc_drive > 3)
    {
      sectors_per_track = 128;
    }

    char *image = diskImage(fdc_drive);
    FILE *f = fopen(image, "rb");
    if (f == NULL)
    {
      fprintf(stderr, "error: can't open disk image '%s'.\n", image);
      exit(1);
    }

    u_int16_t dma = (fdc_dma_high << 8) | fdc_dma_low;
    u_int16_t sector = fdc_track * sectors_per_track + fdc_sector - 1;
    u_int16_t offset = sector * 128;
    uint8_t *sector_data = &memory[dma];
    size_t num_bytes;

    fseek(f, offset, SEEK_SET);

    switch (val)
    {

    case 0: // Disk Read
      num_bytes = fread(sector_data, 128, 1, f);
      break;

      if (num_bytes != 128)
      {
        printf("Bytes read is %d instead of 128\n", num_bytes);
        exit(1);
      }
      break;

    case 1: // Disk Write
      num_bytes = fwrite(sector_data, 128, 1, f);
      if (num_bytes != 128)
      {
        printf("Bytes written is %d instead of 128\n", num_bytes);
        exit(1);
      }

      break;
    }

    fclose(f);
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

static int load_file(const char *filename, uint16_t addr, uint16_t size)
{
  FILE *f = fopen(filename, "rb");
  if (f == NULL)
  {
    fprintf(stderr, "error: can't open file '%s'.\n", filename);
    return 1;
  }

  // copying the bytes in memory:
  size_t result = fread(&memory[addr], sizeof(uint8_t), size, f);
  if (result != size)
  {
    fprintf(stderr, "error: while reading file '%s'\n", filename);
    return 1;
  }

  fclose(f);
  return 0;
}

int main()
{
  // printf("Starting!\n");

  memory = calloc(MEMORY_SIZE, 1);
  if (memory == NULL)
  {
    return 1;
  }

  z80 cpu;
  z80_init(&cpu);
  cpu.read_byte = rb;
  cpu.write_byte = wb;
  cpu.port_in = in;
  cpu.port_out = out;

  load_file("disks/a/DISK.IMG", 0x0000, 0x80);

  while (!cpu.halted)
  {
    // z80_debug_output(&cpu);
    z80_step(&cpu);
  }

  return 0;
}