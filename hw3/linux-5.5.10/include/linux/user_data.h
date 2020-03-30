#ifndef __LINUX_USER_DATA_H
#define __LINUX_USER_DATA_H

#include <linux/types.h>

/*
*  Structure for the Phonebook module data.
*/

struct user_data {
	char   *name,    *surname,    *phone,    *email;
	size_t name_len, surname_len, phone_len, email_len;
	long   age;
};

#endif
