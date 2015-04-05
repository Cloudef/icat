/*
 * icat -- Outputs an image on a 256-color or 24 bit color enabled terminal with UTF-8 locale
 * Andreas Textor <textor.andreas@googlemail.com>
 *
 * Compile: gcc -Wall -pedantic -std=c99 -D_BSD_SOURCE -o icat icat.c -lavutil -lavformat -lswscale
 *
 * Copyright (c) 2012 Andreas Textor. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>

#define VERSION "0.4"

struct av {
   AVFormatContext *format;
   AVCodec *codec;
   AVCodecContext *context;
   AVFrame *frame, *rgb;
   AVIOContext *io;
   struct SwsContext *sws;
   uint8_t *buffer;
   uint64_t frames;
   double fps;
   int video_stream;
   uint32_t width, height;
};

enum mode {
   MODE_NOTHING,
   MODE_INDEXED,
   MODE_24_BIT,
   MODE_BOTH
};

static uint32_t colors[] = {
   // Colors 0 to 15: original ANSI colors
   0x000000, 0xcd0000, 0x00cd00, 0xcdcd00, 0x0000ee, 0xcd00cd, 0x00cdcd, 0xe5e5e5,
   0x7f7f7f, 0xff0000, 0x00ff00, 0xffff00, 0x5c5cff, 0xff00ff, 0x00ffff, 0xffffff,
   // Color cube colors
   0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff, 0x005f00, 0x005f5f,
   0x005f87, 0x005faf, 0x005fd7, 0x005fff, 0x008700, 0x00875f, 0x008787, 0x0087af,
   0x0087d7, 0x0087ff, 0x00af00, 0x00af5f, 0x00af87, 0x00afaf, 0x00afd7, 0x00afff,
   0x00d700, 0x00d75f, 0x00d787, 0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f,
   0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af,
   0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,
   0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f,
   0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f, 0x5fd787, 0x5fd7af,
   0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
   0x870000, 0x87005f, 0x870087, 0x8700af, 0x8700d7, 0x8700ff, 0x875f00, 0x875f5f,
   0x875f87, 0x875faf, 0x875fd7, 0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af,
   0x8787d7, 0x8787ff, 0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff,
   0x87d700, 0x87d75f, 0x87d787, 0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
   0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af,
   0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff,
   0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7, 0xaf87ff, 0xafaf00, 0xafaf5f,
   0xafaf87, 0xafafaf, 0xafafd7, 0xafafff, 0xafd700, 0xafd75f, 0xafd787, 0xafd7af,
   0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff,
   0xd70000, 0xd7005f, 0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f,
   0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af,
   0xd787d7, 0xd787ff, 0xd7af00, 0xd7af5f, 0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff,
   0xd7d700, 0xd7d75f, 0xd7d787, 0xd7d7af, 0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f,
   0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff, 0xff0000, 0xff005f, 0xff0087, 0xff00af,
   0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f, 0xff5f87, 0xff5faf, 0xff5fd7, 0xff5fff,
   0xff8700, 0xff875f, 0xff8787, 0xff87af, 0xff87d7, 0xff87ff, 0xffaf00, 0xffaf5f,
   0xffaf87, 0xffafaf, 0xffafd7, 0xffafff, 0xffd700, 0xffd75f, 0xffd787, 0xffd7af,
   0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffd7, 0xffffff,
   // >= 233: Grey ramp
   0x000000, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,
   0x585858, 0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
   0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0, 0xdadada,
};

// Find an xterm color value that matches an ARGB color
static inline uint8_t
rgb2xterm(uint8_t pixel[3])
{
   uint8_t match = 0;
   uint32_t distance = 1000000000;
   for (uint8_t c = 0; c <= 253; c++) {
      const uint32_t r = ((0xff0000 & colors[c]) >> 16) - pixel[0];
      const uint32_t g = ((0x00ff00 & colors[c]) >> 8) - pixel[1];
      const uint32_t b = (0x0000ff & colors[c]) - pixel[2];
      const uint32_t d = r * r + g * g + b * b;
      if (d < distance) {
         distance = d;
         match = c;
      }
   }

   return match;
}

static inline void
print_usage()
{
   printf("icat (" VERSION ") outputs an image on a 256-color or 24-bit color enabled terminal with UTF-8 locale.\n"
         "Usage: icat [-h|--help] [-x|-y value] [--width|--height value] [-k|--keep] imagefile [imagefile...]\n"
         "-h | --help  -- Display this message\n"
         "-x value     -- Specify the column to print the image in (min. 1)\n"
         "-y value     -- Specify the row to print the image in (min. 1)\n"
         "--width v    -- Force width of the image (min. 1)\n"
         "--height v   -- Force height of the image (min. 1)\n"
         "                This is ignored when more than one image is printed.\n"
         "-k | --keep  -- Keep image size, i.e. do not automatically resize image to fit\n"
         "                the terminal width.\n"
         "-m | --mode indexed|24bit|both\n"
         "             -- Use indexed (256-color, the default), 24-bit color, or both.\n"
         "imagefile    -- The image to print. If the file name is \"-\", the file is\n"
         "                read from stdin.\n"
         "Big images are automatically resized to your terminal width, unless with the -k option.\n"
         "You can achieve the same effect with convert (from the ImageMagick package):\n"
         "convert -resize $((COLUMNS - 2))x image.png - | icat -\n"
         "Or read images from another source:\n"
         "curl -sL http://example.com/image.png | convert -resize $((COLUMNS - 2))x - - | icat -\n");
}

// Find out and return the number of columns in the terminal
static inline uint32_t
terminal_width()
{
   int32_t cols = 0;
   int fds[2] = { STDOUT_FILENO, STDIN_FILENO };
   for (uint8_t i = 0; i < 2 && cols <= 0; ++i) {
#ifdef TIOCGSIZE
      struct ttysize ts;
      ioctl(fds[i], TIOCGSIZE, &ts);
      cols = ts.ts_cols;
#elif defined(TIOCGWINSZ)
      struct winsize ts;
      ioctl(fds[i], TIOCGWINSZ, &ts);
      cols = ts.ws_col;
#endif /* TIOCGSIZE */
   }
   return (cols > 0 ? cols : 80);
}

// Prints two pixels inside one character, p1 below p2.
// Characters in terminal fonts are usually twice as high
// as they are wide.
static inline void
print_pixels(uint8_t p1[4], uint8_t p2[4], bool rgba, enum mode mode)
{
   // Newer xterms should support 24-bit color with the ESC[38;2;<r>;<g>;<b>m sequence.
   // For backward compatibility, we insert the old ESC[38;5;<color_index>m before it.
   const char *upper = "▀", *lower = "▄";
   if (rgba && p1[3] == 0 && p2[3] == 0) {
      // Both pixels are transparent
      printf("\x1b[0m ");
   } else if (rgba && p1[3] == 0 && p2[3] != 0) {
      // Only lower pixel is transparent
      fputs("\x1b[0m", stdout);

      if (mode & MODE_INDEXED) {
         const uint8_t col2 = rgb2xterm(p2);
         printf("\x1b[38;5;%dm", col2);
      }

      if (mode & MODE_24_BIT)
         printf("\x1b[38;2;%d;%d;%dm", p2[0], p2[1], p2[2]);

      fputs(upper, stdout);
   } else if (rgba && p1[3] != 0 && p2[3] == 0) {
      // Only upper pixel is transparent
      fputs("\x1b[0m", stdout);

      if (mode & MODE_INDEXED) {
         const uint8_t col1 = rgb2xterm(p1);
         printf("\x1b[38;5;%dm", col1);
      }

      if (mode & MODE_24_BIT)
         printf("\x1b[38;2;%d;%d;%dm", p1[0], p1[1], p1[2]);

      fputs(lower, stdout);
   } else {
      // Both pixels are opaque
      if (mode & MODE_INDEXED) {
         const uint8_t col1 = rgb2xterm(p1);
         const uint8_t col2 = rgb2xterm(p2);
         printf("\x1b[38;5;%dm\x1b[48;5;%dm", col1, col2);
      }

      if (mode & MODE_24_BIT)
         printf("\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm", p1[0], p1[1], p1[2], p2[0], p2[1], p2[2]);

      fputs(lower, stdout);
   }
}

static inline bool
av_next(struct av *av, unsigned int x, int mode)
{
   int found = 0;
   bool done = false, failure = false;
   while (!found) {
      AVPacket packet;
      if (av_read_frame(av->format, &packet) < 0) {
         done = true;

         if (av->frames > 0)
            return true;
      }

      if (packet.stream_index == av->video_stream) {
         if (avcodec_decode_video2(av->context, av->frame, &found, &packet) < 0)
            done = failure = true;
      }

      av_free_packet(&packet);

      if (done)
         break;
   }

   if (failure)
      return done;

   // Convert Raw to RGB.
   sws_scale(av->sws, (const uint8_t**)av->frame->data, av->frame->linesize, 0, av->context->height, av->rgb->data, av->rgb->linesize);
   size_t data_width = av->rgb->linesize[0];

   if (av->frames > 0)
      fputs("\x1b[H", stdout);

   for (int h = 0; h < av->height; h += 2) {
      // If an x-offset is given, position the cursor in that column
      if (x > 0) {
         printf("\x1b[%dG", x);
      }

      uint8_t *row2 = av->rgb->data[0] + data_width * h;
      uint8_t *row = (h >= av->height - 1 ? NULL : av->rgb->data[0] + data_width * (h + 1));

      // Draw a horizontal line. Each console line consists of two
      // pixel lines in order to keep the pixels square (most console
      // fonts have two times the height for the width of each character)
      for (int w = 0; w < data_width; w += 3) {
         print_pixels((row ? row + w : (uint8_t[]){0,0,0,0}), row2 + w, false, mode);
      }

      fputs("\x1b[0m\n", stdout);
   }

   if (data_width > 0)
      ++av->frames;

   return done;
}

static inline
double r2d(AVRational r)
{
   return (r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den);
}

static bool
av_prepare(struct av *av, const char *filename, bool keep_size, uint32_t userw, uint32_t userh)
{
   if (avformat_open_input(&av->format, filename, NULL, NULL) < 0)
      return false;

   if (avformat_find_stream_info(av->format, NULL) < 0)
      return false;

   av_dump_format(av->format, 0, filename, false);

   if ((av->video_stream = av_find_best_stream(av->format, AVMEDIA_TYPE_VIDEO, -1, -1, &av->codec, 0)) < 0)
      return false;

   if (!(av->context = av->format->streams[av->video_stream]->codec))
      return false;

   av->fps = r2d(av_stream_get_r_frame_rate(av->format->streams[av->video_stream]));

   if (av->fps < 0.000025) {
      av->fps = r2d(av->format->streams[av->video_stream]->avg_frame_rate);
   }

   if (av->fps < 0.000025) {
      av->fps = 1.0 / r2d(av->format->streams[av->video_stream]->codec->time_base);
   }

   uint32_t width = av->context->width;
   uint32_t height = av->context->height;

   // Find out terminal size and resize image to fit, if necessary
   if (!keep_size) {
      int cols = terminal_width();
      if (cols < width - 1) {
         int resized_width = cols - 1;
         int resized_height = (int)(height * ((float)resized_width / width));
         width = resized_width;
         height = resized_height;
      }
   }

   width = (userw > 0 ? userw : width);
   height = (userh > 0 ? userh : height);

   if (!(av->sws = sws_getContext(av->context->width, av->context->height, av->context->pix_fmt, width, height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, NULL, NULL, NULL)))
      return false;

   if (!(av->codec = avcodec_find_decoder(av->context->codec_id)))
      return false;

   if (avcodec_open2(av->context, av->codec, NULL) < 0)
      return false;

   if (!(av->frame = av_frame_alloc()) || !(av->rgb = av_frame_alloc()))
      return false;

   size_t rgb_bytes = avpicture_get_size(PIX_FMT_RGB24, width, height);
   if (!(av->buffer = av_malloc(rgb_bytes)))
      return false;

   if (avpicture_fill((AVPicture*)av->rgb, av->buffer, PIX_FMT_RGB24, width, height) < width * 3)
      return false;

   av->width = width;
   av->height = height;
   return true;
}

static bool
av_init(struct av *av, AVIOContext *io)
{
   memset(av, 0, sizeof(struct av));

   static bool registered = false;
   if (!registered) {
      av_log_set_level(AV_LOG_ERROR);
      avcodec_register_all();
      av_register_all();
      registered = true;
   }

   if (!(av->format = avformat_alloc_context()))
      return false;

   av->format->pb = av->io = io;
   return true;
}

static void
av_release(struct av *av)
{
   if (!av)
      return;

   if (av->io)
      av_free(av->io->buffer);

   av_free(av->io);

   av_free(av->buffer);
   av_free(av->frame);
   av_free(av->rgb);

   if (av->sws)
      sws_freeContext(av->sws);

   if (av->context)
      avcodec_close(av->context);

   if (av->format)
      avformat_close_input(&av->format);
}

static inline
int av_read_stdin(void *opaque, uint8_t *buf, int buf_size)
{
   FILE *stream = opaque;
   return fread(buf, 1, buf_size, stream);
}

static bool running = true;

static void
sigint(int sig)
{
    (void)sig;
    running = false;
}

int main(int argc, char *argv[])
{
   static int display_help = 0;
   uint32_t x = 0, y = 0, width = 0, height = 0;
   bool keep_size = false;
   enum mode mode = MODE_INDEXED;

   struct sigaction action;
   memset(&action, 0, sizeof(struct sigaction));
   action.sa_handler = sigint;
   sigaction(SIGABRT, &action, NULL);
   sigaction(SIGSEGV, &action, NULL);
   sigaction(SIGTRAP, &action, NULL);
   sigaction(SIGINT, &action, NULL);

   for(;;) {
      static struct option long_options[] = {
         {"help", no_argument,       &display_help, 1},
         {"x",    required_argument, 0, 'x'},
         {"y",    required_argument, 0, 'y'},
         {"keep", no_argument,       0, 'k'},
         {"mode", required_argument, 0, 'm'},
         {"width", required_argument, 0, 0x100},
         {"height", required_argument, 0, 0x101},
         {0, 0, 0, 0}
      };

      int c;
      if ((c = getopt_long(argc, argv, "hx:y:km:", long_options, NULL)) == -1)
         break;

      switch (c) {
         case 'x':
            x = atoi(optarg);
            x = (x < 0 ? 0 : x);
            break;

         case 'y':
            y = atoi(optarg);
            y = (y < 0 ? 0 : y);
            break;

         case 'h':
            display_help = true;
            break;

         case 'k':
            keep_size = true;
            break;

         case 0x100:
            width = atoi(optarg);
            break;

         case 0x101:
            height = atoi(optarg);
            break;

         case 'm':
            if (!strcmp(optarg, "indexed")) {
               mode = MODE_INDEXED;
            } else if (!strcmp(optarg, "24bit")) {
               mode = MODE_24_BIT;
            } else if (!strcmp(optarg, "both")) {
               mode = MODE_BOTH;
            } else {
               printf("Mode must be one of 'indexed', '24bit', or 'both'.\n");
               exit(1);
            }
            break;

         case '?':
            break;

         default:
            display_help = true;
            break;
      }
   }

   if (display_help == true || optind == argc) {
      print_usage();
      exit(0);
   }

   // Ignore y value when more than one picture is printed
   if (argc - optind > 1)
      y = 0;

   // Start from top
   fputs("\x1b[H\x1b[?25l", stdout);

   for (uint32_t i = optind; i < argc; i++) {
      // Read from stdin and write temp file. Although a temp file
      // is ugly, imlib can not seek in a pipe and therefore not
      // read an image from it.
      AVIOContext *io = NULL;
      const char *filename = NULL;
      if (strcmp(argv[i], "-") == 0) {
         size_t size = 32768 + FF_INPUT_BUFFER_PADDING_SIZE;

         uint8_t *buf;
         if (!(buf = av_malloc(size)))
            continue;

         if (!(io = avio_alloc_context(buf, size, 0, stdin, &av_read_stdin, NULL, NULL))) {
            av_free(buf);
            continue;
         }

         filename = "stdin";
      } else {
         filename = argv[i];
         io = NULL;
      }

      struct av av;
      if (!av_init(&av, io))
         return EXIT_FAILURE;

      if (!av_prepare(&av, filename, keep_size, width, height))
         goto out;

      // If an y-value is given, position the cursor in that line (and
      // given or default column)
      if (y > 0)
         printf("\x1b[%d;%dH", y, x);

      uint64_t frame = 0;
      const double fps = av.fps, tpf = 1.0 / fps;
      struct timespec start;
      clock_gettime(CLOCK_MONOTONIC, &start);
      while (running) {
         if (av_next(&av, x, mode))
            break;

         fflush(stdout);

         struct timespec end;
         clock_gettime(CLOCK_MONOTONIC, &end);

         ++frame;
         const double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 10e9f;
         const double repose = (frame * tpf) - elapsed;
         // printf("%f %f %f %llu %d\n", fps, elapsed, repose, (repose > 0.0 ? (uint64_t)(repose * 10e6) : 0));
         if (repose > 0.0)
            nanosleep((struct timespec[]){{0, (uint64_t)(repose * 10e6)}}, NULL);
      }

out:
      av_release(&av);
   }

   fputs("\x1b[?25h", stdout);
   fflush(stdout);
   return EXIT_SUCCESS;
}
