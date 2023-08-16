
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "../include/log.h"


void log_f(char *format, ...)
{
    
    // Get current time
    time_t current_time;
    time(&current_time);
    struct tm *time_info = gmtime(&current_time);



    // Format time string
    char time_string[26];
    strftime(time_string, sizeof(time_string), "%Y-%m-%dT%H:%M:%S.000Z", time_info);

    // Print time
    printf("%s, ", time_string);



    // Format message
    va_list args;
    va_start(args, format);
    for (int i = 0; format[i] != '\0'; i++)
    {
        if (format[i] == '%')
        {
            i++;
            switch (format[i])
            {
                case 'd':
                {
                    int value = va_arg(args, int);
                    printf("%d", value);
                    break;
                }
                case 'f':
                {
                    double value = va_arg(args, double);
                    printf("%f", value);
                    break;
                }
                case 's':
                {
                    char *value = va_arg(args, char *);
                    printf("%s", value);
                    break;
                }
                default:
                    printf("Invalid format specifier");
                    break;
            }
        }
        else
        {
            printf("%c", format[i]);
        }
    }

    va_end(args);

    // Print new line
    printf("\n");
}

void flog_f(char *log_file, char *format, ...)
{

    pthread_mutex_lock(&file_lock);//lock file writing by other threads

    // Open log file for appending
    FILE *fp = fopen(log_file, "a");
    if (fp == NULL)
    {
        printf("Error opening log file");
        exit(1);
    }

    // Get current time
    time_t current_time;
    time(&current_time);
    struct tm *time_info = gmtime(&current_time);

    // Format time string
    char time_string[26];
    strftime(time_string, sizeof(time_string), "%Y-%m-%dT%H:%M:%S.000Z", time_info);

    // Print time
    fprintf(fp, "%s, ", time_string);

    // Format message
    va_list args;
    va_start(args, format);
    for (int i = 0; format[i] != '\0'; i++)
    {
        if (format[i] == '%')
        {
            i++;
            switch (format[i])
            {
                case 'd':
                {
                    int value = va_arg(args, int);
                    fprintf(fp, "%d", value);
                    break;
                }
                case 'f':
                {
                    double value = va_arg(args, double);
                    fprintf(fp, "%f", value);
                    break;
                }
                case 's':
                {
                    char *value = va_arg(args, char *);
                    fprintf(fp, "%s", value);
                    break;
                }
                default:
                    fprintf(fp, "Invalid format specifier");
                    break;
            }
        }
        else
        {
            fprintf(fp, "%c", format[i]);
        }
    }
    va_end(args);

    // Print new line
    fprintf(fp, "\n");
    fclose(fp);
    pthread_mutex_unlock(&file_lock);//unlock file writing
}