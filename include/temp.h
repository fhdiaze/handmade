typedef int logger_function(const char *, ...);

extern logger_function *logger;

enum logs { log_pri, log_ign, log_ver, log_num };
