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
#include <fcntl.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <list>
#include <stack>
#include <iomanip>         // setprecision
#include <sstream>         // stringstream
#include <fstream>
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
#define COLON          58
#define ONE_K          (1024)
#define ONE_M          (1024*1024)
#define ONE_G          (1024*1024*1024)
#define COMMAND_LEN    256

#define l_itr(T) list<T>::iterator
#define l_citr(T) list<T>::const_iterator

static struct winsize w;
int n, cursor_r_pos = 1, cursor_c_pos = 1;
static string root_dir;
static string working_dir;

stack<string> bwd_stack;
stack<string> fwd_stack;

enum Mode
{
    MODE_NORMAL,
    MODE_COMMAND
};

struct dir_content
{
    int no_lines;
    string name;
    string content_line;

    dir_content(): no_lines(1) {}
};

static list<dir_content> content_list;
static l_citr(dir_content) start_itr;
static l_citr(dir_content) prev_selection_itr;
static l_citr(dir_content) selection_itr;

int content_list_print(list<dir_content>::const_iterator itr);
bool move_cursor_r(int r, int dr);
void cursor_init();
void refresh_dir_content();

void print_highlighted_line()
{
    cout << "\033[1;33;105m" << selection_itr->content_line << "\033[0m";
    cursor_init();
}

void t_win_resize_handler(int sig)
{
    ioctl(0, TIOCGWINSZ, &w);
    refresh_dir_content();
}

inline void cursor_init()
{
    cout << "\033[" << cursor_r_pos << ";" << cursor_c_pos << "H";
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
    cout << "\033[3J" << "\033[H\033[J";
    cout.flush();
    cursor_r_pos = cursor_c_pos = 1;
    cursor_init();
}

string human_readable_size_get(off_t size)
{
    if((size / ONE_K) == 0)
    {
        stringstream stream;
        stream << setw(7) << size << "K";
        return stream.str();
    }
    else if ((size / ONE_M) == 0)
    {
        double sz = (double) size / ONE_K;
        stringstream stream;
        stream << fixed << setprecision(1) << setw(7) << sz << "K";
        return stream.str();
    }
    else if ((size / ONE_G) == 0)
    {
        double sz = (double) size / ONE_M;
        stringstream stream;
        stream << fixed << setprecision(1) << setw(7) << sz << "M";
        return stream.str();
    }
    else
    {
        double sz = (double) size / ONE_G;
        stringstream stream;
        stream << fixed << setprecision(1) << setw(7) << sz << "G";
        return stream.str();
    }
}

void content_list_create(string &working_dir)
{
    struct dirent **dir_entry_arr;
    struct dirent *dir_entry;
    struct stat dir_entry_stat;          // to retrive the stats of the file/directory
    struct passwd *pUser;            // to determine the file/directory owner
    struct group *pGroup;            // to determine the file/directory group

    string last_modified_time, dir_entry_path;

    int n = scandir(working_dir.c_str(), &dir_entry_arr, NULL, alphasort);
    if(n == FAILURE)
    {
        cout << "Scandir() failed!!\n";
        return;
    }

    content_list.clear();
    for(int i = 0; i < n; ++i)
    {
        dir_entry = dir_entry_arr[i];

        if(((string)dir_entry->d_name) != "." && ((string)dir_entry->d_name) != ".." &&
           dir_entry->d_name[0] == '.')
        {
            continue;
        }

        dir_entry_path = working_dir + dir_entry->d_name;   // "working_dir/dir_entry" defines the path to the entry
        stat(dir_entry_path.c_str(), &dir_entry_stat);      // retrieve information about the entry

        stringstream ss;

        // [file-type] [permissions] [owner] [group] [size in bytes] [time of last modification] [filename]
        switch (dir_entry_stat.st_mode & S_IFMT) {
            case S_IFBLK:  ss << "b"; break;
            case S_IFCHR:  ss << "c"; break; 
            case S_IFDIR:  ss << "d"; break; // It's a (sub)directory 
            case S_IFIFO:  ss << "p"; break; // fifo
            case S_IFLNK:  ss << "l"; break; // Sym link
            case S_IFSOCK: ss << "s"; break;
            default:       ss << "-"; break; // Filetype isn't identified
        }
 
        // [permissions]
        // http://linux.die.net/man/2/chmod 
        ss << ((dir_entry_stat.st_mode & S_IRUSR) ? "r" : "-");
        ss << ((dir_entry_stat.st_mode & S_IWUSR) ? "w" : "-");
        ss << ((dir_entry_stat.st_mode & S_IXUSR) ? "x" : "-");
        ss << ((dir_entry_stat.st_mode & S_IRGRP) ? "r" : "-");
        ss << ((dir_entry_stat.st_mode & S_IWGRP) ? "w" : "-");
        ss << ((dir_entry_stat.st_mode & S_IXGRP) ? "x" : "-");
        ss << ((dir_entry_stat.st_mode & S_IROTH) ? "r" : "-");
        ss << ((dir_entry_stat.st_mode & S_IWOTH) ? "w" : "-");
        ss << ((dir_entry_stat.st_mode & S_IXOTH) ? "x" : "-");


        // [owner] 
        // http://linux.die.net/man/3/getpwuid
        pUser = getpwuid(dir_entry_stat.st_uid);
        ss << "  " << left << setw(12) << pUser->pw_name;

        // [group]
        // http://linux.die.net/man/3/getgrgid
        pGroup = getgrgid(dir_entry_stat.st_gid);
        ss << "  " << setw(12) << pGroup->gr_name;

        // [size in bytes] [time of last modification] [filename]
        ss << " " << human_readable_size_get(dir_entry_stat.st_size);
        
        last_modified_time = ctime(&dir_entry_stat.st_mtime);
        last_modified_time[last_modified_time.length() - 1] = '\0';
        ss << "  " << last_modified_time;
        ss << "  " << dir_entry->d_name;

        dir_content dc;
        dc.name = dir_entry->d_name;
        dc.content_line = ss.str();
        content_list.pb(dc);

        free(dir_entry_arr[i]);
        dir_entry_arr[i] = NULL;
    }
    free(dir_entry_arr);
    dir_entry_arr = NULL;
}

bool is_directory(string str)
{
    struct stat str_stat;          // to retrive the stats of the file/directory
    struct passwd *pUser;          // to determine the file/directory owner
    struct group *pGroup;          // to determine the file/directory group

    stat(str.c_str(), &str_stat);

    if((str_stat.st_mode & S_IFMT) == S_IFDIR)
        return true;
    else
        return false;
}

/* returns the number of character printed */
int print_mode(Mode m)
{
    cursor_r_pos = w.ws_row;
    cursor_init();
    stringstream ss;
    switch(m)
    {
        case MODE_NORMAL:
        default:
            ss << "[NORMAL MODE]";
            break;
        case MODE_COMMAND:
            ss << "[COMMAND MODE] $";
            break;
    }
    cout << "\033[1;33;40m" << ss.str() << "\033[0m" << " ";
    cout.flush();
    return ss.str().length() + 1;
}

int content_list_print(list<dir_content>::const_iterator itr)
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
    print_mode(MODE_NORMAL);
    return nRows_printed;
}

inline void stack_clear(stack<string> &s)
{
    while(!s.empty()) s.pop();
}

void refresh_dir_content()
{
    content_list_create(working_dir);

    start_itr = content_list.begin();
    selection_itr = start_itr;
    prev_selection_itr = content_list.end();

    content_list_print(start_itr);
    cursor_r_pos = TOP_OFFSET + 1;
    cursor_init();
    print_highlighted_line();
}


string full_path_get(string str)
{
    char *str_buf = new char[str.length() + 1];
    strncpy(str_buf, str.c_str(), str.length());
    str_buf[str.length()] = '\0';

    string ret_path = working_dir;
    char *p_str = strtok(str_buf, "/");
    while(p_str)
    {
        string tok(p_str);
        if(tok == ".")
        {
            p_str = strtok (NULL, "/");
        }
        else if(tok == "..")
        {
            if(ret_path != root_dir)
            {
                ret_path = ret_path.substr(0, ret_path.length() - 1);
                size_t fwd_slash_pos = ret_path.find_last_of("/");
                ret_path = ret_path.substr(0, fwd_slash_pos + 1);
            }
            p_str = strtok (NULL, "/");
        }
        else if (tok == "~")
        {
            ret_path = root_dir;
            p_str = strtok (NULL, "/");
        }
        else
        {
            p_str = strtok (NULL, "/");
            if(!p_str)
                ret_path += tok;
            else
                ret_path += tok + "/";
        }
    }

    return ret_path;
}

int copy_file_to_dir(string file, string dest)
{
    if(dest[dest.length() - 1] != '/')
        dest = dest + "/";

    size_t fwd_slash_pos = file.find_last_of("/");
    dest += file.substr(fwd_slash_pos + 1);

    cout << " " << file << " " << dest;

    ifstream in(file);
    ofstream out(dest);

    out << in.rdbuf();
}

void enter_command_mode()
{
    cursor_c_pos = print_mode(MODE_COMMAND) + 1;
    cursor_init();

#if 0
    FILE *input = fopen("/dev/tty", "r");
    if(!input) 
    {
        cout << "fopen() failed!!";
        return;
    }
#endif

    struct termios prev_attr, new_attr;
    tcgetattr(STDIN_FILENO, &prev_attr);
    //tcgetattr(fileno(input), &prev_attr);
    new_attr = prev_attr;
    new_attr.c_lflag |= ICANON;
    new_attr.c_lflag |= ECHO;
    new_attr.c_cc[VKILL] = ESC;
    new_attr.c_cc[VEOF] = ESC;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_attr);
    //tcsetattr(fileno(input), TCSANOW, &new_attr);

    while(1)
    {
        char cmd[COMMAND_LEN + 1];
        cin.getline(cmd, COMMAND_LEN);

        if(!strcmp(cmd, ""))
            break;

        vector<string> command;
        stringstream ss(cmd);
        string part;

        while(getline(ss, part, ' '))
        {
           command.push_back(part);
        }

        screen_clear();
        if(command[0] == "copy")
        {
            string src_path, dest_path;
            dest_path = full_path_get(command.back());
            for(int i = 1; i < command.size() - 1; ++i)
            {
                src_path = full_path_get(command[i]);
                if(is_directory(src_path))
                {
                    ;
                }
                else
                {
                    copy_file_to_dir(src_path, dest_path);
                }
            }
        }
        else if(command[0] == "move")
        {
        
        }
        else if(command[0] == "rename")
        {
            
        }
        else if(command[0] == "create_file")
        {
        
        }
        else if(command[0] == "create_dir")
        {
            
        }
        else if(command[0] == "delete_file")
        {
            
        }
        else if(command[0] == "delete_dir")
        {
        
        }
        else if(command[0] == "goto")
        {
        
        }
        else if(command[0] == "search")
        {
        
        }
        else if(command[0] == "snapshot")
        {
        
        }
        else
        {
            ;   /* NULL */
        }

        getchar();

    }

#if 0
    string cmd;
    char ch;
    while ((ch = cin.get()) != ESC) {
        cmd += ch;
    }
#endif
    tcsetattr( STDIN_FILENO, TCSANOW, &prev_attr);
}

int enter_normal_mode()
{
    struct termios prev_attr, new_attr;
    tcgetattr(STDIN_FILENO, &prev_attr);
    new_attr = prev_attr;
    new_attr.c_lflag &= ~ICANON;
    new_attr.c_lflag &= ~ECHO;
    tcsetattr( STDIN_FILENO, TCSANOW, &new_attr);

    ioctl(0, TIOCGWINSZ, &w);

    while(1)
    {
        refresh_dir_content();

        bool refresh_dir = false;
        while(!refresh_dir)
        {
            char ch;
            ch = cin.get();
            switch(ch)
            {
                case ESC:
                    cin.get();        // skip the [
                    ch = cin.get();
                    switch(ch) 
                    { 
                        case UP:
                            if(move_cursor_r(cursor_r_pos, -1))
                            {
                                if(start_itr != content_list.begin())
                                {
                                    --start_itr;
                                    content_list_print(start_itr);
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
                                cursor_r_pos = content_list_print(start_itr);
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
                            refresh_dir = true;
                            break;

                        case LEFT:
                            if(!bwd_stack.empty())
                            {
                                fwd_stack.push(working_dir);
                                working_dir = bwd_stack.top();
                                bwd_stack.pop();
                            }
                            refresh_dir = true;
                            break;

                        default:
                            break;
                    }
                    break;

                case ENTER:
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
                    refresh_dir = true;
                    break;

                /* HOME */
                case 'h':
                case 'H':
                    if(working_dir != root_dir)
                    {
                        stack_clear(fwd_stack);
                        bwd_stack.push(working_dir);
                        
                        working_dir = root_dir;
                    }
                    refresh_dir = true;
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
                    refresh_dir = true;
                    break;
                }

                case COLON:
                {
                    enter_command_mode();
                    refresh_dir = true;
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

    cin.get();
    root_dir = getenv("PWD");
    if(root_dir != "/")
        root_dir = root_dir + "/";
    working_dir = root_dir;

    return enter_normal_mode();
}
