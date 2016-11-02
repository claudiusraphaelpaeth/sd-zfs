#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "zfs-util.h"

// Buffer for reading ZFS command output
#define BUFSIZE 16
#define ZFS_EXE "/usr/bin/zfs"
#define ZPOOL_EXE "/usr/bin/zpool"
#define ZFS_CMD "zfs"
#define ZPOOL_CMD "zpool"

int execute(char *command, char needOutput, char **output, char *param[]) {
	int pip[2];
	pid_t pid;
	int status;
	char *linebuffer;
	size_t size;
	int nRead;

	// Execute
	if (needOutput == 1) {
		pipe(pip);
	}
	pid = fork();
	if (pid == 0) {
		// Set up pipe
		close(1);
		close(2);
		if (needOutput == 1) {
			close(pip[0]);
			dup(pip[1]);
		}
		// Execute
		execv(command, param);
		exit(254);
	} else if (pid < 0) {
		fprintf(stderr, "Can not fork\n");
		close(pip[0]);
		close(pip[1]);
		return pid;
	}
	if (needOutput == 1) {
		close(pip[1]);
		// Read lines
		linebuffer = malloc(BUFSIZE * sizeof(char));
		while (1) {
			while ((nRead = read(pip[0], linebuffer, BUFSIZE)) > 0) {
				if (*output == NULL) {
					*output = malloc(nRead + 1);
					memcpy(*output, linebuffer, nRead);
					(*output)[nRead] = '\0';
				} else {
					size = strlen(*output) + nRead + 1;
					*output = realloc(*output, size);
					memcpy(&(*output)[strlen(*output)], linebuffer, nRead);
					(*output)[size - 1] = '\0';
				}
			}
			if (nRead == 0) {
				break;
			}
		}
		free(linebuffer);
	}
	// Wait for quit
	waitpid(pid, &status, 0);
	return status;
}

/*
 * Executes a zfs command.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is ZFS_CMD
 */
int executeZfs(char needOutput, char **output, char *param[]) {
	return execute(ZFS_EXE, needOutput, output, param);
}

/*
 * Executes a zpool command.
 * If needOutput is 1, the output of the command is written to output
 * which will be allocated. It must be NULL when passing in.
 * param must be a null-terminated array of parameters where the first
 * is ZPOOL_CMD
 */
int executeZpool(char needOutput, char **output, char *param[]) {
	return execute(ZPOOL_EXE, needOutput, output, param);
}

int zfs_destroy_recursively(char *dataset) {
	char *lines = NULL;
	char *token;

	// Execute command
	char *cmdline[] = { ZFS_CMD, "list", "-tfilesystem", "-Hro", "name", "-Sname", dataset, NULL };
	executeZfs(1, &lines, cmdline);
	if (lines == NULL) {
		return 0;
	}
	// Destoy all datasets
	token = strtok(lines, "\n");
	do {
		if (zfs_destroy(token) != 0) {
			fprintf(stderr, "Not destroying any more datasets\n");
			free(lines);
			return -2;
		}
	} while ((token = strtok(NULL, "\n")) != NULL);

	free(lines);
	return 0;
}

int zfs_destroy(char *dataset) {
	int status;
	char *cmdline[] = { ZFS_CMD, "destroy", dataset, NULL };

	status = executeZfs(0, NULL, cmdline);
	if (status != 0) {
		fprintf(stderr, "ZFS returned error %d\n", status);
	}
	return status;
}

int zfs_snapshot_exists(char *dataset, char *snapshot) {
	int status;
	char *toCheck;

	toCheck = malloc((strlen(dataset) + strlen(snapshot) + 2) * sizeof(char));
	strcpy(toCheck, dataset);
	strcat(toCheck, "@");
	strcat(toCheck, snapshot);
	
	status = zfs_ds_exists(toCheck);
	free(toCheck);
	return status;
}

int zfs_ds_exists(char *dataset) {
	int status;
	char *cmdline[] = { ZFS_CMD, "get", "-H", "type", dataset, NULL };

	status = executeZfs(0, NULL, cmdline);

	if (status == 0) {
		return 0;
	} else {
		return 1;
	}
}

int zfs_get_bootfs(char *rpool, char **bootfs) {
	int status;
	char **cmdline;

	if (rpool == NULL) {
		cmdline = (char*[]) { ZPOOL_CMD, "list", "-Ho", "bootfs", NULL };
	} else {
		cmdline = (char*[]) { ZPOOL_CMD, "list", "-Ho", "bootfs", rpool, NULL };
	}

	*bootfs = NULL;

	status = executeZpool(1, bootfs, cmdline);

	if (status != 0) {
		fprintf(stderr, "zpool get returned %d\n", status);
	}
	return status;
}

int zfs_list_datasets_with_mp(char *dataset, char **datasets) {
	int status;
	char *cmdline[] = { ZFS_CMD, "list", "-r", dataset, "-t", "filesystem", "-Ho", "name,mountpoint", NULL };

	*datasets = NULL;

	status = executeZfs(1, datasets, cmdline);

	if (status != 0) {
		fprintf(stderr, "zfs returned %d\n", status);
	}
	return status;
}
