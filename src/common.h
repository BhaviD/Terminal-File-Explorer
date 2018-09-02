#ifndef _COMMON_H_
#define _COMMON_H_

#define pb push_back

#define FAILURE        -1
#define SUCCESS        0
#define ENTER          10
#define ESC            27
#define UP             65
#define DOWN           66
#define RIGHT          67
#define LEFT           68
#define BACKSPACE      127
#define COLON          58

#include <string>
#include <stack>

enum Mode
{
    MODE_NORMAL,
    MODE_COMMAND
};

char         next_input_char_get();
void         from_cursor_line_clear();
bool         is_directory(std::string str);
void         win_resize_handler(int sig);
std::string  abs_path_get(std::string str);
void         stack_clear(std::stack<std::string> &s);

#endif
