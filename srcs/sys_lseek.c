// sys_lseek.c

#include "types.h"          // uint, pde_t와 같은 기본 타입 정의
#include "defs.h"           // argint, file_get 등의 함수 선언
#include "param.h"          // NDIRECT와 같은 상수 정의
#include "mmu.h"          // 'ts'와 'NSEGS' 오류 해결
#include "fs.h"
#include "spinlock.h"       // spinlock 관련 함수 및 구조체
#include "sleeplock.h"      // sleeplock 구조체 정의
#include "file.h"           // file 구조체 정의 (sleeplock 뒤에 포함)
#include "stat.h"
#include "proc.h"           //fdtofile

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int sys_lseek(void) {
	int fd;
	int offset;
	int whence;
	int	new_offset;

	//parse argument
	if (argint(0, &fd) < 0 ||
		argint(1, &offset) < 0 ||
		argint(2, &whence) < 0)
		return -1;

	struct file *f;
	if (fd < 0 || fd >= NOFILE || (f = myproc()->ofile[fd]) == 0){
		cprintf("lseek: invalid file descriptor\n");
		return -1;
	}
	
	if (f->type != FD_INODE)
		return -1;
	
	acquiresleep(&f->ip->lock);

	switch (whence) {
		case SEEK_SET:
			new_offset = offset;
			break;
		case SEEK_CUR:
			new_offset = f->off + offset;
			break;
		case SEEK_END:
			new_offset = f->ip->size + offset;
			break;
		default:
			releasesleep(&f->ip->lock);
			return -1;
	}

	if (new_offset < 0) {
		releasesleep(&f->ip->lock);
		cprintf("lseek: invalid New offset\n");
		return -1;
	}
	
	f->off = new_offset;
	releasesleep(&f->ip->lock);
	return new_offset;
}