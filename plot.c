#include <math.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "nanovna.h"

#define SWAP(x,y) do { int z=x; x = y; y = z; } while(0)

void cell_draw_marker_info(int m, int n, int w, int h);
void draw_frequencies(void);
static inline void force_set_markmap(void);
void frequency_string(char *buf, size_t len, uint32_t freq);
void markmap_all_markers(void);

//#define GRID_COLOR 0x0863
uint16_t grid_color = 0x1084;

trace_t trace[TRACES_MAX] = {
  { 1, TRC_LOGMAG, 0, 1.0, RGB565(0,255,255), 0 },
  { 1, TRC_LOGMAG, 1, 1.0, RGB565(255,0,40), 0 },
  { 1, TRC_SMITH, 0, 1.0, RGB565(0,0,255), 1 },
  { 1, TRC_PHASE, 1, 1.0, RGB565(50,255,0), 1 }
};


#define CELLWIDTH 32
#define CELLHEIGHT 32

/*
 * CELL_X0[27:31] cell position
 * CELL_Y0[22:26]
 * CELL_N[10:21] original order
 * CELL_X[5:9] position in the cell
 * CELL_Y[0:4]
 */
uint32_t trace_index[TRACES_MAX][101];

#define INDEX(x, y, n) \
  ((((x)&0x03e0UL)<<22) | (((y)&0x03e0UL)<<17) | (((n)&0x0fffUL)<<10)  \
 | (((x)&0x1fUL)<<5) | ((y)&0x1fUL))

#define CELL_X(i) (int)((((i)>>5)&0x1f) | (((i)>>22)&0x03e0))
#define CELL_Y(i) (int)(((i)&0x1f) | (((i)>>17)&0x03e0))
#define CELL_N(i) (int)(((i)>>10)&0xfff)

#define CELL_X0(i) (int)(((i)>>22)&0x03e0)
#define CELL_Y0(i) (int)(((i)>>17)&0x03e0)

#define CELL_P(i, x, y) (((((x)&0x03e0UL)<<22) | (((y)&0x03e0UL)<<17)) == ((i)&0xffc00000UL))

/* indicate dirty cells */
uint16_t markmap[2][8];
uint16_t current_mappage = 0;


marker_t markers[4] = {
  { 1, 30 }, { 0, 40 }, { 0, 60 }, { 0, 80 }
};

int active_marker = 0;



int32_t fstart = 0;
int32_t fstop = 300000000;
int32_t fspan = 300000000;
int32_t fgrid = 50000000;
int16_t grid_offset;
int16_t grid_width;

void set_sweep(int32_t start, int stop)
{
  int32_t gdigit = 100000000;
  int32_t grid;
  fstart = start;
  fstop = stop;
  fspan = stop - start;

  while (gdigit > 100) {
    grid = 5 * gdigit;
    if (fspan / grid >= 4)
      break;
    grid = 2 * gdigit;
    if (fspan / grid >= 4)
      break;
    grid = gdigit;
    if (fspan / grid >= 4)
      break;
    gdigit /= 10;
  }
  fgrid = grid;

  grid_offset = (WIDTH-1) * ((fstart % fgrid) / 100) / (fspan / 100);
  grid_width = (WIDTH-1) * (fgrid / 100) / (fspan / 1000);

  force_set_markmap();
  draw_frequencies();
}

int
circle_inout(int x, int y, int r)
{
  int d = x*x + y*y - r*r;
  if (d <= -r)
    return 1;
  if (d > r)
    return -1;
  return 0;
}


#define POLAR_CENTER_X 146
#define POLAR_CENTER_Y 116
#define POLAR_RADIUS 116

int
smith_grid(int x, int y)
{
  int d = circle_inout(x-146, y-116, 116);
  int c = grid_color;
  if (d < 0)
    return 0;
  else if (d == 0)
    return c;
  x -= 146+116;
  y -= 116;
  
  if (circle_inout(x, y+58, 58) == 0)
    return c;
  if (circle_inout(x, y-58, 58) == 0)
    return c;
  d = circle_inout(x+29, y, 29);
  if (d > 0) return 0;
  if (d == 0) return c;
  if (circle_inout(x, y+116, 116) == 0)
    return c;
  if (circle_inout(x, y-116, 116) == 0)
    return c;
  d = circle_inout(x+58, y, 58);
  if (d > 0) return 0;
  if (d == 0) return c;
  if (circle_inout(x, y+232, 232) == 0)
    return c;
  if (circle_inout(x, y-232, 232) == 0)
    return c;
  if (circle_inout(x+87, y, 87) == 0)
    return c;
  return 0;
}

#if 0
int
rectangular_grid(int x, int y)
{
  int c = grid_color;
  //#define FREQ(x) (((x) * (fspan / 1000) / (WIDTH-1)) * 1000 + fstart)
  //int32_t n = FREQ(x-1) / fgrid;
  //int32_t m = FREQ(x) / fgrid;
  //if ((m - n) > 0)
  //if (((x * 6) % (WIDTH-1)) < 6)
  //if (((x - grid_offset) % grid_width) == 0)
  if (x == 0 || x == (WIDTH-1))
    return c;
  if ((y % 29) == 0)
    return c;
  if ((((x + grid_offset) * 10) % grid_width) < 10)
    return c;
  return 0;
}
#endif

int
rectangular_grid_x(int x)
{
  int c = grid_color;
  if (x == 0 || x == (WIDTH-1))
    return c;
  if ((((x + grid_offset) * 10) % grid_width) < 10)
    return c;
  return 0;
}

int
rectangular_grid_y(int y)
{
  int c = grid_color;
  if ((y % 29) == 0)
    return c;
  return 0;
}

#if 0
int
set_strut_grid(int x)
{
  uint16_t *buf = spi_buffer;
  int y;

  for (y = 0; y < HEIGHT; y++) {
    int c = rectangular_grid(x, y);
    c |= smith_grid(x, y);
    *buf++ = c;
  }
  return y;
}

void
draw_on_strut(int v0, int d, int color)
{
  int v;
  int v1 = v0 + d;
  if (v0 < 0) v0 = 0;
  if (v1 < 0) v1 = 0;
  if (v0 >= HEIGHT) v0 = HEIGHT-1;
  if (v1 >= HEIGHT) v1 = HEIGHT-1;
  if (v0 == v1) {
    v = v0; d = 2;
  } else if (v0 < v1) {
    v = v0; d = v1 - v0 + 1;
  } else {
    v = v1; d = v0 - v1 + 1;
  }
  while (d-- > 0)
    spi_buffer[v++] |= color;
}
#endif

/*
 * calculate log10(abs(gamma))
 */ 
float logmag(float *v)
{
  return log10f(v[0]*v[0] + v[1]*v[1]);
}

/*
 * calculate phase[-2:2] of coefficient
 */ 
float phase(float *v)
{
  return 2 * atan2f(v[1], v[0]) / M_PI;
}

/*
 * calculate abs(gamma) * 8
 */ 
float linear(float *v)
{
  return - sqrtf(v[0]*v[0] + v[1]*v[1]) * 8;
}

/*
 * calculate vswr; (1+gamma)/(1-gamma)
 */ 
float swr(float *v)
{
  float x = sqrtf(v[0]*v[0] + v[1]*v[1]);
  return (1 + x)/(1 - x);
}

#define RADIUS ((HEIGHT-1)/2)
void
cartesian_scale(float re, float im, int *xp, int *yp, float scale)
{
  //float scale = 4e-3;
  int x = re * RADIUS * scale;
  int y = im * RADIUS * scale;
  if (x < -RADIUS) x = -RADIUS;
  if (y < -RADIUS) y = -RADIUS;
  if (x > RADIUS) x = RADIUS;
  if (y > RADIUS) y = RADIUS;
  *xp = WIDTH/2 + x;
  *yp = HEIGHT/2 - y;
}


uint32_t
trace_into_index(int x, int t, int i, float coeff[2])
{
  int y = 0;
  float v = 0;
  switch (trace[t].type) {
  case TRC_LOGMAG:
    v = 1 - logmag(coeff);
    break;
  case TRC_PHASE:
    v = 4 + phase(coeff);
    break;
  case TRC_LINEAR:
    v = 8 + linear(coeff);
    break;
  case TRC_SWR:
    v = 9 - swr(coeff);
    break;
  case TRC_SMITH:
  case TRC_ADMIT:
  case TRC_POLAR:
    cartesian_scale(coeff[0], coeff[1], &x, &y, trace[t].scale);
    return INDEX(x, y, i);
    break;
  }
  if (v < 0) v = 0;
  if (v > 8) v = 8;
  y = v * 29;
  return INDEX(x, y, i);
}

void
trace_get_value_string(int t, char *buf, int len, float coeff[2])
{
  float v;
  switch (trace[t].type) {
  case TRC_LOGMAG:
    v = logmag(coeff);
    chsnprintf(buf, len, "%.2f dB", v * 10);
    break;
  case TRC_PHASE:
    v = phase(coeff);
    chsnprintf(buf, len, "%.2f deg", v * 90);
    break;
  case TRC_LINEAR:
    v = linear(coeff);
    chsnprintf(buf, len, "%.2f", v);
    break;
  case TRC_SWR:
    v = swr(coeff);
    chsnprintf(buf, len, "%.2f", v);
    break;
  case TRC_SMITH:
    chsnprintf(buf, len, "%.2f %.2fj", coeff[0], coeff[1]);
    break;
  }
}

void
trace_get_info(int t, char *buf, int len)
{
  const char *type = trc_type_name[trace[t].type];
  switch (trace[t].type) {
  case TRC_LOGMAG:
    chsnprintf(buf, len, "Ch%d %s %ddB/",
               trace[t].channel, type, (int)(trace[t].scale*10));
    break;
  case TRC_PHASE:
    chsnprintf(buf, len, "Ch%d %s %ddeg/",
               trace[t].channel, type, (int)(trace[t].scale*90));
    break;
  case TRC_SMITH:
  case TRC_ADMIT:
  case TRC_POLAR:
    chsnprintf(buf, len, "Ch%d %s %.1fFS",
               trace[t].channel, type, trace[t].scale);
    break;
  default:
    chsnprintf(buf, len, "Ch%d %s %.1f/",
               trace[t].channel, type, trace[t].scale);
    break;
  }
}

#if 0
void insertionsort(uint32_t *arr, int start, int end)
{
  int i;
  for (i = start + 1; i < end; i++) {
    uint32_t val = arr[i];
    int j = i - 1;
    while (j >= start && val > arr[j]) {
      arr[j + 1] = arr[j];
      j--;
    }
    arr[j + 1] = val;
  }
}

void quicksort(uint32_t *arr, int beg, int end)
{
  if (end - beg <= 1) 
    return;
  else if (end - beg < 10) {
    insertionsort(arr, beg, end);
  } else {
    int l = beg;
    int r = end-1;
    uint32_t piv = arr[(beg + end) / 2];
    while (l < r) {
      while (arr[l] < piv)
        l++;
      while (arr[r] > piv)
        r--;
      if (l < r)
        SWAP(arr[l], arr[r]);
    }

    quicksort(arr, beg, l);
    quicksort(arr, r, end);
  }
}
#endif

static inline void
mark_map(int x, int y)
{
  if (y >= 0 && y < 8 && x >= 0 && x < 16)
    markmap[current_mappage][y] |= 1<<x;
}

static inline int
is_mapmarked(int x, int y)
{
  uint16_t bit = 1<<x;
  return (markmap[0][y] & bit) || (markmap[1][y] & bit);
}

static void
swap_markmap(void)
{
  current_mappage = 1 - current_mappage;
}

static inline void
clear_markmap(void)
{
  memset(markmap[current_mappage], 0, sizeof markmap[current_mappage]);
}

static inline void
force_set_markmap(void)
{
  memset(markmap[current_mappage], 0xff, sizeof markmap[current_mappage]);
}

void
mark_cells_from_index(void)
{
  int t;
  /* mark cells between each neighber points */
  for (t = 0; t < TRACES_MAX; t++) {
    if (!trace[t].enabled)
      continue;
    int x0 = CELL_X(trace_index[t][0]);
    int y0 = CELL_Y(trace_index[t][0]);
    int m0 = x0 >> 5;
    int n0 = y0 >> 5;
    int i;
    mark_map(m0, n0);
    for (i = 1; i < 101; i++) {
      int x1 = CELL_X(trace_index[t][i]);
      int y1 = CELL_Y(trace_index[t][i]);
      int m1 = x1 >> 5;
      int n1 = y1 >> 5;
      while (m0 != m1 || n0 != n1) {
        if (m0 == m1) {
          if (n0 < n1) n0++; else n0--;
        } else if (n0 == n1) {
          if (m0 < m1) m0++; else m0--;
        } else {
          int x = (m0 < m1) ? (m0 + 1)<<5 : m0<<5;
          int y = (n0 < n1) ? (n0 + 1)<<5 : n0<<5;
          int sgn = (n0 < n1) ? 1 : -1;
          if (sgn*(y-y0)*(x1-x0) < sgn*(x-x0)*(y1-y0)) {
            if (m0 < m1) m0++;
            else m0--;
          } else {
            if (n0 < n1) n0++;
            else n0--;
          }
        }
        mark_map(m0, n0);
      }
      x0 = x1;
      y0 = y1;
      m0 = m1;
      n0 = n1;
    }
  }
}

void plot_into_index(float measured[2][101][2])
{
  int i, t;
  for (i = 0; i < 101; i++) {
    int x = i * (WIDTH-1) / (101-1);
    for (t = 0; t < TRACES_MAX; t++) {
      if (!trace[t].enabled)
        continue;
      int n = trace[t].channel;
      trace_index[t][i] = trace_into_index(x, t, i, measured[n][i]);
    }
  }
#if 0
  for (t = 0; t < TRACES_MAX; t++)
    if (trace[t].enabled && trace[t].polar)
      quicksort(trace_index[t], 0, 101);
#endif

  mark_cells_from_index();
  markmap_all_markers();
}

void
cell_drawline(int w, int h, int x0, int y0, int x1, int y1, int c)
{
  if (x0 > x1) {
    SWAP(x0, x1);
    SWAP(y0, y1);
  }

  while (x0 <= x1) {
    int dx = x1 - x0 + 1;
    int dy = y1 - y0;
    if (dy >= 0) {
      dy++;
      if (dy > dx) {
        dy /= dx; dx = 1;
      } else {
        dx /= dy; dy = 1;
      }
    } else {
      dy--;
      if (-dy > dx) {
        dy /= dx; dx = 1;
      } else {
        dx /= -dy; dy = -1;
      }
    }

    if (dx == 1) {
      if (dy > 0) {
        while (dy-- > 0) {
          if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
            spi_buffer[y0*w+x0] |= c;
          y0++;
        }
      } else {
        while (dy++ < 0) {
          if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
            spi_buffer[y0*w+x0] |= c;
          y0--;
        }
      }
      x0++;
    } else {
      while (dx-- > 0) {
        if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
          spi_buffer[y0*w+x0] |= c;
        x0++;
      }
      y0 += dy;
    }
  }
}

int
search_index(int x, int y, uint32_t index[101], int *i0, int *i1)
{
  int i, j;
  int head = 0;
  int tail = 101;
  x &= 0x03e0;
  y &= 0x03e0;
  while (head < tail) {
    i = (head + tail) / 2;
    if (x < CELL_X0(index[i]))
      tail = i+1;
    else if (x > CELL_X0(index[i]))
      head = i;
    else if (y < CELL_Y0(index[i]))
      tail = i+1;
    else if (y > CELL_Y0(index[i]))
      head = i;
    else
      break;
  }

  if (x != CELL_X0(index[i]) || y != CELL_Y0(index[i]))
    return FALSE;
    
  j = i;
  while (j > 0 && x == CELL_X0(index[j-1]) && y == CELL_Y0(index[j-1]))
    j--;
  *i0 = j;
  j = i;
  while (j < 100 && x == CELL_X0(index[j+1]) && y == CELL_Y0(index[j+1]))
    j++;
  *i1 = j;
  return TRUE;
}

int
search_index_x(int x, uint32_t index[101], int *i0, int *i1)
{
  int i, j;
  int head = 0;
  int tail = 101;
  x &= 0x03e0;
  while (head < tail) {
    i = (head + tail) / 2;
    if (x < CELL_X0(index[i]))
      tail = i+1;
    else if (x > CELL_X0(index[i]))
      head = i;
    else
      break;
  }

  if (x != CELL_X0(index[i]))
    return FALSE;

  j = i;
  while (j > 0 && x == CELL_X0(index[j-1]))
    j--;
  *i0 = j;
  j = i;
  while (j < 100 && x == CELL_X0(index[j+1]))
    j++;
  *i1 = j;
  return TRUE;
}

void
draw_marker(int w, int h, int x, int y, int c, int ch)
{
  int i, j;
  for (j = 10; j >= 0; j--) {
    int j0 = j / 2;
    for (i = -j0; i <= j0; i++) {
      int x0 = x + i;
      int y0 = y - j;
      int cc = c;
      if (j <= 9 && j > 2 && i >= -1 && i <= 3) {
        uint16_t bits = x5x7_bits[(ch * 7) + (9-j)];
        if (bits & (0x8000>>(i+1)))
          cc = 0;
      }
      if (y0 >= 0 && y0 < h && x0 >= 0 && x0 < w)
        spi_buffer[y0*w+x0] = cc;
    }
  }
}

void
cell_draw_markers(int m, int n, int w, int h)
{
  int x0 = m * CELLWIDTH;
  int y0 = n * CELLHEIGHT;
  int t, i;
  for (i = 0; i < 4; i++) {
    if (!markers[i].enabled)
      continue;
    for (t = 0; t < TRACES_MAX; t++) {
      if (!trace[t].enabled)
        continue;
      uint32_t index = trace_index[t][markers[i].index];
      int x = CELL_X(index) - x0;
      int y = CELL_Y(index) - y0;
      if (x > -6 && x < w+6 && y >= 0 && y < h+12)
        draw_marker(w, h, x, y, trace[t].color, '1' + i);
    }
  }
}

void
markmap_marker(int marker)
{
  int t;
  if (!markers[marker].enabled)
    return;
  for (t = 0; t < TRACES_MAX; t++) {
    if (!trace[t].enabled)
      continue;
    uint32_t index = trace_index[t][markers[marker].index];
    int x = CELL_X(index);
    int y = CELL_Y(index);
    int m = x>>5;
    int n = y>>5;
    mark_map(m, n);
    if ((x&31) < 6)
      mark_map(m-1, n);
    if ((x&31) > 32-6)
      mark_map(m+1, n);
    if ((y&31) < 12) {
      mark_map(m, n-1);
      if ((x&31) < 6)
        mark_map(m-1, n-1);
      if ((x&31) > 32-6)
        mark_map(m+1, n-1);
    }
  }
}

void
markmap_all_markers(void)
{
  int i;
  for (i = 0; i < 4; i++) {
    if (!markers[i].enabled)
      continue;
    markmap_marker(i);
  }
}


int area_width = WIDTH;
int area_height = HEIGHT;

void
draw_cell(int m, int n)
{
  int x0 = m * CELLWIDTH;
  int y0 = n * CELLHEIGHT;
  int w = CELLWIDTH;
  int h = CELLHEIGHT;
  int x, y;
  int i0, i1;
  int i;
  int t;
  if (x0 + w > area_width)
    w = area_width - x0;
  if (y0 + h > area_height)
    h = area_height - y0;

  PULSE;
  /* draw grid */
  for (x = 0; x < w; x++) {
    uint16_t c = rectangular_grid_x(x+x0);
    for (y = 0; y < h; y++)
      spi_buffer[y * w + x] = c;
  }
  for (y = 0; y < h; y++) {
    uint16_t c = rectangular_grid_y(y+y0);
    for (x = 0; x < w; x++)
      spi_buffer[y * w + x] |= c;
  }
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      //uint16_t c = rectangular_grid(x+x0, y+y0);
      uint16_t c = smith_grid(x+x0, y+y0);
      spi_buffer[y * w + x] |= c;
    }
  }
  PULSE;

#if 1
  /* draw rectanglar plot */
  if (search_index_x(x0, trace_index[0], &i0, &i1)) {
    for (t = 0; t < TRACES_MAX; t++) {
      if (!trace[t].enabled || trace[t].polar)
        continue;
      if (i0 > 0)
        i0--;
      for (i = i0; i < i1; i++) {
        int x1 = CELL_X(trace_index[t][i]);
        int x2 = CELL_X(trace_index[t][i+1]);
        int y1 = CELL_Y(trace_index[t][i]);
        int y2 = CELL_Y(trace_index[t][i+1]);
        int c = trace[t].color;
        cell_drawline(w, h, x1 - x0, y1 - y0, x2 - x0, y2 - y0, c);
      }
    }
  }
#endif
#if 1
  /* draw polar plot */
  for (t = 0; t < TRACES_MAX; t++) {
    int c = trace[t].color;
    if (!trace[t].enabled || !trace[t].polar)
      continue;
    for (i = 1; i < 101; i++) {
      //uint32_t index = trace_index[t][i];
      //uint32_t pindex = trace_index[t][i-1];
      //if (!CELL_P(index, x0, y0) && !CELL_P(pindex, x0, y0))
      //  continue;
      int x1 = CELL_X(trace_index[t][i-1]);
      int x2 = CELL_X(trace_index[t][i]);
      int y1 = CELL_Y(trace_index[t][i-1]);
      int y2 = CELL_Y(trace_index[t][i]);
      cell_drawline(w, h, x1 - x0, y1 - y0, x2 - x0, y2 - y0, c);
    }
  }
#endif
#if 0
  /* draw polar plot */
  for (t = 0; t < TRACES_MAX; t++) {
    int prev = -100;
    int c = trace[t].color;
    if (!trace[t].enabled || !trace[t].polar)
      continue;
    if (search_index(x0, y0, trace_index[t], &i0, &i1)) {
      for (i = i0; i < i1; i++) {
        uint32_t index = trace_index[t][i];
        uint32_t pindex;
        int n = i;
        if (!CELL_P(index, x0, y0))
          continue;
        n = CELL_N(index);
        if (n - prev == 1) {
          pindex = trace_index[t][prev];
          cell_drawline(w, h, CELL_X(pindex) - x0, CELL_Y(pindex) - y0, CELL_X(index) - x0, CELL_Y(index) - y0, c);
          }
          prev = n;
      }
    }
  }
#endif

  PULSE;
  cell_draw_markers(m, n, w, h);
  cell_draw_marker_info(m, n, w, h);
  PULSE;

  ili9341_bulk(OFFSETX + x0, OFFSETY + y0, w, h);
}

void
draw_cell_all(void)
{
  int m, n;
  for (m = 0; m < (area_width+CELLWIDTH-1) / CELLWIDTH; m++)
    for (n = 0; n < (area_height+CELLHEIGHT-1) / CELLHEIGHT; n++) {
      if (is_mapmarked(m, n))
        draw_cell(m, n);
      //ui_process();
    }

  // keep current map for update
  swap_markmap();
  // clear map for next plotting
  clear_markmap();
}

void
redraw_marker(int marker)
{
  // mark map on new position of marker
  markmap_marker(marker);

  // mark cells on marker info
  markmap[current_mappage][0] = 0xffff;

  draw_cell_all();
}

void
force_draw_cells(void)
{
  int n, m;
  for (m = 7; m <= 9; m++)
    for (n = 0; n < (area_height+CELLHEIGHT-1) / CELLHEIGHT; n++)
      draw_cell(m, n);
}


void
cell_drawchar_5x7(int w, int h, uint8_t ch, int x, int y, uint16_t fg)
{
  uint16_t bits;
  int c, r;
  if (y <= -7 || y >= h || x <= -5 || x >= w)
    return;
  for(c = 0; c < 7; c++) {
    if ((y + c) < 0 || (y + c) >= h)
      continue;
    bits = x5x7_bits[(ch * 7) + c];
    for (r = 0; r < 5; r++) {
      if ((x+r) >= 0 && (x+r) < w && (0x8000 & bits)) 
        spi_buffer[(y+c)*w + (x+r)] = fg;
      bits <<= 1;
    }
  }
}

void
cell_drawstring_5x7(int w, int h, char *str, int x, int y, uint16_t fg)
{
  while (*str) {
    cell_drawchar_5x7(w, h, *str, x, y, fg);
    x += 5;
    str++;
  }
}

void
cell_draw_marker_info(int m, int n, int w, int h)
{
  char buf[24];
  int t;
  if (active_marker < 0)
    return;
  if (n != 0 || m > 8)
    return;
  int idx = markers[active_marker].index;
#if 1
  int j = 0;
  for (t = 0; t < TRACES_MAX; t++) {
    if (!trace[t].enabled)
      continue;
    int xpos = 1 + (j%2)*152;
    int ypos = 1 + (j/2)*7;
    xpos -= m * w;
    ypos -= n * h;
    trace_get_info(t, buf, sizeof buf);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, trace[t].color);
    xpos += 84;
    trace_get_value_string(t, buf, sizeof buf, measured[trace[t].channel][idx]);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, trace[t].color);
    j++;
  }    
  int xpos = 192;
  int ypos = 1 + (j/2)*7;
  xpos -= m * w;
  ypos -= n * h;
  chsnprintf(buf, sizeof buf, "%d:", active_marker + 1);
  cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
  xpos += 16;
  frequency_string(buf, sizeof buf, frequencies[idx]);
  cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
#else
  if (m == 0 || m == 1 || m == 2) {
    int xpos = 1;
    int ypos = 1;
    xpos -= m * w;
    ypos -= n * h;
    for (t = 0; t < TRACES_MAX; t++) {
      if (!trace[t].enabled)
        continue;
      trace_get_info(t, buf, sizeof buf);
      cell_drawstring_5x7(w, h, buf, xpos, ypos, trace[t].color);
      ypos += 7;
    }
  }
#if 1
  if (m == 2 || m == 3 || m == 4) {
    int xpos = 87;
    int ypos = 1;
    xpos -= m * w;
    ypos -= n * h;
    for (t = 0; t < TRACES_MAX; t++) {
      trace_get_value_string(t, buf, sizeof buf, measured[trace[t].channel][idx]);
      if (!trace[t].enabled)
        continue;
      cell_drawstring_5x7(w, h, buf, xpos, ypos, trace[t].color);
      ypos += 7;
    }
  }
  if (m == 7 || m == 8) {
    int idx = markers[active_marker].index;
    int xpos = 224;
    int ypos = 1;
    xpos -= m * w;
    ypos -= n * h;
    frequency_string(buf, sizeof buf, frequencies[idx]);
    cell_drawstring_5x7(w, h, buf, xpos, ypos, 0xffff);
  }
#endif
#endif
}

void
frequency_string(char *buf, size_t len, uint32_t freq)
{
  chsnprintf(buf, len, "%d.%03d %03d MHz",
             (int)(freq / 1000000),
             (int)((freq / 1000) % 1000),
             (int)(freq % 1000));
}

void
draw_frequencies(void)
{
  char buf[24];
  chsnprintf(buf, 24, "START%3d.%03d %03d MHz",
             (int)(fstart / 1000000),
             (int)((fstart / 1000) % 1000),
             (int)(fstart % 1000));
  ili9341_drawstring_5x7(buf, OFFSETX, 233, 0xffff, 0x0000);
  chsnprintf(buf, 24, "STOP %3d.%03d %03d MHz",
             (int)(fstop / 1000000),
             (int)((fstop / 1000) % 1000),
             (int)(fstop % 1000));
  ili9341_drawstring_5x7(buf, 205, 233, 0xffff, 0x0000);
}

void
redraw(void)
{
  ili9341_fill(0, 0, 320, 240, 0);
  draw_frequencies();
}

void
plot_init(void)
{
  force_set_markmap();
}
