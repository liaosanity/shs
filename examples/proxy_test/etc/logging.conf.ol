######################################################################
slog.appender.DEFAULT_ERROR=RollingFileAppender
slog.appender.DEFAULT_ERROR.MaxFileSize=10000000000

slog.appender.DEFAULT_ERROR.File=/home/test/proxy_test/log/error.log
slog.appender.DEFAULT_ERROR.filters.1=LogLevelRangeFilter
slog.appender.DEFAULT_ERROR.filters.1.LogLevelMin=ERROR
slog.appender.DEFAULT_ERROR.filters.1.LogLevelMax=OFF
slog.appender.DEFAULT_ERROR.filters.1.AcceptOnMatch=true
slog.appender.DEFAULT_ERROR.MaxBackupIndex=20
slog.appender.DEFAULT_ERROR.ImmediateFlush=TRUE
slog.appender.DEFAULT_ERROR.layout=PatternLayout
slog.appender.DEFAULT_ERROR.layout.ConversionPattern=%D:%d{%q} [%-5p][%5P:%14t] <%F:%L> %c %x - %m%n

######################################################################
slog.appender.DEFAULT_WARN=RollingFileAppender
slog.appender.DEFAULT_WARN.MaxFileSize=10000000000

slog.appender.DEFAULT_WARN.File=/home/test/proxy_test/log/warning.log
slog.appender.DEFAULT_WARN.filters.1=LogLevelRangeFilter
slog.appender.DEFAULT_WARN.filters.1.LogLevelMin=WARN
slog.appender.DEFAULT_WARN.filters.1.LogLevelMax=ERROR
slog.appender.DEFAULT_WARN.filters.1.AcceptOnMatch=true
slog.appender.DEFAULT_WARN.MaxBackupIndex=20
slog.appender.DEFAULT_WARN.layout=PatternLayout
slog.appender.DEFAULT_WARN.layout.ConversionPattern=%D:%d{%q} [%-5p][%5P:%14t] <%F:%L> %c %x - %m%n

######################################################################
slog.appender.DEFAULT_INFO=RollingFileAppender
slog.appender.DEFAULT_INFO.MaxFileSize=10000000000

slog.appender.DEFAULT_INFO.File=/home/test/proxy_test/log/info.log
slog.appender.DEFAULT_INFO.filters.1=LogLevelRangeFilter
slog.appender.DEFAULT_INFO.filters.1.LogLevelMin=INFO
slog.appender.DEFAULT_INFO.filters.1.LogLevelMax=WARN
slog.appender.DEFAULT_INFO.filters.1.AcceptOnMatch=true
slog.appender.DEFAULT_INFO.MaxBackupIndex=20
slog.appender.DEFAULT_INFO.layout=PatternLayout
slog.appender.DEFAULT_INFO.layout.ConversionPattern=%D:%d{%q} [%-5p] [%P:%14t] <%F:%L> %c %x - %m%n

######################################################################
slog.appender.DEFAULT_DEBUG=RollingFileAppender
slog.appender.DEFAULT_DEBUG.MaxFileSize=10000000000

slog.appender.DEFAULT_DEBUG.File=/home/test/proxy_test/log/debug.log
slog.appender.DEFAULT_DEBUG.filters.1=LogLevelRangeFilter
slog.appender.DEFAULT_DEBUG.filters.1.LogLevelMin=DEBUG
slog.appender.DEFAULT_DEBUG.filters.1.LogLevelMax=INFO
slog.appender.DEFAULT_DEBUG.filters.1.AcceptOnMatch=true
slog.appender.DEFAULT_DEBUG.MaxBackupIndex=20
slog.appender.DEFAULT_DEBUG.layout=PatternLayout
slog.appender.DEFAULT_DEBUG.layout.ConversionPattern=%D:%d{%q} [%-5p][%5P:%14t] <%F:%L> %c %x - %m%n

######################################################################
slog.appender.DEFAULT_TRACE=RollingFileAppender
slog.appender.DEFAULT_TRACE.MaxFileSize=10000000000

slog.appender.DEFAULT_TRACE.File=/home/test/proxy_test/log/trace.log
slog.appender.DEFAULT_TRACE.filters.1=LogLevelRangeFilter
slog.appender.DEFAULT_TRACE.filters.1.LogLevelMin=TRACE
slog.appender.DEFAULT_TRACE.filters.1.LogLevelMax=DEBUG
slog.appender.DEFAULT_TRACE.filters.1.AcceptOnMatch=true
slog.appender.DEFAULT_TRACE.MaxBackupIndex=20
slog.appender.DEFAULT_TRACE.layout=PatternLayout
slog.appender.DEFAULT_TRACE.layout.ConversionPattern=%D:%d{%q} [%-5p] [%P] <%F:%L> %c %x - %m%n

######################################################################
slog.appender.Module=RollingFileAppender
slog.appender.Module.MaxFileSize=10000000000

slog.appender.Module.File=/home/test/proxy_test/log/module.log
slog.appender.Module.ImmediateFlush=TRUE
slog.appender.Module.layout=PatternLayout
slog.appender.Module.MaxBackupIndex=20
slog.appender.Module.layout.ConversionPattern=%D:%d{%q} [%-5p] [%P] <%F:%L> %c %x - %m%n

######################################################################
#slog.rootLogger=ALL, DEFAULT_ERROR, DEFAULT_WARN, DEFAULT_INFO, DEFAULT_DEBUG 
slog.rootLogger=ALL, DEFAULT_ERROR, DEFAULT_INFO

