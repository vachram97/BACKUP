/*
 ============================================================================
 Name        : backup.c
 Author      : vachram
 Version     : 1.0
 Copyright   : No copyright
 Description : This program is aimed to copy files or directories. It can 
 copy only file to file or directory to directory, in dependence on the 
 first argument. Usage: command [-g] source destination. Key '-g' means that
 all files are going to be zipped.
 ============================================================================
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <utime.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#define BUF_SIZE 8192
//#define PATH_MAX 4096  ---->in linux/limits.h
//#define NAME_MAX 255   ---->in linux/limits.h

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int copy_files(const char*, const char*);
int copy_data(int, int);
int zip_file(const char*);
int copy_directory(const char*, const char*);
int mkdir_recurs(const char *, mode_t);
int zipping = 0; //zip flag (0 - nozip, 1 - zip)

int main(int argc, char** argv) {

	int opt;
	while (-1 != (opt = getopt(argc, argv, "hg"))) {
		if (opt == '?') {
			printf ("Usage: %s source destination\n", argv[0]);
			return -1;
		}
		//help option
		if ((opt == 'h') && (argc == 2)) {
			printf("This program is aimed to copy files or directories."
							" It can copy only file to file or directory to directory,"
							" in dependence on the first argument. If second argument"
							" doesn't exists, its type (reg_file or dir) will be equal"
							" to the type of the first argument\n\n"
							"Usage: command [-g] source destination.\n"
							"Keys:\n"
							"\t-g -- zipping all files\n");
			return 0;
		}
		//enable zipping
		if ((opt == 'g') && (argc == 4)) {
			zipping = 1;
			continue;
		}
		printf ("Usage: %s source  destination\n", argv[0]);
		return -1;
	}
	
	//checking number of arg
	if (argc != optind + 2) {
		printf ("Usage: %s source destination\n", argv[0]);
		return -1;
	}

	//trying to understand what is given for copying
	struct stat stat_in, stat_out;
	if (-1 == stat(argv[optind], &stat_in)) {
		perror("Problem with first arg");
		return 0;
	}
	
	if (-1 == stat(argv[optind], &stat_out)) {
		if (errno == ENOENT) {
			//if second file/dir doesn't exist, we assume that their type are equal to type of the first arg
			stat_out.st_mode = stat_in.st_mode;
		} else {
			perror("Problem with second arg");
			return 0;
		}
	}
	
	int stat;
	
	//file and file
	if ((S_ISREG(stat_in.st_mode)) && (S_ISREG(stat_out.st_mode))) {
		stat = copy_files(argv[optind], argv[optind + 1]);
		return stat;
	}
	
	//dir and dir
	if ((S_ISDIR(stat_in.st_mode)) && (S_ISDIR(stat_out.st_mode))) {
		stat = copy_directory(argv[optind], argv[optind + 1]);
		return stat;
	}
	
	//dir and file
	if ((S_ISDIR(stat_in.st_mode)) && (S_ISREG(stat_out.st_mode))) {
		printf("I suppose it is not a good idea to copy directory in file\n");
		return -1;
	}
	
	/*if ((S_ISREG(stat_in.st_mode)) && (S_ISDIR(stat_out.st_mode))) {
	 stat = copy_file_in_directory(argv[1], argv[2]);
	 return stat;
	 }*/
	printf("These types of files are unsupported\n");
	return -1;

}

//copies files, if directories for new file are created before
//returns 0 on success
int copy_files(const char* source, const char* destination) {
	printf("Copying %s\n", source);
	int fd_in, fd_out; //file descriptors
	//opening for reading
	if (-1 == (fd_in = open(source, O_RDONLY))) {
		perror("Failed to open source");
		return 1;
	}
	//receiving info about file
	struct stat stat_in;
	if (-1 == (fstat(fd_in, &stat_in))) {
		perror("Failed to receive stat");
		return 1;
	}
	//changing mask for correct copying of rights
	umask(0);
	//opening for writing
	if (-1
			== (fd_out = open(destination, O_WRONLY | O_CREAT | O_TRUNC,
					stat_in.st_mode & 0777))) {
		perror("Failed to open/create destination");
		close(fd_in);
		return 1;
	}
	copy_data(fd_in, fd_out);
	//time changing
	struct timespec time_in[2];
	time_in[0] = stat_in.st_atim;
	time_in[1] = stat_in.st_mtim;
	//for utime(old):
	//time_in.actime = stat_in.st_atime;
	//time_in.modtime = stat_in.st_mtime;
	futimens(fd_out, time_in);
	//closing files
	close(fd_in);
	close(fd_out);
	if (zipping == 1) {
		int ret = zip_file(destination);
		if (ret != 0) {
			printf("Error occurs while zipping '%s', error code : %d\n", destination, ret);
		}
		return ret;
	}
	else return 0;
}

//function copies data from first file descriptor to another until eof
//returns 0 on success, -1 otherwise
int copy_data(int fd_in, int fd_out) {
	ssize_t count;
	char buf[BUF_SIZE];
	while (1) {
		if (-1 == (count = read(fd_in, buf, sizeof(buf)))) {
			perror("Failed to read");
			return -1;
		}
		if (count == 0)
			break;
		if (-1 == (write(fd_out, buf, count))) {
			perror("Failed to write");
			return -1;
		}
	}
	return 0;
}

//function archivate file, returns result of 'gzip' or -1 on error
//while creating new process, 0 on success
int zip_file(const char *file) {
	printf("Zipping %s\n", file);
	pid_t zip_pid = fork();
	int stat_zip;
	if (zip_pid == -1) {
		perror("Error occured while creating new process");
		return -1;
	}
	if (zip_pid == 0) {
		execlp("gzip", "gzip", "-f", file, (char *) NULL);
		return -1;
	} else {
		waitpid(zip_pid, &stat_zip, 0);
	}
	return stat_zip;
}

//copies directory with its files from first_dir to second_dir
//returns 0 on success
int copy_directory(const char* first_dir, const char* second_dir) {
	//opening directories
	DIR* input_d;
	DIR* output_d;

	if (NULL == (input_d = opendir(first_dir))) {
		perror("Error occured while opening directory");
		return -1;
	}

	struct stat dir_stats;
	if (-1 == stat(first_dir, &dir_stats)) {
		printf("Error while gettings stats of '%s': %s\n", first_dir, strerror(errno));
		return -1;
	}
	//create all directories to open head directory
	if (NULL == (output_d = opendir(second_dir))) {
		if (errno == ENOENT) {
			if (-1 == mkdir_recurs(second_dir, dir_stats.st_mode & 0777)) {
				printf("Error occured while creating directory");
				closedir(input_d);
				return -1;
			} else {
				if (NULL == (output_d = opendir(second_dir))) {
					perror("Error occured while opening directory");
					closedir(input_d);
					return -1;
				}
			}
		} else {
			perror("Error while opening directory");
			closedir(input_d);
			return -1;
		}
	}

	char in_dirent_path[PATH_MAX];
	char out_dirent_path[PATH_MAX];
	struct dirent *curr_dirent;
	int stat_res;

	while (NULL != (curr_dirent = readdir(input_d))) {
		if ((strcmp(curr_dirent->d_name, ".") == 0)
				|| (strcmp(curr_dirent->d_name, "..")) == 0)
			continue; //to avoid some mistakes (like directories . and ..

		//creating next pair of paths
		if (snprintf(in_dirent_path, PATH_MAX, "%s/%s", first_dir, curr_dirent->d_name) > PATH_MAX - 1) {
			printf("Too long path: ignored\n");
			continue;
		}
		if (snprintf(out_dirent_path, PATH_MAX, "%s/%s", second_dir, curr_dirent->d_name) > PATH_MAX - 1) {
			printf("Too long path: ignored\n");
			continue;
		}
		
		struct stat stat_in, stat_out;
		stat(in_dirent_path, &stat_in);
		
		//checking zipflag to look for zipped entries in directories
		if ((zipping == 1) && (!S_ISDIR(stat_in.st_mode)))
			strcat(out_dirent_path, ".gz\0");

		//looking for already backuped file
		if (-1 == (stat_res = stat(out_dirent_path, &stat_out))) {
			if (errno != ENOENT) {
				fprintf(stderr, "Error copying '%s' to '%s': %s\n",
							in_dirent_path, out_dirent_path, strerror(errno));
				continue;
			}
		}
		
		//making full path of second entry
		strcpy(out_dirent_path, second_dir);
		strcat(out_dirent_path, "/");
		strcat(out_dirent_path, (curr_dirent)->d_name);

		//copying current element
		//if directory
		if (curr_dirent->d_type == DT_DIR) {
			
			if (stat_res == 0) {
				if (-1 == copy_directory(in_dirent_path, out_dirent_path)) {
					fprintf(stderr, "Error copying '%s' to '%s'\n",
							in_dirent_path, out_dirent_path);
				}
				continue;
			} else {
				//no dir found, create it and continue copying
				umask(0);
				printf("Creating directory '%s'\n", out_dirent_path);
				mkdir(out_dirent_path, stat_in.st_mode & 0777);
				if (-1 == copy_directory(in_dirent_path, out_dirent_path)) {
					fprintf(stderr, "Error copying '%s' to '%s'\n",
							in_dirent_path, out_dirent_path);
				}
				continue;
			}
		}
		//if regular file 
		if (curr_dirent->d_type == DT_REG) {
			if (stat_res == 0) {
				//checking for zipping flag
				char out_dirent_path_zipped[PATH_MAX];
				strcpy(out_dirent_path_zipped, out_dirent_path);
				if (zipping == 1) {
					strcat(out_dirent_path_zipped, ".gz");
				}
				stat(out_dirent_path_zipped, &stat_out);
				
				//compare time in order to not doing extra work
				if (stat_out.st_mtime >= stat_in.st_mtime) {
					printf("File '%s' is already backuped\n", in_dirent_path);
					continue;
				}
				printf("File '%s' has been modified, making a new backup...\n",
						in_dirent_path);
				remove(out_dirent_path_zipped);
				
				if (0 != copy_files(in_dirent_path, out_dirent_path)) {
					fprintf(stderr, "Error copying '%s' to '%s'\n", in_dirent_path,
							out_dirent_path);
				}
				continue;
			} else {
				//if no file entry found, we'll copy it
				if (0 != copy_files(in_dirent_path, out_dirent_path)) {
					fprintf(stderr, "Error copying '%s' to '%s'\n", in_dirent_path,
							out_dirent_path);
				}
				continue;
			}
		}
		printf("Type of '%s' is unsupported: ignored\n", in_dirent_path);
	}
	closedir(input_d);
	closedir(output_d);
	return 0;
}

//creates a directory, if one directories of path doesn't exist, creates it
//return -1 on fail, 0 otherwise
int mkdir_recurs(const char *dir_path, mode_t mode) {
	umask(0);
	if (0 == mkdir(dir_path, mode)) {
		return 0;
	} else {
		if (errno == ENOENT) {
			char tmp[PATH_MAX];
			strcpy(tmp, dir_path);
			//changing last '/' on '\0' we get path of previous directory
			char *slash_pos = strrchr(tmp, '/');
			if (slash_pos == 0) {
				return -1;
			} else {
				*slash_pos = '\0';
				printf("%s\n", tmp);
				if (-1 == mkdir_recurs(tmp, mode)) {
					return -1;
				} else {
					//don't forgot create directory
					if (-1 == mkdir(dir_path, mode)) {
						return -1;
					} else
						return 0;
				}
			}
		} else {
			return -1;
		}
	}
}
