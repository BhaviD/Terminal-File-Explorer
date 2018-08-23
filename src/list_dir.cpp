#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <termios.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#define FAILURE -1
#define SUCCESS 0

int ls_dir(const char* dir)
{
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt);

    struct dirent **entry_list;
    struct dirent *entry;
    struct stat entry_stat;          // to retrive the stats of the file/directory
    struct passwd *pUser;            // to determine the file/directory owner
    struct group *pGroup;            // to determine the file/directory group

    char buf[512], curr_dir[512], working_dir[512];
    snprintf(working_dir, strlen(dir) + 1, "%s", dir);

    char ret_time[26];
    int r = 1, c = 1;

    while(1)
    {
        r = c = 1;
        cout << "\033[3J";
        cout << "\033[2J";
        cout << "\033[H\033[J";
        cout << "\033[" << r << ";" << c << "H";

        int n = scandir(working_dir, &entry_list, NULL, alphasort);
        if(n == FAILURE)
        {
            cout << "Scandir() failed!!\n";
            return FAILURE;
        }

        for(int i = 0; i < n; ++i)
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
            printf(" %-20s\n", entry->d_name);
        }

        r = c = 1;
        cout << "\033[" << r << ";" << c << "H";
        char ch;
        int i = 0;
        bool done = false;
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
                            --i;
                            cout << "\033[" << --r << ";" << c << "H";
                            break;
                        case 'B':       // down
                            ++i;
                            cout << "\033[" << ++r << ";" << c << "H";
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
        if(i < 0 || i >= n)
            continue;

        cout << "\033[3J";
        cout << "\033[2J";
        cout << "\033[H\033[J";
        cout << "\033[" << 0 << ";" << 0 << "H";

        snprintf(curr_dir, sizeof(curr_dir), "%s", working_dir);
        snprintf(working_dir, sizeof(working_dir), "%s/%s", curr_dir, entry_list[i]->d_name);

        for(i = 0; i < n; ++i)
            free(entry_list[i]);
        free(entry_list);
    }

    return SUCCESS;
}

int main(int argc, char* argv[])
{
    return ls_dir(argv[1]);
}
