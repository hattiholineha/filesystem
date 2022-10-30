#include<iostream>
#include <iterator>
#include <list>
#include <stdio.h>


using namespace std;
#define DIRECTORY_ENTRY_COUNT 25
#define MAX_USER_DATA 504
#define MAX_NAME_LENGTH 9
#define SECTOR_COUNT 100

// TODO: copying mem contents to file and restore file system
struct data_block {
	int back_fixed;
	int front_fixed;
	char user_data[MAX_USER_DATA];
};

struct directory_entry {
	char name[MAX_NAME_LENGTH];
	char type;
	int first_block;
	short last_block_bytes;
	
};

struct directory {
	int back_fixed;
	int front_fixed;
	int free_fixed;
	struct directory_entry de[DIRECTORY_ENTRY_COUNT];
	//char filler[16];
};

union block {
    struct directory dir;
    struct data_block file;
};

struct curr_open_file {
    int fp;
    int idx;
    string name;
    char mode;
    int dir_idx; // index of parent directory with this file's entry. Used for write cmd
    int de_idx; // Index of the file in the parent directory structure. Used for write cmd
};

//100 blocks of 512 bytes
union block blocks[SECTOR_COUNT];

// This structure keeps track of current active file
struct curr_open_file open_file;
// Start allotment of blocks from index 1, since root is 0.
//int next_free_block = 1;

// List of blocks that are free
list<int> free_blocks;

int file_size(int idx, int last_block_bytes) {
    int i = idx;
    int size = 0;
    while (i != -1) {
        if (blocks[i].file.front_fixed != -1) {
            size += MAX_USER_DATA;
        }
        i = blocks[i].file.front_fixed;
    }
    size += last_block_bytes;
    return size;
}

void print(int inp_idx, char type) {
    int count = 0;
    int idx = inp_idx;
    if (type == 'D') {
        cout << "back_fixed: " << blocks[idx].dir.back_fixed << endl;
        cout << "front_fixed: " << blocks[idx].dir.front_fixed << endl;
print_start:
        for (int i = 0; i < DIRECTORY_ENTRY_COUNT; i++) {
            if (blocks[idx].dir.de[i].type != 'F') {
                cout << i << ": type: " << blocks[idx].dir.de[i].type;
                cout << "\t" << i << ": name: " << blocks[idx].dir.de[i].name;
                cout << "\t" << i << ": first block: " << blocks[idx].dir.de[i].first_block;
                cout << "\t" << i << ": last block bytes: " << blocks[idx].dir.de[i].last_block_bytes;
                if (blocks[idx].dir.de[i].type == 'U') {
                    // Calculate size of file
                    int fs = file_size(blocks[idx].dir.de[i].first_block, blocks[idx].dir.de[i].last_block_bytes);
                    cout << "\t" << i << ": file size: " << fs;
                }
                cout << endl;
            } else {
                count++;
            }
        }
        if (blocks[idx].dir.front_fixed != -1) {
            idx = blocks[idx].dir.front_fixed;
            goto print_start;
        }
        cout << "Free blocks count: " << count << endl;
    } else if (type == 'U') {
        int next_idx = idx;
        string data;
        while (next_idx != -1) {
            cout << blocks[next_idx].file.user_data << endl;
            int char_count;
            if (strlen(blocks[next_idx].file.user_data) < MAX_USER_DATA) {
                char_count = strlen(blocks[next_idx].file.user_data);
            } else {
                char_count = MAX_USER_DATA;
            }
            data.append(blocks[next_idx].file.user_data, 0, char_count);
            next_idx = blocks[next_idx].file.front_fixed;
        }
        cout << "Size: " << data.size();
        cout << "Data of file: " << endl;
        cout << data;
    }
    cout << endl;
}

int get_next_free_block() {
    int new_block = -1;
    //if (next_free_block == SECTOR_COUNT) {
      if (free_blocks.size() == 0) {
        cout << "We are out of blocks! Exiting program" << endl;
        exit(0);
    } else {
        //new_block = next_free_block;
        new_block = free_blocks.front();
        cout << "get_next_free_block: " << new_block << endl;
        free_blocks.pop_front();
        //next_free_block++;
    }
    return new_block;
}

void release_block(int idx) {
    list<int>::iterator findIter = std::find(free_blocks.begin(), free_blocks.end(), idx);
    if (findIter != free_blocks.end()) {
        cout << "Element already in free list!" << endl;
    } else {
        free_blocks.push_back(idx);
    }
}

void copy_bckup(struct curr_open_file &dest, struct curr_open_file src) {
    dest.name = src.name;
    dest.fp = src.fp;
    dest.idx = src.idx;
    dest.dir_idx = src.dir_idx;
    dest.de_idx = src.de_idx;
}

list<string> split_string(string name, string delimiter) {
    list<string> split_name;
    size_t pos = 0;
    bool sub_dir = false;

    while ((pos = name.find(delimiter)) != std::string::npos) {
        sub_dir = true;
        string token = name.substr(0, pos);
        split_name.push_back(token);
        name.erase(0, pos + delimiter.length());
    }
    split_name.push_back(name);
    return split_name;
}

int get_parent_idx(list<string> split_path) {
    string dir_name;
    list<string>::iterator it;
    int i = 0, parent_idx = 0;

    //Skipping the directory/file to be created
    int depth = split_path.size() - 1;

    it = split_path.begin();
    while ( depth > 0 && it != split_path.end()) {
        dir_name = *it;

        //Look for the directory name
        int j = 0;
        while(1) {
            if (blocks[i].dir.de[j].type == 'D' || blocks[i].dir.de[j].type == 'd') {
                if(dir_name == blocks[i].dir.de[j].name) {
                    parent_idx = blocks[i].dir.de[j].first_block;
                    break;
                }
            }
            j++;

            //We need to go to next block for this directory
            if (j == DIRECTORY_ENTRY_COUNT) {
                if (blocks[i].dir.front_fixed != -1) {
                    i = blocks[i].dir.front_fixed;
                } else {
                    cout << "Could not find the parent directory: " << dir_name << endl;
                    return -1;
                }
            }
        }

        ++it;
        depth--;
        i = parent_idx;
    }
    return parent_idx;
}

int extend_directory(int idx) {
    struct directory ext;
    int i;
    int new_idx = get_next_free_block();
    blocks[idx].dir.front_fixed = new_idx;
    ext.back_fixed = idx;
    ext.front_fixed = -1;
    for (i = 0; i < DIRECTORY_ENTRY_COUNT; i++) {
        ext.de[i].type = 'F';
    }

    blocks[new_idx].dir = ext;
    return new_idx;
}

bool update_dir_in_parent_directory(int parent_idx, string name, char type, int &link) {
    int i;
start:
    for (i = 0; i < DIRECTORY_ENTRY_COUNT; i++) {
        if (type == 'D' || type == 'd') {
            if((blocks[parent_idx].dir.de[i].type == 'D' ||
                blocks[parent_idx].dir.de[i].type == 'd')) {
                if (blocks[parent_idx].dir.de[i].name == name) {
                    cout << "Directory already exists!\n";
                    return false;
                }
            } else if (blocks[parent_idx].dir.de[i].type == 'F' ||
            blocks[parent_idx].dir.de[i].type == 'f') {
                blocks[parent_idx].dir.de[i].type = 'D';
                strncpy(blocks[parent_idx].dir.de[i].name, name.c_str(), 9);
                link = i;
                return true;
            } else {}
        }
    }

    // Traversed till last entry in the directory but did not find empty space.
    // Go to next block of the directory if it exists.
    if (i == DIRECTORY_ENTRY_COUNT) {
        if (blocks[parent_idx].dir.front_fixed != -1) {
            parent_idx = blocks[parent_idx].dir.front_fixed;
            i = 0;
            goto start;
        } else {
            //Create another directory for parent block since its current block is full
            int ext_parent_idx = extend_directory(parent_idx);
            blocks[ext_parent_idx].dir.de[0].type = 'D';
            strncpy(blocks[ext_parent_idx].dir.de[0].name, name.c_str(), 9);
            link = 0;
            return true;
        }
    }
    return false;
}

bool update_file_in_parent_directory(int &parent_idx, string name, char type, int &link) {
    int i;
    int next_parent_idx = parent_idx;

    // same_file_check becomes false after checking that file with same name does not exist.
    bool same_file_check = true;
    
    // This loop is to check if file with same name exists and if it does, recreate the same file.
start:
    for (i = 0; i < DIRECTORY_ENTRY_COUNT; i++) {
        if (same_file_check == true) {
            if((blocks[next_parent_idx].dir.de[i].type == 'U' ||
                        blocks[next_parent_idx].dir.de[i].type == 'u')) {
                if (blocks[next_parent_idx].dir.de[i].name == name) {
                    cout << "Same file exists. Clearing its data" << endl;
                    link = i;
                    blocks[next_parent_idx].dir.de[i].last_block_bytes = 0;
                    parent_idx = next_parent_idx;

                    //File with same name exists. Releasing existing file's blocks.
                    int file_idx = blocks[next_parent_idx].dir.de[i].first_block;
                    while (file_idx != -1) {
                        release_block(file_idx);
                        file_idx = blocks[file_idx].file.front_fixed;
                    }
                    return true;
                }
            }
        } else if (same_file_check == false &&
                (blocks[next_parent_idx].dir.de[i].type == 'F')) {
            blocks[next_parent_idx].dir.de[i].type = 'U';
            strncpy(blocks[next_parent_idx].dir.de[i].name, name.c_str(), 9);
            link = i;
            blocks[next_parent_idx].dir.de[i].last_block_bytes = 0;
            parent_idx = next_parent_idx;
            return true;
        }
    }

    // Traversed till last entry in the directory but did not find empty space.
    // Go to next block of the directory if it exists.
    if (i == DIRECTORY_ENTRY_COUNT) {
        if (blocks[next_parent_idx].dir.front_fixed != -1) {
            next_parent_idx = blocks[next_parent_idx].dir.front_fixed;
            i = 0;
            goto start;
        } else {
            if (same_file_check == true) {
                // Re iterate the directory to look for free block.
                next_parent_idx = parent_idx;
                same_file_check = false;
                goto start;
            } else {
                // No free entry exists. Create a new block for directory
                // and update the file details
                int ext_parent_idx = extend_directory(next_parent_idx);
                blocks[ext_parent_idx].dir.de[0].type = 'U';
                strncpy(blocks[ext_parent_idx].dir.de[0].name, name.c_str(), 9);
                link = 0;
                parent_idx = ext_parent_idx;
                return true;
            }
        }
    }
    return false;
}

int main()
{
    FILE *restore_file = NULL;

    memset(blocks, 0, sizeof(blocks));
    // Pointers maintained for currently opened 
    open_file.fp = -1;
    open_file.idx = -1;
    open_file.name = "";

    for (int i = 1; i < SECTOR_COUNT; i++) {
        free_blocks.push_back(i);
    }
    // Create root
    blocks[0].dir.back_fixed = blocks[0].dir.front_fixed = -1;
    blocks[0].dir.free_fixed = 1;
    for (int i = 0; i < DIRECTORY_ENTRY_COUNT; i++) {
        blocks[0].dir.de[i].type = 'F';
        blocks[0].dir.de[i].first_block = -1;
        blocks[0].dir.de[i].last_block_bytes = -1;
    }

    restore_file = fopen ("filesystem.dat", "r");
    if (restore_file != NULL) {
        cout << "Back up exists." << endl << "Restoring the file system" << endl;
        int i = 0;
        while(i < SECTOR_COUNT) {
            fread(&blocks[i], sizeof(union block), 1, restore_file);
            i++;
        }
        fclose(restore_file);
    }

	while(1) {
        string input;
		cout << "\t\tCREATE type name\n"
			"\t\tOPEN mode name\n"
			"\t\tCLOSE\n"
			"\t\tDELETE name\n"
			"\t\tREAD n\n"
			"\t\tWRITE n 'data'\n"
			"\t\tWRITE2 nbytes\n"
			"\t\tSEEK base offset\n"
            "\t\tPRINT\n"
			"\t\tEXIT\n";
        getline(cin, input);

        int pos = input.find(" ");
        string cmd = input.substr(0, pos);
        cout << "Command is: " << cmd << endl;
        if (cmd == "CREATE") {
            //Increment pos to get type of block to be created
            pos++;
            char type = input[pos];
            if (type != 'D' && type != 'U') {
                cout << "Invalid type: " << type << " .Try again" << endl;
                continue;
            }

            //Check if previous file is closed. If not, throw error
            if (type == 'U' && open_file.fp != -1) {
                cout << "Please close file: " << open_file.name << " and try again." << endl;
                continue;
            }

            //Next character in the command is space
            pos++;
            if (input[pos] != ' ') {
                cout << "Invalid input. Try again." << endl;
                continue;
            }

            // Now, fetch the input path name
            pos++;
            string input_name = input.substr(pos);

            // directory name must be specified
            if (input_name.empty()) {
                cout << "Invalid. Input not specified. Try again." << endl;
                continue;
            }

            // Split the pathname
            list<string> split_name = split_string(input_name, "/");
            int size = split_name.size();
            int parent_idx = 0;

            // Check if the length of file/directory to be created is not greater than max 
            string name = split_name.back();
            cout << "Name of file/directory to be created: " << name << endl;
            if (name.size() > MAX_NAME_LENGTH) {
                cout << "Name cannot exceed character count: " << MAX_NAME_LENGTH << endl;
                cout << "Try again" << endl;
                continue;
            }

            // Traverse the parent directory path.
            if(size > 1) {
                parent_idx = get_parent_idx(split_name); 
                if (parent_idx == -1) {
                    cout << "Invalid path! Try again" << endl;
                    continue;
                }
            }

            // Create the directory block
            if (type == 'D' || type == 'd') {


                // Get the directory name to be created
                // update the directory details in parent directory structure
                int link = 0;
                bool ret = update_dir_in_parent_directory(parent_idx, name, type, link);
                int new_block = get_next_free_block();

                if (ret) {
                    blocks[new_block].dir.back_fixed = blocks[new_block].dir.front_fixed = -1;
                    for(int i = 0; i < DIRECTORY_ENTRY_COUNT; i ++) {
                        blocks[new_block].dir.de[i].type = 'F';
                    }
                    blocks[parent_idx].dir.de[link].first_block = new_block;
                    cout << "Successfully created directory idx:" << new_block << " Parent dir: " << parent_idx << endl;;
                    continue;
                }

            } else if (type == 'U' || type == 'u') {
                cout << "It is file creation command" << endl;

                // Get the directory name to be created
                // update the directory details in parent directory structure
                int link = 0;
                bool ret = update_file_in_parent_directory(parent_idx, name, type, link);

                if (ret) {
                    int new_block = get_next_free_block();
                    blocks[new_block].file.back_fixed = blocks[new_block].file.front_fixed = -1;
                    blocks[parent_idx].dir.de[link].first_block = new_block;
                    memset(blocks[new_block].file.user_data, '\0', sizeof(blocks[new_block].file.user_data));

                    // Get a new block for file
                    open_file.fp = 0;
                    open_file.idx = new_block;
                    open_file.name = name;
                    open_file.mode = 'O';
                    open_file.dir_idx = parent_idx;
                    open_file.de_idx = link;
                    cout << "Successfully created file at idx: " << new_block << endl;
                    continue;
                }
            } else {
                cout << "Invalid input. Try again." << endl;
                continue;
            }
        } else if (cmd == "OPEN") {
            if (open_file.fp != -1) {
                cout << "Please close file: " << open_file.name << " and try again." << endl;
                continue;
            } else {
                struct curr_open_file bckup;
                copy_bckup(bckup, open_file);

                pos++;
                char mode = input[pos];
                if (mode == 'I' || mode == 'U') {
                    open_file.fp = 0;
                } else if (mode == 'O') {
                    // Need to set the pointer at end
                } else {
                    cout << "Invalid mode. Try again." << endl;
                    continue;
                }

                //Next character in the command is space
                pos++;
                if (input[pos] != ' ') {
                    cout << "Invalid input. Try again." << endl;
                    continue;
                }

                // Now, fetch the input path name
                pos++;
                string input_name = input.substr(pos);

                // directory name must be specified
                if (input_name.empty()) {
                    cout << "Invalid. Input not specified. Try again." << endl;
                    continue;
                }

                // Split the pathname
                list<string> split_name = split_string(input_name, "/");
                int size = split_name.size();
                int parent_idx = 0;

                // Check if the length of file/directory to be created is not greater than max 
                string name = split_name.back();
                if (name.size() > MAX_NAME_LENGTH) {
                    cout << "Invalid filename: " << name << endl;
                    cout << "Try again" << endl;
                    continue;
                }

                // Traverse the parent directory path.
                if(size > 1) {
                    parent_idx = get_parent_idx(split_name); 
                    if (parent_idx == -1) {
                        cout << "Invalid path! Try again" << endl;
                        continue;
                    }
                }

                // Get the file's index
                int next_idx = parent_idx;
                bool found = false;
                int i;
                while (found == false) {
start:
                    for (i = 0; i < DIRECTORY_ENTRY_COUNT; i++) {
                        if (blocks[next_idx].dir.de[i].name == name) {
                            found = true;
                            open_file.dir_idx = next_idx;
                            open_file.de_idx = i;
                            open_file.idx = blocks[next_idx].dir.de[i].first_block;
                            open_file.name = name;
                            open_file.mode = mode;
                            if (mode == 'O') {

                                while (blocks[open_file.idx].file.front_fixed != -1) {
                                    open_file.idx = blocks[open_file.idx].file.front_fixed;
                                }
                                open_file.fp = strlen(blocks[open_file.idx].file.user_data);
                                cout << "For O mode, cursor is at: " << open_file.fp << endl;
                            }
                            break;
                        }
                    }
                    if (i == DIRECTORY_ENTRY_COUNT && found == false) {
                        if (blocks[next_idx].dir.front_fixed != -1) {
                            next_idx = blocks[next_idx].dir.front_fixed;
                            goto start;
                        } else {
                            cout << "File does not exist in given path" << endl;
                            copy_bckup(open_file,bckup);
                            break;
                        }
                    }
                }
                if (found == true) {
                    cout << "File: " << open_file.name << " opened in mode: " << open_file.mode << " Cursor at index: "
                    << open_file.fp << endl;
                } else {
                    // Reset the open file pointers
                    open_file.fp = -1;
                    open_file.idx = -1;
                    open_file.name = "";
                    open_file.dir_idx = -1;
                    open_file.de_idx = -1;
                }
                continue;
            }
        } else if (cmd == "CLOSE") {
            // Reset the open file pointers
            open_file.fp = -1;
            open_file.idx = -1;
            open_file.name = "";
        } else if (cmd == "DELETE") {
            // Free the file blocks and delete directory content
            // Now, fetch the input path name
            pos++;
            string input_name = input.substr(pos);

            // directory name must be specified
            if (input_name.empty()) {
                cout << "Invalid. Input not specified. Try again." << endl;
                continue;
            }

            // Split the pathname
            list<string> split_name = split_string(input_name, "/");
            int size = split_name.size();
            int parent_idx = 0;

            // Check if the length of file/directory to be created is not greater than max 
            string name = split_name.back();

            // Traverse the parent directory path.
            if(size > 1) {
                parent_idx = get_parent_idx(split_name); 
                if (parent_idx == -1) {
                    cout << "Invalid path! Try again" << endl;
                    continue;
                }
            }

            // Get the file's index
            int next_idx = parent_idx;
            bool found = false;
            int i;
            while (found == false) {
start_del:
                for (i = 0; i < DIRECTORY_ENTRY_COUNT; i++) {
                    if (blocks[next_idx].dir.de[i].name == name) {
                        found = true;
                        break;
                    }
                }
                if (i == DIRECTORY_ENTRY_COUNT && found == false) {
                    if (blocks[next_idx].dir.front_fixed != -1) {
                        next_idx = blocks[next_idx].dir.front_fixed;
                        goto start_del;
                    } else {
                        cout << "File does not exist in given path" << endl;
                        break;
                    }
                }
            }
            if (found == true) {
                cout << "Entity to be deleted: " << blocks[next_idx].dir.de[i].name << endl;
                if(open_file.idx == blocks[next_idx].dir.de[i].first_block) {
                    // Reset the open file pointers
                    cout << "Deleting opened file. Resetting open file pointer." << endl;
                    open_file.fp = -1;
                    open_file.idx = -1;
                    open_file.name.assign("");
                    open_file.dir_idx = -1;
                    open_file.de_idx = -1;
                }

                int del_idx = blocks[next_idx].dir.de[i].first_block;
                if (blocks[next_idx].dir.de[i].type == 'U') {
                    while(del_idx != -1) {
                        release_block(del_idx);
                        del_idx = blocks[del_idx].file.front_fixed;
                    }
                } else {
                    while(del_idx != -1) {
                        release_block(del_idx);
                        del_idx = blocks[del_idx].dir.front_fixed;
                    }
                }
                memset(blocks[next_idx].dir.de[i].name, '\0', sizeof(char) * MAX_NAME_LENGTH);
                blocks[next_idx].dir.de[i].first_block = -1;
                blocks[next_idx].dir.de[i].last_block_bytes = -1;
                blocks[next_idx].dir.de[i].type = 'F';
            } else {
                cout << "File does not exist in given path" << endl;
            }

        } else if (cmd == "READ") {
            if (open_file.fp == -1) {
                cout << "No file opened. Open the file in I/U mode to read" << endl;
                continue;
            }
            if (open_file.mode == 'O') {
                cout << "File in output mode. You can only write to the file" << endl;
                continue;
            }
            if (input[pos] != ' ') {
                cout << "Invalid input. Try again." << endl;
                continue;
            }
            pos++;

            // get number of bytes to be read
            string no_of_bytes = input.substr(pos);
            int num = stoi(no_of_bytes);
            int num_of_bytes = num;

            // Get the string from block where fp is pointing to.


            string str_read;
            int next_idx = open_file.idx;
            int fp = open_file.fp;
            while (num > 0) {
                int len = strlen(blocks[next_idx].file.user_data);
                //Taking care since buffer is not appended with NULL
                if (len > MAX_USER_DATA) {
                    len = MAX_USER_DATA;
                }

                if (len - fp >= num) {
                    // Just read num bytes and go
                    str_read.append(blocks[next_idx].file.user_data, fp, num);
                    break;
                } else {
                    str_read.append(blocks[next_idx].file.user_data, fp, len);
                    num = num - len;
                    // There is more data to be read from front blocks

                    if (blocks[next_idx].file.front_fixed != -1) {
                        next_idx = blocks[next_idx].file.front_fixed;
                        cout << "===reading next block== " << next_idx << endl;
                        fp = 0;
                        continue;
                    } else {
                        // EOF reached.
                        break;
                    }
                }
            }
            cout << "\n=>Bytes read: " << str_read.size() <<"\nContents of file: " << endl;
            cout << str_read << endl;
            if (str_read.size() < num_of_bytes) {
                cout << "\n====End of file reached.====" << endl;
            }
            continue;
        } else if (cmd == "WRITE" || cmd == "WRITE2") {
            if (open_file.fp == -1) {
                cout << "No file opened. Open the file in I/U mode to read" << endl;
                continue;
            }

            if (open_file.mode == 'I') {
                cout << "File in output mode. You can only write to the file" << endl;
                continue;
            }

            if (input[pos] != ' ') {
                cout << "Invalid input. Try again." << endl;
                continue;
            }

            pos++;
            string str1 = input.substr(pos);
            int nbytes = stoi(str1);

            string final_data;
            if (cmd == "WRITE2") {
                // Write random n bytes
                final_data.assign(nbytes, 'A');
            } else {
                pos = str1.find(" ");
                pos++;
                string str2 = str1.substr(pos);
                final_data.assign(str2.substr(1, str2.size() - 2));
                if (final_data.size() < nbytes) {
                    int diff = nbytes - final_data.size();
                    cout << "Appending " << diff << " spaces due to difference in input byte and length of string" << endl;
                    string s;
                    s.assign(diff, ' ');
                    final_data.append(s);
                }
            }
            // got to write final_data in file
            int size_available = MAX_USER_DATA - open_file.fp; 
            if (size_available >= final_data.size()) {
                strncat(blocks[open_file.idx].file.user_data, final_data.c_str(), final_data.size());


                open_file.fp = strlen(blocks[open_file.idx].file.user_data);
            } else {
                // Copy how much space is available and create new file block.
                // Assuming we are not writing more than 504 characters
                int remaining = final_data.size();
                if (size_available == MAX_USER_DATA) {
                    strncpy(blocks[open_file.idx].file.user_data, final_data.c_str(), size_available);
                } else {
                    strncat(blocks[open_file.idx].file.user_data, final_data.c_str(), size_available);
                }
                //blocks[new_block].file.user_data[size_available] = '\0';

                remaining -= size_available;
                final_data.assign(final_data.substr(size_available));

                while (remaining > 0) {
                    // Get a new block for file
                    int new_block = get_next_free_block();
                    blocks[new_block].file.back_fixed = open_file.idx;
                    blocks[new_block].file.front_fixed = -1;
                    blocks[open_file.idx].file.front_fixed = new_block;

                    open_file.idx = new_block;
                    memset(blocks[open_file.idx].file.user_data, '\0', MAX_USER_DATA);

                    int size;
                    if (MAX_USER_DATA <= remaining) {
                        size = MAX_USER_DATA;
                    } else {
                        size = remaining;
                    }
                    strncpy(blocks[new_block].file.user_data, final_data.c_str(), size);
                    //blocks[new_block].file.user_data[size] = '\0';

                    remaining -= strlen(blocks[new_block].file.user_data);
                    final_data.assign(final_data.substr(strlen(blocks[new_block].file.user_data)));
                }
                // After writing all data, keep cursor at the end
                open_file.fp = strlen(blocks[open_file.idx].file.user_data);

                // Update last block size in directory
                cout << "last_block_bytes: " << open_file.fp << endl;
            }
            blocks[open_file.dir_idx].dir.de[open_file.de_idx].last_block_bytes = open_file.fp;
        } else if (cmd == "SEEK") {
            if (open_file.fp == -1) {
                cout << "No file opened. Open the file in I/U mode to read" << endl;
                continue;
            }

            if (open_file.mode == 'O') {
                cout << "File in output mode. You can only write to the file" << endl;
                continue;
            }

            if (input[pos] != ' ') {
                cout << "Invalid input. Try again." << endl;
                continue;
            }

            // Store the open_file values to revert if there is error

            pos++;
            string s_base = input.substr(pos);
            int base = stoi(s_base);
            pos = s_base.find(" ");
            pos++;
            string s_offset = s_base.substr(pos);
            int offset = stoi(s_offset);

            int block_idx = open_file.idx;;
            if (base == -1) {
                // Beginning of the file
                open_file.fp = 0;
                while (blocks[open_file.idx].file.back_fixed != -1) {
                    open_file.idx = blocks[open_file.idx].file.back_fixed;
                }
            } else if (base == 0) {
                // No update on base.
            } else if (base == 1) {
                // traverse till end of the file
                while(blocks[block_idx].file.front_fixed != -1) {
                    block_idx = blocks[block_idx].file.front_fixed;
                }
                open_file.fp = strlen(blocks[block_idx].file.user_data);
            } else {
                cout << "Invalid base: " << base << endl;
                continue;
            }

            //Update the base
            open_file.fp += offset;
            if (open_file.fp > strlen(blocks[block_idx].file.user_data)) {
                // Pointer is exceeding the last character index. So point at last
                if (blocks[open_file.idx].file.front_fixed == -1) {
                    cout << "offset exceeding EOF. Pointing at end." << endl;
                    open_file.fp = strlen(blocks[block_idx].file.user_data) - 1;
                } else {
                    while(blocks[open_file.idx].file.front_fixed != -1) {
                        open_file.idx = blocks[open_file.idx].file.front_fixed;
                        open_file.fp -= MAX_USER_DATA;
                        if (open_file.fp <= strlen(blocks[block_idx].file.user_data)) {
                            break;
                        } else {
                            if (blocks[open_file.idx].file.front_fixed == -1) {
                                cout << "offset exceeding EOF. Pointing at end." << endl;
                                open_file.fp = strlen(blocks[block_idx].file.user_data) - 1;
                                break;
                            } else {
                                continue;
                            }
                        }
                    }
                }
            } else if (open_file.fp < 0) {
                // Need to traverse backwards of the file
                if (blocks[block_idx].file.back_fixed != -1) {
                    while (open_file.fp < 0) {
                        if (blocks[block_idx].file.back_fixed != -1) {
                            block_idx = blocks[block_idx].file.back_fixed;
                            open_file.fp = open_file.fp + MAX_USER_DATA;
                            open_file.idx = block_idx;
                            continue;
                        } else {
                            cout << "Reached first block. Offset lesser than start of file."
                            "So pointing at first byte of the file." << endl; 
                            open_file.fp = 0;
                            open_file.idx = block_idx;
                            break;
                        }
                    }
                } else {
                    // There is no back block. So pointing at first byte of the file block
                    cout << "Offset lesser than start of file. So pointing at first byte of the file." << endl; 
                    open_file.fp = 0;
                    open_file.idx = block_idx;
                }
            }
            continue;
        } else if (cmd == "PRINT") {
            int idx;
            char type;
            cout << "Give index to be printed" << endl;
            cin >> idx;
            cout << "Tell the type of block U/D" << endl;
            cin >> type;
            print(idx, type);
            cin.clear();
            cin.ignore();
            continue;
        } else if (cmd == "EXIT") {
            break;
        } else {
            cout << "Invalid input. Try again!" << endl;
        }
    }

    cout << "BAcking up file system..." << endl;
    restore_file = fopen("filesystem.dat", "w");
    int j = 0;
    while (j < SECTOR_COUNT) {
        fwrite(&blocks[j], sizeof(union block), 1, restore_file);
        j++;
    }

}
