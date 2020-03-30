#ifndef __LINUX_USER_DATA_H
#define __LINUX_USER_DATA_H

/*
*  Structure for the Phonebook module data.
*/

struct user_data {
	char *name, *surname, *phone, *email;
	long       age;
};

#endif
