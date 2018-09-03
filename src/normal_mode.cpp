#include <sys/ioctl.h>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstddef>         // std::size_t
#include <fcntl.h>
#include "command_mode.h"
#include "normal_mode.h"
#include "common.h"
#include "includes.h"

#include <iomanip>         // setprecision

using namespace std;

#define BOTTOM_OFFSET  1
#define ONE_K          (1024)
#define ONE_M          (1024*1024)
#define ONE_G          (1024*1024*1024)
#define CHILD          0

#define l_citr(T) list<T>::const_iterator

extern struct winsize w;

int cursor_r_pos;
int cursor_c_pos;
int cursor_left_limit;
int cursor_right_limit;
string root_dir;
string working_dir;
struct termios prev_attr, new_attr;

stack<string> bwd_stack;
stack<string> fwd_stack;

list<dir_content> content_list;
static l_citr(dir_content) start_itr;
static l_citr(dir_content) prev_selection_itr;
static l_citr(dir_content) selection_itr;
int bottom_limit, top_limit;
bool is_search_content;

Mode current_mode;

void print_highlighted_line()
{
    int saved_cursor_r_pos = cursor_r_pos;
    cout << "\033[1;33;105m";
    ranked_content_line_print(selection_itr);
    cout << "\033[0m";
    cursor_r_pos = saved_cursor_r_pos;
    cursor_init();
}

void cursor_init()
{
    cout << "\033[" << cursor_r_pos << ";" << cursor_c_pos << "H";
    cout.flush();
}

/* prints the directory contents by wrapping the line according to 
 * window width
 */
void ranked_content_line_print(list<dir_content>::const_iterator itr)
{
    int i;
    for(i = 0; i < itr->no_lines - 1; ++i)
    {
        cout << itr->content_line.substr((i*w.ws_col), (i+1)*w.ws_col);
        ++cursor_r_pos;
        cursor_init();
    }
    cout << itr->content_line.substr(i*w.ws_col);
    ++cursor_r_pos;
    cursor_init();
}

/* moves the cursor up or down
 * returns true if scrolling is to be done
 * other it returns false.
 */
bool move_cursor_r(int r, int dr)
{
    bool ret = false;
    if(dr == 0)
    {
        cursor_r_pos = r;
    }
    else if(dr < 0)     // move up
    {
        if(selection_itr == content_list.begin())
            return false;

        prev_selection_itr = selection_itr;
        --selection_itr;

        if(r + dr >= top_limit)
        {
            ranked_content_line_print(prev_selection_itr);
            cursor_r_pos = r - selection_itr->no_lines;
            cursor_init();
        }
        else
        {
            --start_itr;
            ret = true;
        }
    }
    else if(dr > 0)     // move down
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

        if(r + prev_selection_itr->no_lines <= bottom_limit)
        {
            ranked_content_line_print(prev_selection_itr);
            cursor_r_pos = r + prev_selection_itr->no_lines;
            cursor_init();
        }
        else
        {
            /* decides the entries to be skipped from the top if scroll down is done
             *  and the next entry is spanned over multiple lines
             */
            for(int sum = 0; sum < selection_itr->no_lines; ++start_itr)
            {
                sum += start_itr->no_lines;
            }
            ret = true;
        }
    }
    return ret;
}


void screen_clear()
{
    cout << "\033[3J" << "\033[H\033[J";
    cout.flush();
    cursor_r_pos = cursor_c_pos = 1;
    cursor_init();
}

/* returns the size in human readable form i.e. K, M, G */
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

/* return all the information of a file/directory as a string */
string content_line_get(string abs_path)
{
    struct stat dir_entry_stat;          // to retrive the stats of the file/directory
    struct passwd *pUser;            // to determine the file/directory owner
    struct group *pGroup;            // to determine the file/directory group

    string last_modified_time;

    stat(abs_path.c_str(), &dir_entry_stat);      // retrieve information about the entry

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

    size_t fwd_slash_pos = abs_path.find_last_of("/");
    ss << "  " << abs_path.substr(fwd_slash_pos + 1);

    return ss.str();
}

/* creates the information list of all sub-directories and files in a directory */
void content_list_create()
{
    struct dirent **dir_entry_arr;
    struct dirent *dir_entry;

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

        dir_content dc;
        dc.name = dir_entry->d_name;
        dc.content_line = content_line_get(working_dir + dir_entry->d_name);
        if(dc.content_line.length() % w.ws_col)
        {
            dc.no_lines = (dc.content_line.length() / w.ws_col) + 1;
        }
        else
        {
            dc.no_lines = (dc.content_line.length() / w.ws_col);
        }
        content_list.pb(dc);

        free(dir_entry_arr[i]);
        dir_entry_arr[i] = NULL;
    }
    free(dir_entry_arr);
    dir_entry_arr = NULL;
}

/* prints the current mode in the status bar
 * returns the number of characters printed
 */
void print_mode()
{
    cursor_r_pos = w.ws_row;
    cursor_c_pos = 1;
    cursor_init();
    from_cursor_line_clear();

    stringstream ss;
    switch(current_mode)
    {
        case MODE_NORMAL:
        default:
            ss << "[NORMAL MODE]";
            cout << "\033[1;33;40m" << ss.str() << "\033[0m" << " ";

#if 0
            if(is_status_pending)
            {
                cout << "\033[1;31m" << status_str << "\033[0m";
            }
#endif
            cout.flush();
            break;

        case MODE_COMMAND:
            ss << "[COMMAND MODE] :";
            cout << "\033[1;33;40m" << ss.str() << "\033[0m" << " ";
            cout.flush();
            break;
    }
    if(current_mode == MODE_COMMAND)
    {
        cursor_c_pos = ss.str().length() + 2;       // two spaces
        cursor_init();
        cursor_left_limit = cursor_right_limit = cursor_c_pos;
    }
}

/* prints the list of information of a directory */
pair<int, int> content_list_print(list<dir_content>::const_iterator itr)
{
    string pwd_str;
    int nWin_rows = w.ws_row;
    int pwd_rank, num_extra_entries = 0;
    bool selected_line_printed = false;
    screen_clear();

    int nRows_printed;
    stringstream ss;
    if(working_dir == root_dir)
        ss << "PWD: ~/";
    else
        ss << "PWD: ~/" << working_dir.substr(root_dir.length());

    if(ss.str().length() % w.ws_col)
        pwd_rank = (ss.str().length() / w.ws_col) + 1;
    else
        pwd_rank = (ss.str().length() / w.ws_col);

    int i = 0;
    for(; i < pwd_rank - 1; ++i)
    {
        cout << "\033[1;33;40m" << ss.str().substr((i*w.ws_col), (i+1)*w.ws_col) << "\033[0m";
        ++cursor_r_pos;
        cursor_init();
    }
    cout << "\033[1;33;40m" << ss.str().substr(i*w.ws_col) << "\033[0m";
    ++cursor_r_pos;
    cursor_init();
    top_limit = cursor_r_pos;

    for(nRows_printed = cursor_r_pos-1; itr != content_list.end(); ++itr)
    {
        if(itr->no_lines > nWin_rows - nRows_printed - BOTTOM_OFFSET)
            break;

        if(selected_line_printed)
            ++num_extra_entries;

        if(itr == selection_itr)
            selected_line_printed = true;

        for(i = 0; i < itr->no_lines - 1; ++i)
        {
            cout << itr->content_line.substr((i*w.ws_col), (i+1)*w.ws_col);
            ++cursor_r_pos;
            cursor_init();
        }
        cout << itr->content_line.substr(i*w.ws_col);
        ++cursor_r_pos;
        cursor_init();
        nRows_printed = cursor_r_pos - 1;
    }
    print_mode();
    return make_pair(nRows_printed, num_extra_entries+1);
}

/* clears screen and prints the entire list again */
void display_refresh()
{
    if(!is_search_content)
        content_list_create();

    start_itr = content_list.begin();
    selection_itr = start_itr;
    prev_selection_itr = content_list.end();

    auto p = content_list_print(start_itr);
    bottom_limit = p.first;
    if(current_mode == MODE_NORMAL)
    {
        cursor_r_pos = top_limit;
        cursor_init();
        print_highlighted_line();
    }
}

/* launches a file by forking a child process and using xdg-open */
void launch_file(string file_path)
{
    pid_t pid = fork();
    if(FAILURE == pid)
    {
        return;
    }
    else if (CHILD == pid)
    {
        char c_file_path[file_path.length() + 1];
        snprintf(c_file_path, sizeof(c_file_path), "%s", file_path.c_str());
        execl("/usr/bin/xdg-open", "xdg-open", c_file_path, NULL);
    }
}

int enter_normal_mode()
{
    bool explorer_exit = false;
    current_mode = MODE_NORMAL;

    ioctl(0, TIOCGWINSZ, &w);

    while(!explorer_exit)
    {
        display_refresh();

        bool refresh_dir = false;
        char ch;
        while(!refresh_dir)
        {
            ch = next_input_char_get();
            #if 0
            if(is_status_pending)
            {
                is_status_pending = false;
                print_mode();
                cursor_r_pos = top_limit;
                cursor_init();
            }
            #endif
            switch(ch)
            {
                case ESC:
                {
                    bool done = false;
                    while(!done)
                    {
                        screen_clear();
                        cout << "\033[1;31m" << "Exit File Explorer? [y/n]: " << "\033[0m";
                        ch = next_input_char_get();
                        switch(ch)
                        {
                            case 'y':
                            case 'Y':
                                done = refresh_dir = explorer_exit = true;
                                break;

                            case 'n':
                            case 'N':
                                done = refresh_dir = true;
                                break;

                            default:
                                break;
                        }
                    }
                    break;
                }

                case UP:
                    if(move_cursor_r(cursor_r_pos, -1))
                    {
                        content_list_print(start_itr);
                        cursor_r_pos = top_limit;
                        cursor_init();
                    }
                    print_highlighted_line();
                    break;

                case DOWN:
                    if(move_cursor_r(cursor_r_pos, 1))
                    {
                        auto p = content_list_print(start_itr);
                        bottom_limit = p.first;
                        cursor_r_pos = bottom_limit + 1;
                        auto itr = selection_itr;
                        for(int i = 0; i < p.second; ++i, ++itr)
                        {
                            cursor_r_pos -= itr->no_lines;
                        }
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
                    if(is_search_content)
                        is_search_content = false;

                    refresh_dir = true;
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
                        refresh_dir = true;
                    }
                    else
                    {
                        string selected_str;

                        if(is_search_content)
                        {
                            size_t fwd_slash_pos = (selection_itr->content_line).find_first_of("/");
                            selected_str = root_dir + (selection_itr->content_line).substr(fwd_slash_pos + 1);

                            if(is_directory(selected_str))
                            {
                                stack_clear(fwd_stack);
                                bwd_stack.push(working_dir);
                                working_dir = selected_str + "/";
                                is_search_content = false;
                                refresh_dir = true;
                            }
                            else
                            {
                                launch_file(selected_str);
                            }
                        }
                        else
                        {
                            selected_str = working_dir + selection_itr->name;
                            if(is_directory(selected_str))
                            {
                                stack_clear(fwd_stack);
                                bwd_stack.push(working_dir);
                                working_dir = selected_str + "/";
                                refresh_dir = true;
                            }
                            else
                            {
                                launch_file(selected_str);
                            }
                        }
                    }
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
    screen_clear();
    return SUCCESS;
}

int main(int argc, char* argv[])
{
    //cin.get();
    signal (SIGWINCH, win_resize_handler);

    tcgetattr(STDIN_FILENO, &prev_attr);
    new_attr = prev_attr;
    new_attr.c_lflag &= ~ICANON;
    new_attr.c_lflag &= ~ECHO;
    tcsetattr( STDIN_FILENO, TCSANOW, &new_attr);

    root_dir = getenv("PWD");
    if(root_dir != "/")
        root_dir = root_dir + "/";
    working_dir = root_dir;

    enter_normal_mode();

    tcsetattr( STDIN_FILENO, TCSANOW, &prev_attr);

}
