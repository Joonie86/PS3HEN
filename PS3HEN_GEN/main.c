#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <inttypes.h>

int main(int argc, char **argv)
{
	if(argc<4)
	{
		printf("Usage:stackframe.bin stage2 stage0\n");
		return -1;
	}
	
	FILE *sp=fopen(argv[1],"rb+");
	FILE *stage2=fopen(argv[2],"rb");
	FILE *stage0=fopen(argv[3],"rb");
	
	if(!sp || !stage2 || !stage0)
	{
		printf("error opening file(s)!\n");
		return -1;
	}
	
	struct stat buffer;
	int status;
	uint32_t truncate_len=0x110000;

	status = stat(argv[1], &buffer);
	if(status == 0)
	{
		if(stat(argv[2],&buffer)==0)
		{
			uint8_t *stage2_buf=(uint8_t *)malloc(buffer.st_size);
			fread(stage2_buf,buffer.st_size,1,stage2);
			fclose(stage2);
			fseek(sp,0x110000,SEEK_SET);
			fwrite(stage2_buf,buffer.st_size,1,sp);
			truncate_len+=buffer.st_size;
			free(stage2_buf);
			if(stat(argv[3],&buffer)==0)
			{
				uint8_t *stage0_buf=(uint8_t *)malloc(buffer.st_size);
				fread(stage0_buf,buffer.st_size,1,stage0);
				fclose(stage0);
				fseek(sp,0x102000,SEEK_SET);
				fwrite(stage0_buf,buffer.st_size,1,sp);
				free(stage0_buf);
			}
		}
	}
	_chsize( fileno(sp), truncate_len);
	fclose(sp);
	printf("DONE!\n");
	return 0;
}