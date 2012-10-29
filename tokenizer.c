/* WebVTT parser
Copyright 2011 Mozilla Foundation

This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#define BUFFER_SIZE 4096
#define DEBUG 1
#define DEFAULT 20

enum type {
  undefined,
  string,
  start_tag,
  end_tag,
  timestamp_tag,
  eof
};

enum tag {
  no_tag,
  c_tag,
  i_tag,
  b_tag,
  u_tag,
  ruby_tag,
  rt_tag,
  v_tag
};

typedef struct item item;
struct item {
  void *_item;
  struct item *_next;
};
item* new_item(void *in) {
  item *re = (item*)malloc(sizeof(item));
  re->_item = in;
  re->_next = NULL;
  return re;
}

typedef struct ordered_list ordered_list;
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

typedef struct internal_node internal_node;
struct internal_node {
  void *_obj;
  enum tag tag_name;
  ordered_list *children;
  ordered_list *applicable_classes;
};
internal_node* new_internal_node() {
  internal_node *node = (internal_node*)malloc(sizeof(internal_node));
  node->_obj = NULL;
  node->tag_name = no_tag;
  node->children = new_ordered_list();
  node->applicable_classes = new_ordered_list();
  return node;
}

typedef struct class_obj class_obj;
struct class_obj {
  internal_node *parent;
};
internal_node* new_class_obj() {
  internal_node *bnode = new_internal_node();
  class_obj *obj = (class_obj*)malloc(sizeof(class_obj));
  obj->parent = NULL;
  bnode->_obj = obj;
  bnode->tag_name = c_tag;
  return bnode;
}

typedef struct text_node text_node;
struct text_node {
  char *text;
};
text_node* new_text_node(char *text) {
  text_node *node = (text_node*)malloc(sizeof(text_node));
  node->text = text;
  return node;
}

typedef struct token token;
struct token {
  void *_obj;
  enum type _type;
};
token* new_token() {
  token *_token = (token*)malloc(sizeof(token));
  _token->_obj = NULL;
  _token->_type = undefined;
  return _token;
}

typedef struct string_token string_token;
struct string_token {
  char *text;
};
token* new_string_token(char *text) {
  token *btoken = new_token();
  string_token *_token = (string_token*)malloc(sizeof(string_token));
  _token->text = text;
  btoken->_obj = _token;
  btoken->_type = string;
  return btoken;
}

typedef struct start_token start_token;
struct start_token {
  char *tag_name;
  ordered_list *classes;
  char *annotation;
};
token* new_start_token(char *text, ordered_list *classes, char *annotation) {
  token *btoken = new_token();
  start_token *_token = (start_token*)malloc(sizeof(start_token));
  _token->tag_name = text;
  if (classes != NULL)
    _token->classes = classes;
  else
    _token->classes = new_ordered_list();
  _token->annotation = annotation;
  btoken->_obj = _token;
  btoken->_type = start_tag;
  return btoken;
}

typedef struct end_token end_token;
struct end_token {
  char *tag_name;
};
token* new_end_token(char *text) {
  token *btoken = new_token();
  end_token *_token = (end_token*)malloc(sizeof(end_token));
  _token->tag_name = (char*)malloc(strlen(text)+1);
  sprintf(_token->tag_name, "%s", text);
  btoken->_obj = _token;
  btoken->_type = end_tag;
  return btoken;
}

typedef struct timestamp_token timestamp_token;
struct timestamp_token {
  char *tag_name;
};
token* new_timestamp_token(char *text) {
  token *btoken = new_token();
  timestamp_token *_token = (timestamp_token*)malloc(sizeof(timestamp_token));
  _token->tag_name = (char*)malloc(strlen(text)+1);
  sprintf(_token->tag_name, "%s", text);
  btoken->_obj = _token;
  btoken->_type = timestamp_tag;
  return btoken;
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
    data, escape, tag, startTag, startTagClass, startTagAnnotation, endTag,
    timestampTag
  };

  enum tokenizer_states tok_state = data;
  char *result = "";
  char *buffer = "";
  char *tb;
  int ti = 0;
  ordered_list *classes = new_ordered_list();
  char c = text[*i];

  while(1) {
    switch (tok_state) {
    case data:
      switch (c) {
      case '&':
        append_char(&buffer,c);
        tok_state = escape;
        break;
      case '<':
        if (is_empty(result)) {
          tok_state = tag;
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
      break; // case data
    case escape:
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
        tok_state = data;
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
        tok_state = data;
      } // end c switch

      break; // case escape
    case tag:
      switch (c) {
      case '\t':
      case '\r':
      case '\n':
      case '\f':
      case ' ':
        tok_state = startTagAnnotation;
        break;
      case '.':
        tok_state = startTagClass;
        break;
      case '/':
        tok_state = endTag;
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
          tok_state = timestampTag;
          break;
        }
        tok_state = startTag;
        break;
      } // end c switch

      break; // case tag
    case startTag:
      switch (c) {
      case '\t':
      case '\f':
      case ' ':
        tok_state = startTagAnnotation;
        break;
      case '\r':
      case '\n':
        buffer = "";
        append_char(&buffer, c);
        tok_state = startTagAnnotation;
        break;
      case '.':
        tok_state = startTagClass;
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
    case startTagClass:
      switch (c) {
      case '\t':
      case '\f':
      case ' ':
        // classes append buffer
        append_to_list(classes, buffer);
        buffer = "";
        tok_state = startTagAnnotation;
        break;
      case '\r':
      case '\n':
        append_to_list(classes, buffer);
        buffer = "";
        append_char(&buffer, c);
        tok_state = startTagAnnotation;
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
      break; // end startTagClass
    case startTagAnnotation:
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
      break; // end startTagAnnotation
    case endTag:
      switch (c) {
      case '>':
        (*i)++;
      case '\0':
        return new_end_token(result);
        break;
      default:
        append_char(&result, c);
      } // end c switch
      break; // end endTag
    case timestampTag:
      switch (c) {
      case '>':
        (*i)++;
      case '\0':
        return new_timestamp_token(result);
        break;
      default:
        append_char(&result, c);
      } // end c switch
      break; // end timestampTag
    } // end tok_state switch

    c = text[++(*i)];
  }
}

// 3.3
ordered_list* parse_cue_text(char *text) {
  int position = 0;
  ordered_list *result = new_ordered_list();
  internal_node *current = new_internal_node(), *obj;
  append_to_list(result, current);
  token *_token;
  void *temp_ptr;
  // 5
  while (1) {
    if (text[position] == '\0') {
      return result;
    }
    _token = text_tokenizer(text, &position);

    switch (_token->_type) {
    case string:
      temp_ptr = _token->_obj;
      append_to_list(current->children, new_text_node(((string_token*)temp_ptr)->text));
      break;
    case start_tag:
      temp_ptr = _token->_obj;
      switch (((start_token*)temp_ptr)->tag_name[0]) {
      case 'c':
        obj = new_class_obj();
        item *temp_list = ((start_token*)temp_ptr)->classes->start;
        while (temp_list != NULL) {
          if (strcmp((char*)temp_list->_item, "") != 0) {
            append_to_list(obj->applicable_classes, temp_list->_item);
          }
          temp_list = temp_list->_next;
        }
        append_to_list(current->children, obj);
        ((class_obj*)obj->_obj)->parent = current;
        current = obj;
        break;
      }
      break; // end start_tag
    case end_tag:
      temp_ptr = _token->_obj;
      switch (((start_token*)temp_ptr)->tag_name[0]) {
      case 'c':
        current = ((class_obj*)current->_obj)->parent;
        break;
      }
      break;
    }
  }
  return 0;
}

int main() {
  ordered_list *test = parse_cue_text("<c.asd>this</c> is a test");
  ordered_list *ol = ((internal_node*)test->start->_item)->children;

  internal_node *it = (internal_node*)ol->start->_item;
  if (it->tag_name == c_tag) {
    ordered_list *il = it->children;
    char *class_name = (char*)it->applicable_classes->start->_item;
    printf("%s\n", class_name);
    printf("%s", ((text_node*)il->start->_item)->text);
  } else {
    item *leaf = ol->start;
    printf("%s", ((text_node*)leaf->_item)->text);
  }

  return 0;
}