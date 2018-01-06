#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct vertex {
  int x;
  int y;
  int z;
  int offset;
  int x_rotation;
  int y_rotation;
} vertex;

struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
uint8_t * fbp;
int tty;

FILE *fp;

int log_pixel = 0;

uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b)
{
  return (r << vinfo.red.offset) | (g << vinfo.green.offset) | (b << vinfo.blue.offset);
}

void draw_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
  if(log_pixel == 1)
    fprintf(fp, "pixel: x=%d - y=%d\n", x, y);
  long location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y+vinfo.yoffset) * finfo.line_length;
  *((uint32_t*)(fbp + location)) = pixel_color(r, g, b);
}

void draw_background() {
  int x, y;
  for(x = 0; x < vinfo.xres; x++)
    for(y = 0; y < vinfo.yres; y++) {
      draw_pixel(x, y, 0x01, 0x01, 0x01);
    }
}

void draw_horizontal_line(int y, int begin_x, int end_x, uint8_t r, uint8_t g, uint8_t b) {
  int true_begin_x;
  int true_end_x;
  if(begin_x < end_x) {
    true_begin_x = begin_x;
    true_end_x = end_x;
  } else {
    true_begin_x = end_x;
    true_end_x = begin_x;
  }
  int x;
  for(x = true_begin_x; x < true_end_x; ++x) {
    draw_pixel(x, y, r, g, b);
  }
}

void draw_vertical_line(int x, int begin_y, int end_y, uint8_t r, uint8_t g, uint8_t b) {
  int true_begin_y;
  int true_end_y;
  if(begin_y < end_y) {
    true_begin_y = begin_y;
    true_end_y = end_y;
  } else {
    true_begin_y = end_y;
    true_end_y = begin_y;
  }
  fprintf(fp, "x1=%d - y1=%d - x2=%d - y2=%d\n", x, true_begin_y ,x, true_end_y);
  log_pixel = 1;
  int y;
  for(y = true_begin_y; y < true_end_y; ++y) {
    draw_pixel(x, y, r, g, b);
  }
  log_pixel = 0;
}

void draw_slope(int x1, int x2, int y1, int y2, uint8_t r, uint8_t g, uint8_t b) {
  double slope = (double)(y2 - y1) / (x2 - x1);
  int true_begin_x;
  int true_end_x;
  if(x1 < x2) {
    fprintf(fp, "x1=%d - y1=%d - x2=%d - y2=%d\n", x1, y1, x2, y2);
    true_begin_x = x1;
    true_end_x = x2;
  } else {
    fprintf(fp, "x1=%d - y1=%d - x2=%d - y2=%d\n", x2, y2, x1, y1);
    true_begin_x = x2;
    true_end_x = x1;
  }
  int x;
  double x1_t_m = slope * x1;
  for(x = true_begin_x; x < true_end_x; ++x) {
    int y = slope * x - x1_t_m + y1;
    draw_pixel(x, y, r, g, b);

  }
}

vertex update_vertex(vertex * vertex) {
  if(vertex->offset >= 100) {
    vertex->x_rotation = -5;
  } else if(vertex->offset <= -100) {
    vertex->x_rotation = 5;
  }
  
  if(vertex->offset >= 100) {
    vertex->y_rotation = -5;
  } else if(vertex->offset <= -100) {
    vertex->y_rotation = 5;
  }
  vertex->offset += vertex->x_rotation;
  vertex->offset += vertex->y_rotation;
  vertex->x += vertex->x_rotation;
  vertex->y += vertex->y_rotation;
}  

void cya() {
  fclose(fp);
  ioctl(tty, KDSETMODE, KD_TEXT);
  exit(0);
}

int main()
{
  int fb_fd = open("/dev/fb0", O_RDWR);
  ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
  vinfo.grayscale = 0;
  vinfo.bits_per_pixel = 32;
  ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
  ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
  ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);
  
  tty = open("/dev/tty4", O_RDWR);
  ioctl(tty, KDSETMODE, KD_GRAPHICS);

  long screensize = vinfo.yres_virtual * finfo.line_length;
  fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);
  struct input_event ev;
  int fd = open("/dev/input/event3", O_RDONLY | O_NONBLOCK);

  fp = fopen("/tmp/test.txt", "w+");
  
  int mid_x = vinfo.xres * .5;
  int mid_y = vinfo.yres * .5;
  
  vertex vertices[8] = {
    (vertex){mid_x+-100, mid_y+100, 100, 5, 5},
    (vertex){mid_x+100, mid_y+100, 100, 5, 5},
    (vertex){mid_x+-100, mid_y+-100, 100, 5, 5},
    (vertex){mid_x+100, mid_y+-100, 100, 5, 5},
    (vertex){mid_x+-100, mid_y+100, -100, -5, -5},
    (vertex){mid_x+100, mid_y+100, -100, -5, -5},
    (vertex){mid_x+-100, mid_y+-100, -100, -5, -5},
    (vertex){mid_x+100, mid_y+-100, -100, -5, -5}
  };
  
  int edges[12][2] = {
    {0, 1},
    {0, 2},
    {0, 4},
    {1, 3},
    {1, 5},
    {2, 3},
    {2, 6},
    {3, 7},
    {4, 5},
    {4, 6},
    {5, 7},
    {6, 7}
  };

  while(1) {
    read(fd, &ev, sizeof ev);
      if(ev.type == 0x01 && ev.code == 16)
        cya(); 

    draw_background();

    int edge_count;
    for(edge_count = 0; edge_count < 12; ++edge_count) {
      int vertex1_index = edges[edge_count][0];
      int vertex2_index = edges[edge_count][1];
      vertex * vertex1 = &vertices[vertex1_index];
      vertex * vertex2 = &vertices[vertex2_index];
      fprintf(fp, "edge count=%d - x1=%d - x2=%d\n ", edge_count, vertex1->x, vertex2->x);
      if(vertex1->x == vertex2->x) {
        draw_vertical_line(vertex1->x, vertex1->y, vertex2->y, 0xFF, 0x00, 0xFF);
      } else {
        draw_slope(vertex1->x, vertex2->x, vertex1->y, vertex2->y, 0xFF, 0x00, 0xFF);
      }

    }
    for(edge_count = 0; edge_count < 12; ++edge_count) {
      int vertex1_index = edges[edge_count][0];
      int vertex2_index = edges[edge_count][1];
      vertex * vertex1 = &vertices[vertex1_index];
      vertex * vertex2 = &vertices[vertex2_index];

      update_vertex(vertex1);
      update_vertex(vertex2);
    }          
  }
}
