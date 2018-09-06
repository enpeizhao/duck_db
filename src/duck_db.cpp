// a journey of a thousand miles begins with a single step
// author: enpeizhao
// blog: www.enpeizhao.com

#include "../include/bpt.h"
#include "../include/TextTable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>	
#include <iostream>
#include <fstream>

using namespace bpt;
using namespace std;


const char *errorMessage = "> your input is invalid,print \".help\" for more infomation!\n";
const char *nextLineHeader ="> ";
const char *exitMessage = "> bye!\n";
const char *dbFileName = "./data/db.bin";


clock_t startTime,finishTime;   

// function prototype
void printHelpMess();
void selectCommand();
int insertRecord(bplus_tree *treePtr,int *key, value_t *values);
int deleteRecord(bplus_tree *treePtr,int *index);
int searchRecord(bplus_tree *treePtr,int *index, value_t *return_val);
int searchAll(bplus_tree *treePtr,int *i_start, int *i_end);
int updateRecord(bplus_tree *treePtr,int *key, value_t *values);
void printTable(int *index, value_t *values);
void intToKeyT(bpt::key_t *a,int *b);
bool is_file_exist(const char *fileName);
double durationTime(clock_t *f,clock_t *s);


bplus_tree *duck_db_ptr;

// initial
void initialSystem(){
	// step 1 : print help message
	printHelpMess();
	// step 2 : initial database from file
	bplus_tree duck_db(dbFileName, (!is_file_exist(dbFileName)));
	duck_db_ptr = &duck_db;
	// step 3 : REPL select commdand (insert,delete,update,search)
	selectCommand();
	
}
// print help message
void printHelpMess(){
	cout << "*********************************************************************************************"<<endl<<endl
		<<" 				Welcome to the duck_db\n 				db file locates in \"./data/db.bin\" \n 				Author: enpei\n 				www.enpeizhao.com\n 				2018-08-31"<<endl<<endl
		<<"*********************************************************************************************"<<endl
		<<"  .help 							print help message;"<<endl
		<<"  .exit 							exit program;"<<endl
		<<"  .reset 							delete db file;"<<endl
		<<"  insert db {index} {name} {age} {email}; 			insert record;"<<endl
		<<"  delete from db where id ={index}; 				delete record;"<<endl
		<<"  update db {name} {age} {email} where id={index}; 		update a record;"<<endl
		<<"  select * from db where id={index}; 				search a record by index;"<<endl
		<<"  select * from db where id in({minIndex},{maxIndex}); 		search records between indexs;"<<endl
		<<"*********************************************************************************************"<<endl
		<<endl << nextLineHeader;
}

// select command
void selectCommand(){

	// REPL
	char *userCommand = new char[256];
	
	while(true){

		cin.getline(userCommand,256);
		
		if(strcmp(userCommand,".exit") == 0){

	    	cout << exitMessage;
			break;

	    }else if(strcmp(userCommand,".help") == 0){

	    	printHelpMess();

	    }else if(strcmp(userCommand,".reset") == 0){
	    	if( remove( dbFileName) != 0 )
	    		cout<< "can't delete file"<< nextLineHeader;
	    	else
				cout << "DB file has been deleted!"<< endl << endl;

	    	initialSystem();

	    }else if(strncmp(userCommand,"insert",6) == 0){

	    	int *keyIndex = new int;
	    	value_t *insertData = new value_t;

	    	int okNum = sscanf(userCommand,"insert db %d %s %d %s;", 
	    		keyIndex, insertData->name,&(insertData->age),insertData->email);

			if(okNum < 3){
				
				cout << errorMessage<< nextLineHeader;

			}else{

        		startTime = clock(); 

				int return_code = insertRecord(duck_db_ptr,keyIndex,insertData);

				finishTime = clock(); 

				if (return_code == 0){
					// cout << ">insert\n";
					cout << "> executed insert index:"<<   *keyIndex << ", time : "<< durationTime(&finishTime,&startTime) <<" seconds\n"<< nextLineHeader;
				}else if(return_code == 1){
					cout << "> failed: already exist index:"<<   *keyIndex << "\n"<< nextLineHeader;
				}else{
					cout << "> failed!\n"<< nextLineHeader;
				}
			}


	    }else if(strncmp(userCommand,"delete",6) == 0){

	    	int *keyIndex = new int;

	    	int okNum = sscanf(userCommand,"delete from db where id=%d;", keyIndex);

			if(okNum < 1){
				cout << errorMessage<< nextLineHeader;
			}else{
				startTime = clock(); 

				int return_code = deleteRecord(duck_db_ptr,keyIndex);

				finishTime = clock(); 

				if (return_code == 0){
					cout << "> executed delete index:"<<   *keyIndex << ", time : "<< durationTime(&finishTime,&startTime) <<" seconds\n"<< nextLineHeader;
				}else if(return_code == -1){
					cout << "> failed ! no index:"<<   *keyIndex << "\n"<< nextLineHeader;
				}else{
					cout << "> failed!\n"<< nextLineHeader;
				}
			}


	    }else if(strncmp(userCommand,"select",6) == 0){

	    	if( ! strstr (userCommand,"=")){

	    		int i_start,i_end;

	    		int okNum = sscanf(userCommand,"select * from db where id in(%d,%d);", &i_start,&i_end);

				if(okNum < 2){
					cout << errorMessage<< nextLineHeader;
				}else{
					startTime = clock(); 

					searchAll(duck_db_ptr,&i_start, &i_end);

					finishTime = clock(); 
					cout << "> executed search, time : "<< durationTime(&finishTime,&startTime) <<" seconds\n"<< nextLineHeader;
				}


	    	}else{

		    	int *keyIndex = new int;
		    	int okNum = sscanf(userCommand,"select * from db where id=%d;", keyIndex);

				if(okNum < 1){
					cout << errorMessage<< nextLineHeader;
				}else{

					value_t *return_val = new value_t;
					startTime = clock(); 

					int return_code = searchRecord(duck_db_ptr,keyIndex,return_val);

					finishTime = clock(); 
	    	
					if (return_code != 0){
						cout << "> index:"<< *keyIndex << " doesn't exist, time : "<< durationTime(&finishTime,&startTime) <<" seconds\n"<< nextLineHeader;
					}else{
						printTable( keyIndex , return_val);
						cout << "> executed search, time : "<< durationTime(&finishTime,&startTime) <<" seconds\n"<< nextLineHeader;
						
					}
				}
			}


	    }else if(strncmp(userCommand,"update",6) == 0){

	    	int *keyIndex = new int;
	    	value_t *updateData = new value_t;

	    	int okNum = sscanf(userCommand,"update db %s %d %s where id=%d;", 
	    		 updateData->name,&(updateData->age),updateData->email,keyIndex);

			if(okNum < 3){
				cout << errorMessage<< nextLineHeader;
			}else{
				startTime = clock(); 

				int return_code = updateRecord(duck_db_ptr,keyIndex,updateData);

				finishTime = clock(); 

				if (return_code == 0){
					cout << "> executed update index:"<<   *keyIndex << ", time : "<< durationTime(&finishTime,&startTime) <<" seconds\n"<< nextLineHeader;
				}else{
					cout << "> failed! no index:"<<   *keyIndex << ", time : "<< durationTime(&finishTime,&startTime) <<" seconds\n"<< nextLineHeader;
				}
			}

	    }
	    else{
	    	cout << errorMessage<< nextLineHeader;
	    }
	}



}

// insert
int insertRecord(bplus_tree *treePtr,int *index, value_t *values){
	
	bpt::key_t key;
	intToKeyT(&key,index);
	return (*treePtr).insert(key, *values);
}

// delete 
int deleteRecord(bplus_tree *treePtr,int *index){
	
	bpt::key_t key;
	intToKeyT(&key,index);

    return (*treePtr).remove(key);
}

// update
int updateRecord(bplus_tree *treePtr,int *index, value_t *values){
	bpt::key_t key;
	intToKeyT(&key,index);
	return (*treePtr).update(key, *values);
}

// search by index
int searchRecord(bplus_tree *treePtr,int *index, value_t *return_val){
	bpt::key_t key;
	intToKeyT(&key,index);
	return (*treePtr).search(key, return_val); 
}
// search all
int searchAll(bplus_tree *treePtr,int *start, int *end){

	TextTable t( '-', '|', '+' );

    t.add( " id " );
    t.add( " name " );
    t.add( " age " );
    t.add( " email " );
    t.endOfRow();

	bpt::key_t key;
	value_t *return_val = new value_t;

	for (int i = *start; i <= *end; ++i)
	{
		
		intToKeyT(&key,&i);
		
		int return_code = (*treePtr).search(key, return_val);
		switch(return_code){
			case -1:
			// no exist
				break;
			case 0:
			// find
				t.add( to_string(i) );
			    t.add( return_val ->name );
			    t.add( to_string(return_val ->age));
			    t.add( return_val ->email );
			    t.endOfRow();
				break;
			case 1:
			// deleted
				break;
		}
		

	}
	cout << t << endl;
	
}
// print table
void printTable(int *index, value_t *values){


	TextTable t( '-', '|', '+' );

    t.add( " id " );
    t.add( " name " );
    t.add( " age " );
    t.add( " email " );
    t.endOfRow();

    t.add( to_string(*index) );
    t.add( values ->name );
    t.add( to_string(values ->age));
    t.add( values ->email );
    t.endOfRow();

    cout << t << endl;
}
// int to key_t
void intToKeyT(bpt::key_t *a,int *b){
	char key[16] = { 0 };
	sprintf(key, "%d", *b);
	*a = key;
}

bool is_file_exist(const char *fileName)
{
    ifstream ifile(fileName);
  	return ifile.good();
}

double durationTime(clock_t *f,clock_t *s){
	return (double)(*f - *s) / CLOCKS_PER_SEC;	
}


int main(int argc, char *argv[])
{
	initialSystem();

}
