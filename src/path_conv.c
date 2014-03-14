#include "path_conv.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static const char* ROOT_PATH = "C:/msys2";

static const int true = 1;
static const int false = 0;

typedef enum PATH_TYPE_E {
    NONE = 0,
    SIMPLE_WINDOWS_PATH,
    ESCAPE_WINDOWS_PATH,
    WINDOWS_PATH_LIST,
    UNC,
    ESCAPED_PATH,
    ROOTED_PATH,
    POSIX_PATH_LIST,
    URL,
} path_type;


path_type find_path_start_and_type(const char** src, int recurse, const char* end);
void copy_to_dst(const char* from, const char* to, char** dst, const char* dstend);
void convert_path(const char* from, const char* to, path_type type, char** dst, const char* dstend);

//Transformations
//SIMPLE_WINDOWS_PATH converter. Copy as is. Hold C:\Something\like\this
void swp_convert(const char* from, const char* to, char** dst, const char* dstend);
//ESCAPE_WINDOWS_PATH converter. Turn backslashes to slashes and skip first /. Hold /C:\Somethind\like\this
void ewp_convert(const char* from, const char* to, char** dst, const char* dstend);
//WINDOWS_PATH_LIST converter. Copy as is. Hold /something/like/this;
void wpl_convert(const char* from, const char* to, char** dst, const char* dstend);
//UNC convert converter. Copy as is. Hold //somethig/like/this
void unc_convert(const char* from, const char* to, char** dst, const char* dstend);
//ESCAPED_PATH converter. Turn backslashes to slashes and skip first /. Hold //something\like\this
void ep_convert(const char* from, const char* to, char** dst, const char* dstend);
//ROOTED_PATH converter. Prepend root dir to front. Hold /something/like/this
void rp_convert(const char* from, const char* to, char** dst, const char* dstend);
//URL converter. Copy as is.
void url_convert(const char* from, const char* to, char** dst, const char* dstend);
//POSIX_PATH_LIST. Hold x::x/y:z
void ppl_convert(const char* from, const char* to, char** dst, const char* dstend);

void sub_convert(const char* from, const char* to, char** dst, const char* dstend, char end_with) {
    const char* copy_from = from;
    path_type type = find_path_start_and_type(&from, false, to);

    if (type != NONE) {
        copy_to_dst(copy_from, from, dst, dstend);
        convert_path(from, to, type, dst, dstend);
    }

    if (*dst != dstend) {
        **dst = end_with;
        *dst += 1;
    }
}

const char* convert(char *dst, size_t dstlen, const char *src) {
    const char* srcit = src;
    const char* srcbeg = src;
    char* dstit = dst;
    char* dstend = dst + dstlen;

    int prev_was_space = 0;
    for (;*srcit!= '\0'; ++srcit) {
        if (isspace(*srcit)) {
            if (prev_was_space) {
                continue;
            }

            prev_was_space = true;
            sub_convert(srcbeg, srcit, &dstit, dstend, ' ');
        }

        if (!isspace(*srcit) && prev_was_space) {
            prev_was_space = false;
            srcbeg = srcit;
        }
    }

    sub_convert(srcbeg, srcit, &dstit, dstend, '\0');

    return dst;
}

void copy_to_dst(const char* from, const char* to, char** dst, const char* dstend) {
    for (; (*from != '\0') && (from != to) && (*dst != dstend); ++from, ++(*dst)) {
        **dst = *from;
    }
}

int is_spec_start_symbl(char ch) {
    return (ch == '-') || (ch == '"') || (ch == '\'') || (ch == '@');
}

const char** move(const char** p, int count) {
    *p += count;
    return p;
}

path_type find_path_start_and_type(const char** src, int recurse, const char* end) {
    const char* it = *src;

    if (*it == '\0' || it == end) return NONE;

    path_type result = NONE;

    if (isalpha(*it) && *(it + 1) == ':') {
        if ((recurse && *(it + 2) == '/') ||
            (*(it + 2) == '\\')) {
            return SIMPLE_WINDOWS_PATH;
        }
    }

    if (*it == '/') {
        it += 1;

        if (isalpha(*it) && *(it + 1) == ':') {
            return ESCAPE_WINDOWS_PATH;
        }

        if (*it == '.' && *(it + 1) == '.') {
            return SIMPLE_WINDOWS_PATH;
        }

        int double_slashed = 0;
        if (*it == '/') {
            double_slashed = 1;
            it += 1;
        }

        if (double_slashed && *it == ':') {
            return URL;
        }


        for (; *it != '\0' && it != end; ++it) {
            switch(*it) {
                case ':': {char ch = *(it + 1); if (ch == '/' || ch == ':' || ch == '.') return POSIX_PATH_LIST;}
                case '/': result = (double_slashed) ? UNC : ROOTED_PATH; break;
                case ';': return WINDOWS_PATH_LIST;
            }
        }

        if (result != NONE) {
            return result;
        }

        return (double_slashed) ? ESCAPED_PATH : ROOTED_PATH;
    }

    if (*it == '@') {
        return find_path_start_and_type(move(src, 1), true, end);
    }

    if (*it == '"' || *it == '\'') {
        return find_path_start_and_type(move(src, 1), true, end);
    }

    int starts_with_minus = (*it == '-');

    if (*it == '-' && *(it + 2) == '/') {
        it += 2;
        result = ROOTED_PATH;
    }


    int not_starte_with_spec = recurse == 0
                                ? !is_spec_start_symbl(*it)
                                : !is_spec_start_symbl(*(it - 1));

    for (const char* it2 = it; *it2 != '\0' && it2 != end; ++it2) {
        char ch = *it2;
        if (ch == '=') {
            *src = it2 + 1;
            return find_path_start_and_type(src, true, end);
        }

        if (ch == ',' && starts_with_minus) {
            *src = it2 + 1;
            return find_path_start_and_type(src, true, end);
        }

        if (ch == ':' && not_starte_with_spec) {
            it2 += 1;
            ch = *it2;
            if (ch == '/' || ch == ':' || ch == '.') {
                return POSIX_PATH_LIST;
            } else {
                return SIMPLE_WINDOWS_PATH;
            }
        }
    }

    if (result != NONE) {
        *src = it;
        return result;
    }

    return SIMPLE_WINDOWS_PATH;
}

void convert_path(const char* from, const char* to, path_type type, char** dst, const char* dstend) {
    switch(type) {
        case SIMPLE_WINDOWS_PATH: swp_convert(from, to, dst, dstend); break;
        case ESCAPE_WINDOWS_PATH: ewp_convert(from, to, dst, dstend); break;
        case WINDOWS_PATH_LIST: wpl_convert(from, to, dst, dstend); break;
        case UNC: unc_convert(from, to, dst, dstend); break;
        case ESCAPED_PATH: ep_convert(from, to, dst, dstend); break;
        case ROOTED_PATH: rp_convert(from, to, dst, dstend); break;
        case URL: url_convert(from, to, dst, dstend); break;
        case POSIX_PATH_LIST: ppl_convert(from, to, dst, dstend); break;
        case NONE: // prevent warnings;
        default:
                return;
    }
}

void swp_convert(const char* from, const char* to, char** dst, const char* dstend) {
    copy_to_dst(from, to, dst, dstend);
}

void ewp_convert(const char* from, const char* to, char** dst, const char* dstend) {
    unc_convert(from + 1, to, dst, dstend);
}

void wpl_convert(const char* from, const char* to, char** dst, const char* dstend) {
    swp_convert(from, to, dst, dstend);
}

void unc_convert(const char* from, const char* to, char** dst, const char* dstend) {
    const char* it = from;
    for (; (*it != '\0' && it != to) && (*dst != dstend); ++it, ++(*dst)) {
        if (*it == '\\') {
            **dst = '/';
        } else {
            **dst = *it;
        }
    }
}

void ep_convert(const char* from, const char* to, char** dst, const char* dstend) {
    ewp_convert(from, to, dst, dstend);
}

void rp_convert(const char* from, const char* to, char** dst, const char* dstend) {
    copy_to_dst(ROOT_PATH, NULL, dst, dstend);

    const char* it = from;
    if (*(it + 1) != '\0' && *(it + 1) != '\'' && *(it + 1) != '"') {
        for (; (*it != '\0' && it != to) && (*dst != dstend); ++it, ++(*dst)) {
            if (*it == '\\') {
                **dst = '/';
            } else {
                **dst = *it;
            }
        }
    } else {
        it += 1;
    }

    if ((*dst != dstend) && (*it != '\0' && it != to)) {
        char ch = *it;
        if (ch == '\'' || ch == '"') {
            **dst = ch;
            *dst += 1;
            it += 1;
        }
    }
}

void url_convert(const char* from, const char* to, char** dst, const char* dstend) {
    unc_convert(from, to, dst, dstend);
}

void subp_convert(const char* from, const char* end, int is_url, char** dst, const char* dstend) {
    const char* begin = from;
    path_type type = find_path_start_and_type(&from, 0, end);
    copy_to_dst(begin, from, dst, dstend);

    if (type == NONE) {
        return;
    }

    char* start = *dst;
    convert_path(from, end, type, dst, dstend);

    if (!is_url) {
        for (; start != *dst; ++start) {
            if (*start == '/') {
                *start = '\\';
            }
        }
    }
}

void ppl_convert(const char* from, const char* to, char** dst, const char* dstend) {
    const char* it = from;
    const char* beg = it;
    int prev_was_simc = 0;
    int is_url = 0;
    for (; (*it != '\0') && (*dst != dstend); ++it) {
        if (*it == ':') {
            if (prev_was_simc) {
                continue;
            }
            if (from + 2 < it && *(it - 2) == '/' && *(it - 1) == '/') {
                is_url = 1;
                continue;
            }
            prev_was_simc = 1;
            subp_convert(beg, it, is_url, dst, dstend);
            is_url = 0;

            **dst = ';';
            *dst += 1;
        }

        if (*it != ':' && prev_was_simc) {
            prev_was_simc = 0;
            beg = it;
        }
    }

    subp_convert(beg, it, is_url, dst, dstend);
}