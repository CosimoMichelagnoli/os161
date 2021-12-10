#include <stdio.h>
#include <unistd.h>
#include <string.h>



int main(void){
	
	int fd,newfd=30;
	char buf[10];
	size_t byte;
	fd=open("test.txt",O_CREAT|O_RDWR, NULL);
	newfd=dup2(fd,newfd);
	if(newfd==-1)
	{
		printf("test failed\n");
		close(fd);
		return -1;
	}

	byte=5;
	if(write(fd,"test\0",byte)!=(int)byte)
	{
		printf("write failed\n");
		close(fd);
		close(newfd);
		return -1;
	}
	if(lseek(newfd,0,SEEK_SET)==-1){

		printf("lseek failed\n");
		close(fd);
		close(newfd);
		return -1;
	}
	if(read(newfd,buf,byte)!=(int)byte)
	{
		printf("read failed\nbuf:%s",buf);
		close(fd);
		close(newfd);
		return -1;
	}
	if(!strcmp(buf,"test\0")){
		printf("test succeded\n");
		close(fd);
		close(newfd);
		return 0;
	}
	
	close(fd);
	close(newfd);
	return -1;

}
