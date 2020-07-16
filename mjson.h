// Copyright (c) 2018 Cesanta Software Limited
// All rights reserved
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ATTR
#define ATTR
#endif


typedef int (*mjson_cb_t)(int ev, const char *s, int off, int len, void *ud);
typedef int (*mjson_print_fn_t)(const char *buf, int len, void *userdata);
typedef int (*mjson_vprint_fn_t)(mjson_print_fn_t, void *, va_list *);

int ATTR mjson_print_dbl(mjson_print_fn_t fn, void *fndata, double d,
                         const char *fmt);
int ATTR mjson_print_buf(mjson_print_fn_t fn, void *fndata, const char *buf,
                         int len);


struct mjson_fixedbuf {
  char *ptr;
  int size, len;
};

int mjson_printf(mjson_print_fn_t, void *, const char *fmt, ...);
int mjson_vprintf(mjson_print_fn_t, void *, const char *fmt, va_list ap);
int mjson_print_str(mjson_print_fn_t, void *, const char *s, int len);
int mjson_print_int(mjson_print_fn_t, void *, int value, int is_signed);
int mjson_print_long(mjson_print_fn_t, void *, long value, int is_signed);

int mjson_print_null(const char *ptr, int len, void *userdata);
int mjson_print_file(const char *ptr, int len, void *userdata);
int mjson_print_fixed_buf(const char *ptr, int len, void *userdata);
int mjson_print_dynamic_buf(const char *ptr, int len, void *userdata);
int maybe_escape(char ch, mjson_print_fn_t fn, void *fndata);
int unicode_escape(unsigned int ch, mjson_print_fn_t fn, void *fndata);
int get_utf_len(unsigned char ch);
int convert_utf(unsigned char first_ch, const char *s, int len);


/* staiic int mjson_esc(int c, int esc) { */
/*   const char *p, *esc1 = "\b\f\n\r\t\\\"", *esc2 = "bfnrt\\\""; */
/*   for (p = esc ? esc1 : esc2; *p != '\0'; p++) { */
/*     if (*p == c) return esc ? esc2[p - esc1] : esc1[p - esc2]; */
/*   } */
/*   return 0; */
/* } */

int ATTR mjson_print_fixed_buf(const char *ptr, int len, void *fndata) {
  struct mjson_fixedbuf *fb = (struct mjson_fixedbuf *) fndata;
  int i, left = fb->size - 1 - fb->len;
  if (left < len) len = left;
  for (i = 0; i < len; i++) fb->ptr[fb->len + i] = ptr[i];
  fb->len += len;
  fb->ptr[fb->len] = '\0';
  return len;
}

int ATTR mjson_print_dynamic_buf(const char *ptr, int len, void *fndata) {
  char *s, *buf = *(char **) fndata;
  int curlen = buf == NULL ? 0 : strlen(buf);
  if ((s = (char *) realloc(buf, curlen + len + 1)) == NULL) {
    return 0;
  } else {
    memcpy(s + curlen, ptr, len);
    s[curlen + len] = '\0';
    *(char **) fndata = s;
    return len;
  }
}

int ATTR mjson_print_null(const char *ptr, int len, void *userdata) {
  (void) ptr;
  (void) userdata;
  return len;
}

int ATTR mjson_print_file(const char *ptr, int len, void *userdata) {
  return fwrite(ptr, 1, len, (FILE *) userdata);
}

int ATTR mjson_print_buf(mjson_print_fn_t fn, void *fndata, const char *buf,
                         int len) {
  return fn(buf, len, fndata);
}

int ATTR mjson_print_int(mjson_print_fn_t fn, void *fndata, int value,
                         int is_signed) {
  char buf[20];
  int len = snprintf(buf, sizeof(buf), is_signed ? "%d" : "%u", value);
  return fn(buf, len, fndata);
}

int ATTR mjson_print_long(mjson_print_fn_t fn, void *fndata, long value,
                          int is_signed) {
  char buf[20];
  const char *fmt = (is_signed ? "%ld" : "%lu");
  int len = snprintf(buf, sizeof(buf), fmt, value);
  return fn(buf, len, fndata);
}

int ATTR mjson_print_dbl(mjson_print_fn_t fn, void *fndata, double d,
                         const char *fmt) {
  char buf[40];
  int n = snprintf(buf, sizeof(buf), fmt, d);
  return fn(buf, n, fndata);
}


int get_utf_len(unsigned char ch) {
  if ((ch & 0xe0) == 0xc0)
    return 1;

  if ((ch & 0xf0) == 0xe0)
    return 2;

  if ((ch & 0xf8) == 0xf0)
    return 3;

  fprintf(stderr, "Invalid first byte in utf: %02.2x", ch);
  exit(1);
}

int convert_utf(unsigned char first_ch, const char *s, int len) {
  int mask = 0x3f >> len;
  int result = (first_ch & mask);

  while (--len) {
    unsigned char next = *s++;
    if ((next & 0xc0) != 0x80) {
      fprintf(stderr, "Invalid continuation for UTF: %02.2x\n", next);
    }
    result = (result << 6) | (next & 0x3f);
  }
  return result;
}

int unicode_escape(unsigned int ch, mjson_print_fn_t fn, void *fndata) {
  char result[7];
  sprintf(result, "\\u%04.4x", ch);
  return fn(result, 6, fndata);
}

int ATTR mjson_print_str(
    mjson_print_fn_t fn,
    void *fndata,
    const char *s,
    int len
) {
  int i = 0,
      n = fn("\"", 1, fndata);
 
  while (i < len) {
    const unsigned char ch = s[i++];
    if (ch < 128) {
      n += maybe_escape(ch, fn, fndata);
    }
    else {
      const int len = get_utf_len(ch);
      unsigned int unicode = convert_utf(ch, s+i, len);
      n += unicode_escape(unicode, fn, fndata);
      i += len;
    }
  }
  return n + fn("\"", 1, fndata);
}

const char offset = 'b' - '\b';

int maybe_escape(char ch, mjson_print_fn_t fn, void *fndata) {
  char new_char;

  switch (ch) {
    case '"':
      return fn("\\\"", 2, fndata);
    case '\\':
      return fn("\\\\", 2, fndata);

    case '\b':
    case '\f':
    case '\t':
      new_char = ch + offset;
      return fn("\\", 1, fndata) + fn(&new_char, 1, fndata);

    case '\n':
      return fn("\\n", 2, fndata);

    case '\r':
      return fn("\\r", 2, fndata);

    default:
      if (ch < 32)
        return unicode_escape(ch, fn, fndata);
      else
        return fn(&ch, 1, fndata);
  }
}

int ATTR mjson_vprintf(mjson_print_fn_t fn, void *fndata, const char *fmt,
                       va_list xap) {
  int i = 0, n = 0;
  va_list ap;
  va_copy(ap, xap);
  while (fmt[i] != '\0') {
    if (fmt[i] == '%') {
      char fc = fmt[++i];
      int is_long = 0;
      if (fc == 'l') {
        is_long = 1;
        fc = fmt[i + 1];
      }
      if (fc == 'Q') {
        char *buf = va_arg(ap, char *);
        n += mjson_print_str(fn, fndata, buf ? buf : "", buf ? strlen(buf) : 0);
      } else if (strncmp(&fmt[i], ".*Q", 3) == 0) {
        int len = va_arg(ap, int);
        char *buf = va_arg(ap, char *);
        n += mjson_print_str(fn, fndata, buf, len);
        i += 2;
      } else if (fc == 'd' || fc == 'u') {
        int is_signed = (fc == 'd');
        if (is_long) {
          long val = va_arg(ap, long);
          n += mjson_print_long(fn, fndata, val, is_signed);
          i++;
        } else {
          int val = va_arg(ap, int);
          n += mjson_print_int(fn, fndata, val, is_signed);
        }
      } else if (fc == 'B') {
        const char *s = va_arg(ap, int) ? "true" : "false";
        n += mjson_print_buf(fn, fndata, s, strlen(s));
      } else if (fc == 's') {
        char *buf = va_arg(ap, char *);
        n += mjson_print_buf(fn, fndata, buf, strlen(buf));
      } else if (strncmp(&fmt[i], ".*s", 3) == 0) {
        int len = va_arg(ap, int);
        char *buf = va_arg(ap, char *);
        n += mjson_print_buf(fn, fndata, buf, len);
        i += 2;
      } else if (fc == 'g') {
        n += mjson_print_dbl(fn, fndata, va_arg(ap, double), "%g");
      } else if (fc == 'f') {
        n += mjson_print_dbl(fn, fndata, va_arg(ap, double), "%f");
      } else if (fc == 'H') {
        const char *hex = "0123456789abcdef";
        int i, len = va_arg(ap, int);
        const unsigned char *p = va_arg(ap, const unsigned char *);
        n += fn("\"", 1, fndata);
        for (i = 0; i < len; i++) {
          n += fn(&hex[(p[i] >> 4) & 15], 1, fndata);
          n += fn(&hex[p[i] & 15], 1, fndata);
        }
        n += fn("\"", 1, fndata);
      } else if (fc == 'M') {
        mjson_vprint_fn_t vfn = va_arg(ap, mjson_vprint_fn_t);
        n += vfn(fn, fndata, &ap);
      }
      i++;
    } else {
      n += mjson_print_buf(fn, fndata, &fmt[i++], 1);
    }
  }
  va_end(xap);
  va_end(ap);
  return n;
}

int ATTR mjson_printf(mjson_print_fn_t fn, void *fndata, const char *fmt, ...) {
  va_list ap;
  int len;
  va_start(ap, fmt);
  len = mjson_vprintf(fn, fndata, fmt, ap);
  va_end(ap);
  return len;
}

static inline int is_digit(int c) {
  return c >= '0' && c <= '9';
}

/* NOTE: strtod() implementation by Yasuhiro Matsumoto. */
double ATTR strtod(const char *str, char **end) {
  double d = 0.0;
  int sign = 1, n = 0;
  const char *p = str, *a = str;

  /* decimal part */
  if (*p == '-') {
    sign = -1;
    ++p;
  } else if (*p == '+')
    ++p;
  if (is_digit(*p)) {
    d = (double) (*p++ - '0');
    while (*p && is_digit(*p)) {
      d = d * 10.0 + (double) (*p - '0');
      ++p;
      ++n;
    }
    a = p;
  } else if (*p != '.')
    goto done;
  d *= sign;

  /* fraction part */
  if (*p == '.') {
    double f = 0.0;
    double base = 0.1;
    ++p;

    if (is_digit(*p)) {
      while (*p && is_digit(*p)) {
        f += base * (*p - '0');
        base /= 10.0;
        ++p;
        ++n;
      }
    }
    d += f * sign;
    a = p;
  }

  /* exponential part */
  if ((*p == 'E') || (*p == 'e')) {
    int e = 0;
    ++p;

    sign = 1;
    if (*p == '-') {
      sign = -1;
      ++p;
    } else if (*p == '+')
      ++p;

    if (is_digit(*p)) {
      while (*p == '0') ++p;
      e = (int) (*p++ - '0');
      while (*p && is_digit(*p)) {
        e = e * 10 + (int) (*p - '0');
        ++p;
      }
      e *= sign;
    } else if (!is_digit(*(a - 1))) {
      a = str;
      goto done;
    } else if (*p == 0)
      goto done;

    if (d == 2.2250738585072011 && e == -308) {
      d = 0.0;
      a = p;
      goto done;
    }
    if (d == 2.2250738585072012 && e <= -308) {
      d *= 1.0e-308;
      a = p;
      goto done;
    }
    {
      int i;
      for (i = 0; i < 10; i++) d *= 10;
    }
    a = p;
  } else if (p > str && !is_digit(*(p - 1))) {
    a = str;
    goto done;
  }

done:
  if (end) *end = (char *) a;
  return d;
}
