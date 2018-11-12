#ifndef UTIL_H
#define UTIL_H

#include <string>

typedef volatile int atomic_t;

ssize_t writen(int fd, const void* vptr, size_t n);
ssize_t readn(int fd, void* vptr, size_t n);
int get_file_content(const char* filepath, char** content);
double get_current_sec();

int atomic_inc(atomic_t *v);
int atomic_dec(atomic_t *v);
int is_url(std::string &str);
void trim(std::string &str);
bool erase_angle_quotes(std::string &str);
double calc_cosine(double s1, short *freq1, double s2, short *freq2);
double calc_square_sum(char *p, int n, short *freq);

#endif
