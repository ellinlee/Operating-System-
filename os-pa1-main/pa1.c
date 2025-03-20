/**********************************************************************
 * Copyright (c) 2020-2024
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct alias
{
	char keyword[100];
	char command[100][10];
	int commandCount;
};

struct alias aliases[100];
int aliasCount = 0;
int status;
int returnValue = 0;

int run_single_command(int nr_tokens, char *tokens[])
{
	if (strcmp(tokens[0], "alias") == 0)
	{
		if (nr_tokens == 1)
		{
			for (int i = 0; i < aliasCount; i++)
			{
				fprintf(stderr, "%s: ", aliases[i].keyword);
				for (int j = 0; j < aliases[i].commandCount; j++)
				{
					fprintf(stderr, "%s ", aliases[i].command[j]);
				}
				fprintf(stderr, "\n");
			}
		}
		else if (nr_tokens > 1)
		{
			strcpy(aliases[aliasCount].keyword, tokens[1]);
			aliases[aliasCount].commandCount = 0;
			for (int i = 2; i < nr_tokens; i++)
			{
				strcpy(aliases[aliasCount].command[i - 2], tokens[i]);
				aliases[aliasCount].commandCount++;
			}
			aliasCount++;
		}
	}
	else
	{
		char *newTokens[32] = {NULL};
		int top = 0;

		for (int i = 0; i < nr_tokens; i++)
		{
			int isAlias = 0;
			for (int j = 0; j < aliasCount; j++)
			{
				if (strcmp(tokens[i], aliases[j].keyword) == 0)
				{
					// alias word
					isAlias = 1;
					for (int k = 0; k < aliases[j].commandCount; k++)
					{
						newTokens[top++] = strdup(aliases[j].command[k]);
					}
					break;
				}
			}
			// no alias word
			if (isAlias == 0)
			{
				newTokens[top++] = strdup(tokens[i]);
			}
			isAlias = 0;
		}

		nr_tokens = top;
		pid_t pid = fork();

		if (pid > 0)
		{
			wait(&status);
		}

		else if (pid == 0)
		{
			if (strcmp(newTokens[0], "cd") == 0)
			{
				char *homedir = getenv("HOME");
				if (nr_tokens == 1 || strcmp(newTokens[1], "~") == 0)
				{
					chdir(homedir);
				}
				else
				{
					chdir(newTokens[1]);
				}
			}

			else
			{
				returnValue = execvp(newTokens[0], newTokens);

				if (returnValue < 0)
				{
					return -1;
					exit(-1);
				}
			}
		}
	}
	return 1;
}

/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
int run_command(int nr_tokens, char *tokens[])
{
	char *newTokens[32] = {NULL};
	int countToken = 0;

	for (int i = 0; i < nr_tokens; i++)
	{
		if (strcmp(tokens[i], "|") == 0)
		{
			returnValue = run_single_command(countToken, newTokens);
			for (int j = 0; j < 32; j++)
			{
				free(newTokens[j]);
				newTokens[j] = NULL;
			}
			countToken = 0;
		}
		else
		{
			newTokens[countToken++] = strdup(tokens[i]);
		}
	}

	returnValue = run_single_command(countToken, newTokens);

	if (returnValue == -1)
	{
		return -1;
	}
	else if (returnValue == 0 || strcmp(tokens[0], "exit") == 0)
	{
		return 0;
		exit(0);
	}
	return 1;
}

/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
int initialize(int argc, char *const argv[])
{
	return 0;
}

/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
void finalize(int argc, char *const argv[])
{
}
