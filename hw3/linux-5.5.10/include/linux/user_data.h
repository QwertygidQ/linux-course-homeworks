#ifndef __LINUX_USER_DATA_H
#define __LINUX_USER_DATA_H

/*
*  Structure for the Phonebook module data.
*/

struct user_data {
	const char *name, *surname, *phone, *email, *to_split;
	long       age;
	int        successfully_created;
};

#endif
