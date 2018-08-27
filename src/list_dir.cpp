#include <sys/stat.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "utility.h"
#include <sys/wait.h>
#include <cstddef>         // std::size_t

#include <iostream>
#include <algorithm>
#include <string>
#include <list>
#include <stack>
using namespace std;

#define FAILURE        -1
#define SUCCESS        0
#define FIRST_ROW_NUM  1
#define BOTTOM_OFFSET  1
#define TOP_OFFSET     1
#define ENTER          10
#define ESC            27
#define UP             65
#define DOWN           66
#define RIGHT          67
#define LEFT           68
#define BACKSPACE      127

#define l_itr(T) list<T>::iterator
#define l_citr(T) list<T>::const_iterator

static struct winsize w;
struct dirent **entry_list;
int n, cursor_r_pos = 1;
static string root_dir;
static string working_dir;

stack<string> bwd_stack;
stack<string> fwd_stack;

struct dir_content
{
    int no_lines;
    string name;
    string content_line;

    dir_content(): no_lines(1) {}
};

int print_content_list(list<dir_content>::const_iterator itr);
bool move_cursor_r(int r, int dr);
void cursor_init();

static list<dir_content> content_list;
static l_citr(dir_content) start_itr;
static l_citr(dir_content) prev_selection_itr;
static l_citr(dir_content) selection_itr;

void print_highlighted_line()
{
    cout << "\033[1;33;105m" << selection_itr->content_line << "\033[0m";
    cursor_init();
}

void t_win_resize_handler(int sig)
{
    ioctl(0, TIOCGWINSZ, &w);
    int cursor_pos_save = 0;
    if(cursor_r_pos < (w.ws_row/2))
        cursor_pos_save = cursor_r_pos;
    else
        advance(start_itr, cursor_r_pos - (w.ws_row/2) - 1);

    print_content_list(start_itr);

    if(cursor_pos_save)
        move_cursor_r(cursor_pos_save, 0);
    else
        move_cursor_r(w.ws_row/2, 0);
}

inline void cursor_init()
{
    cout << "\033[" << cursor_r_pos << ";" << 1 << "H";
    cout.flush();
}

bool move_cursor_r(int r, int dr)
{
    bool ret = false;
    if(dr == 0)
    {
        cursor_r_pos = r;
    }
    else if(dr < 0)
    {
        if(selection_itr == content_list.begin())
            return false;

        prev_selection_itr = selection_itr;
        --selection_itr;

        if(r + dr >= FIRST_ROW_NUM + TOP_OFFSET)
        {
            cursor_r_pos = r + dr;
        }
        else
        {
            cursor_r_pos = FIRST_ROW_NUM + TOP_OFFSET;
            ret = true;
        }
    }
    else if(dr > 0)
    {
        ++selection_itr;
        if(selection_itr == content_list.end())
        {
            --selection_itr;
            return false;
        }

        --selection_itr;
        prev_selection_itr = selection_itr;
        ++selection_itr;

        if(r + dr <= w.ws_row - BOTTOM_OFFSET)
        {
            cursor_r_pos = r + dr;
        }
        else
        {
            cursor_r_pos = w.ws_row - BOTTOM_OFFSET;
            ret = true;
        }
    }

    cout << prev_selection_itr->content_line;
    cursor_init();
    return ret;
}

inline void screen_clear()
{
    //cout << "\033[3J" << "\033[2J" << "\033[H\033[J";
    cout << "\033[3J" << "\033[H\033[J";
    cout.flush();
    cursor_r_pos = 1;
    cursor_init();
}

void create_content_list(string &working_dir)
{
    struct dirent **entry_list;
    struct dirent *entry;
    struct stat entry_stat;          // to retrive the stats of the file/directory
    struct passwd *pUser;            // to determine the file/directory owner
    struct group *pGroup;            // to determine the file/directory group

    char buf[512], ret_time[26];

    int n = scandir(working_dir.c_str(), &entry_list, NULL, alphasort);
    if(n == FAILURE)
    {
        cout << "Scandir() failed!!\n";
        return;
    }

    content_list.clear();
    for(int i = 0; i < n; ++i)
    {
        // "dir/entry" defines the path to the entry
        entry = entry_list[i];
        sprintf(buf, "%s/%s", working_dir.c_str(), entry->d_name);
        stat(buf, &entry_stat);            // retrieve information about the entry

        dir_content dc;

        // [file-type] [permissions] [owner] [group] [size in bytes] [time of last modification] [filename]

        switch (entry_stat.st_mode & S_IFMT) {
            case S_IFBLK:  dc.content_line += "b"; break;
            case S_IFCHR:  dc.content_line += "c"; break; 
            case S_IFDIR:  dc.content_line += "d"; break; // It's a (sub)directory 
            case S_IFIFO:  dc.content_line += "p"; break; // fifo
            case S_IFLNK:  dc.content_line += "l"; break; // Sym link
            case S_IFSOCK: dc.content_line += "s"; break;
                           // Filetype isn't identified
            default:       dc.content_line += "-"; break;
        }

        // [permissions]
        // http://linux.die.net/man/2/chmod 
        dc.content_line += (entry_stat.st_mode & S_IRUSR) ? "r" : "-";
        dc.content_line += (entry_stat.st_mode & S_IWUSR) ? "w" : "-";
        dc.content_line += (entry_stat.st_mode & S_IXUSR) ? "x" : "-";
        dc.content_line += (entry_stat.st_mode & S_IRGRP) ? "r" : "-";
        dc.content_line += (entry_stat.st_mode & S_IWGRP) ? "w" : "-";
        dc.content_line += (entry_stat.st_mode & S_IXGRP) ? "x" : "-";
        dc.content_line += (entry_stat.st_mode & S_IROTH) ? "r" : "-";
        dc.content_line += (entry_stat.st_mode & S_IWOTH) ? "w" : "-";
        dc.content_line += (entry_stat.st_mode & S_IXOTH) ? "x" : "-";

        // [owner] 
        // http://linux.die.net/man/3/getpwuid
        pUser = getpwuid(entry_stat.st_uid);
        dc.content_line = dc.content_line + "  " + pUser->pw_name + " ";

        // [group]
        // http://linux.die.net/man/3/getgrgid
        pGroup = getgrgid(entry_stat.st_gid);
        dc.content_line = dc.content_line + "  " + pGroup->gr_name + " ";

        // [size in bytes] [time of last modification] [filename]
        dc.content_line = dc.content_line + to_string(entry_stat.st_size) + " ";
        strcpy(ret_time, ctime(&entry_stat.st_mtime));
        ret_time[24] = '\0';
        dc.content_line = dc.content_line + ret_time + " ";
        dc.content_line = dc.content_line + entry->d_name;

        dc.name = entry->d_name;
        content_list.pb(dc);
        free(entry_list[i]);
        entry_list[i] = NULL;
    }
    free(entry_list);
    entry_list = NULL;
}

void print_mode()
{
    cursor_r_pos = w.ws_row;
    cursor_init();
    cout << "\033[1;33;40m" << "-- NORMAL MODE --" << "\033[0m";
    cout.flush();
}

int print_content_list(list<dir_content>::const_iterator itr)
{
    int nWin_rows = w.ws_row;
    screen_clear();

    int nRows_printed;
    if(working_dir == root_dir)
        cout << "\033[1;33;40m" << "PWD: ~/" << "\033[0m";
    else
        cout << "\033[1;33;40m" << "PWD: ~/" << working_dir.substr(root_dir.length()) << "\033[0m";
    ++cursor_r_pos;
    cursor_init();
    for(nRows_printed = TOP_OFFSET; nRows_printed < nWin_rows - BOTTOM_OFFSET && itr != content_list.end(); ++nRows_printed, ++itr)
    {
        cout << itr->content_line;
        ++cursor_r_pos;
        cursor_init();
    }
    print_mode();
    return nRows_printed;
}

inline void stack_clear(stack<string> &s)
{
    while(!s.empty()) s.pop();
}

int run()
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);

    ioctl(0, TIOCGWINSZ, &w);

    while(1)
    {
        bool done = false;
        char ch;

        create_content_list(working_dir);

        start_itr = content_list.begin();
        selection_itr = start_itr;
        prev_selection_itr = content_list.end();

        print_content_list(start_itr);
        cursor_r_pos = TOP_OFFSET + 1;
        cursor_init();
        print_highlighted_line();

        while(!done)
        {
            ch = getchar();
            switch(ch)
            {
                case ESC:
                    getchar();      // skip the [
                    ch = getchar();
                    switch(ch) 
                    { 
                        case UP:
                            if(move_cursor_r(cursor_r_pos, -1))
                            {
                                if(start_itr != content_list.begin())
                                {
                                    --start_itr;
                                    print_content_list(start_itr);
                                    cursor_r_pos = FIRST_ROW_NUM + TOP_OFFSET;
                                    cursor_init();
                                }
                            }
                            print_highlighted_line();
                            break;

                        case DOWN:
                            if(move_cursor_r(cursor_r_pos, 1))
                            {
                                ++start_itr;
                                cursor_r_pos = print_content_list(start_itr);
                                cursor_init();
                            }
                            print_highlighted_line();
                            break;

                        case RIGHT:
                            if(!fwd_stack.empty())
                            {
                                bwd_stack.push(working_dir);
                                working_dir = fwd_stack.top();
                                fwd_stack.pop();
                            }
                            done = true;
                            break;

                        case LEFT:
                            if(!bwd_stack.empty())
                            {
                                fwd_stack.push(working_dir);
                                working_dir = bwd_stack.top();
                                bwd_stack.pop();
                            }
                            done = true;
                            break;

                        default:
                            break;
                    }
                    break;

                case ENTER:                // <enter>
                    if(selection_itr->name == ".")
                        continue;

                    if(selection_itr->name == "..")
                    {
                        if(working_dir != root_dir)
                        {
                            stack_clear(fwd_stack);
                            bwd_stack.push(working_dir);

                            working_dir = working_dir.substr(0, working_dir.length() - 1);
                            size_t fwd_slash_pos = working_dir.find_last_of("/");
                            working_dir = working_dir.substr(0, fwd_slash_pos + 1);
                        }
                    }
                    else
                    {
                        stack_clear(fwd_stack);
                        bwd_stack.push(working_dir);

                        working_dir = working_dir + selection_itr->name + "/";
                    }
                    done = true;
                    break;

                case 'h':
                case 'H':
                    if(working_dir != root_dir)
                    {
                        stack_clear(fwd_stack);
                        bwd_stack.push(working_dir);
                        
                        working_dir = root_dir;
                    }
                    done = true;
                    break;

                case BACKSPACE:
                {
                    if(working_dir != root_dir)
                    {
                        stack_clear(fwd_stack);
                        bwd_stack.push(working_dir);

                        working_dir = working_dir.substr(0, working_dir.length() - 1);
                        size_t fwd_slash_pos = working_dir.find_last_of("/");
                        working_dir = working_dir.substr(0, fwd_slash_pos + 1);
                    }
                    done = true;
                    break;
                }

                default:
                    break;
            }
        }
    }
    return SUCCESS;
}

int main(int argc, char* argv[])
{
    signal (SIGWINCH, t_win_resize_handler);

    getchar();
    root_dir = getenv("PWD");
    if(root_dir != "/")
        root_dir = root_dir + "/";
    working_dir = root_dir;

    return run();
}
