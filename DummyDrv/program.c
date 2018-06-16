//headers
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h> // sysconf();
#include <sys/mman.h> // flag definitions: PROT_READ, ...
#include <string.h> // strcmp(), strncpy()
#include <sys/ioctl.h> // ioctl()
//#include "../include/ioctl_commands.h" //DUMMY_SYNC

//struct for ioctl queries
struct mem_t{
  unsigned long* data;
  unsigned long size;
};

#define DUMMY_SYNC   _IO('q', 1001)
#define DUMMY_WRITE _IOW('q', 1002, struct mem_t*)
#define DUMMY_READ  _IOW('q', 1003, struct mem_t*)

//colors
#define RES   "\033[0m"
#define RED   "\033[31m"
#define GRE   "\033[32m"
#define YEL   "\033[33m"
#define MAG   "\033[35m"
#define CYA   "\033[36m"
#define BOL   "\033[1m"
#define BRE   "\033[1m\033[31m"
#define BGR   "\033[1m\033[32m"
#define BYE   "\033[1m\033[33m"
#define BMA   "\033[1m\033[35m"
#define BBL   "\033[1m\033[36m"

//constants
#define DEVICE "/dev/dummy_two"

int main(){
  int fd;
  char option[3] = "xx", buf[sysconf(_SC_PAGE_SIZE)];
  char* maped_mem = NULL;
  long page_size = sysconf(_SC_PAGE_SIZE);
  char menu[] =
    RED " dr" RES ": driver read     │" MAG " is" RES ": ioctl sync      \n"
    RED " dw" RES ": driver write    │" MAG " iw" RES ": ioctl write     \n"
    GRE " ur" RES ": user read       │" MAG " ir" RES ": ioctl read      \n"
    GRE " uw" RES ": user write      │                                   \n"
    CYA " mm" RES ": perform mmap    │" RES " h " RES ": shows this menu \n"
    CYA " mu" RES ": perform munmap  │" RES " ex" RES ": exit            \n";



  
  fd=open(DEVICE,O_RDWR); //open for reading and writing
  if(fd==-1){
    printf("file %s either does not exist or has been locked by another process\n",DEVICE);
    exit(fd);
  }
  printf(BOL "\n\nAvailable options:\n" RES);
  printf("%s",menu);
  
  while(strcmp(option,"ex")){
    printf("\nEnter option (h for help): ");
    scanf("%2s",option);
    int ch; while ( (ch=getchar()) != '\n' && ch != EOF ); //clear input buffer
    
    if(!strcmp(option,"dw")){
      printf(RED "  Driver write\n" RES);
      printf("  data: ");
      if(!scanf("%[^\n]",buf)){//store everything up to just before \n
	printf("  No modification\n");
      }else{
	write(fd,buf,strlen(buf));
      }
    }
    
    else if(!strcmp(option,"dr")){
      read(fd,buf,strlen(buf));
      printf(RED "  Driver read\n" RES);
      printf("  data: [" YEL "%s" RES "]\n",buf);
    }
    
    else if(!strcmp(option,"mm")){
      printf(CYA "  Perform mmap\n" RES);
      maped_mem = (char*) mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); //can read and write
      printf("  memory mapped\n");
    }
    
    else if(!strcmp(option,"mu")){
      printf(CYA "  Perform munmap\n" RES);
      if(!munmap(maped_mem, page_size)){
	maped_mem = NULL;
	printf("  memory unmapped\n");
      }else{
	printf(BRE "could not unmap the page\n" RES);
      }
    }
    
    else if(!strcmp(option,"uw")){
      printf(GRE "  User write\n" RES);
      if(maped_mem != NULL){
	printf("  data: ");
	scanf("%[^\n]",buf);
	//strncpy(to,from,length);
	strncpy(maped_mem,buf,strlen(buf));
      }else{
	printf("  memory not mapped, mmap first\n");
      }
    }
    
    else if(!strcmp(option,"ur")){
      printf(GRE "  User read\n" RES);
      if(maped_mem != NULL){
	printf("  data: [" YEL "%s" RES "]\n",maped_mem);
      }else{
	printf("  memory not mapped, mmap first\n");
      }
    }
    
    else if(!strcmp(option,"is")){
      printf(MAG "  ioctl sync (not implemented)\n" RES);
      if(!ioctl(fd,DUMMY_SYNC,0)){
	printf("  successful\n");
      }else{
	printf(BRE "  fail\n" RES);
      }
    }

    else if(!strcmp(option,"iw")){
      struct mem_t umem;
      printf(MAG "  ioctl write\n" RES);
      printf("  data: ");
      umem.size = page_size;
      umem.data = malloc(umem.size);
      memset(umem.data,0,umem.size);
      if(!scanf("%[^\n]",(char*)umem.data)){//store everything up to just before \n
	printf("  No modification\n");
      }else{
	umem.size=strlen((char*)umem.data);
	if(!ioctl(fd,DUMMY_WRITE,&umem)){
	  printf("  successful\n");
	}else{
	  printf(BRE "  fail\n" RES);
	}
      }
      if(umem.data){
	free(umem.data);
      }
    }

    else if(!strcmp(option,"ir")){
      struct mem_t umem;
      printf(MAG "  ioctl read\n" RES);
      umem.size=page_size;
      umem.data = malloc(umem.size);
      memset(umem.data,0,umem.size);
      if(!ioctl(fd,DUMMY_READ,&umem)){
	printf("  data: [" YEL "%s" RES "]\n",(char*)umem.data);
      }else{
	printf(BRE "  fail\n" RES);
      }
      if(umem.data){
	free(umem.data);
      }
    }

    else if(!strcmp(option,"h")){
      printf("%s",menu);
    }
    
    else if(!strcmp(option,"ex")){
      printf("  exit\n");
      close(fd);
    }
    
    else{
      printf(BRE "  %s is an incorrect option\n" RES, option);
    }
  }
  return 0;
}
 
