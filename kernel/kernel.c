#include "multiboot2.h"

/*  Some screen stuff. */
/*  The number of columns. */
#define COLUMNS                 80
/*  The number of lines. */
#define LINES                   24
/*  The attribute of an character. */
#define ATTRIBUTE               7
/*  The video memory address. */
#define VIDEO                   0xb8000

/*  Variables. */
/*  Save the X position. */
static int xpos;
/*  Save the Y position. */
static int ypos;
/*  Point to the video memory. */
static volatile unsigned char *video;


static void
itoa (char *buf, int base, int d)
{
  char *p = buf;
  char *p1, *p2;
  unsigned long ud = d;
  int divisor = 10;
  

  if (base == 'd' && d < 0)
    {
      *p++ = '-';
      buf++;
      ud = -d;
    }
  else if (base == 'x')
    divisor = 16;

  do
    {
      int remainder = ud % divisor;
      
      *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    }
  while (ud /= divisor);

  *p = 0;
  

  p1 = buf;
  p2 = p - 1;
  while (p1 < p2)
    {
      char tmp = *p1;
      *p1 = *p2;
      *p2 = tmp;
      p1++;
      p2--;
    }
}


/*  Put the character C on the screen. */
static void
putchar (int c)
{
  if (c == '\n' || c == '\r')
    {
    newline:
      xpos = 0;
      ypos++;
      if (ypos >= LINES)
        ypos = 0;
      return;
    }

  *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
  *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

  xpos++;
  if (xpos >= COLUMNS)
    goto newline;
}

/*  Clear the screen and initialize VIDEO, XPOS and YPOS. */
static void
cls (void)
{
  int i;

  video = (unsigned char *) VIDEO;
  
  for (i = 0; i < COLUMNS * LINES * 2; i++)
    *(video + i) = 0;

  xpos = 0;
  ypos = 0;
}

/*  Format a string and print it on the screen, just like the libc
   function printf. */
void
printf (const char *format, ...)
{
  char **arg = (char **) &format;
  int c;
  char buf[20];

  arg++;
  
  while ((c = *format++) != 0)
    {
      if (c != '%')
        putchar (c);
      else
        {
          char *p, *p2;
          int pad0 = 0, pad = 0;
          
          c = *format++;
          if (c == '0')
            {
              pad0 = 1;
              c = *format++;
            }

          if (c >= '0' && c <= '9')
            {
              pad = c - '0';
              c = *format++;
            }

          switch (c)
            {
            case 'd':
            case 'u':
            case 'x':
              itoa (buf, c, *((int *) arg++));
              p = buf;
              goto string;
              break;

            case 's':
              p = *arg++;
              if (! p)
                p = "(null)";

            string:
              for (p2 = p; *p2; p2++);
              for (; p2 < p + pad; p2++)
                putchar (pad0 ? '0' : ' ');
              while (*p)
                putchar (*p++);
              break;

            default:
              putchar (*((int *) arg++));
              break;
            }
        }
    }
}


int cmain(unsigned long addr, unsigned long magic)
{
	struct multiboot_tag *tag;
	unsigned size;

	cls();

	if ((int)magic != MULTIBOOT2_BOOTLOADER_MAGIC)
	{
		printf ("Invalid magic number: 0x%x\n", (unsigned) magic);
		return;
	}

	if (addr & 7)
	{
		printf ("Unaligned mbi: 0x%x\n", addr);
		return;
	}

	size = *(unsigned *) addr;
	printf ("Announced mbi size 0x%d\n", size);
	for (tag = (struct multiboot_tag *) (addr + 8);
	tag->type != MULTIBOOT_TAG_TYPE_END;
	tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
                                       + ((tag->size + 7) & ~7)))
	{
		printf ("Tag 0x%x, Size 0x%x\n", tag->type, tag->size);
		switch (tag->type)
		{
			case MULTIBOOT_TAG_TYPE_CMDLINE:
				printf ("Command line = %s\n",
				((struct multiboot_tag_string *) tag)->string);
				break;
			case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
				printf ("Boot loader name = %s\n",
				((struct multiboot_tag_string *) tag)->string);
				break;
			case MULTIBOOT_TAG_TYPE_MODULE:
				printf ("Module at 0x%x-0x%x. Command line %s\n",
				((struct multiboot_tag_module *) tag)->mod_start,
				((struct multiboot_tag_module *) tag)->mod_end,
				((struct multiboot_tag_module *) tag)->cmdline);
				break;
			case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
				printf ("mem_lower = %uKB, mem_upper = %uKB\n",
				((struct multiboot_tag_basic_meminfo *) tag)->mem_lower,
				((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
				break;
			case MULTIBOOT_TAG_TYPE_BOOTDEV:
				printf ("Boot device 0x%x,%u,%u\n",
				((struct multiboot_tag_bootdev *) tag)->biosdev,
				((struct multiboot_tag_bootdev *) tag)->slice,
				((struct multiboot_tag_bootdev *) tag)->part);
				break;
			case MULTIBOOT_TAG_TYPE_MMAP:
			{
				multiboot_memory_map_t *mmap;

				printf ("mmap\n");
      
				for (mmap = ((struct multiboot_tag_mmap *) tag)->entries;
				(multiboot_uint8_t *) mmap < (multiboot_uint8_t *) tag + tag->size;
				mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + 
				((struct multiboot_tag_mmap *) tag)->entry_size))
				printf (" base_addr = 0x%x%x,"
				" length = 0x%x%x, type = 0x%x\n",
				(unsigned) (mmap->addr >> 32),
				(unsigned) (mmap->addr & 0xffffffff),
				(unsigned) (mmap->len >> 32),
				(unsigned) (mmap->len & 0xffffffff),
				(unsigned) mmap->type);
				}
				break;
			case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
			{
				break;
			}

        }
    }
	tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
                                  + ((tag->size + 7) & ~7));
	printf ("Total mbi size 0x%x\n", (unsigned) tag - addr);

	while(1){}
}
