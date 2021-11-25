#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ROTLOCK_READ 399
#define ROTUNLOCK_READ 401


void prime_factor(int num) {
	printf("%d =  ", num);
	int i =2;
	while(i<num) {
		if(num%i)
			i++;
		else {
			num/=i;
			printf("%d*", i);
		}
	}
	printf("%d\n", num);
}


int main(int argc, char *argv[]){
	int num;
	int identifier;
    FILE *fp;
    num = 0;
	if(argc!=2)
	{
		printf("error: use only one argument\n");
		return 0;
	}
	identifier=atoi(argv[1]);
	while(1)
	{
		syscall(ROTLOCK_READ, 90, 90);
		fp = fopen("integer", "r");
		if(fp==0)
		{
			printf("error: no such file integer\n");
			return 0;
		}
		fscanf(fp, "%d", &num);
		printf("trial-%d : ", identifier);
		prime_factor(num);
		fclose(fp);
		syscall(ROTUNLOCK_READ, 90, 90);
        sleep(1);
	}
}


