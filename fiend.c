/* NAME: Michelle Goh
   NetId: mg2657 */
#include "fiend.h"
#define st_mtime st_mtim.tv_sec
#include <libgen.h>
ino_t* inodes;
int inodeAmount = 1;

// Print error message unless COND is true
void testExpression(char* name, ino_t inodeNum, struct timespec time, char **argv, int argc, bool defaultPrint);

//-depth, traverse directories and print post-order; dir is item that opendir
//and readdir reads; depth represents # of traversals; prefix is needed to 
//print complete path; maxDepth to constrain # of traversals; bool for if 
//postorder or preorder
void traverse(char* dir, unsigned long int maxDepth, bool depth, bool followSym, 
	char **argv, int argc, bool printD)
{
	DIR *dp;
	struct dirent *entry;
	struct stat buf;

	if(followSym) //-L, use stat
	{
		if (stat (dir, &buf) < 0) //if stat fails, issue warning about file
		{
			if(errno == ELOOP)
			{
				WARN("stat failed %s", dir);
				return;
			}
			else if(lstat (dir, &buf) < 0)
			{
				WARN("lstat %s failed", dir);
				return;
			}
		}
	}
	else
	{
		if(lstat (dir, &buf) < 0)
		{
			WARN("lstat %s failed", dir);
			return;
		}
	}


	if (S_ISDIR(buf.st_mode)) //directory or symbollic link
	{

		bool recurse = true; //check if we recurse (see if there are loops or maxDepth = 0)

		bool linkLoop = false;
		
		if(followSym) //check symbolic links
		{	
			int loop;
			for(loop = 0; loop < inodeAmount; loop++)
			{
				if(inodes[loop] == buf.st_ino)
				{
					WARN("loop starting from(%s) detected", dir);
					recurse = false; 
					linkLoop = true;
					break;
				}
			}
			
			if(recurse) //not a loop
			{
				ino_t* temp = inodes;
				inodes = malloc(sizeof(ino_t)*(inodeAmount+1)); //one more for new inode, zero ending
				for(int fill = 0; fill < inodeAmount - 1; fill++)
				{
					inodes[fill] = temp[fill];
				}				
				inodes[inodeAmount-1] = buf.st_ino;
				inodes[inodeAmount] = 0;

				inodeAmount++;

				free(temp);
			}
		}

		if(!depth && !linkLoop)  //if not depth, evaluate parents first; if so, children
		{
			testExpression(dir, buf.st_ino, buf.st_mtim, argv, argc, printD);
		}

		if((dp = opendir (dir)) == NULL)
		{
			WARN("opendir %s failed", dir);
			recurse = false;
		}

		if((maxDepth > 0) && recurse)
		{
			while ((entry = readdir (dp))) 
			{
				if(strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
				{
					continue;
				}
				else
				{
					char* s = malloc((strlen(dir)+strlen(entry->d_name)+2)*sizeof(char)); //size of dir(now path)
					strcpy(s, dir);

					if(dir[strlen(dir)-1] == '/')
					{
						;
					}
					else
					{
						strcat(s, "/");
					}
					
					strcat(s, entry->d_name);

					traverse(s, maxDepth-1, depth, followSym, argv, argc, printD);

					free(s);
				}
			}

			free(inodes);
			inodes = malloc(sizeof(ino_t));
			inodes[0] = 0;
			inodeAmount = 1;

		}

		if(depth && !linkLoop)  //default print
		{
			testExpression(dir, buf.st_ino, buf.st_mtim, argv, argc, printD);
		}

		closedir (dp); 			
	}
	else //file
	{
		testExpression(dir, buf.st_ino, buf.st_mtim, argv, argc, printD);
	}
}

//state machine to test each file
void testExpression(char* name, ino_t inodeNum, struct timespec time,
	char **argv, int argc, bool defaultPrint)
{
	int testing = 1;
	int i = 0;
	if(argc == 0) //no actions, print
	{
		if (defaultPrint)
			printf("%s\n", name);
		return;
	}
	while(true)
	{
		switch(testing)
		{
			case 1:
			if(i >= argc)
			{
				if(defaultPrint)
					printf("%s\n", name);
				return;
			}
			else if(strcmp(argv[i], "-o") == 0) //short circuit or
			{
				if(defaultPrint)
					printf("%s\n", name);
				return;
			}
			else if(strcmp(argv[i], "-depth") == 0) //skip bc global options taken already
			{
				i++;
			}
			else if(strcmp(argv[i], "-maxdepth") == 0)
			{
				i++;
				i++;
			}
			else if(strcmp(argv[i], "-name") == 0)
			{
				char *base = strdup(name);
				if(fnmatch(argv[i+1], basename(base), 0) != 0)
				{
					testing = 0;
				}
				i++;
				i++;
				free(base);
			}
			else if(strcmp(argv[i], "-newer") == 0)
			{
				struct stat newerBuf;
				if (lstat (argv[i+1], &newerBuf) < 0) //fail stat check
					DIE("stat(%s) failed in testing", argv[i+1]);

				time_t entry = time.tv_sec;
				time_t newer = newerBuf.st_mtim.tv_sec;

				if (difftime(entry, newer) < 0)
					testing = 0;
				else if(difftime(entry, newer) == 0)
				{
					long entrySec = time.tv_nsec;
					long newerSec = newerBuf.st_mtim.tv_nsec;
					if(entrySec - newerSec < 0)
						testing = 0;
				}

				i++;
				i++;
			}
			else if(strcmp(argv[i], "-print") == 0)
			{
				printf("%s\n", name);
				i++;
			}
			else if(strcmp(argv[i], "-exec") == 0)
			{
				int e;
				int swap = -1; //swap {} with file name
				for(e = i+2; e < argc; e++)
				{
					if(strcmp(argv[e], ";") == 0)
					{
						break;
					}
					else if(strcmp(argv[e], "{}") == 0)
					{
						swap = e;
					}
				}

				int commandSize = 0;
				for(int m = i+1; m < e; m++) //include everything after -exec up to not including ;
				{
					if(swap == m)
						commandSize = commandSize + strlen(name) + 1; //space needed
					else
						commandSize = commandSize + strlen(argv[m]) + 1; //space needed
				}

				char *command = malloc(sizeof(char)*(commandSize));
				strcpy(command, argv[i+1]);

				for(int m = i+2; m < e; m++) //create the char* command for system;
				{
					strcat(command, " ");
					if(swap == m)
						strcat(command, name);
					else
						strcat(command, argv[m]);
				}

				strcat(command, "\0");

				fflush(stdout);

				if(system(command) != 0)
					testing = 0;
				free(command);
				i = e+1;
			}
			else if(strcmp(argv[i], "-a") == 0)
			{
				i++;
			}
			break;
			case 0:
			if(i >= argc)
			{
				return;
			}
			else if(strcmp(argv[i], "-o") == 0)
			{
				i++;
				testing = 1;
			}
			else
				i++;
			break;
		}
		
	}
	return;
}

int main(int argc, char **argv)
{
	bool depth = false;
	unsigned long int maxDepth = INT_MAX;

	bool defaultPrint = true; //add print statement if no action specified
	bool currentDir = false;

	bool followSym = false;

	int nonAct = 0; //see if -depth or fiend follows nonaction

	int i = 1; //skip ./fiend
	int num = 0; //separate files and expressions

	while(i < argc)
	{
		if(strcmp(argv[i], "-P") == 0)
		{
			followSym = false;
			i++;
		}
		else if(strcmp(argv[i], "-L") == 0)
		{
			followSym = true;
			i++;
		}
		else
		{
			break;
		}
	} //after going through -P's and -L's

	int fileStart = i; //after the -P's and L's are where files start

	bool actions = false; //no actions

	if (i < argc) //traverse until you find actions
	{
		while(i < argc)
		{
			if((strcmp(argv[i], "-depth") == 0) || (strcmp(argv[i], "-maxdepth") == 0) 
				|| (strcmp(argv[i], "-name") == 0) || (strcmp(argv[i], "-newer") == 0)
				|| (strcmp(argv[i], "-print") == 0) || (strcmp(argv[i], "-exec") == 0))
			{
				actions = true;
				break;
			}
			else if (argv[i][0] == '-')
			{
				struct stat dashBuf;
				if(lstat (argv[i], &dashBuf) < 0)
				{
					DIE("%s is bad argument", argv[i]); //file's can't start with "-" (?)
				}
				i++;
			}
			else
			{
				i++;
			}
		}
	}
	else //current directory, default print; if no files, the incrementing by 1 before would set i = argc
	{
		currentDir = true;
	}

	if(actions && (fileStart == i)) //filename does not exist; default dir
	{		
		currentDir = true;
	}

	num = i;

	int argcInput = 0;

	if(actions) //check if there are actions
	{
		num = i; //first action "starts with -"

		while(i < argc) //detect errors in command like args; recurse until end
		{
				if(strcmp(argv[i], "-depth") == 0) //dfs
				{
					if(nonAct == 1)
						WARN("-depth follows non-option %s", argv[i]);
					
					depth = true;
					i++;
				}
				else if(strcmp(argv[i], "-maxdepth") == 0)
				{

					if(i + 1 < argc)
					{
						if(nonAct == 1)
							WARN("-depth follows non-option %s", argv[i]);

						if((argv[i+1][0] == '-') || (argv[i+1][0] == '+'))
						{
							DIE("invalid maxdepth arg %s", argv[i+1]);
						}

						char *ptr;
						maxDepth = strtoul(argv[i+1], &ptr, 10);

						if((errno == EINVAL) || (errno == ERANGE))
							DIE("invalid maxdepth arg %s", argv[i+1]);

						if(ptr == argv[i+1])	//no conversion
						{ 						
							DIE("invalid maxdepth arg %s", argv[i+1]);
						}
						else if(*ptr == '\0') //entire string converted
						{
							;
						}
						else //middle of string
						{
							DIE("invalid maxdepth arg %s", argv[i+1]);
						}
						
						i++; //increment by two after
						i++;
					}
					else
					{
						DIE("no maxdepth arg %s", argv[i]);
					}
				}
				else if(strcmp(argv[i], "-name") == 0)
				{
					if(i+1 < argc)
					{
						nonAct = 1;
						i++;
						i++;
					}
					else
					{
						DIE("no name arg %s", argv[i]);
					}
				}
				else if(strcmp(argv[i], "-newer") == 0)
				{
					if(i+1 < argc)
					{
						nonAct = 1;
						struct stat newerBuf;
						if (lstat (argv[i+1], &newerBuf) < 0) //fail stat check
						{
							DIE("stat(%s) failed", argv[i+1]);
						}
						
						i++;
						i++;

					}
					else
					{
						DIE("no newer arg %s", argv[i]);
					}
				}
				else if(strcmp(argv[i], "-print") == 0)
				{
					nonAct = 1;
					defaultPrint = false; //action encountered
					i++;
				}
				else if(strcmp(argv[i], "-exec") == 0)
				{
					if(i+2 < argc) //-exec ends with ;
					{
						nonAct = 1;
						defaultPrint = false;
						int e;
						bool valid = false;
						for(e = i+2; e<argc; e++)
						{
							if(strcmp(argv[e], ";") == 0)
							{
								valid = true;
								break;
							}

						}
						if(!valid)
						{
							DIE("bad exec arguments %s", argv[i]);
						}
						else
						{
							i = e + 1; //increment
						}
					}
					else
					{
						DIE("improper exec arguments %s", argv[i]);
					}

				}
				else if((strcmp(argv[i], "-o") == 0) || (strcmp(argv[i], "-a") == 0))
				{
					if((i-1 >= 1) || (i+1 < argc)) //valid # of operands
					{
						if((strcmp(argv[i-1], "-o") != 0) && (strcmp(argv[i-1], "-a") != 0) 
							&& (strcmp(argv[i+1], "-o") != 0) && (strcmp(argv[i+1], "-a") != 0))
						{
							i++;
						}
						else
						{
							DIE("bad operands %s", argv[i]);
						}
					}
					else
					{
						DIE("bad # of operands %s", argv[i]);
					}
				}
				else
				{
					DIE("bad command line arg %s", argv[i]);
				}
			}

			argcInput = argc - num;
		}


		if(currentDir)
		{
			inodes = malloc(sizeof(ino_t));
			inodes[0] = 0;
			inodeAmount = 1;
			traverse(".", maxDepth, depth, followSym, &argv[num], argcInput, defaultPrint);
			free(inodes);
		}
		else
		{

			for(int d = fileStart; d < num; d++)
			{
				struct stat buf;
				if(lstat (argv[d], &buf) < 0)
				{
					WARN("lstat %s failed", argv[d]);
				}
				else
				{

					inodes = malloc(sizeof(ino_t));
					inodes[0] = 0;
					inodeAmount = 1;
					traverse(argv[d], maxDepth, depth, followSym, &argv[num], argcInput,  defaultPrint);
					free(inodes);
				}
			}
		}
	}
