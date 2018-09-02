#include "common.h"
#include "includes.h"
#include "normal_mode.h"
#include "command_mode.h"

using namespace std;

extern string  working_dir;
extern string  root_dir;
extern termios new_attr;

struct winsize w;

char next_input_char_get()
{
    cin.clear();
    char ch = cin.get();
    switch(ch)
    {
        case ESC:
            new_attr.c_cc[VMIN] = 0;
            new_attr.c_cc[VTIME] = 1;
            tcsetattr( STDIN_FILENO, TCSANOW, &new_attr);

            if(FAILURE != cin.get())    // FAILURE is return if ESC is pressed
            {
                ch = cin.get();         // For UP, DOWN, LEFT, RIGHT
            }
            new_attr.c_cc[VMIN] = 1;
            new_attr.c_cc[VTIME] = 0;
            tcsetattr( STDIN_FILENO, TCSANOW, &new_attr);
            break;

        default:
            break;
    }
    return ch;
}

void from_cursor_line_clear()
{
    cout << "\e[0K";
    cout.flush();
}

bool is_directory(string str)
{
    struct stat str_stat;          // to retrive the stats of the file/directory
    stat(str.c_str(), &str_stat);

    if((str_stat.st_mode & S_IFMT) == S_IFDIR)
        return true;
    else
        return false;
}

string abs_path_get(string str)
{
    char *str_buf = new char[str.length() + 1];
    strncpy(str_buf, str.c_str(), str.length());
    str_buf[str.length()] = '\0';

    string ret_path = working_dir, prev_tok = working_dir;
    char *p_str = strtok(str_buf, "/");
    while(p_str)
    {
        string tok(p_str);
        if(tok == ".")
        {
            prev_tok = tok;
            p_str = strtok (NULL, "/");
        }
        else if(tok == "..")
        {
            if(ret_path != root_dir)
            {
                ret_path.erase(ret_path.length() - 1);
                //ret_path = ret_path.substr(0, ret_path.length() - 1);
                size_t fwd_slash_pos = ret_path.find_last_of("/");
                ret_path = ret_path.substr(0, fwd_slash_pos + 1);
            }
            prev_tok = tok;
            p_str = strtok (NULL, "/");
        }
        else if (tok == "~")
        {
            ret_path = root_dir;
            prev_tok = tok;
            p_str = strtok (NULL, "/");
        }
        else if(tok == "")
        {
            if(!prev_tok.empty())
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

void t_win_resize_handler(int sig)
{
    ioctl(0, TIOCGWINSZ, &w);
    display_refresh();
}

void stack_clear(stack<string> &s)
{
    while(!s.empty()) s.pop();
}
