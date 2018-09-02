#ifndef _NORMAL_MODE_H_
#define _NORMAL_MODE_H_

#include <string>
#include <list>
#include <cstdio>
#include <utility>

struct dir_content
{
    int no_lines;
    std::string name;
    std::string content_line;

    dir_content(): no_lines(1) {}
};

void print_highlighted_line();
void cursor_init();
void ranked_content_line_print(std::list<dir_content>::const_iterator);
bool move_cursor_r(int, int);
void screen_clear();
std::string human_readable_size_get(off_t);
std::string content_line_get(std::string);
void content_list_create();
void print_mode();
std::pair<int, int> content_list_print(std::list<dir_content>::const_iterator);
void display_refresh();
void launch_file(std::string);
int enter_normal_mode();

#endif
