#ifndef _NORMAL_MODE_H_
#define _NORMAL_MODE_H_

#include <string>

struct dir_content
{
    int no_lines;
    std::string name;
    std::string content_line;

    dir_content(): no_lines(1) {}
};

void display_refresh();
void cursor_init();

#endif
