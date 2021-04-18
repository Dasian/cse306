#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"

#include "kernel/fs.h"
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/file.h"

#include "kernel/hwinit.h"
#include "user.h"


/*
  Added for hw4
  prints the curr num of free blocks and free inodes on disk1
*/

int main(int argc, char* argv[]) {

	// if hw4 is disabled
	#if !HW4_ddn
	printf(1, "%s\n", "The df function has been turned off due to a kernel/hwinit.h macro");
	exit();
	#endif

	#if HW4_ddn
	// tracks number of free blocks and free inodes
	int fblocks = 0, fnodes = 0;

	#if HW4_debug
	printf(1, "%s\n", "df: starting; opening disk1");
	#endif

	// open disk1
	int fd;
	if((fd=open("/disk1", O_RDWR)) < 0) {
		printf(1, "Error: disk1 not present\n");
		exit();
	}

	#if HW4_debug
	printf(1, "%s\n", "df: disk1 opened; getting superblock");
	#endif

	// get superblock info
	struct superblock sb;
	lseek(fd, BSIZE, SEEK_SET); // skip over the boot block
	read(fd, &sb, sizeof(sb));

	#if HW4_debug
	printf(1, "%s\n", "df: superblock obtained; starting freeblocks");
	printf(1, "superblock data: size %d nblocks %d ninodes %d\n",sb.size, sb.nblocks, sb.ninodes);
	#endif

	// count number of free blocks in bitmap
	char tmpb[BSIZE];
	int bmap_size = sb.size - sb.nblocks - sb.bmapstart;
	int db_checked = 0;
	for(int i=sb.bmapstart; i<bmap_size+sb.bmapstart; i++) {
		#if HW4_debug
		printf(1, "Checking bmap block %d; starting lseek\n", i-sb.bmapstart+1);
		#endif
		// get a bitmap block from disk
		lseek(fd, i*BSIZE, SEEK_SET);
		#if HW4_debug
		printf(1, "df: lseek successful; starting read\n");
		#endif
		read(fd, tmpb, BSIZE);
		#if HW4_debug
		printf(1, "df: read successful; analyzing blocks\n");
		#endif
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

	#if HW4_debug
	printf(1, "df: free blocks found: %d; starting inodes\n\n", fblocks);
	#endif

	// scan the inodes to count free ones
	struct dinode tmpi;
	int nodes_checked = 0;
	int tmpb_off = 0;
	for(int i=sb.inodestart; i<sb.bmapstart; i++) {
		tmpb_off = 0;
		#if HW4_debug
		printf(1, "Checking inode block %d; starting lseek\n", i-sb.inodestart+1);
		#endif
		// get an dinode block from disk
		lseek(fd, i*BSIZE, SEEK_SET);
		#if HW4_debug
		printf(1, "df: lseek successful; starting read\n");
		#endif
		read(fd, tmpb, BSIZE);
		#if HW4_debug
		printf(1, "df: read successful; analyzing blocks\n");
		#endif
		for(int j=0; j<IPB; j++) {
			memmove(&tmpi, tmpb+tmpb_off, sizeof(struct dinode));
			tmpb_off += sizeof(struct dinode);
			// this inode is free; increment
			if(tmpi.type == 0)
				fnodes++;
			if(++nodes_checked==sb.ninodes)
				break;
		}
		if(nodes_checked==sb.ninodes)
			break;
		#if HW4_debug
		printf(1, "df: analysis successful! curr free inodes: %d\n",fnodes);
		#endif
	}

	lseek(fd, 0, SEEK_SET);
	#if HW4_debug
	printf(1, "%s\n", "df: free inodes found");
	#endif

	// print free blocks and free inodes
	printf(1, "Free blocks: %d\b Free Inodes: %d\n", fblocks, fnodes);

	#if HW4_debug
	printf(1, "Closing fd and exiting\n");
	#endif
	// close(fd);
	exit();
	#endif
}