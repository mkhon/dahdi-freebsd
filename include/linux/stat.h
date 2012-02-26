#ifndef _LINUX_STAT_H_
#define _LINUX_STAT_H_

#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

#define S_IRUGO         (S_IRUSR|S_IRGRP|S_IROTH)

#endif /* _LINUX_STAT_H_ */
