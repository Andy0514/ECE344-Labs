#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>

/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}


// Forward Declarations /////////////////////////////////////
void copy_file(const char* src_file, const char* dst_location);
void copy_dir(const char* src_dir, const char* dst_dir);


/////////////////////////////////////////////////////////////



int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
	}
	

	copy_dir(argv[1], argv[2]);

	return 0;
}


// Copy source file to the destination location, and set the 
// appropriate permissions. Assume that dst_location does not include
// the file name
void copy_file(const char* src_file, const char* dst_location)
{
	struct stat statbuf;
	int stat_result = stat(src_file, &statbuf);
	if (stat_result < 0) {
		syserror(stat, src_file);
	}
	
	if ((statbuf.st_mode && S_IFREG) == 0)
	{
		// This is not a regular file
		assert(0);
	}

	int src_file_permissions = statbuf.st_mode & ~S_IFMT;


	// actual opening the source file
	int src_fd = open(src_file, O_RDONLY);
	if (src_fd < 0) {
		syserror(open, src_file);
	}

	char buf[4096];

	// get the file name by looking for the last /
	char* file_name = NULL;
	for (int i = strlen(src_file); i != -2; i--)
	{
		if (src_file[i] == '/' || i == -1)
		{
			file_name = (char*) malloc(sizeof(char) * (strlen(src_file) - i));
			strcpy(file_name, src_file + i + 1);
			assert(file_name[strlen(file_name)] == '\0');
			break;
		}
	}
	assert(file_name != NULL);

	// concatenate the dst_location with file_name to get the full path
	// used in creat
	char* dst_full_path = (char*) malloc(sizeof(char) * (strlen(dst_location) + strlen(file_name) + 2));
	strcpy(dst_full_path, dst_location);
	strcat(strcat(dst_full_path, "/"), file_name);
	assert(dst_full_path[strlen(dst_full_path)] == '\0');

	// create a file in the destination location for writing
	int dst_fd = open(dst_full_path, O_CREAT | O_WRONLY);
	if (dst_fd < 0) {
		syserror(open, dst_full_path);
	}

	while (true)
	{
		int read_result = read(src_fd, buf, 4096); // returns num bytes read
		if (read_result < 0) {
			syserror(read, src_file);
		}
		if (read_result == 0) break;
		int write_result = write(dst_fd, buf, read_result);
		if (write_result < 0) {
			syserror(write, dst_full_path);
		}
	}


	// Change write file permission
	if (fchmod(dst_fd, src_file_permissions) < 0) {
		syserror(fchmod, dst_full_path);
	}

	
	if (close(src_fd) < 0) {
		syserror(close, src_file);
	}
	if (close(dst_fd) < 0) {
		syserror(close, dst_full_path);
	}

	// Memory cleanup
	free(dst_full_path);
	free(file_name);
}



void copy_dir(const char* src_dir, const char* dst_dir)
{
	DIR* dir_p = opendir(src_dir);
	if (dir_p == NULL) {
		syserror(opendir, src_dir);
	}

	// Get the current directory's permissions
	struct stat dirstat;
	int stat_result = stat(src_dir, &dirstat);
	if (stat_result < 0) {
		syserror(stat, src_dir);
	}
	int dir_permission = dirstat.st_mode & ~S_IFMT;

	// create the dst_dir with full permission
	int mkdir_result = mkdir(dst_dir, S_IRWXU);
	if (mkdir_result < 0) {
		syserror(mkdir, dst_dir);
	}

	struct dirent* dirent_p = NULL;
	while (true)
	{
		dirent_p = readdir(dir_p);
		if (dirent_p == NULL) {
			break;
		}

		// skip . and .. directories
		if (strcmp(dirent_p->d_name, ".") == 0 || strcmp(dirent_p->d_name, "..") == 0) {
			continue;
		}

		char* full_src_path = (char*) malloc(sizeof(char*) * (strlen(src_dir) + strlen(dirent_p->d_name) + 2));
		strcpy(full_src_path, src_dir);
		strcat(full_src_path, "/");
		strcat(full_src_path, dirent_p->d_name);
		assert(full_src_path[strlen(full_src_path)] == '\0');

		// check if this is a file or directory
		struct stat statbuf;
		int stat_result = stat(full_src_path, &statbuf);
		if (stat_result < 0) {
			syserror(stat, full_src_path);
		}
		
		if (S_ISREG(statbuf.st_mode))
		{
			// This is a file
			copy_file(full_src_path, dst_dir);
		}
		else if (S_ISDIR(statbuf.st_mode))
		{
			// This is a directory
			// construct dst directory name
			char* full_dst_path = (char*) malloc(sizeof(char*) * (strlen(dst_dir) + strlen(dirent_p->d_name) + 2));
			strcpy(full_dst_path, dst_dir);
			strcat(full_dst_path, "/");
			strcat(full_dst_path, dirent_p->d_name);
			assert(full_dst_path[strlen(full_dst_path)] == '\0');

			// recursive call
			copy_dir(full_src_path, full_dst_path);

			// cleanup
			free(full_dst_path);
		}

		free(full_src_path);	
	}

	// set the destination directory permission
	if (chmod(dst_dir, dir_permission) < 0) {
		syserror(chmod, dst_dir);
	}

	if (closedir(dir_p) < 0) {
		syserror(closedir, src_dir);
	}
}