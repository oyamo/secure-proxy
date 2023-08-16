//
// Created by oyamo on 3/7/23.
//

#ifndef SECURE_PROXY_LOG_H
#define SECURE_PROXY_LOG_H
/**
 * Log the message
 * @param format
 * @param ...
 */
void log_f(char *format, ...);


/**
 * mutex for log file
*/
pthread_mutex_t file_lock;

/**
 * Log the message and exit
 * @param format
 * @param ...
 */
void flog_f(char *log_file, char *format, ...);
#endif //SECURE_PROXY_LOG_H
