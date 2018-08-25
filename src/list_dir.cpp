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

#include <iostream>
#include <algorithm>
#include <string>
#include <list>
using namespace std;

#define FAILURE -1
#define SUCCESS 0

static struct winsize w;
struct dirent **entry_list;
int n, cursor_r_pos;

struct dir_content
{
    int no_lines;
    string name;
    string content_line;

    dir_content(): no_lines(1) {}
};

int print_content_list(list<dir_content>::const_iterator itr);
bool move_cursor_r(int r, int dr);

static list<dir_content> content_list;
static list<dir_content>::const_iterator current_itr;

void t_win_resize_handler(int sig)
{
    ioctl(0, TIOCGWINSZ, &w);
    int cursor_pos_save = 0;
    if(cursor_r_pos < (w.ws_row/2))
        cursor_pos_save = cursor_r_pos;
    else
        advance(current_itr, cursor_r_pos - (w.ws_row/2) - 1);

    print_content_list(current_itr);

    if(cursor_pos_save)
        move_cursor_r(cursor_pos_save, 0);
    else
        move_cursor_r(w.ws_row/2, 0);
}

bool move_cursor_r(int r, int dr)
{
    bool ret = false;
    r += dr;
    if(r < 1)
    {
        cursor_r_pos = 1;
        ret = true;
    }
    else if (r > content_list.size())
    {
        cursor_r_pos = content_list.size();
    }
    else if(r > w.ws_row)
    {
        cursor_r_pos = w.ws_row;
        ret = true;
    }
    else
    {
        cursor_r_pos = r;
    }

    cout << "\033[" << cursor_r_pos << ";" << 1 << "H";
    cout.flush();
    return ret;
}

inline void clear_screen()
{
    cout << "\033[3J" << "\033[2J" << "\033[H\033[J";
    cout.flush();
    move_cursor_r(1, 0);
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

        // [permissions] [owner] [group] [size in bytes] [time of last modification] [filename]

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

int print_content_list(list<dir_content>::const_iterator itr)
{
    int nWin_rows = w.ws_row;
    clear_screen();

    int nRows_printed;
    for(nRows_printed = 0; nRows_printed < nWin_rows && itr != content_list.end(); ++nRows_printed, ++itr)
    {
        cout << itr->content_line;
        cout.flush();
        move_cursor_r(++cursor_r_pos, 0);
    }
    return nRows_printed;
}

int ls_dir(const char* dir)
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);

    string working_dir(dir);

    ioctl(0, TIOCGWINSZ, &w);

    while(1)
    {
        bool done = false;
        char ch;

        create_content_list(working_dir);
        current_itr = content_list.begin();
        print_content_list(current_itr);
        move_cursor_r(1, 0);

        while(!done)
        {
            ch = getchar();
            switch(ch)
            {
                case 27:            // if the first value is esc
                    getchar();      // skip the [
                    ch = getchar();
                    switch(ch) 
                    { 
                        case 'A':       // up
                            if(move_cursor_r(cursor_r_pos, -1))
                            {
                                if(current_itr != content_list.begin())
                                {
                                    --current_itr;
                                    print_content_list(current_itr);
                                    move_cursor_r(1, 0);
                                }
                            }
                            break;
                        case 'B':       // down
                            if(move_cursor_r(cursor_r_pos, 1))
                            {
                                ++current_itr;
                                cursor_r_pos = print_content_list(current_itr);
                                move_cursor_r(cursor_r_pos, 0);
                            }
                            break;
                        default:
                            break;
#if 0
                        case 'C':       // right
                            break;
                        case 'D':       // left
                            break;
#endif
                    }
                    break;

                case 10:
                    done = true;
                    break;

                default:
                    break;
            }
        }

        auto selection_itr = current_itr;
        advance(selection_itr, cursor_r_pos - 1);
        working_dir = working_dir + "/" + selection_itr->name;
    }

    return SUCCESS;
}

int main(int argc, char* argv[])
{
    signal (SIGWINCH, t_win_resize_handler);

    if(argc == 1)
        return ls_dir(".");

    return ls_dir(argv[1]);
}
