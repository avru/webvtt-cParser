typedef struct item item;
typedef struct ordered_list ordered_list;

typedef struct node node;
typedef struct voice_node voice_node;
voice_node* new_voice_node();
typedef struct lang_node lang_node;
lang_node* new_lang_node();
typedef struct text_node text_node;
text_node* new_text_node();
typedef struct time_node time_node;
time_node* new_time_node();

typedef struct token token;
typedef struct string_token string_token;
typedef struct start_token start_token;
typedef struct end_token end_token;
typedef struct timestamp_token timestamp_token;

typedef enum type type;
enum type {
  undefined,
  string,
  start_tag,
  end_tag,
  timestamp_tag,
  eof
};

typedef enum node_type node_type;
enum node_type {
  list_type,
  class_type,
  italic_type,
  bold_type,
  underline_type,
  ruby_type,
  ruby_text_type,
  voice_type,
  language_type,
  text_type,
  timestamp_type
};

typedef enum tag tag;
enum tag {
  unknown_tag,
  c_tag,
  i_tag,
  b_tag,
  u_tag,
  ruby_tag,
  rt_tag,
  v_tag,
  lang_tag
};
