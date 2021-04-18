#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "kernel/hwinit.h"
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
	#if HW4_ddn
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

	// count number of free blocks in bitmap
	char tmpb[BSIZE];
	int bmap_size = sb.size - sb.nblocks - sb.ninodes - sb.nlog;
	int db_checked = 0;
	for(int i=sb.bmapstart; i<bmap_size; i++) {
		// get a bitmap block from disk
		lseek(fd, i*BSIZE, SEEK_SET);
		read(fd, tmpb, BSIZE);
		// check every byte in the block
		for(int j=0;j<BSIZE;j++) {
			char byte = tmpb[j];
			// check if there are 0s in the byte and increment
			for(int k=0;k<8;k++) {
				if(!byte&1)
					fblocks++;
				byte = byte >> 1;
				db_checked++;
				if(db_checked==sb.nblocks)
					break;
			}
			if(db_checked==sb.nblocks)
				break;
		}
		if(db_checked==sb.nblocks)
			break;
	}

	// scan the inodes to count free ones
	struct dinode tmpi;
	int nodes_checked = 0;
	for(int i=sb.inodestart; i<sb.bmapstart; i++) {
		// get an dinode block from disk
		lseek(fd, i*BSIZE, SEEK_SET);
		read(fd, tmpb, BSIZE);
		for(int j=0; j<IPB; j++) {
			memmove(&tmpi, tmpb, sizeof(dinode));
			tmpb += sizeof(dinode);
			// this inode is free; increment
			if(tmpi.type == 0)
				fnodes++;
			if(++nodes_checked==sb.ninodes)
				break;
		}
		if(nodes_checked==sb.ninodes)
			break;
	}

	// print free blocks and free inodes
	printf(1, "Free blocks: %d\b Free Inodes: %d\n", fblocks, fnodes);

	exit();
	#endif

	// if hw4 is disabled
	#if !HW4_ddn
	printf(1, "%s\n", "The df function has been turned off due to a kernel/hwinit.h macro");
	exit();
	#endif
}