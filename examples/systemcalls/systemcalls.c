#include "systemcalls.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success 
 *   or false() if it returned a failure
*/

  printf("\n\ndo_system: ************************************************\n");
  if (system(cmd) != 0)
  {
    return false;  
  }

  return true;


   
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count+1];
/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *   
*/

    printf("\n\ndo_exec: ************************************************");
    printf("\nProcess id of current process in %d", getpid());
   

    for(int i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        //printf("\ncommand[%d] :%s", i, command[i]);
    }
    
    command[count] = NULL;
    pid_t process_id;
    int status;

    /*******************fork********************/
    process_id = fork();

    if(process_id == -1)
    {
        printf("\nfork failed");
        return false;
    }
    

    /*******************Execv()********************/
    else if (process_id == 0) {
        printf("\nExecutiing execv : ");
        for(int i=0; i<count; i++)
        {
            printf("%s ", command[i]);
        } 
        printf("\n");
        execv(command[0], command); 
        
         exit(-1);
    }

    
    if (waitpid(process_id, &status, 0) == -1) //returns -1 in case of error
    {
        printf("\nwait failed\n");
        return false;
    }
    else 
    {
        if (WIFEXITED(status) == true)    //returns true if child exited normally
        {
            printf("\nChild process terminated normally with exit status %d\n", WEXITSTATUS (status));
            // return WEXITSTATUS (status);
            if(WEXITSTATUS(status) != 0)
                return false;
            else
                return true;
        }
    }

    

    
    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int status;
    int i;
    int ret_dup;
    pid_t process_id;

    int fd = creat(outputfile, 0644);

    printf("\n\ndo_exec_redirect: ************************************************");
    printf("\nProcess id of current process in %d", getpid());

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        //printf("\ncommand[%d] :%s", i, command[i]);
    }

    command[count] = NULL;

    /*******************fork********************/
    process_id = fork();
    if(process_id == -1)
    {
        printf("\nfork failed");
        return false;
    }
    

    /*******************Execv********************/
    else if (process_id == 0) {
        printf("\nExecutiing execv : ");
        for(int i=0; i<count; i++)
        {
            printf("%s ", command[i]);
        } 
        printf("\n");

        ret_dup = dup2(fd, STDOUT_FILENO);
        if(ret_dup == -1)
            return false;

        execv(command[0], command); 

        exit(-1);
    }
    
        


    /********************Wait*********************/

    if (waitpid(process_id, &status, 0) == -1) //returns -1 in case of error
    {
        printf("\nwait failed\n");
        return false;
    }
    else 
    {
        if (WIFEXITED(status) == true)    //returns true if child exited normally
        {
            printf("\nChild process terminated normally with exit status %d\n", WEXITSTATUS (status));
            if(WEXITSTATUS(status) != 0)
                return false;
            else
                return true;
        }
    }

    va_end(args);
    
    return true;
}
