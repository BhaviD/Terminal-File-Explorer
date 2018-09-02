#ifndef _COMMAND_MODE_H_
#define _COMMAND_MODE_H_

#include <vector>
#include <string>
#include <fcntl.h>

void enter_command_mode();

int  copy_cb(const char*, const struct stat*, int);
void copy_command(std::vector<std::string>&);
int  copy_file_to_dir(std::string, std::string);
int  copy_dir_to_dir(std::string, std::string);

int  delete_cb(const char*, const struct stat*, int, struct FTW*);
void delete_command(std::string);

void move_command(std::vector<std::string>&);

int search_cb(const char*, const struct stat*, int, struct FTW*);
int snapshot_cb(const char*, const struct stat*, int, struct FTW*);


#endif
