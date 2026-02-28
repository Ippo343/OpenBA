set pagination off
set print thread-events off
set confirm off

# Let Windows signals through normally but stop on SEGV
handle SIGSEGV stop print nopass
handle SIGILL stop print nopass
handle SIGABRT stop print nopass

# Don't stop on benign Windows loader signals
handle SIG32 nostop noprint pass
handle SIG42 nostop noprint pass
handle SIGPIPE nostop noprint pass

# Start the program
start

# Continue past entry point
continue
