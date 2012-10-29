/* WebVTT parser
Copyright 2011 Mozilla Foundation

This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#ifndef _WEBVTT_H_
#define _WEBVTT_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>

  /* webvtt files are a sequence of cues
  each cue has a start and end time for presentation
  and some text content (which my be marked up)
  there may be other attributes, but we ignore them
  we store these in a linked list */
  typedef struct webvtt_cue webvtt_cue;
  struct webvtt_cue {
    char *text;       /** text value of the cue */
    long start, end;  /** timestamps in milliseconds */
    webvtt_cue *next; /** pointer to the next cue */
    char *cueID;
    char *settings;
    int pauseOnExit;
    char *vertical;
    int snapToLine;
    long line;
    long position;
    long size;
    char *align;
  };


  /* context structure for our parser */
  typedef struct webvtt_parser webvtt_parser;

  /* allocate and initialize a parser context */
  webvtt_parser *webvtt_parse_new(void);

  /* shut down and release a parser context */
  void webvtt_parse_free(webvtt_parser *ctx);

  /* read a webvtt file stored in a buffer */
  struct webvtt_cue *
    webvtt_parse_buffer(webvtt_parser *ctx, char *buffer, long length);

  /* read a webvtt file from an open file */
  struct webvtt_cue *
    webvtt_parse_file(webvtt_parser *ctx, FILE *in);

  /* read a webvtt file from a named file */
  struct webvtt_cue *
    webvtt_parse_filename(webvtt_parser *ctx, const char *filename);

  static inline int isNewline(char c)
  {
    return c == '\n' || c == '\f' || c == '\r' || c == '\0';
  }
  static inline int isASpace(char c)
  {
    return c == ' ' || c == '\t';
  }
  static inline int is_a_number(char c)
  {
    return c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9';
  }

  enum ParseState { Initial, Header, Id, TimingsAndSettings, CueText, NextCue, BadCue };
  int get_timing_and_settings(webvtt_parser *ctx, webvtt_cue *cue);
  double collect_timestamp(webvtt_parser *ctx);
  void parse_settings(char *settings, webvtt_cue *cue);
  char* get_line(webvtt_parser *ctx);

#if defined(__cplusplus)
} /* close extern "C" */
#endif

#endif /* _WEBVTT_H_ */
