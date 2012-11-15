/* WebVTT parser
Copyright 2012 Mozilla Foundation

This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "cue_text_parser.h"
#define BUFFER_SIZE 4096

struct item {
  void *_value;
  struct item *_next;
};
item* new_item(void *in) {
  item *re = (item*)malloc(sizeof(item));
  re->_value = in;
  re->_next = NULL;
  return re;
}

struct ordered_list {
  item *start, *end;
};
ordered_list* new_ordered_list() {
  ordered_list *ol = (ordered_list*)malloc(sizeof(ordered_list));
  ol->start = ol->end = NULL;
  return ol;
}
void append_to_list(ordered_list *list, void *in) {
  item *temp = new_item(in);
  if (temp) {
    if (list->start == NULL) {
      list->start = list->end = temp;
    } else {
      list->end->_next = temp;
      list->end = temp;
    }
  }
}

struct node {
  node_type _type;
  void *_node;
  struct node* _next;
  struct node* _parent;
  ordered_list* _applicable_classes;
};
node* new_node(node_type type) {
  node *new_node = (node*)malloc(sizeof(node));
  new_node->_type = type;
  if (type == text_type)
    new_node->_node = new_text_node();
  else if (type == voice_type)
    new_node->_node = new_voice_node();
  else
    new_node->_node = NULL;
  new_node->_next = NULL;
  new_node->_parent = NULL;
  new_node->_applicable_classes = NULL;
  return new_node;
}

void append_node(node *target, node *in) {
  while (target->_next != NULL)
    target = target->_next;
  target->_next = in;
}

struct voice_node {
  char *_voice_name;
};
voice_node* new_voice_node() {
  voice_node *node = (voice_node*)malloc(sizeof(voice_node));
  node->_voice_name = NULL;
  return node;
}

struct lang_node {
  char *_lang_name;
};
lang_node* new_lang_node() {
  lang_node *node = (lang_node*)malloc(sizeof(lang_node));
  node->_lang_name = NULL;
  return node;
}

struct text_node {
  char *_text;
};
text_node* new_text_node() {
  text_node *node = (text_node*)malloc(sizeof(text_node));
  node->_text = NULL;
  return node;
}

// TODO, this is a dummy atm
struct time_node {
  char *_text;
};
time_node* new_time_node() {
  time_node *node = (time_node*)malloc(sizeof(time_node));
  node->_text = NULL;
  return node;
}

struct token {
  void *_obj;
  type _type;
};
token* new_token() {
  token *_token = (token*)malloc(sizeof(token));
  _token->_obj = NULL;
  _token->_type = undefined;
  return _token;
}

struct string_token {
  char *text;
};
token* new_string_token(char *text) {
  token *ntoken = new_token();
  string_token *_token = (string_token*)malloc(sizeof(string_token));
  _token->text = text;
  ntoken->_obj = _token;
  ntoken->_type = string;
  return ntoken;
}

tag what_tag(char *text) {
  if (memcmp("c" ,text, 2) == 0)
    return c_tag;
  if (memcmp("i" ,text, 2) == 0)
    return i_tag;
  if (memcmp("b" ,text, 2) == 0)
    return b_tag;
  if (memcmp("u" ,text, 2) == 0)
    return u_tag;
  if (memcmp("ruby" ,text, 5) == 0)
    return ruby_tag;
  if (memcmp("rt" ,text, 3) == 0)
    return rt_tag;
  if (memcmp("v" ,text, 2) == 0)
    return v_tag;
  if (memcmp("lang" ,text, 5) == 0)
    return lang_tag;
  else
    return unknown_tag;
}

struct start_token {
  tag _tag;
  char *tag_name;
  ordered_list *classes;
  char *annotation;
};
token* new_start_token(char *text, ordered_list *classes, char *annotation) {
  token *ntoken = new_token();
  start_token *_token = (start_token*)malloc(sizeof(start_token));
  _token->tag_name = text;
  _token->_tag = what_tag(text);

  if (classes != NULL)
    _token->classes = classes;
  else
    _token->classes = new_ordered_list();

  _token->annotation = annotation;
  ntoken->_obj = _token;
  ntoken->_type = start_tag;
  return ntoken;
}

struct end_token {
  tag _tag;
  char *tag_name;
};
token* new_end_token(char *text) {
  token *ntoken = new_token();
  end_token *_token = (end_token*)malloc(sizeof(end_token));
  _token->tag_name = text;
  _token->_tag = what_tag(text);
  ntoken->_obj = _token;
  ntoken->_type = end_tag;
  return ntoken;
}

struct timestamp_token {
  char *tag_name;
};
token* new_timestamp_token(char *text) {
  token *ntoken = new_token();
  timestamp_token *_token = (timestamp_token*)malloc(sizeof(timestamp_token));
  _token->tag_name = (char*)malloc(strlen(text)+1);
  sprintf(_token->tag_name, "%s", text);
  ntoken->_obj = _token;
  ntoken->_type = timestamp_tag;
  return ntoken;
}

void append_char(char **dest, char c) {
  char buffer[BUFFER_SIZE];
  sprintf(buffer, "%s%c", *dest, c);
  *dest = (char*)malloc(strlen(buffer)+1);
  sprintf(*dest, "%s", buffer);
}

void append_string(char **dest, char *source) {
  char buffer[BUFFER_SIZE];
  sprintf(buffer, "%s%s", *dest, source);
  *dest = (char*)malloc(strlen(buffer)+1);
  sprintf(*dest, "%s", buffer);
}

int is_empty(char *text) {
  return strcmp(text, "") == 0;
}

token* text_tokenizer(char *text, int *i) {
  enum tokenizer_states {
    data_state, escape_state, tag_state, start_tag_state,
    start_tag_class_state, start_tag_annotation_state, end_tag_state,
    timestamp_tag_state
  };

  enum tokenizer_states token_state = data_state;
  char *result = "";
  char *buffer = "";
  char *tb;
  int ti = 0;
  ordered_list *classes = new_ordered_list();
  char c = text[*i];

  while(1) {
    switch (token_state) {
    case data_state:
      switch (c) {
      case '&':
        append_char(&buffer,c);
        token_state = escape_state;
        break;
      case '<':
        if (is_empty(result)) {
          token_state = tag_state;
        } else {
          return new_string_token(result);
        }
        break;
      case '\0':
        return new_string_token(result);
        break;
      default:
        append_char(&result, c);
      } // end c switch
      break; // case data_state
    case escape_state:
      switch (c) {
      case '&':
        append_string(&result, buffer);
        buffer = "";
        append_char(&buffer, c);
        break;
      case ';':
        if (strcmp(buffer, "&amp") == 0) {
          append_char(&result, '&');
        }
        else if (strcmp(buffer, "&lt") == 0) {
          append_char(&result, '<');
        }
        else if (strcmp(buffer, "&gt") == 0) {
          append_char(&result, '>');
        }
        else if (strcmp(buffer, "&lrm") == 0) {
          append_char(&result, 'lr');
        }
        else if (strcmp(buffer, "&rlm") == 0) {
          append_char(&result, 'rl');
        }
        else if (strcmp(buffer, "&nbsp") == 0) {
          append_char(&result, ' ');
        }
        else {
          append_string(&result, buffer);
          append_char(&result, ';');
        }
        token_state = data_state;
        break; // case ;
      case '<':
      case '\0':
        append_string(&result, buffer);
        return new_string_token(result);
        break;
      default:
        if (isalnum(c)) {
          append_char(&buffer, c);
          break;
        }
        append_string(&result, buffer);
        append_char(&result, c);
        token_state = data_state;
      } // end c switch

      break; // case escape_state
    case tag_state:
      switch (c) {
      case '\t':
      case '\r':
      case '\n':
      case '\f':
      case ' ':
        token_state = start_tag_annotation_state;
        break;
      case '.':
        token_state = start_tag_class_state;
        break;
      case '/':
        token_state = end_tag_state;
        break;
      case '>':
        (*i)++;
      case '\0':
        return new_start_token("", NULL, NULL);
        break;
      default:
        result = "";
        append_char(&result, c);
        if (isdigit(c)) {
          token_state = timestamp_tag_state;
          break;
        }
        token_state = start_tag_state;
        break;
      } // end c switch

      break; // case tag_state
    case start_tag_state:
      switch (c) {
      case '\t':
      case '\f':
      case ' ':
        token_state = start_tag_annotation_state;
        break;
      case '\r':
      case '\n':
        buffer = "";
        append_char(&buffer, c);
        token_state = start_tag_annotation_state;
        break;
      case '.':
        token_state = start_tag_class_state;
        break;
      case '>':
        (*i)++;
      case '\0':
        return new_start_token(result, NULL, NULL);
        break;
      default:
        append_char(&result, c);
      } // end c switch
      break; // end starTag
    case start_tag_class_state:
      switch (c) {
      case '\t':
      case '\f':
      case ' ':
        // classes append buffer
        append_to_list(classes, buffer);
        buffer = "";
        token_state = start_tag_annotation_state;
        break;
      case '\r':
      case '\n':
        append_to_list(classes, buffer);
        buffer = "";
        append_char(&buffer, c);
        token_state = start_tag_annotation_state;
        break;
      case '.':
        append_to_list(classes, buffer);
        buffer = "";
        break;
      case '>':
        (*i)++;
      case '\0':
        append_to_list(classes, buffer);
        return new_start_token(result, classes, NULL);
        break;
      default:
        append_char(&buffer, c);
      } // end c switch
      break; // end start_tag_class_state
    case start_tag_annotation_state:
      switch (c) {
      case '>':
        (*i)++;
      case '\0':
        // trim leading space
        ti = 0;
        while (isspace(buffer[ti])) {
          ti++;
        }
        tb = buffer + ti;
        // trim trailing space
        ti = strlen(tb) - 1;
        if (ti <= 0) { // empty trailing space
          return new_start_token(result, NULL, NULL);
        }
        while (isspace(tb[ti])) {
          ti--;
        }
        tb[ti+1] = '\0';

        buffer = "";
        append_string(&buffer, tb);

        return new_start_token(result, classes, buffer);
        break;
      default:
        append_char(&buffer, c);
      } // end c switch
      break; // end start_tag_annotation_state
    case end_tag_state:
      switch (c) {
      case '>':
        (*i)++;
      case '\0':
        return new_end_token(result);
        break;
      default:
        append_char(&result, c);
      } // end c switch
      break; // end end_tag_state
    case timestamp_tag_state:
      switch (c) {
      case '>':
        (*i)++;
      case '\0':
        return new_timestamp_token(result);
        break;
      default:
        append_char(&result, c);
      } // end c switch
      break; // end timestamp_tag_state
    } // end token_state switch

    c = text[++(*i)];
  }
}

node* attach_to_node(node* current, node_type ntype, ordered_list *classes) {
  node* n_node = new_node(ntype);
  n_node->_applicable_classes = classes;
  n_node->_parent = current;
  append_node(current, n_node);
  current = n_node;

  return current;
}

// 3.3
node* parse_cue_text(char *text) {
  int position = 0;
  node *result = new_node(list_type);
  node *current = result;
  token *_token;
  node *n_node;
  void *temp_ptr;
  // 6
  while (1) {
    if (text[position] == '\0') {
      return result;
    }
    _token = text_tokenizer(text, &position);

    switch (_token->_type) {
    case string:
      temp_ptr = _token->_obj;
      n_node = new_node(text_type);
      ((text_node*)n_node->_node)->_text = ((string_token*)temp_ptr)->text;
      n_node->_parent = current;
      append_node(current, n_node);
      break;
    case start_tag:
      temp_ptr = _token->_obj;
      switch (((start_token*)temp_ptr)->_tag) {
      case c_tag:
        current = attach_to_node(current, class_type, ((start_token*)temp_ptr)->classes);
        break;
      case i_tag:
        current = attach_to_node(current, italic_type, ((start_token*)temp_ptr)->classes);
        break;
      case b_tag:
        current = attach_to_node(current, bold_type, ((start_token*)temp_ptr)->classes);
        break;
      case u_tag:
        current = attach_to_node(current, underline_type, ((start_token*)temp_ptr)->classes);
        break;
      case ruby_tag:
        current = attach_to_node(current, ruby_type, ((start_token*)temp_ptr)->classes);
        break;
      case rt_tag:
        current = attach_to_node(current, ruby_text_type, ((start_token*)temp_ptr)->classes);
        break;
      case v_tag:
        current = attach_to_node(current, voice_type, ((start_token*)temp_ptr)->classes);
        if (((start_token*)temp_ptr)->annotation)
          ((voice_node*)current->_node)->_voice_name = ((start_token*)temp_ptr)->annotation;
        else
          ((voice_node*)current->_node)->_voice_name = "";
        break;
      case lang_tag:
        // not coded
        break;
      }
      break; // end start_tag
    case end_tag:
      temp_ptr = _token->_obj;
      switch (((end_token*)temp_ptr)->_tag) {
      case c_tag:
        if (current->_type == class_type)
          current = current->_parent;
        break;
      case i_tag:
        if (current->_type == italic_type)
          current = current->_parent;
        break;
      case b_tag:
        if (current->_type == bold_type)
          current = current->_parent;
        break;
      case u_tag:
        if (current->_type == underline_type)
          current = current->_parent;
        break;
      case ruby_tag:
        if (current->_type == ruby_type)
          current = current->_parent;
        if (current->_type == ruby_text_type)
          current = current->_parent->_parent;
        break;
      case rt_tag:
        if (current->_type == ruby_text_type)
          current = current->_parent;
        break;
      case v_tag:
        if (current->_type == voice_type)
          current = current->_parent;
        break;
      case lang_tag:
        if (current->_type == language_type) {
          current = current->_parent;
          // TODO
        }
        break;
      }
      break; // end end_tag
    case timestamp_tag:
      // TODO
      break; // end timestamp_tag
    }
  }
  return 0;
}

void print_olist(ordered_list *list) {
  item *temp = list->start;
  if (!temp)
    printf("none\n");
  while (temp) {
    printf("%s\n", (char*)temp->_value);
    temp = temp->_next;
  }
}

node* print_node(node *current, node *parent, int depth) {
  if (current->_type == list_type) {
    printf("<ROOT>\n");
    current = current->_next;
  }

  while(current != NULL) {
    int i = 0;
    for (; i != depth; i++)
      printf(">", depth);

    switch (current->_type) {
    case text_type:
      printf("Text node: ");
      printf("%s\n", ((text_node*)current->_node)->_text);
      if (current->_next != NULL && current->_next->_parent != current->_parent)
        return current;
      break;
    case class_type:
      printf("Class node: \n");
      printf("_Applicable classes: ");
      print_olist(current->_applicable_classes);
      printf("_Children: \n");
      current = print_node(current->_next, current, depth+1);
      break;
    case italic_type:
      printf("Italic node: \n");
      current = print_node(current->_next, current, depth+1);
      break;
    case bold_type:
      printf("Bold node: \n");
      current = print_node(current->_next, current, depth+1);
      break;
    case underline_type:
      printf("Underline node: \n");
      current = print_node(current->_next, current, depth+1);
      break;
    case ruby_type:
      printf("Ruby node: \n");
      current = print_node(current->_next, current, depth+1);
      break;
    case ruby_text_type:
      printf("Ruby text node: \n");
      current = print_node(current->_next, current, depth+1);
      break;
    case voice_type:
      printf("Voice node: \n");
      printf("Speaker: %s\n", ((voice_node*)current->_node)->_voice_name);
      current = print_node(current->_next, current, depth+1);
      break;
    case language_type:
      printf("Language node: \n");
      current = print_node(current->_next, current, depth+1);
      break;
    }
    
    if (current == NULL)
      return 0;
    if (depth > 0 && current->_next != NULL && current->_next->_parent != parent)
      return current;

    current = current->_next;

  }
  printf("<END ROOT>\n");
  return 0;
}

int main() {
  char *temp = "BEGIN: <v testSpeaker>test</v><c.testClass>in<b>c, b <v> c,b,v</v></b> c</c> a test. <i>Italic<b>bold and italic here <u> b,i,u </u></b> continue italic text</i> ha";
  node *test = parse_cue_text(temp);
  printf("Input: %s\n\n", temp);
  print_node(test, NULL, 0);

  return 0;
}