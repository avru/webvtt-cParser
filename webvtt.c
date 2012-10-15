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

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define FAIL(msg) { \
  fprintf(stderr, "ERROR: " msg "\n"); \
  exit(-1); \
}

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
  err = fprintf(out, " --> %02d:%02d:%02d.%03d\n", h, m, s, ms);

  err = fprintf(out, "%s\n", cue->text);

  return err;
}

int hasRequiredFileIdentifier(webvtt_parser *ctx) {
  char *p = ctx->buffer;
  // Check for signature
  if (ctx->length < 6) {
    fprintf(stderr, "Too short. Not a webvtt file\n");
    return 0;
  }
  if (p[0] == (char)0xef && p[1] == (char)0xbb && p[2] == (char)0xbf) {
    fprintf(stderr, "Byte order mark\n");
    ctx->offset += 3;
    if (ctx->length < 9) {
      fprintf(stderr, "Too short. Not a webvtt file\n");
      return 0;
    }
  }
  if (memcmp(p + ctx->offset, "WEBVTT", 6)) {
    fprintf(stderr, "Bad magic. Not a webvtt file?\n");
    return 0;
  }
  ctx->offset += 6;
  fprintf(stderr, "Found signature\n");
  return 1;
}

int moveToNextLine(webvtt_parser *ctx) {
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

int getCueId(webvtt_parser *ctx, webvtt_cue *cue) {
  char *p = ctx->buffer;
  unsigned originalOffSet = ctx->offset;
  while (!moveToNextLine(ctx) && ctx->offset < ctx->length) {
    ctx->offset++;
    if (!memcmp(p + ctx->offset, "-->", 3)) {
      ctx->offset = originalOffSet;
      return TimingsAndSettings;
    }
  }
  ctx->offset = originalOffSet;
  cue->cueID = getLine(ctx);
  return TimingsAndSettings;
}

int getTimingAndSettings(webvtt_parser *ctx, webvtt_cue *cue) {
  char *p = ctx->buffer;
  int smin,ssec,smsec;
  int emin,esec,emsec;
  int items = sscanf(p + ctx->offset, "%d:%d.%d --> %d:%d.%d",
                        &smin, &ssec, &smsec, &emin, &esec, &emsec);
  if (items < 6) {
    FAIL("Couldn't parse cue timestamps\n");
  }
  double start_time = smin*60 + ssec + smsec * 1e-3;
  double end_time = emin*60 + esec + emsec * 1e-3;

  while (!moveToNextLine(ctx)) {
    ctx->offset++;
    if (isalpha(*(p + ctx->offset))) {
      cue->settings = getLine(ctx);
      break;
    }
  }
  cue->start = start_time * 1e3;
  cue->end = end_time * 1e3;
  return CueText;
}

char* getLine(webvtt_parser *ctx) {
  char *p = ctx->buffer + ctx->offset;
  while (ctx->offset < ctx->length && !moveToNextLine(ctx))
    ctx->offset++;
  char *e = ctx->buffer + ctx->offset;
  char *text = (char*)malloc(e - p);
  if (text == NULL) {
    fprintf(stderr, "Couldn't allocate cue text buffer\n");
    return NULL;
  }
  memcpy(text, p, e - p);
  text[e - p] = '\0';
  return text;
}

int getCueText(webvtt_parser *ctx, webvtt_cue *cue) {
  cue->text = getLine(ctx);
  return BadCue;
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
      if (!hasRequiredFileIdentifier(ctx))
        return NULL;
      moveToNextLine(ctx);
      ctx->state = Header;
      break;
    case Header:
      if (moveToNextLine(ctx))
        ctx->state = Id;
	  // this is for the first cue, only occurs once
      cue = (webvtt_cue*)malloc(sizeof(*cue));
      if (cue == NULL) {
        fprintf(stderr, "Couldn't allocate cue structure\n");
        return NULL;
      }
      cue->cueID = NULL;
      cue->settings = NULL;
      break;
    case Id:
      cue = (webvtt_cue*)malloc(sizeof(*cue));
      if (cue == NULL) {
        fprintf(stderr, "Couldn't allocate cue structure\n");
        return NULL;
      }
      cue->cueID = NULL;
      cue->settings = NULL;

      if (moveToNextLine(ctx))
        break;
      ctx->state = getCueId(ctx, cue);
      break;
    case TimingsAndSettings:
      ctx->state = getTimingAndSettings(ctx, cue);
      break;
    case CueText:
      ctx->state = getCueText(ctx, cue);
      cue->next = NULL;
      break;
    case BadCue:
      if (!head)
        current = head = cue;
      else if(head->next == NULL){
        current = cue;
        head->next = current;
      } else {
        current->next = cue;
        current = cue;
      }
      ctx->state = Id;
      break;
    }
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
