Below Commands are provided in the filesystem project:
CREATE type name
    This command creates file/directory. For file type = U ; for directory. name should not exceed 9 characters.
    When file is created, it is current opened file in write mode.
	
OPEN mode name
    This command opens the file named:name in one of the 3 modes:
    U-update mode. Places the cursor at beginning of the file. You can read/write/seek the file.
    I-Input mode. Places the cursor at the beginning of the file. You can read/seek the file.
    O-Output mode. Places the cursor at the end of the file. You can write to the file.

CLOSE
    Closes the last opened/created file.

DELETE name
    Deletes file/directory named name. Directory needs to be empty before deletion.

READ n
    File needs to be in I/U mode. Reads n bytes. If file content is file than n bytes, prints message that EOF reached.

WRITE n 'data'
    File needs to be in O/U mode. Writes n bytes to the file. Data to be written is specified between single quotes. If
    length is data is lesser than n bytes, additional characters is filled with space.

WRITE2 nbytes
    Writes nbytes to the file. This command if for testing.

SEEK base offset
    File needs to be in I/U mode. base specifies where the cursor needs to be placed. -1=start of file; 0=current
    position; +1=end of file. offset moves the cursor from base.

PRINT
    Prints the index specified block's content.

EXIT
    Exit the program.

The in-memory file system is backed up on disk in filesytem.dat on exit and restored on restart of the program.

