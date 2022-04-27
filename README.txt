CS 4348 - Project 2 Part 2
Group 18 - Adham Kassem, Dhruv Thoutireddy, Basma Mahamid

README

To compile the mod-v6.c program, run the following command:

	gcc mod-v6.c -o mod-v6

To run the program, run the follwing command:

	./mod-v6

The mod-v6 program has five commands implemented: initfs(Initialize File System), cpin(Copy In), cpout(Copy Out), rm(Remove), and q(Quit).

To run the initfs command, consider the following:
	
	initfs filename n1 n2
	where filename is the name of the file to represent the disk drive, n1 is the file system size in number of blocks, and n2 is the number of blocks devoted to the i-nodes.

To run the cpin command, consider the following:
	
	cpin externalfile internalfile
	where externalfile is the name of the external file to have contents copied, and internalfile is the name of the newly created v6-file with contents of external file.
	The function creates a new file called internalfile in the v6 file system and fill the contents of the newly created file with the contents of the externalfile.

To run the cpout command, consider the following:
	
	cpout sourcefile destinationfile
	where sourcefile is the name of the v6-file to have contents copied, and destinationfile is the name of the newly created file with contents of sourcefile.
	The function creates destinationfile and make the destinationfile's contents equal to sourcefile.

To run the rm command, consider the following:
	
	rm v6-file
	where v6-file is the name of the v6-file to be deleted.
	The function deletes the file v6_file from the v6 file system and removes all the data blocks of the file, frees the i-node and removes the directory entry.

To run the q command, consider the following:

	q
	where q quits the program.