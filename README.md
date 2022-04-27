# LinuxV6FileSystem
This is a group project and 3-member groups have been formed on elearning.
V6 file system is highly restrictive. A modification has been done: Block size is 1024 Bytes, i-node size is
64 Bytes and i-node’s structure and directory entry struc have been modified as well and given below:
typedef struct {
 int isize;
 int fsize;
 int nfree;
 unsigned int free[200];
 char flock;
 char ilock;
 char fmod;
 unsigned int time;
} superblock_type; // Block size is 1024 Bytes; not all bytes of superblock
//are used.
superblock_type superBlock;
// i-node Structure
typedef struct {
unsigned short flags;
unsigned short nlinks;
unsigned int uid;
unsigned int gid;
unsigned int size0;
unsigned int size1;
unsigned int addr[9];
unsigned int actime;
unsigned int modtime;
} inode_type; //64 Bytes in size
typedef struct {
 unsigned int inode;
 char filename[28];
} dir_type;//32 Bytes long
Flags field has a small change: bits 1, b, c are as before. Bits d and e are to represent if the file is
small/medium/long/super long file (00 = small file, 01=medium, 10=long and 11 = super long file). Bit f is
for set uid on execution and bit g is for set gid on execution. Other bits remain the same.
If file is small addr[9] has 9 direct block addresses. If file is medium, addr[9] has addresses of 9 single
indirect blocks. If file is large, each element of addr[] is address of a double indirect block. If file is super
long, each element of addr[] is address of a triple indirect block.
You need to develop a program called mod-v6.c (or mod-v6.cc) that implements the following 2
commands in C/C++:
1. initfs file_name n1 n2
Here, file_name is the name of the file in the native Unix machine (where you are running your
program) that represents the disk drive.
where n1 is the file system size in number of blocks and n2 is the number of blocks devoted to
the i-nodes. In this case, set all data blocks free,except for one data block for storing the
contents of i-node number 1, representing the root, which has the two entries . and .. All i-nodes
except i-node number 1 are (unallocated) set to free. Make sure that all free blocks are
accessible from free[] array of the super block. One of the data blocks contains the root
directory’s contents (two entries . and ..)
2. q
Quit the program
Some useful Unix system calls: lseek(), read(), write(), open()
Build two functions add_free_block(int) and get_free_block(). The first one takes the address of a block
to be added to the free list and returns 1 if all went well and -1 if something went wrong. The second
function returns the address of a free data block (from the free list) if everything is good and returns -1
should there be an error somewhere (say no free blocks left).
This project must be done in C/C++ only.
Check the program I uploaded on elearning and use that as a starting point.
Due date: March 31, 2022 11:59 pm.


# Part 2
You need to implement the following additional commands and add to the project 2-part 1:
Keep in mind that all files in v6 file system that you need to deal with in this project part are in the
root directory and there will be no subdirectories.
(a) cpin externalfile v6-file
Creat a new file called v6-file in the v6 file system and fill the contents of the newly created file
with the contents of the externalfile.
(b) cpout v6-file externalfile
If the v6-file exists, create externalfile and make the externalfile's contents equal to v6-file.
(c) rm v6-file
Delete the file v6_file from the v6 file system.
Remove all the data blocks of the file, free the i-node and remove the directory entry.
(d) q
Save all changes and quit.
For example the following sequence:
initfs mydisk 1000 100
cpin success.c /f1
cpin a.out /f2
cpout /f2 a.out-copy
cpout /f1 success
is possible where f1 and f2 are is created in the directory / of the v6 file system and success.c and
a.out are existing files in the native Unix machine where the modified program mod-v6.c is
developed, compiled and executed.
Due date: April 26, 2022 11:55 pm.
