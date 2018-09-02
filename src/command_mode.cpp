#include "normal_mode.h"
#include "command_mode.h"
#include "common.h"
#include "includes.h"

using namespace std;

extern int                cursor_c_pos;
extern int                cursor_left_limit;
extern int                cursor_right_limit;
extern int                current_mode;
extern struct winsize     w;
extern list<dir_content>  content_list;
extern stack<string>      fwd_stack;
extern string             working_dir;
extern string             root_dir;
extern bool               is_search_content;

static string search_str;
static string snapshot_folder_path;
static string dumpfile_path;

constexpr int ftw_max_fd = 100;

void enter_command_mode()
{
    bool command_mode_exit = false;
    current_mode = MODE_COMMAND;

    while(1)
    {
        display_refresh();

        char ch;
        string cmd;
        bool enter_pressed = false;
        while(!enter_pressed && !command_mode_exit)
        {
            ch = next_input_char_get();
            switch(ch)
            {
                case ESC:
                    command_mode_exit = true;
                    break;

                case ENTER:
                    enter_pressed = true;
                    break;

                case BACKSPACE:
                    if(cmd.length())
                    {
                        --cursor_c_pos;
                        --cursor_right_limit;
                        cursor_init();
                        from_cursor_line_clear();
                        cmd.erase(cursor_c_pos - cursor_left_limit, 1);
                        cout << cmd.substr(cursor_c_pos - cursor_left_limit);
                        cout.flush();
                        cursor_init();
                    }
                    break;

                case UP:
                case DOWN:
                    break;

                case LEFT:
                    if(cursor_c_pos != cursor_left_limit)
                    {
                        --cursor_c_pos;
                        cursor_init();
                    }
                    break;

                case RIGHT:
                    if(cursor_c_pos != cursor_right_limit)
                    {
                        ++cursor_c_pos;
                        cursor_init();
                    }
                    break;

                default:
                    cmd.insert(cursor_c_pos - cursor_left_limit, 1, ch);
                    cout << cmd.substr(cursor_c_pos - cursor_left_limit);
                    cout.flush();
                    ++cursor_c_pos;
                    cursor_init();
                    ++cursor_right_limit;
                    break;
            }
        }
        if(command_mode_exit)
            break;

        if(cmd.empty())
            continue;

        string part;
        vector<string> command;

        for(unsigned int i = 0; i < cmd.length(); ++i)
        {
            if(cmd[i] == ' ')
            {
                if(!part.empty())
                {
                    command.pb(part);
                    part = "";
                }
            }
            else if(cmd[i] == '\\' && (i < cmd.length() - 1) && cmd[i+1] == ' ')
            {
                part += ' ';
                ++i;
            }
            else
            {
                part += cmd[i];
            }
        }
        if(!part.empty())
            command.pb(part);

        if(command[0] == "copy")
        {
            copy_command(command);
        }
        else if(command[0] == "move")
        {
            move_command(command);
        }
        else if(command[0] == "rename")
        {
            string old_path = abs_path_get(command[1]);
            string new_path = abs_path_get(command[2]);
            if(FAILURE == rename(old_path.c_str(), new_path.c_str()))
            {
                cout << "rename failed!! errno: " << errno;
            }
            display_refresh();
        }
        else if(command[0] == "create_file")
        {
            string dest_path = abs_path_get(command[2]);
            if(dest_path[dest_path.length() - 1] != '/')
                dest_path = dest_path + "/";

            dest_path += command[1];
            int fd = open(dest_path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
            if(FAILURE == fd)
                cout << "open failed!! errno: " << errno;
            else
                close(fd);
            display_refresh();
        }
        else if(command[0] == "create_dir")
        {
            string dest_path = abs_path_get(command[2]);
            if(dest_path[dest_path.length() - 1] != '/')
                dest_path = dest_path + "/";

            dest_path += command[1];
            mkdir(dest_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            display_refresh();
        }
        else if(command[0] == "delete_file")
        {
            string rem_path = abs_path_get(command[1]);
            if(FAILURE == unlinkat(0, rem_path.c_str(), 0))
            {
                cout << "unlinkat failed!! errno: " << errno;
            }
            display_refresh();
        }
        else if(command[0] == "delete_dir")
        {
            string rem_path = abs_path_get(command[1]);
            delete_command(rem_path);
        }
        else if(command[0] == "goto")
        {
            working_dir = abs_path_get(command[1]);
            if(working_dir[working_dir.length() - 1] != '/')
                working_dir = working_dir + "/";
            display_refresh();
        }
        else if(command[0] == "search")
        {
            search_str = command[1];
            content_list.clear();
            nftw(working_dir.c_str(), search_cb, ftw_max_fd, 0);
            is_search_content = true;
            stack_clear(fwd_stack);
            break;
        }
        else if(command[0] == "snapshot")
        {
            snapshot_folder_path = abs_path_get(command[1]);
            dumpfile_path = abs_path_get(command[2]);
            ofstream dumpfile (dumpfile_path.c_str(), ios::out | ios::trunc);
            dumpfile.close();
            if(snapshot_folder_path[snapshot_folder_path.length() - 1] == '/')
                snapshot_folder_path.erase(snapshot_folder_path.length() - 1);

            nftw(snapshot_folder_path.c_str(), snapshot_cb, ftw_max_fd, 0);
        }
        else
        {
            ;   /* NULL */
        }
    }
    current_mode = MODE_NORMAL;
}


int copy_file_to_dir(string src_file_path, string dest_dir_path)
{
    if(dest_dir_path[dest_dir_path.length() - 1] != '/')
        dest_dir_path = dest_dir_path + "/";

    struct stat src_file_stat;
    size_t fwd_slash_pos = src_file_path.find_last_of("/");
    string dest_file_path = dest_dir_path;
    dest_file_path += src_file_path.substr(fwd_slash_pos + 1);

    ifstream in(src_file_path);
    ofstream out(dest_file_path);

    out << in.rdbuf();

    stat(src_file_path.c_str(), &src_file_stat);
    chmod(dest_file_path.c_str(), src_file_stat.st_mode);
    chown(dest_file_path.c_str(), src_file_stat.st_uid, src_file_stat.st_gid);
    return 0;
}

string dest_root;
int src_dir_pos;

int copy_cb(const char* src_path, const struct stat* sb, int typeflag, struct FTW *ftwbuf) {
    string src_path_str(src_path);
    string dst_path = dest_root + src_path_str.substr(src_dir_pos);

    switch(typeflag) {
        case FTW_D:
            mkdir(dst_path.c_str(), sb->st_mode);
            break;
        case FTW_F:
            copy_file_to_dir(src_path_str, dst_path.substr(0, dst_path.find_last_of("/")));
    }
    return 0;
}

int copy_dir_to_dir(string src_dir_path, string dest_dir_path) {
    dest_root = dest_dir_path;
    src_dir_pos = src_dir_path.find_last_of("/");
    nftw(src_dir_path.c_str(), copy_cb, ftw_max_fd, 0);
    return 0;
}

void copy_command(vector<string> &cmd)
{
    string src_path, dest_path;
    dest_path = abs_path_get(cmd.back());
    for(unsigned int i = 1; i < cmd.size() - 1; ++i)
    {
        src_path = abs_path_get(cmd[i]);
        if(is_directory(src_path))
        {
            copy_dir_to_dir(src_path, dest_path);
        }
        else
        {
            copy_file_to_dir(src_path, dest_path);
        }
    }
    display_refresh();
}

int delete_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    string rem_path(path);

    if(is_directory(rem_path))
    {
        if(FAILURE == unlinkat(0, rem_path.c_str(), AT_REMOVEDIR))
            cout << "unlinkat failed!! errno " << errno;
    }
    else
    {
        if(FAILURE == unlinkat(0, rem_path.c_str(), 0))
            cout << "unlinkat failed!! errno " << errno;
    }
    return 0;
}

void delete_command(string rem_path)
{
    nftw(rem_path.c_str(), delete_cb, ftw_max_fd, FTW_DEPTH | FTW_PHYS);
    display_refresh();
}

void move_command(vector<string> &cmd)
{
    copy_command(cmd);
    string rem_path;
    for(unsigned int i = 1; i < cmd.size() - 1; ++i)
    {
        rem_path = abs_path_get(cmd[i]);
        delete_command(rem_path);
    }
}

int search_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    string path_str(path), cmp_str;
    size_t fwd_slash_pos = path_str.find_last_of("/");
    cmp_str = path_str.substr(fwd_slash_pos + 1);

    if(cmp_str == search_str)
    {
        dir_content dc;
        size_t fwd_slash_pos = path_str.find_last_of("/");
        dc.name = path_str.substr(fwd_slash_pos + 1);
        dc.content_line = "~/" + path_str.substr(root_dir.length());
        if(dc.content_line.length() % w.ws_col)
        {
            dc.no_lines = (dc.content_line.length() / w.ws_col) + 1;
        }
        else
        {
            dc.no_lines = (dc.content_line.length() / w.ws_col);
        }
        content_list.pb(dc);
    }
    return 0;
}


int snapshot_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    string snap_path(path);
    if(!is_directory(snap_path))
        return 0;

    if(snap_path.find("/.") != string::npos)
        return 0;

    string line_str;

    struct dirent **dir_entry_arr;
    struct dirent *dir_entry;

    ofstream dumpfile (dumpfile_path.c_str(), ios::app);
    line_str = "." + snap_path.substr(snapshot_folder_path.length()) + ":";
    dumpfile << line_str << endl;


    int n = scandir(snap_path.c_str(), &dir_entry_arr, NULL, alphasort);
    if(n == FAILURE)
    {
        cout << "Scandir() failed!!\n";
        return 0;
    }

    for(int i = 0; i < n; ++i)
    {
        dir_entry = dir_entry_arr[i];

        if(((string)dir_entry->d_name) == "." || ((string)dir_entry->d_name) == ".." ||
           dir_entry->d_name[0] == '.')
        {
            continue;
        }

        dumpfile << dir_entry->d_name << endl;

        free(dir_entry_arr[i]);
        dir_entry_arr[i] = NULL;
    }
    free(dir_entry_arr);
    dir_entry_arr = NULL;

    dumpfile << endl;

    return 0;
}
