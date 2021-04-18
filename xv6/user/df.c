#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user.h"

/*
  Added for hw4
  prints the curr num of free blocks and free inodes on disk1
*/

int main(int argc, char* argv[]) {

	/*
		ssize_t read(int fd, void* buf, size_t count)
		read is a syscall in xv6 already
		it reads count bytes from file and places into buf

		off_t lseek(int fd, off_t offset, int whence)
		lseek changes a file's offset
		lseek() needs to be created and should be a syscall
		whence has some flags
			SEEK_SET sets an offset to start_pos + offset
			SEEK_CURR sets offset to curr_pos + offset
			SEEK_END sets offset to EOF+offset
	*/

	// tracks number of free blocks and free inodes
	int fblocks = 0, fnodes = 0;

	// open disk1
	int fd;
	if((fd=open("/disk1", O_RDWR)) < 0) {
		printf(1, "Error: disk1 not present\n");
		exit();
	}

	// get superblock info
	struct superblock sb;
	lseek(fd, BSIZE, SEEK_SET); // skip over the boot block
	read(fd, &sb, sizeof(sb));

	// read bitmap
	char tmpb[BSIZE];
	lseek(fd, 0, SEEK_SET);
	for(int i=sb.bmapstart; i<sb.size; i++) {
		read(fd, tmpb, BSIZE);
		lseek(fd, i*BSIZE, SEEK_SET);
		fblocks++;
	}

	// scan the inode blocks; not actually how it works just frame
	lseek(fd, 0, SEEK_SET);
	struct inode tmpi;
	for(int i=sb.inodestart; i<sb.bmapstart; i++) {
		read(fd, &tmpi, BSIZE);
		lseek(fd, i*BSIZE, SEEK_SET);
		fnodes++;
	}

	// print free blocks and free inodes
	printf(1, "Free blocks: %d\b Free Inodes: %d\n", fblocks, fnodes);

	exit();
}