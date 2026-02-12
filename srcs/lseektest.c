//lseektest.c
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h" // O_RDWR 정의

int main(int argc, char **argv)
{
	int fd;
	char buf[100];

	//인자 개수 확인
	if (argc < 4) {
		printf(1, "usage : lseek_test <filename> <offset> <string>\n");
		exit();
	}

	//기존 파일 내용 출력
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		printf(1, "error: open %s failed!\n", argv[1]);
		exit();
	}

	while (read(fd, buf, 99) > 0) {
		buf[99] = 0;
		printf(1, "%s", buf);
	}
	printf(1, "\n");
	close(fd);

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		printf(1, "error: open %s failed!\n", argv[1]);
		exit();
	}

	//파일 내용 변경
	if (lseek(fd, atoi(argv[2]), SEEK_SET) < 0) {
		printf(1, "error: lseek %s failed!\n", argv[1]);
		exit();
	}

	if (write(fd, argv[3], strlen(argv[3])) < 0) {
		printf(1, "error: write %s failed!\n", argv[1]);
		exit();
	}

	//변경 이후 파일 내용 출력
	if (lseek(fd, 0, SEEK_SET) < 0) {
		printf(1, "error: lseek %s failed!\n", argv[1]);
		exit();
	}

	while (read(fd, buf, 99) > 0) {
		buf[99] = 0;
		printf(1, "%s", buf);
	}
	printf(1, "\n");

	close(fd);
	exit();
}
