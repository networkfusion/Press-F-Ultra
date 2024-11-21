#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <libdragon.h>

#include "libpressf/src/emu.h"
#include "libpressf/src/input.h"
#include "libpressf/src/screen.h"
#include "libpressf/src/hw/beeper.h"
#include "libpressf/src/hw/vram.h"

/* ROM data can be added here to compile it into the program. */

const static unsigned char bios_a[] = {};
unsigned int bios_a_size = 0;

const static unsigned char bios_b[] = {};
unsigned int bios_b_size = 0;

const static unsigned char rom[] = {};
unsigned int rom_size = 0;

typedef enum
{
  PFU_SCALING_1_1 = 0,
  PFU_SCALING_4_3,

  PFU_SCALING_SIZE
} pfu_scaling_type;

typedef struct
{
  u16* video_buffer;
  surface_t video_frame;
  pfu_scaling_type video_scaling;
  f8_system_t system;
} pfu_emu_ctx_t;

typedef enum
{
  PFU_ENTRY_TYPE_NONE = 0,

  PFU_ENTRY_TYPE_BACK,
  PFU_ENTRY_TYPE_FILE,
  PFU_ENTRY_TYPE_BOOL,

  PFU_ENTRY_TYPE_SIZE
} pfu_entry_type;

typedef struct
{
  char key[256];
  pfu_entry_type type;
} pfu_menu_entry_t;

typedef struct
{
  pfu_menu_entry_t entries[16];
  char menu_title[256];
  char menu_subtitle[256];
  int entry_count;
  int cursor;
} pfu_menu_ctx_t;

static pfu_emu_ctx_t emu;

int pfu_load_rom(unsigned address, const char *path)
{
  FILE *file;
  char buffer[0x0400];
  char fullpath[256];

  snprintf(fullpath, sizeof(fullpath), "sd:/press-f/%s", path);
  file = fopen(fullpath, "r");
  if (file)
  {
    int file_size;
    int i;

    printf("Loading %s...\n", fullpath);

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    rewind(file);

    printf("Size: %04X\n", file_size);

    for (i = 0; i < file_size; i += 0x0400)
    {
      int bytes_read;

      bytes_read = fread(buffer, sizeof(char), 0x0400, file);
      f8_write(&emu.system, address + i, buffer, bytes_read);
      printf("Loaded %04X bytes to %04X\n", bytes_read, address + i);
    }
    fclose(file);

    return 1;
  }

  return 0;
}

void pfu_error_no_rom(void)
{
  console_clear();
  printf("Press F requires ROM data\n"
         "to be stored on the SD Card\n"
         "in the \"press-f\" directory.\n\n"
         "Please include the sl31253.bin\n"
         "and sl31254.bin BIOS images.\n\n"
         "Alternatively, it can be\n"
         "compiled in statically.\n\n"
         "See GitHub for instructions.");
  console_render();
  exit(1);
}

void pfu_video_render_1_1(void)
{
  surface_t *disp = display_get();
  
  rdpq_attach_clear(disp, NULL);
  rdpq_set_mode_standard();
  rdpq_tex_blit(&emu.video_frame, 14, 66, &(rdpq_blitparms_t){ .scale_x = 6.0f, .scale_y = 6.0f});
  rdpq_detach_show();
}

void pfu_video_render_4_3(void)
{
  surface_t *disp = display_get();

  rdpq_attach_clear(disp, NULL);
  rdpq_set_mode_standard();
  rdpq_tex_blit(&emu.video_frame, 0, 0, &(rdpq_blitparms_t){ .scale_x = 640.0f / SCREEN_WIDTH, .scale_y = 480.0f / SCREEN_HEIGHT});
  rdpq_detach_show();
}

int main(void)
{
  /* Initialize console */
  console_init();
  console_set_render_mode(RENDER_MANUAL);

  /* Initialize controller */
  controller_init();
  
  /* Initialize video */
  display_init(RESOLUTION_640x480, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_RESAMPLE);
  rdpq_init();
  emu.video_buffer = (u16*)malloc_uncached_aligned(64, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
  emu.video_frame = surface_make_linear(emu.video_buffer, FMT_RGBA16, SCREEN_WIDTH, SCREEN_HEIGHT);
  emu.video_scaling = PFU_SCALING_4_3;

  /* Initialize audio */
  audio_init(PF_SOUND_FREQUENCY, 2);

  /* Initialize emulator */
  pressf_init(&emu.system);
  f8_system_init(&emu.system, &pf_systems[0]);
  if (bios_a_size)
    f8_write(&emu.system, 0x0000, bios_a, bios_a_size);
  if (bios_b_size)
    f8_write(&emu.system, 0x0400, bios_b, bios_b_size);
  if (rom_size)
    f8_write(&emu.system, 0x0800, rom, rom_size);

  /* Setup ROM menu if available */
  if (debug_init_sdfs("sd:/", -1))
  {
    dir_t dir;
    pfu_menu_ctx_t menu;
    int err = dir_findfirst("sd:/press-f", &dir);
    int count = 1;
    bool finished = false;

    /* Set up dummy file entry to not load a ROM */
    snprintf(menu.entries[0].key, sizeof(menu.entries[0].key), "%s", "Boot to BIOS");
    menu.entries[0].type = PFU_ENTRY_TYPE_BACK;

    while (!err)
    {
      if (dir.d_type == DT_REG)
      {
        /* Load BIOS if found on SD Card */
        if (!strncmp(dir.d_name, "sl31253.bin", 8))
          bios_a_size = pfu_load_rom(0x0000, dir.d_name);
        else if (!strncmp(dir.d_name, "sl31254.bin", 8))
          bios_b_size = pfu_load_rom(0x0400, dir.d_name);
        else if (strlen(dir.d_name) && dir.d_name[0] != '.')
        {
          /* List all other files */
          snprintf(menu.entries[count].key, sizeof(dir.d_name), "%s", dir.d_name);
          menu.entries[count].type = PFU_ENTRY_TYPE_FILE;
          count++;
        }
      }
      err = dir_findnext("sd:/press-f", &dir); 
    }

    if (!bios_a_size || !bios_b_size)
      pfu_error_no_rom();

    menu.entry_count = count;
    menu.cursor = 0;

    do
    {
      struct controller_data keys;
      int i;

      /* Process menu controller logic */
      controller_scan();
      keys = get_keys_down();
      if (keys.c[0].up && menu.cursor > 0)
        menu.cursor--;
      else if (keys.c[0].down && menu.cursor < menu.entry_count - 1)
        menu.cursor++;
      else if (keys.c[0].A)
      {
        if (menu.entries[menu.cursor].type == PFU_ENTRY_TYPE_FILE)
          pfu_load_rom(0x0800, menu.entries[menu.cursor].key);
        else if (menu.entries[menu.cursor].type == PFU_ENTRY_TYPE_BACK)
        {
          /* Zero some data so it doesn't persist between boots */
          int dummy = 0;
          f8_write(&emu.system, 0x0800, &dummy, sizeof(dummy));
        }
        finished = true;
      }

      /* Render menu entries */
      console_clear();
      printf("\n\n");
      for (i = 0; i < menu.entry_count; i++)
        printf("%c %s\n", i == menu.cursor ? '>' : '-', menu.entries[i].key);
      console_render();
    } while (!finished);

    debug_close_sdfs();
  }
  else if (!bios_a_size || !bios_b_size)
    pfu_error_no_rom();
  console_close();

  /* Main loop */
  while (1)
  {
    struct controller_data keys;

    controller_scan();
    keys = get_keys_pressed();

    /* Handle console input */
    set_input_button(0, INPUT_TIME, keys.c[0].A);
    set_input_button(0, INPUT_MODE, keys.c[0].B);
    set_input_button(0, INPUT_HOLD, keys.c[0].Z);
    set_input_button(0, INPUT_START, keys.c[0].start);

    /* Handle player 1 input */
    set_input_button(4, INPUT_RIGHT, keys.c[0].right);
    set_input_button(4, INPUT_LEFT, keys.c[0].left);
    set_input_button(4, INPUT_BACK, keys.c[0].down);
    set_input_button(4, INPUT_FORWARD, keys.c[0].up);
    set_input_button(4, INPUT_ROTATE_CCW, keys.c[0].C_left);
    set_input_button(4, INPUT_ROTATE_CW, keys.c[0].C_right);
    set_input_button(4, INPUT_PULL, keys.c[0].C_up);
    set_input_button(4, INPUT_PUSH, keys.c[0].C_down);

    /* Handle player 2 input */
    set_input_button(1, INPUT_RIGHT, keys.c[1].right);
    set_input_button(1, INPUT_LEFT, keys.c[1].left);
    set_input_button(1, INPUT_BACK, keys.c[1].down);
    set_input_button(1, INPUT_FORWARD, keys.c[1].up);
    set_input_button(1, INPUT_ROTATE_CCW, keys.c[1].C_left);
    set_input_button(1, INPUT_ROTATE_CW, keys.c[1].C_right);
    set_input_button(1, INPUT_PULL, keys.c[1].C_up);
    set_input_button(1, INPUT_PUSH, keys.c[1].C_down);

    /* Handle hotkeys */
    if (keys.c[0].L)
      emu.video_scaling = PFU_SCALING_1_1;
    else if (keys.c[0].R)
      emu.video_scaling = PFU_SCALING_4_3;

    /* Emulation */
    pressf_run(&emu.system);

    /* Video */
    draw_frame_rgb5551(((vram_t*)emu.system.f8devices[3].device)->data, emu.video_buffer);

    /* Audio */
    audio_push(((f8_beeper_t*)emu.system.f8devices[7].device)->samples, PF_SOUND_SAMPLES, false);

    /* Just blit the frame */
    if (emu.video_scaling == PFU_SCALING_1_1)
      pfu_video_render_1_1();
    else
      pfu_video_render_4_3();
  }
}
