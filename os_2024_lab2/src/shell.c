#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){
	if (p->in_file != NULL) {
		p->in = open(p->in_file, O_RDONLY);
		if (p->in == -1) {
			perror("open");
			exit(1);
		}
	// 	dup2(p->in, STDIN_FILENO);
	// 	close(p->in);
	}
	if (p->out_file != NULL) {
		p->out = open(p->out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (p->out == -1) {
			perror("open");
			exit(1);
		}
		// dup2(p->out, STDOUT_FILENO);
		// close(p->out);
	}
	dup2(p->in, STDIN_FILENO);
	// close(p->in);
	dup2(p->out, STDOUT_FILENO);
	// close(p->out);
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p)
{
	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		return STATUS_FAIL;
	}
	if (pid == 0) { // child proc
		// printf("spawn_proc: execvp(%s)\n", p->args[0]);
		redirection(p);
		execvp(p->args[0], p->args);
		perror("execvp");
		exit(1);
	}
	// parent proc
	int status;
	waitpid(pid, &status, 0);
	if (status == -1) {
		perror("waitpid");
		return STATUS_FAIL;
	}
	// printf("end of spawn_proc, status: %d\n", status);
	return STATUS_SUCCESS;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd)
{
	int status = STATUS_SUCCESS;
	int num_pipes = cmd->pipe_num;
	struct cmd_node *cur_cmd = cmd->head;
	int pipes_fd[2][2];

	for (int i = 0; i <= num_pipes; ++i) {
		if (i < num_pipes) {
			if (pipe(pipes_fd[i % 2]) == -1) {
				perror("pipe");
				return 0;
			}
		}
		
		cur_cmd->in = i == 0 ? STDIN_FILENO : pipes_fd[(i + 1) % 2][0];
		cur_cmd->out = i == num_pipes ? STDOUT_FILENO : pipes_fd[i % 2][1];

		if (spawn_proc(cur_cmd) == 0) {
			return 0;
		}

		// parent proc
		if (i < num_pipes) {
			close(pipes_fd[i % 2][1]);
		}
		if (i > 0) {
			close(pipes_fd[(i + 1) % 2][0]);
		}
		cur_cmd = cur_cmd->next;
	}

	for (int i = 0; i <= num_pipes; ++i) {
		int child_status;
		wait(&child_status);
		if (child_status == STATUS_FAIL) {
			status = STATUS_FAIL;
		}
	}

	// printf("end of fork_cmd_node\n");

	return status;

}
// ===============================================================


void shell()
{
	while (true) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 || out == -1) perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status, temp);

				// recover shell stdin and stdout
				if (temp->in_file) dup2(in, 0);
				if (temp->out_file) dup2(out, 1);
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
