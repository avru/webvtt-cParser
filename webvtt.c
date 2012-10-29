/* WebVTT parser
Copyright 2011 Mozilla Foundation

This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "webvtt.h"

#define BUFFER_SIZE 4096
#define DEBUG 1
#define DEFAULT 20

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define FAIL(msg) { \
  fprintf(stderr, "ERROR: " msg "\n"); \
  exit(-1); \
}

#if DEBUG
#define FAILD(msg) { \
  fprintf(stderr, "ERROR: " msg "\n"); \
  exit(-1); \
}
#else
#define FAILD(msg) { \
  return BadCue; \
}
#endif

#if DEBUG
#define ERROR(msg) { \
  fprintf(stderr, "ERROR: " msg "\n"); \
  exit(-1); \
}
#endif

struct webvtt_parser {
  int state;
  char *buffer;
  unsigned offset, length;
};

webvtt_parser *
  webvtt_parse_new(void)
{
  webvtt_parser *ctx = (webvtt_parser*)malloc(sizeof(*ctx));
  if (ctx) {
    ctx->state = 0;
    ctx->buffer = (char*)malloc(BUFFER_SIZE);
    if (ctx->buffer == NULL) {
      free(ctx);
      return NULL;
    }
    ctx->offset = 0;
    ctx->length = 0;
  }
  return ctx;
}

void
  webvtt_parse_free(webvtt_parser *ctx)
{
  if (ctx) {
    ctx->state = 0;
    if (ctx->buffer) {
      free(ctx->buffer);
      ctx->buffer = NULL;
    }
    ctx->offset = 0;
    ctx->length = 0;
  }
}

int
  webvtt_print_cue(FILE *out, webvtt_cue *cue)
{
  int err;
  int h, m, s, ms;
  long time;

  if (out == NULL || cue == NULL)
    return -1;
  if (cue->cueID != NULL)
    err = fprintf(out, "%s", cue->cueID);
  if (cue->settings != NULL)
    err = fprintf(out, "%s", cue->settings);
  time = cue->start;
  h = time/3600000;
  time %= 3600000;
  m = time/60000;
  time %= 60000;
  s = time/1000;
  ms = time%1000;
  err = fprintf(out, "%02d:%02d:%02d.%03d", h, m, s, ms);

  time = cue->end;
  h = time/3600000;
  time %= 3600000;
  m = time/60000;
  time %= 60000;
  s = time/1000;
  ms = time%1000;
  err = fprintf(out, " --> %02d:%02d:%02d.%03d\n", h, m, s, ms);

  err = fprintf(out, "%s\n", cue->text);

  return err;
}

int has_file_identifier(webvtt_parser *ctx) {
  char *p = ctx->buffer;
  // Check for signature
  if (ctx->length < 6) {
    FAIL("Too short. Not a webvtt file\n");
  }
  if (p[0] == (char)0xef && p[1] == (char)0xbb && p[2] == (char)0xbf) {
    fprintf(stderr, "Byte order mark\n");
    ctx->offset += 3;
    if (ctx->length < 9) {
      FAIL("Too short. Not a webvtt file\n");
    }
  }
  if (memcmp(p + ctx->offset, "WEBVTT", 6)) {
    FAIL("Bad magic. Not a webvtt file?\n");
  }
  ctx->offset += 6;
  fprintf(stderr, "Found signature\n");
  return 1;
}

int move_to_next_line(webvtt_parser *ctx) {
  char *p = ctx->buffer;
  unsigned originalOffSet = ctx->offset;
  while (ctx->offset < ctx->length) {
    if (isASpace(*(p + ctx->offset)))
      ctx->offset += 1;
    else if(isNewline(*(p + ctx->offset))) {
      if (!memcmp(p + ctx->offset, "\r\n", 2) || !memcmp(p + ctx->offset, "\n\r", 2))
        ctx->offset += 2;
      else
        ctx->offset += 1;
      return 1;
    } else {
      ctx->offset = originalOffSet;
      return 0;
    }
  }
}

int get_cue_id(webvtt_parser *ctx, webvtt_cue *cue) {
  char *p = ctx->buffer;
  unsigned originalOffSet = ctx->offset;
  while (!move_to_next_line(ctx) && ctx->offset < ctx->length) {
    if (!memcmp(p + ctx->offset, "-->", 3)) {
      ctx->offset = originalOffSet;
      return TimingsAndSettings;
    }
    ctx->offset++;
  }
  ctx->offset = originalOffSet;
  cue->cueID = get_line(ctx);
  return TimingsAndSettings;
}

int get_timing_and_settings(webvtt_parser *ctx, webvtt_cue *cue) {
  char *p = ctx->buffer;

  double start_time = collect_timestamp(ctx);

  if (!isASpace(p[ctx->offset])) {
    ERROR("Need a space after timestamp");
  }

  while (isASpace(p[ctx->offset]))
    ctx->offset++;

  if (memcmp(p + ctx->offset, "-->", 3) != 0) {
    ERROR("No --> after timestamp");
  }
  ctx->offset+=3;
  if (!isASpace(p[ctx->offset])) {
    ERROR("Need a space after -->");
  }

  while (isASpace(p[ctx->offset]))
    ctx->offset++;

  double end_time = collect_timestamp(ctx);

  if (start_time > end_time) {
    ERROR("Start time cannot be > end time");
  }

  while (!move_to_next_line(ctx)) {
    ctx->offset++;
    if (isalpha(*(p + ctx->offset))) {
      cue->settings = get_line(ctx);
      parse_settings(cue->settings, cue);
      break;
    }
  }
  cue->start = start_time * 1e3;
  cue->end = end_time * 1e3;
  return CueText;
}

char* get_number(char *text, unsigned *position) {
  char *num = (char*)malloc(DEFAULT);
  int i = 0;
  while (is_a_number(text[*position])) {
    num[i++] = text[*position];
    (*position)++;
  }
  num[i] = '\0';
  return num;
}

double collect_timestamp(webvtt_parser *ctx) {
  if (move_to_next_line(ctx)) {
    ERROR("Couldn't parse cue timestamps");
  }
  enum Mode { minutes, hours };
  enum Mode mode = minutes;
  char *num1, *num2, *num3, *num4;
  int number1, number2, number3, number4;
  unsigned i = 0;
  char *p = ctx->buffer;

  while (isASpace(p[ctx->offset]))
    ctx->offset++;

  if (!is_a_number(p[ctx->offset])) {
    ERROR("Parse cue timestamps: Not a number");
  } else {
    num1 = get_number(p, &ctx->offset);
    sscanf(num1, "%d", &number1);
  }
  if (number1 > 59 || strlen(num1) < 2) {
    mode = hours;
  }
  if (p[ctx->offset] != ':') {
    ERROR("Parse cue timestamps: Expected ':'");
  }
  else {
    ctx->offset++;
  }
  if (!is_a_number(p[ctx->offset])) {
    ERROR("Parse cue timestamps: Not a number after':'");
  } else {
    num2 = get_number(p, &ctx->offset);
    if (strlen(num2) < 2) {
      ERROR("Parse cue timestamps: Minute digit < 2");
    }
    sscanf(num2, "%d", &number2);
  }
  //12.1
  if (mode == hours || (!move_to_next_line(ctx) && p[ctx->offset] == ':')) {
    if (move_to_next_line(ctx) || p[ctx->offset] != ':') {
      ERROR("Parse cue timestamps: No minute");
    } else {
      ctx->offset++;
    }
    if (!is_a_number(p[ctx->offset])) {
      ERROR("Parse cue timestamps: Not a number after second ':'");
    } else {
      num3 = get_number(p, &ctx->offset);
      if (strlen(num3) != 2) {
        ERROR("Parse cue timestamps: Minute digit != 2");
      }
      sscanf(num3, "%d", &number3);
    }
  } else {
    number3 = number2;
    number2 = number1;
    number1 = 0;
  }
  //13
  if (p[ctx->offset] != '.') {
    ERROR("Parse cue timestamps: No millisecond");
  } else {
    ctx->offset++;
  }
  //14
  if (!is_a_number(p[ctx->offset])) {
    ERROR("Parse cue timestamps: Not a number at millisecond");
  } else {
    num4 = get_number(p, &ctx->offset);
    if (strlen(num4) != 3) {
      ERROR("Parse cue timestamps: Millisecond digit != 3");
    }
    sscanf(num4, "%d", &number4);
  }
  //17
  if (number2 > 59 || number3 > 59) {
    ERROR("Parse cue timestamps: Minute or second is bigger than 59");
  }

  return number1*60*60 + number2*60 + number3 + (double)number4/1000;
}

char* get_word(char *text, int *position) {
  char setting[DEFAULT];
  int i = 0;
  while (!isspace(text[*position]) && text[*position] != '\0') {
    setting[i++] = text[*position];
    (*position)++;
  }
  setting[i] = '\0';
  // skip space
  while (isspace(text[*position])) {
    (*position)++;
  }

  return setting;
}

void word_to_lower(char *text) {
  unsigned i = 0;
  while (text[i] != '\0') {
    text[i] = tolower(text[i]);
    i++;
  }
}

int is_empty(char *text) {
  int i = 0;
  while(isASpace(text[i]))
    i++;
  if (text[i] != '\0')
    return 0;
  return 1;
}

void parse_settings(char *settings, webvtt_cue *cue) {
  int position = 0, i = 0, i2 = 0, num = 0;
  char *setting;
  char setting_name[DEFAULT];
  char *setting_value = (char*)malloc(DEFAULT);
  while (!isNewline(settings[position])) {
    setting = get_word(settings, &position);

    if (setting[0] == ':' || setting[0] == '\0')
      continue;

    i = 0;
    while (setting[i] != ':' && setting[i] != '\0') {
      setting_name[i] = setting[i];
      i++;
    }
    if (setting[i] == '\0') {
      ERROR("Bogus setting");
      continue;
    }

    setting_name[i] = '\0';

    i++;
    i2 = 0;
    while (setting[i] != '\0') {
      setting_value[i2++] = setting[i];
      i++;
    }
    setting_value[i2] = '\0';
    if (is_empty(setting_value)) {
      ERROR("There is no setting");
      continue;
    }

    // uncomment this for case insensitive
    //word_to_lower(setting_name);
    //word_to_lower(setting_value);

    switch (setting_name[0]) {
    case 'v':
      if (strcmp(setting_name, "vertical") != 0) {
        ERROR("Invalid vertical name?");
        continue;
      }
      if (strcmp(setting_value, "rl") == 0 || strcmp(setting_value, "lr") == 0)
        cue->vertical = setting_value;
      else {
        ERROR("Invalid verticle setting");
        continue;
      }
      break;
    case 'a':
      if (strcmp(setting_name, "align") != 0) {
        ERROR("Invalid setting name");
        continue;
      }
      // I hope there is a better way
      if (strcmp(setting_value, "start") != 0 &&
        strcmp(setting_value, "middle") != 0 &&
        strcmp(setting_value, "end") != 0 &&
        strcmp(setting_value, "left") != 0 &&
        strcmp(setting_value, "right") != 0) {
          ERROR("Invalid align value");
          continue;
      }

      cue->align = setting_value;
      break;
    case 'l':
      if (strcmp(setting_name, "line") != 0) {
        ERROR("Invalid line name?");
        continue;
      }

      i = 0;

      while (setting_value[i] != '\0') {
        if (setting_value[i] != '-' && setting_value[i] != '%' && !is_a_number(setting_value[i])) {
          ERROR("Invalid line value");
          break;
        }
        if (sscanf(setting_value, "%d", &num) <= 0) {
          ERROR("Invalid line value: no number");
          break;
        }
        if (i && setting_value[i] == '-') {
          ERROR("Invalid line value: '-'");
          break;
        }
        if (setting_value[i+1] != '\0' && setting_value[i] == '%') {
          ERROR("Invalid line value: '%%'");
          break;
        }
        i++;
      }
      if (setting_value[i] != '\0')
        continue;
      sscanf(setting_value, "%d", &num);

      if (setting_value[i-1] == '%' && (num < 0 || num > 100)) {
        ERROR("Invalid line value: Invalid percentage");
        continue;
      } else {
        cue->line = num;
      }
      break;
    case 'p':
      if (strcmp(setting_name, "position") != 0) {
        ERROR("Invalid position name?");
        continue;
      }

      i = 0;

      while (setting_value[i] != '\0') {
        if (setting_value[i] != '%' && !is_a_number(setting_value[i])) {
          ERROR("Invalid position value");
          break;
        }
        if (sscanf(setting_value, "%d", &num) <= 0) {
          ERROR("Invalid position value: no number");
          break;
        }
        i++;
      }
      if (setting_value[i] != '\0')
        continue;
      if (setting_value[i-1] != '%') {
        ERROR("Invalid position value: no %% at the end");
        continue;
      }
      sscanf(setting_value, "%d", &num);

      if (num < 0 || num > 100) {
        ERROR("Invalid position value: Invalid percentage");
        continue;
      } else {
        cue->position = num;
      }
      break;
    case 's':
      if (strcmp(setting_name, "size") != 0) {
        ERROR("Invalid size name?");
        continue;
      }

      i = 0;

      while (setting_value[i] != '\0') {
        if (setting_value[i] != '%' && !is_a_number(setting_value[i])) {
          ERROR("Invalid size value");
          break;
        }
        if (sscanf(setting_value, "%d", &num) <= 0) {
          ERROR("Invalid size value: no number");
          break;
        }
        i++;
      }
      if (setting_value[i] != '\0')
        continue;
      if (setting_value[i-1] != '%') {
        ERROR("Invalid size value: no %% at the end");
        continue;
      }
      sscanf(setting_value, "%d", &num);

      if (num < 0 || num > 100) {
        ERROR("Invalid size value: Invalid percentage");
        continue;
      } else {
        cue->size = num;
      }

      break;
    default:
      ERROR("Unknown setting name");
    }
    setting_value = (char*)malloc(DEFAULT);
  }
}

char* get_line(webvtt_parser *ctx) {
  char *p = ctx->buffer + ctx->offset;
  while (ctx->offset < ctx->length && !move_to_next_line(ctx))
    ctx->offset++;
  char *e = ctx->buffer + ctx->offset;
  char *text = (char*)malloc(e - p);
  if (text == NULL) {
    FAIL("Couldn't allocate cue text buffer\n");
  }
  memcpy(text, p, e - p);
  text[e - p] = '\0';
  return text;
}

int get_cue_text(webvtt_parser *ctx, webvtt_cue *cue) {
  cue->text = get_line(ctx);

  // multiple line support
  if (!move_to_next_line(ctx)) {
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "%s", cue->text);
    while (!move_to_next_line(ctx)) {
      sprintf(buffer, "%s %s", buffer, get_line(ctx));
    }
    char *ret = (char*)malloc(strlen(buffer)+1);
    sprintf(ret, "%s", buffer);
    cue->text = ret;
  }

  return NextCue;
}

char* append_char(char *text, char c) {
  char buffer[BUFFER_SIZE];
  sprintf(buffer, "%s", text);
  int len = strlen(buffer)+1;
  buffer[len] = c;
  buffer[len + 1] = c;
  char *ret = (char*)malloc(strlen(buffer)+1);
  sprintf(ret, "%s", buffer);
  return ret;
}

int ignore_bad_cue(webvtt_parser *ctx) {
  do {
    get_line(ctx);
  } while (!move_to_next_line(ctx));
  return Id;
}

webvtt_cue* new_cue() {
  webvtt_cue *cue = (webvtt_cue*)malloc(sizeof(*cue));
  if (cue == NULL) {
    FAIL("Couldn't allocate cue structure\n");
  }
  cue->cueID = NULL;
  cue->pauseOnExit = 0;
  cue->vertical = NULL; //horizontal
  cue->snapToLine = 1;
  cue->line = 0; //?
  cue->position = 50;
  cue->size = 100;
  cue->align = "middle";
  cue->text = NULL;
  cue->settings = NULL;
  return cue;
}

webvtt_cue *
  webvtt_parse(webvtt_parser *ctx)
{
  webvtt_cue *cue = NULL;
  webvtt_cue *head = NULL;
  webvtt_cue *current = NULL;
  char *p = ctx->buffer;
  while (ctx->offset < ctx->length) {
    switch (ctx->state) {
    case Initial:
      if (!has_file_identifier(ctx))
        return NULL;
      move_to_next_line(ctx);
      ctx->state = Header;
      break;
    case Header:
      if (move_to_next_line(ctx))
        ctx->state = Id;

      // this is for the first cue
      cue = new_cue();

      break;
    case Id:
      if (move_to_next_line(ctx))
        break;

      cue = new_cue();

      ctx->state = get_cue_id(ctx, cue);
      if (move_to_next_line(ctx)) {
        free(cue->settings);
        cue->settings = NULL;
#if DEBUG
        FAIL("Cue identifier cannot be standalone");
#else
        ctx->state = Id;
#endif
      }
      break;
    case TimingsAndSettings:
      ctx->state = get_timing_and_settings(ctx, cue);
      break;
    case CueText:
      ctx->state = get_cue_text(ctx, cue);
      cue->next = NULL;
      break;
    case NextCue:
      if (!head)
        current = head = cue;
      else if(head->next == NULL) {
        current = cue;
        head->next = current;
      } else {
        current->next = cue;
        current = cue;
      }
      ctx->state = Id;
      break;
    case BadCue:
      ctx->state = ignore_bad_cue(ctx);
      break;
    default:
      FAIL("Something is seriously wrong");
    }
  }
  if (current != cue) {
    current->next = cue;
    current = cue;
  }
  while (head != NULL) {
    webvtt_print_cue(stderr, head);
    head = head->next;
  }
  return cue;
}

webvtt_cue *
  webvtt_parse_buffer(webvtt_parser *ctx, char *buffer, long length)
{
  long bytes = MIN(length, BUFFER_SIZE - ctx->length);

  memcpy(ctx->buffer, buffer, bytes);
  ctx->length += bytes;

  return webvtt_parse(ctx);
}

webvtt_cue *
  webvtt_parse_file(webvtt_parser *ctx, FILE *in)
{
  ctx->length = fread(ctx->buffer, 1, BUFFER_SIZE, in);
  ctx->offset = 0;

  if (ctx->length >= BUFFER_SIZE)
    fprintf(stderr, "WARNING: truncating input at %d bytes."
    " This is a bug,\n", BUFFER_SIZE);

  return webvtt_parse(ctx);
}

webvtt_cue *
  webvtt_parse_filename(webvtt_parser *ctx, const char *filename)
{
  FILE *in = fopen(filename, "r");
  webvtt_cue *cue = NULL;

  if (in) {
    cue = webvtt_parse_file(ctx, in);
    fclose(in);
  }

  return cue;
}
