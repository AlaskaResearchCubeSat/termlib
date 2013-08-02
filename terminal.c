#include <msp430.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <ctl.h>
#include "terminal.h"

//used to break up a string into arguments for parsing
//*argv[] is a vector for the arguments and contains pointers to each argument
//*dst is a buffer that needs to be big enough to hold all the arguments
//returns the argument count
unsigned short make_args(char *argv[],const char *src,char *dst){
    unsigned short argc=0;
    argv[0]=dst;
    for(;;){
        while(isspace(*src))src++;
        //copy non space characters to dst
        while(!isspace(*src) && *src)
            *dst++=*src++;
        //terminate string bit
        *dst++=0;
        //at the end of src?
        if(*src==0)break;
        argc++;
        argv[argc]=dst;
    }
    //don't count null strings
    if(*argv[argc]==0)argc--;
    return argc;
}

//print a list of all commands
int helpCmd(char **argv,unsigned short argc){
  int i,rt=0;
  if(argc!=0){
    //arguments given, print help for given command
    //loop through all commands
    for(i=0;cmd_tbl[i].name!=NULL;i++){
      //look for a match
      if(!strcmp(cmd_tbl[i].name,argv[1])){
        //match found, print help and exit
        printf("%s %s\r\n",cmd_tbl[i].name,cmd_tbl[i].helpStr);
        return 0;
      }
    }
    //no match found print error
    printf("Error : command \'%s\' not found\r\n",argv[1]);
    //fall through and print a list of commands and return -1 for error
    rt=-1;
  }
  //print a list of commands
  printf("Possible Commands:\r\n");
  for(i=0;cmd_tbl[i].name!=NULL;i++){
    printf("\t%s\r\n",cmd_tbl[i].name);
  }
  return rt;
}

//execute a command
int doCmd(const char *cs){
  //buffers for args and arg vector
  //NOTE: this limits the maximum # of arguments
  //      and total length of all arguments
  char args[50];
  char *argv[10];
  unsigned short argc;
  int i;
  //split string into arguments
  argc=make_args(argv,cs,args);
  //search for command
  for(i=0;cmd_tbl[i].name!=NULL;i++){
    //look for a match
    if(!strcmp(cmd_tbl[i].name,argv[0])){
      //match found, run command and return
      return cmd_tbl[i].cmd(argv,argc);
    }
  }
  //unknown command, print help message
  printf("unknown command \'%s\'\r\n",argv[0]);
  helpCmd(NULL,0);
  //unknown command, return error
  return 1;
}

#define CMD_LEN           64
#define HISTORY_SIZE      10

//prototype for __putchar to make the compiler happy
int __putchar(int __c);

short hist_incr(short h){
  h++;
  if(h>=HISTORY_SIZE){
    h=0;
  }
  return h;
}

short hist_decr(short h){
  h--;
  if(h<0){
    h=HISTORY_SIZE-1;
  }
  return h;
}

//task to communicate with the user over USB
void terminal(void *p) __toplevel{
  TERM_SPEC *spec=p;
  //character from port
  int c=0;
  //buffer for command
  char cmd[CMD_LEN];
  //buffer for command history
  static char history[HISTORY_SIZE][CMD_LEN];
  short hist_idx=0,hist_lookback=0;
  //for parsing escape codes
  short esc_idx=-2,num;
  CTL_TIME_t esctime;
  char escbuffer[10],cmd_id;
  int i;
  //command string index
  unsigned int cIdx=0;
  //initialize command and history buffers
  cmd[0]=0;
  memset(history,0,sizeof(history));
  /*for(i=0;i<HISTORY_SIZE;i++){
    history[i][0]=0;
  }*/
  //check for NULL
  if(spec==NULL){
    //print error
    printf("\rNULL pointer passed to \"%s\" task\r\n",ctl_task_executing->name);
    //kill task
    ctl_task_die();
  }
  printf("\r%s\r\n>",spec->startMsg);
  for(;;){
    //get character
    c=spec->inputFCN();
    //check for EOF
    if(c==EOF){
      //EOF returned, input is not ready. wait a bit
      ctl_timeout_wait(ctl_get_current_time()+1024);
      continue;
    }
    if(esc_idx>-2 && (ctl_get_current_time()-esctime)>10){
      //printf("Timeout\r\n");
      esc_idx=-2;
    }
    if(esc_idx==-2){
      //process received character
      switch(c){
        case '\r':
        case '\n':
          //return key run command
          if(cIdx==0){
            /*//if nothing entered, do last command
            printf(history[hist_idx]);       //print command
            strcpy(cmd,history[hist_idx]);   //copy into command buffer*/
            //if nothing entered, ring bell
            putchar(0x07);
            break;
          }else{
            //run command from buffer
            cmd[cIdx]=0;    //terminate command string
            cIdx=0;         //reset the command index
            //save command in history
            hist_idx=hist_incr(hist_idx);
            strcpy(history[hist_idx],cmd);
            hist_lookback=hist_idx;
          }
          //send carriage return and new line
          printf("\r\n");
          //run command
          doCmd(cmd);
          //print prompt char
          putchar('>');
          continue;
        case '\x7f':
        case '\b':
          //backspace
          if(cIdx==0)continue;
          //backup and write over char
          printf("\b \b");
          //decrement command index
          cIdx--;
          continue;
        case '\t':
          //ignore tab character
          continue;
        case 0x1B://escape char
          //start processing escape sequence
          esc_idx=-1;
          esctime=ctl_get_current_time();
          continue;
      }
      //check for control char
      if(!iscntrl(c) && cIdx<(sizeof(cmd)/sizeof(cmd[0]) - 1)){
        //echo character
        putchar(c);
        //put character in command buffer
        cmd[cIdx++]=c;
      }
    }else if(esc_idx==-1){
      //check for escape sequence
      if(c=='['){
        //start of sequence found keep going
        esc_idx++;
      }else{
        //start of sequence not found
        //TODO: possibly reprocess chars in the sequence
        esc_idx=-2;
      }
    }else{
      if(esc_idx<sizeof(escbuffer)){
        escbuffer[esc_idx]=c;
      }
      if(c>='@' && c<='~'){
        //end of control sequence, do stuff
        cmd_id=escbuffer[esc_idx];
        switch(cmd_id){
          case 'A':
            //CCU - Cursor Up
            //printf("Up\r\n");
            //set default number of lines
            num=1;
            //check if argument is present
            if(esc_idx>0){
              //get argument
              num=escbuffer[0];
            }
            //check for end of history
            if(hist_decr(hist_lookback)==hist_idx || history[hist_lookback][0]=='\0'){
              putchar(0x07);
              break;
            }
            //replace command
            printf("\r\x1B[0K>%s",history[hist_lookback]);
            strcpy(cmd,history[hist_lookback]);
            cIdx=strlen(cmd);
            hist_lookback=hist_decr(hist_lookback);
          break;
          case 'B':
            //CUD - Cursor Down
            //printf("Down\r\n");
            //set default number of lines
            num=1;
            //check if argument is present
            if(esc_idx>0){
              //get argument
              num=escbuffer[0];
            }
            //check for end of history
            if(hist_lookback==hist_idx){
              putchar(0x07);
              printf("\r\x1B[0K>");
              cIdx=0;
              break;
            }
            hist_lookback=hist_incr(hist_lookback);
            printf("\r\x1B[0K>%s",history[hist_lookback]);
            strcpy(cmd,history[hist_lookback]);
            cIdx=strlen(cmd);
          break;
          case 'C':
            //CUF - Cursor Forward
            //printf("Forward\r\n");
            //set default number of lines
            num=1;
            //check if argument is present
            if(esc_idx>0){
              //get argument
              num=escbuffer[0];
            }
            //printf("num = %i\r\n",num);
          break;
          case 'D':
            //CUB - Cursor Back
            //printf("Back\r\n");
            //set default number of lines
            num=1;
            //check if argument is present
            if(esc_idx>0){
              //get argument
              num=escbuffer[0];
            }
            //printf("num = %i\r\n",num);
          break;
          case '~':
            escbuffer[esc_idx]=0;
            num=atoi(escbuffer);
            //printf("Code = %i\r\n",num);
            switch(num){
              case 1:
                //home key
              break;
              case 2:
                //insert key
              break;
              case 4:
                //end Key
              break;
              case 5:
                //page up key
              break;
              case 6:
                //page down key
                /*for(i=0;i<HISTORY_SIZE;i++){
                  printf("\r%c%i : %s\r\n",i==hist_idx?'>':' ',i,history[i]);
                }*/
              break;
            }
          break;
          //default:
            /*printf("\r\nUnknown command \'%c\' recieved with length %i:\r\n\\e[",cmd_id,esc_idx);
            for(i=0;i<=esc_idx;i++){
              printf("0x%02X, ",escbuffer[i]);
            }
            printf("\r\n");*/
        }
        esc_idx=-3;
      }
      esc_idx++;
    }
  } 
}

