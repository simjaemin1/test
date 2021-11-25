#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ROTLOCK_WRITE 400
#define ROTUNLOCK_WRITE 402


int main(int argc, char *argv[]){
	int num;
    FILE* fp;

	if(argc!=2)
	{
		printf("error: use only one argument\n");
		return 0;
	}
	num=atoi(argv[1]);
	while(1) {
		syscall(ROTLOCK_WRITE, 90, 90);
		fp=fopen("integer","w");
		if(fp==0)
		{
			printf("error: unable to open the file\n");
			return 0;
		}
		fprintf(fp, "%d", num);
		fclose(fp);
		printf("selector: %d\n", num++);
		syscall(ROTUNLOCK_WRITE, 90, 90);
	}
}
