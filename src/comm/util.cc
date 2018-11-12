#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <iostream>
#include <sys/time.h>
#include <math.h>

#include "util.h"

using namespace std;

ssize_t writen(int fd, const void* vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char* ptr = (const char*)vptr;

    nleft = n;

    while (nleft > 0)
    {
        if ((nwritten = write(fd, ptr, nleft)) <= 0)
        {
            if (errno == EINTR)
            {
                nwritten = 0;
            }
            else
            {
                return -1;
            }
        }

        nleft -= nwritten;
        ptr += nwritten;
    }

    return n;
}

ssize_t readn(int fd, void* vptr, size_t n)
{
    ssize_t nleft;
    ssize_t nread;
    char *ptr = (char*)vptr;

    nleft = n;
    while (nleft > 0)
    {
        if ((nread = read (fd, ptr, nleft)) < 0)
        {
            if (errno == EINTR)
            {
                nread = 0;
            }
            else
            {
                return -1;
            }
        }
        else if (nread == 0)
        {
            break;
        }

        nleft -= nread;
        ptr += nread;
    }

    return (n - nleft);
}

int get_file_content(const char* filepath, char** content)
{
    int ret = -1;
    int fd;
    char *buf = NULL;
    struct stat st;

    fd = open(filepath, O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }

    if (fstat(fd, &st) != 0) 
    {
        goto ERROR;
    }

    if (st.st_size <= 0)
    {
        goto ERROR;
    }

    buf = (char*)calloc(1, st.st_size + 1);
    if (buf == NULL)
    {
        goto ERROR;
    }

    ret = readn(fd, buf, st.st_size);
    if (ret != st.st_size)
    {
        free(buf);
        ret = -1;
        goto ERROR;
    }

    *content = buf;

ERROR:
    close (fd);

    return ret;
}

double get_current_sec()
{
    struct timeval tv; 
    gettimeofday( &tv, NULL );

    return tv.tv_sec + (double)tv.tv_usec/1000000.0f;
}

int atomic_inc(atomic_t *v) 
{
    return __sync_add_and_fetch((int *)v, 1);
}

int atomic_dec(atomic_t *v) 
{
    return __sync_sub_and_fetch((int *)v, 1);
}

static int check_host(string &host) 
{
    boost::to_lower(host);
    static string top_domain_ending[] = 
        { ".com", ".net", ".org", ".edu", ".gov", ".biz", ".info", ".name", 
          ".mobi", ".mil", ".me", ".ad", ".ae", ".af", ".ag", ".ai", ".al", 
          ".am", ".an", ".ao", ".aq", ".ar", ".as", ".at", ".au", ".aw", 
          ".az", ".ba", ".bb", ".bd", ".be", ".bf", ".bg", ".bh", ".bi", 
          ".bj", ".bm", ".bn", ".bo", ".br", ".bs", ".bt", ".bv", ".bw", 
          ".by", ".bz", ".ca", ".cc", ".cf", ".cg", ".ch", ".ci", ".ck", 
          ".cl", ".cm", ".cn", ".co", ".cq", ".cr", ".cu", ".cv", ".cx", 
          ".cy", ".cz", ".de", ".dj", ".dk", ".dm", ".do", ".dz", ".ec",
          ".ee", ".eg", ".eh", ".es", ".et", ".ev", ".fi", ".fj", ".fk", 
          ".fm", ".fo", ".fr", ".ga", ".gb", ".gd", ".ge", ".gf", ".gh", 
          ".gi", ".gl", ".gm", ".gn", ".gp", ".gr", ".gt", ".gu", ".gw", 
          ".gy", ".hk", ".hm", ".hn", ".hr", ".ht", ".hu", ".id", ".ie", 
          ".il", ".in", ".io", ".iq", ".ir", ".is", ".it", ".jm", ".jo", 
          ".jp", ".ke", ".kg", ".kh", ".ki", ".km", ".kn", ".kp", ".kr", 
          ".kw", ".ky", ".kz", ".la", ".lb", ".lc", ".li", ".lk", ".lr", 
          ".ls", ".lt", ".lu", ".lv", ".ly", ".ma", ".mc", ".md", ".mg", 
          ".mh", ".ml", ".mm", ".mn", ".mo", ".mp", ".mq", ".ms", ".mt", 
          ".mv", ".mw", ".mx", ".my", ".mz", ".na", ".nc", ".ne", ".nf", 
          ".ng", ".ni", ".nl", ".no", ".np", ".nr", ".nt", ".nu", ".nz",
          ".om", ".qa", ".pa", ".pe", ".pf", ".pg", ".ph", ".pk", ".pl", 
          ".pm", ".pn", ".pr", ".pt", ".pw", ".py", ".re", ".ro", ".ru", 
          ".rw", ".sa", ".sb", ".sc", ".sd", ".se", ".sg", ".sh", ".si", 
          ".sj", ".sk", ".sl", ".sm", ".sn", ".so", ".sr", ".st", ".su", 
          ".sy", ".sz", ".tc", ".td", ".tf", ".tg", ".th", ".tj", ".tk", 
          ".tm", ".tn", ".to", ".tp", ".tr", ".tt", ".tv", ".tw", ".tz", 
          ".ua", ".ug", ".uk", ".us", ".uy", ".va", ".vc", ".ve", ".vg", 
          ".vn", ".vu", ".wf", ".ws", ".ye", ".yu", ".za", ".zm", ".zr", 
          ".zw" };
    boost::replace_all(host, ",", ".");
    boost::replace_all(host, "。", ".");
    boost::replace_all(host, "，", ".");
    boost::replace_all(host, "，", ".");
    boost::replace_all(host, "；", ".");
    boost::replace_all(host, "..", ".");
    if (host.length() < 3)
    {
        return 0;
    }

    if (host[host.length() - 1] == '.')
    {
        host.resize(host.length() -1);
    }

    size_t tmp_pos = host.rfind(".comcn");
    if (tmp_pos != string::npos && tmp_pos + 6 == host.size()) 
    {
        host = host.substr(0, tmp_pos + 4) + ".cn";
    }

    boost::replace_all(host, " ", "");

    for (size_t i = 0; i < host.length(); ++i) 
    {
        if (host[i] != '.' && host[i] != '-' 
            && host[i] != '_' && (!isdigit(host[i])) && (!isalpha(host[i])))
        {
            return 0;
        }
    }

    for (size_t i = 0; 
        i < sizeof(top_domain_ending) / sizeof(top_domain_ending[0]); 
        ++i) 
    {
        size_t pos = host.rfind(top_domain_ending[i]);
        if (pos != string::npos && pos > 0 
            && pos + top_domain_ending[i].size() == host.size()) 
        {
            int doc_pos = 0;
            if (host.at(0) == '.')
            {
                doc_pos = 1;
            }

            if (host.find(".", doc_pos) == pos) 
            {
                return 2 + doc_pos;
            }

            return 1;
        }
    }

    int dot_num = 0, last_dot = -1, ip_num = 0;
    for (size_t i = 0; i < host.size(); ++i) 
    {
        if (host.at(i) == '.') 
        {
            if (i == static_cast<size_t>(last_dot + 1))
            {
                return 0;
            }

            if (ip_num > 255)
            {
                return 0;
            }
            ++dot_num;
            ip_num = 0;
            last_dot = i;
        } 
        else if (isdigit(host.at(i)))
        {
            ip_num = ip_num * 10 + (host.at(i) - '0');
        } 
        else 
        {
            return 0;
        }
    }

    if (dot_num == 3 && ip_num <= 255)
    {
        return 1;
    }

    return 0;
}

int is_url(string &str) 
{
    string tmp_str1 = str;
    boost::to_lower(tmp_str1);
    size_t tmp_pos = tmp_str1.find("http");
    if (tmp_pos == 0) 
    {
        str[0] = 'h';
        str[1] = 't';
        str[2] = 't';
        str[3] = 'p';
        boost::replace_all(str, "http.//", "");
        boost::replace_all(str, "http''//", "");
        boost::replace_all(str, "http；//", "");
        boost::replace_all(str, "http;//", "");
        boost::replace_all(str, "http\"//", "");
        boost::replace_all(str, "http''", "");
        boost::replace_all(str, "http ", "");
        boost::replace_all(str, "http//", "");
        boost::replace_all(str, "http.", "");
    }
    string tmp_str = str;
    string protocol = "http://";
    string left_part;
    size_t protocol_pos = tmp_str.find("://");
    if (protocol_pos != string::npos) 
    {
        for (size_t i = 0; i < protocol_pos; ++i) 
        {
            if (!isalpha(tmp_str[i]))
            {
                return 0;
            }
        }
        tmp_str = tmp_str.substr(protocol_pos + 3, 
            tmp_str.length() - protocol_pos - 3);
        protocol = str.substr(0, protocol_pos + 3);
        str = tmp_str;
    }

    size_t question_pos = tmp_str.find("?");
    if (question_pos != string::npos) 
    {
        tmp_str = tmp_str.substr(0, question_pos);
    }

    char split_char = '/';
    size_t slash_pos = tmp_str.find("/");
    if (slash_pos != string::npos) 
    {
        tmp_str = tmp_str.substr(0, slash_pos);
    } 
    else 
    {
        if (question_pos != string::npos) 
        {
            str = str.substr(0, question_pos) + "/" 
                + str.substr(question_pos);
        } 
        else 
        {
            str += "/";
        }
    }

    size_t port_pos = tmp_str.find(":");
    if (port_pos != string::npos) 
    {
        tmp_str = tmp_str.substr(0, port_pos);
        split_char = ':';
    }

    int host_flag = check_host(tmp_str);
    if (host_flag >= 1) 
    {
        if (host_flag == 3) 
        {
            tmp_str = "www" + tmp_str;
        } 
        else if (host_flag == 2) 
        {
            tmp_str = "www." + tmp_str;
        }
        str = protocol + tmp_str + str.substr(str.find(split_char));

        return 1;
    }

    return 0;
}

void trim(std::string &str) 
{
    boost::trim(str);
}

bool erase_angle_quotes(std::string &str) 
{
    size_t old_size = str.size();
    boost::replace_all(str, "\"", "");
    boost::replace_all(str, "“", "");
    boost::replace_all(str, "”", "");
    boost::replace_all(str, "《", "");
    boost::replace_all(str, "》", "");

    return old_size != str.size();
}

double calc_square_sum(char *p, int n, short *freq) 
{
    double r = 0.0;

    if (freq[256]) 
    {
        memset(freq, 0, sizeof(short) * 257);
    }

    freq[256] = 1;

    for (int i = 0; i < n; i++) 
    {
        freq[(unsigned char)(p[i])]++;
    }

    for (int i = 0; i < 256; i++) 
    {
        r += freq[i] * freq[i];
    }

    return sqrt(r);
}

double calc_cosine(double s1, short *freq1, double s2, short *freq2) 
{
    double r = 0.0;
    for (int i = 0; i < 256; i++) 
    {
        r += freq1[i] * freq2[i];
    }

    return r / (s1 * s2);
}
