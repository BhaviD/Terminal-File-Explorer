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

#include <iostream>
#include <algorithm>
using namespace std;

#define FAILURE -1
#define SUCCESS 0

static struct winsize w;
int curr_i_beg, curr_i_end;
char curr_dir[512], working_dir[512];
struct dirent **entry_list;
int n, entry_idx, v_row;

void print_dir_range(char* working_dir, struct dirent **entry_list, int i_begin, int i_end, int cursor_r);

void t_win_resize_handler(int sig)
{
    ioctl(0, TIOCGWINSZ, &w);
    curr_i_beg = 0;
    curr_i_end = min(curr_i_beg + w.ws_row, n);
    entry_idx = 0;
    print_dir_range(working_dir, entry_list, curr_i_beg, curr_i_end, 1);
}

bool move_cursor_h(int r, int dr)
{
    bool ret = false;
    r += dr;
    if(r < 1)
    {
        v_row = 1;
        ret = true;
    }
    else if(r > w.ws_row)
    {
        v_row = w.ws_row;
        ret = true;
    }
    else
        v_row = r;

    cout << "\033[" << v_row << ";" << 1 << "H";
    return ret;
}

inline void clear_screen()
{
    cout << "\033[3J" << "\033[2J" << "\033[H\033[J";
    move_cursor_h(1, 0);
}

void print_dir_range(char* working_dir, struct dirent **entry_list, int i_begin, int i_end, int cursor_r)
{
    struct dirent *entry;
    struct stat entry_stat;          // to retrive the stats of the file/directory
    struct passwd *pUser;            // to determine the file/directory owner
    struct group *pGroup;            // to determine the file/directory group

    char buf[512], ret_time[26];

    clear_screen();

    //printf("Working Directory %s\n", working_dir);
    for(int i = i_begin; i < i_end; ++i)
    {
        // "dir/entry" defines the path to the entry
        entry = entry_list[i];
        sprintf(buf, "%s/%s", working_dir, entry->d_name);
        stat(buf, &entry_stat);            // retrieve information about the entry

        // [permissions] [owner] [group] [size in bytes] [time of last modification] [filename]

        // [permissions]
        // http://linux.die.net/man/2/chmod 
        printf( (entry_stat.st_mode & S_IRUSR) ? "r" : " -");
        printf( (entry_stat.st_mode & S_IWUSR) ? "w" : "-");
        printf( (entry_stat.st_mode & S_IXUSR) ? "x" : "-");
        printf( (entry_stat.st_mode & S_IRGRP) ? "r" : "-");
        printf( (entry_stat.st_mode & S_IWGRP) ? "w" : "-");
        printf( (entry_stat.st_mode & S_IXGRP) ? "x" : "-");
        printf( (entry_stat.st_mode & S_IROTH) ? "r" : "-");
        printf( (entry_stat.st_mode & S_IWOTH) ? "w" : "-");
        printf( (entry_stat.st_mode & S_IXOTH) ? "x" : "-");

        // [owner] 
        // http://linux.die.net/man/3/getpwuid
        pUser = getpwuid(entry_stat.st_uid);
        printf("  %s ", pUser->pw_name);

        // [group]
        // http://linux.die.net/man/3/getgrgid
        pGroup = getgrgid(entry_stat.st_gid);
        printf("  %s ", pGroup->gr_name);

        // [size in bytes] [time of last modification] [filename]
        printf("%-5ld",entry_stat.st_size);
        strcpy(ret_time, ctime(&entry_stat.st_mtime));
        ret_time[24] = '\0';
        printf(" %s", ret_time);
        if(i == i_end-1)
        {
            printf(" %-20s", entry->d_name);
            fflush(stdout);
        }
        else
            printf(" %-20s\n", entry->d_name);
    }
    move_cursor_h(cursor_r, 0);
}

int ls_dir(const char* dir)
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);

    //struct dirent **entry_list;

    //char curr_dir[512], working_dir[512];
    snprintf(working_dir, strlen(dir) + 1, "%s", dir);

    ioctl(0, TIOCGWINSZ, &w);

    while(1)
    {
        bool done = false;
        char ch;

        n = scandir(working_dir, &entry_list, NULL, alphasort);
        if(n == FAILURE)
        {
            cout << "Scandir() failed!!\n";
            return FAILURE;
        }

        entry_idx = 0;
        curr_i_beg = 0;
        curr_i_end = min(curr_i_beg + w.ws_row, n);
        print_dir_range(working_dir, entry_list, curr_i_beg, curr_i_end, 1);

        while(!done)
        {
            move_cursor_h(v_row, 0);
            ch = getchar();
            switch(ch)
            {
                case 27:            // if the first value is esc
                    getchar();      // skip the [
                    ch = getchar();
                    switch(ch) 
                    { 
                        case 'A':       // up
                            entry_idx = max(entry_idx-1, 0);
                            if(move_cursor_h(v_row, -1))
                            {
                                curr_i_beg = max(curr_i_beg - 1, 0);
                                curr_i_end = min(curr_i_beg + w.ws_row, n);
                                print_dir_range(working_dir, entry_list, curr_i_beg, curr_i_end, v_row);
                            }
                            break;
                        case 'B':       // down
                            entry_idx = min(entry_idx+1, n-1);
                            if(move_cursor_h(v_row, 1))
                            {
                                curr_i_beg = min(curr_i_beg + 1, n-1);
                                curr_i_end = min(curr_i_beg + w.ws_row, n);
                                print_dir_range(working_dir, entry_list, curr_i_beg, curr_i_end, v_row);
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

#if 0
        cout << "\033[3J";
        cout << "\033[2J";
        cout << "\033[H\033[J";
        cout << "\033[" << 0 << ";" << 0 << "H";
#endif

        snprintf(curr_dir, sizeof(curr_dir), "%s", working_dir);
        snprintf(working_dir, sizeof(working_dir), "%s/%s", curr_dir, entry_list[entry_idx]->d_name);

        for(int i = 0; i < n; ++i)
            free(entry_list[i]);
        free(entry_list);
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
