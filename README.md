# tigerlog
Tigerlog is a local logging system, using UDP packets sent to the listening port of the tigerlog program.

*Features:*
- Can have a many log files as you like.
- Can place the logs in a ramdisk to save SD cards.
- Can create log files dynamically.
- Can delete log files at any time.

The tigerlog system only listens to the 127.0.0.1 subnet.
Add at the end of the /etc/services file.
  `sudo echo "tigerlog\t5252/udp" >> /etc/services`

Edit the tigerlog.ini file to your liking.\
The fields have the following meaning.\
  basePath = The directory were log files will be kept.\
  logName = Normally this is 'tigerlog'\
  maxLogs = Max number of log files that can be created and managed.\
  maxRecv = Max number of bytes a log message can be.\
  maxLogSize = Max size of all log files in MB

The basePath can be a ramdisk but logs will be lost if system reboots.

Create a directory called /usr/local/etc/tigerlog\
Copy the tigerlog.ini file to the /usr/local/etc/tigerlog directory.\
  sudo chmod 0400 /usr/local/etc/tigerlog/tigerlog.ini\
  sudo chown root:root /usr/local/etc/tigerlog/tigerlog.ini

Log files can be created in 2 ways.\
If you create a file in the basePath directory called logName.log then when the tigerlog\
program runs it will automatically create the log file entry in the tigerlog program.\
  sudo touch /basePath/logName.log

Use the tigerlogctl sctipt to create, delete or archive a log file.\
  tigerlogctl new newLogName\
  tigerlogctl delete LogName\
  tigerlogctl archive LogName

The delete action does not delete the log file.  Only stops tigerlog program from logging any more messages.\
If the tigerlog program is restarted and the log file exists in the basePath directory then it will be enabled again.

Copy the tigerlog.service file\
  sudo cp tigerlog.service /etc/systemd/system/tigerlog.service

  sudo systemctl enable tigerlog.service\
  sudo systemctl start tigerlog.service

The script logit can be used to send message to a logfile.\
  logit logName "message"

Also you can use the tigerlog library to also send messsages.\
  int tigerLogSocket();		// Returns a UDP socket.\
  int tigerLogNew(int sock, char *logName);\
  int tigerLogArc(int sock, char *logName);\
  int tigerLogDel(int sock, char *logName);\
The library is found in my utils repo. See testdir directory for an example.

NOTE: Messages can NOT have a '~' character.

Use the tigerlogctl script to create a new log file.\
Use the tigerlogctl script to remove a log file entry from tigerlog program.\
Use the tigerlogctl script to archive a log file. Only two archive files a kept.\

Remember that UDP packets can be lost but it is unlikely since packets never leave the system.
