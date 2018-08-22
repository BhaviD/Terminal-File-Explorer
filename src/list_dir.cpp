#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

#include <iostream>
using namespace std;

#define FAILURE -1
#define SUCCESS 0

int ls_dir(const char* dir)
{
    struct dirent **entry_list;
    struct dirent *entry;
    struct stat entry_stat;                // to retrive the stats of the file/directory
    struct passwd *pUser;               // to determine the file/directory owner
    struct group *pGroup;               // to determine the file/directory group

    char buf[512];
    char ret_time[26];

    int n = scandir(dir, &entry_list, NULL, alphasort);
    if(n == FAILURE)
    {
        cout << "Scandir() failed!!\n";
        return FAILURE;
    }
    for(int i = 0; i < n; ++i)
    {
        // "dir/entry" defines the path to the entry
        entry = entry_list[i];
        sprintf(buf, "%s/%s", dir, entry->d_name);
        stat(buf, &entry_stat);            // retrieve information about the entry

        // [permissions] [owner] [group] [size in bytes] [time of last modification] [filename]

        // [permissions]
        // http://linux.die.net/man/2/chmod 

        printf( (entry_stat.st_mode & S_IRUSR) ? " r" : " -");
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

        //[group]
        // http://linux.die.net/man/3/getgrgid
        pGroup = getgrgid(entry_stat.st_gid);
        printf("  %s ", pGroup->gr_name);

        //And the easy-cheesy part
        //[size in bytes] [time of last modification] [filename]
        printf("%-5ld",entry_stat.st_size);
        strcpy(ret_time, ctime(&entry_stat.st_mtime));
        ret_time[24] = '\0';
        printf(" %s", ret_time);
        printf(" %-20s\n", entry->d_name);
        free(entry_list[i]);
    }
    free(entry_list);
    return SUCCESS;
}

int main(int argc, char* argv[])
{
    return ls_dir(argv[1]);
}
