
#include "main.h"

int _printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int printed_chars = 0;
    
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'c': {
                    char c = (char) va_arg(args, int);
                    putchar(c);
                    printed_chars++;
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char *);
                    while (*str != '\0') {
                        putchar(*str);
                        str++;
                        printed_chars++;
                    }
                    break;
                }
                case '%': {
                    putchar('%');
                    printed_chars++;
                    break;
                }
                default:
                    // handle invalid format specifier
                    break;
            }
        } else {
            putchar(*format);
            printed_chars++;
        }
        format++;
    }
    
    va_end(args);
    return printed_chars;
}

