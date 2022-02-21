#!/bin/bash

######################################################################
# Neuron Syslog configuration script.
#
# This script automatically configures rsyslog for Neuron.
#
# Modified from
# https://github.com/loggly/install-script/tree/master/Linux%20Script
#
######################################################################

#trapping Control + C
#these statements must be the first statements in the script to trap the CTRL C event

trap ctrl_c INT

function ctrl_c() {
  logMsg "INFO" "Aborting the script."
  exit 1
}

##########  Variable Declarations - Start  ##########

#directory location for syslog
RSYSLOG_ETCDIR_CONF=/etc/rsyslog.d
#name and location of neuron syslog file
NEURON_RSYSLOG_CONFFILE=$RSYSLOG_ETCDIR_CONF/neuron.conf
#name and location of neuron syslog backup file
NEURON_RSYSLOG_CONFFILE_BACKUP=$NEURON_RSYSLOG_CONFFILE.neuron.bk

#rsyslog service name
RSYSLOG_SERVICE=rsyslog
#syslog-ng
SYSLOG_NG_SERVICE=syslog-ng
#rsyslogd
RSYSLOGD=rsyslogd
#minimum version of rsyslog to enable logging
MIN_RSYSLOG_VERSION=5.8.0
#this variable will hold the users syslog version
RSYSLOG_VERSION=

#this variable will hold the name of the linux distribution
LINUX_DIST=

#if this variable is set to true then suppress all prompts
SUPPRESS_PROMPT="false"

#Instruction link on how to configure neuron on linux manually.
MANUAL_CONFIG_INSTRUCTION="
Instructions to manually re-configure rsyslog for Neuron
=======================================================

1. Create the file $NEURON_RSYSLOG_CONFFILE with the following content:

if $programname contains 'neuron' then /var/log/neuron.log

2. Once you are done configuring syslog-ng or rsyslog, restart it
   Example:  sudo service rsyslog restart
"

#this variable will hold if the check env function for linux is invoked
LINUX_ENV_VALIDATED="false"

#this variable will inform if verification needs to be performed
LINUX_DO_VERIFICATION="true"

#Setting NEURON_REMOVE to false
NEURON_REMOVE="false"

##########  Variable Declarations - End  ##########

#check if the Linux environment is compatible.
#Also set few variables after the check.
checkLinuxCompatibility() {
  #check if the user has root permission to run this script
  checkIfUserHasRootPrivileges

  #check if the OS is supported by the script. If no, then exit
  checkIfSupportedOS

  #checking if syslog-ng is configured as a service
  checkifSyslogNgConfiguredAsService

  #check if rsyslog is configured as service. If no, then exit
  checkIfRsyslogConfiguredAsService

  #check if multiple rsyslog are present in the system. If yes, then exit
  checkIfMultipleRsyslogConfigured

  #check for the minimum version of rsyslog i.e 5.8.0. If no, then exit
  checkIfMinVersionOfRsyslog

  #check if selinux service is enforced. if yes, ask the user to manually disable and exit the script
  checkIfSelinuxServiceEnforced

  LINUX_ENV_VALIDATED="true"
}

# executing the script to configure rsyslog.
installConf() {
  logMsg "INFO" "Initiating Configure Neuron for Linux."

  if [ "$LINUX_ENV_VALIDATED" = "false" ]; then
    checkLinuxCompatibility
  fi

  #if all the above check passes, write the neuron.conf file
  writeContentsAndRestartRsyslog

  logMsg "SUCC" "Linux system successfully configured to send logs for Neuron."

}

#remove neuron configuration from Linux system
removeConf() {
  logMsg "INFO" "Initiating Remove Neuron Syslog Configuration for Linux."

  #check if the user has root permission to run this script
  checkIfUserHasRootPrivileges

  #check if the OS is supported by the script. If no, then exit
  checkIfSupportedOS

  #remove neuron.conf file
  removeNeuronConfFile

  #restart rsyslog service
  restartRsyslog

  #log success message
  logMsg "SUCC" "Uninstalled Neuron configuration from Linux system."
}

#checks if user has root privileges
checkIfUserHasRootPrivileges() {
  #This script needs to be run as root
  if [[ $EUID -ne 0 ]]; then
    logMsg "ERROR" "This script must be run as root."
    exit 1
  fi
}

#check if supported operating system
checkIfSupportedOS() {
  getOs

  LINUX_DIST_IN_LOWER_CASE=$(echo $LINUX_DIST | tr "[:upper:]" "[:lower:]")

  case "$LINUX_DIST_IN_LOWER_CASE" in
  *"ubuntu"*)
    logMsg "INFO"  "Operating system is Ubuntu."
    ;;
  *"red"*)
    logMsg "INFO" "Operating system is Red Hat."
    ;;
  *"centos"*)
    logMsg "INFO" "Operating system is CentOS."
    ;;
  *"debian"*)
    logMsg "INFO" "Operating system is Debian."
    ;;
  *"amazon"*)
    logMsg "INFO" "Operating system is Amazon AMI."
    ;;
  *"darwin"*)
    #if the OS is mac then exit
    logMsg "ERROR" "This script is for Linux systems, and Darwin or Mac OSX are not currently supported."
    exit 1
    ;;
  *)
    logMsg "WARN" "The linux distribution '$LINUX_DIST' has not been previously tested with Neuron."
    if [ "$SUPPRESS_PROMPT" == "false" ]; then
      while true; do
        read -p "Would you like to continue anyway? (yes/no)" yn
        case $yn in
        [Yy]*)
          break
          ;;
        [Nn]*)
          exit 1
          ;;
        *) echo "Please answer yes or no." ;;
        esac
      done
    fi
    ;;
  esac
}

getOs() {
  # Determine OS platform
  UNAME=$(uname | tr "[:upper:]" "[:lower:]")
  # If Linux, try to determine specific distribution
  if [ "$UNAME" == "linux" ]; then
    # If available, use LSB to identify distribution
    if [ -f /etc/lsb-release -o -d /etc/lsb-release.d ]; then
      LINUX_DIST=$(lsb_release -i | cut -d: -f2 | sed s/'^\t'//)
      # If system-release is available, then try to identify the name
    elif [ -f /etc/system-release ]; then
      LINUX_DIST=$(cat /etc/system-release | cut -f 1 -d " ")
      # Otherwise, use release info file
    else
      LINUX_DIST=$(ls -d /etc/[A-Za-z]*[_-][rv]e[lr]* | grep -v "lsb" | cut -d'/' -f3 | cut -d'-' -f1 | cut -d'_' -f1)
    fi
  fi

  # For everything else (or if above failed), just use generic identifier
  if [ "$LINUX_DIST" == "" ]; then
    LINUX_DIST=$(uname)
  fi
}

#check if rsyslog is configured as service. If it is configured as service and not started, start the service
checkIfRsyslogConfiguredAsService() {
  if [ -f /etc/init.d/$RSYSLOG_SERVICE ]; then
    logMsg "INFO" "$RSYSLOG_SERVICE is present as service."
  elif [ -f /usr/lib/systemd/system/$RSYSLOG_SERVICE.service ]; then
    logMsg "INFO" "$RSYSLOG_SERVICE is present as service."
  else
    logMsg "ERROR" "$RSYSLOG_SERVICE is not present as service."
    exit 1
  fi

  #checking if syslog-ng is running as a service
  checkifSyslogNgConfiguredAsService

  if [ $(ps -A | grep "$RSYSLOG_SERVICE" | wc -l) -eq 0 ]; then
    logMsg "INFO" "$RSYSLOG_SERVICE is not running. Attempting to start service."
    service $RSYSLOG_SERVICE start
  fi
}

checkifSyslogNgConfiguredAsService() {
  if [ $(ps -A | grep "$SYSLOG_NG_SERVICE" | wc -l) -gt 0 ]; then
    logMsg "ERROR" "This script does not currently support syslog-ng."
    exit 1
  fi
}

#check if multiple versions of rsyslog is configured
checkIfMultipleRsyslogConfigured() {
  if [ $(ps -A | grep "$RSYSLOG_SERVICE" | wc -l) -gt 1 ]; then
    logMsg "ERROR" "Multiple (more than 1) $RSYSLOG_SERVICE is running."
    exit 1
  fi
}

#check if minimum version of rsyslog required to configure Neuron is met
checkIfMinVersionOfRsyslog() {
  RSYSLOG_VERSION=$($RSYSLOGD -version | grep "$RSYSLOGD")
  RSYSLOG_VERSION=${RSYSLOG_VERSION#* }
  RSYSLOG_VERSION=${RSYSLOG_VERSION%,*}
  RSYSLOG_VERSION=$RSYSLOG_VERSION | tr -d " "
  if [ $(compareVersions "$RSYSLOG_VERSION" "$MIN_RSYSLOG_VERSION" 3) -lt 0 ]; then
    logMsg "ERROR" "Minimum rsyslog version required to run this script is 5.8.0. Please upgrade your rsyslog version or follow the manual instructions."
    exit 1
  fi
}

#check if SeLinux service is enforced
checkIfSelinuxServiceEnforced() {
  isSelinuxInstalled=$(getenforce -ds 2>/dev/null)
  if [ $? -ne 0 ]; then
    logMsg "INFO" "selinux status is not enforced."
  elif [ $(getenforce | grep "Enforcing" | wc -l) -gt 0 ]; then
    logMsg "ERROR" "selinux status is 'Enforcing'. Please manually restart the rsyslog daemon or turn off selinux by running 'setenforce 0' and then rerun the script."
    exit 1
  fi
}

#check if authentication token is valid and then write contents to neuron.conf file to /etc/rsyslog.d directory
writeContentsAndRestartRsyslog() {
    writeContents
    restartRsyslog
}

confString() {
  inputStr="
#          -------------------------------------------------------
#          Syslog Logging Directives for Neuron
#          -------------------------------------------------------

if \$programname contains 'neuron' then /var/log/neuron.log

#     -------------------------------------------------------
  "
}


#write the contents to neuron.conf
writeContents() {
  confString
  WRITE_SCRIPT_CONTENTS="false"

  if [ -f "$NEURON_RSYSLOG_CONFFILE" ]; then
    logMsg "INFO" "Neuron rsyslog file $NEURON_RSYSLOG_CONFFILE already exist."

    STR_SIZE=${#inputStr}
    SIZE_FILE=$(stat -c%s "$NEURON_RSYSLOG_CONFFILE")

    #actual file size and variable size with same contents always differ in size with one byte
    STR_SIZE=$((STR_SIZE + 1))

    if [ "$STR_SIZE" -ne "$SIZE_FILE" ]; then

      logMsg "WARN" "Neuron rsyslog file $NEURON_RSYSLOG_CONFFILE content has changed."
      if [ "$SUPPRESS_PROMPT" == "false" ]; then
        while true; do
          read -p "Do you wish to override $NEURON_RSYSLOG_CONFFILE and re-verify configuration? (yes/no)" yn
          case $yn in
          [Yy]*)
            logMsg "INFO" "Going to back up the conf file: $NEURON_RSYSLOG_CONFFILE to $NEURON_RSYSLOG_CONFFILE_BACKUP"
            mv -f $NEURON_RSYSLOG_CONFFILE $NEURON_RSYSLOG_CONFFILE_BACKUP
            WRITE_SCRIPT_CONTENTS="true"
            break
            ;;
          [Nn]*)
            LINUX_DO_VERIFICATION="false"
            logMsg "INFO" "Skipping Linux verification."
            break
            ;;
          *) echo "Please answer yes or no." ;;
          esac
        done
      else
        logMsg "INFO" "Going to back up the conf file: $NEURON_RSYSLOG_CONFFILE to $NEURON_RSYSLOG_CONFFILE_BACKUP"
        mv -f $NEURON_RSYSLOG_CONFFILE $NEURON_RSYSLOG_CONFFILE_BACKUP
        WRITE_SCRIPT_CONTENTS="true"
      fi
    else
      LINUX_DO_VERIFICATION="false"
    fi
  else
    WRITE_SCRIPT_CONTENTS="true"
  fi

  if [ "$WRITE_SCRIPT_CONTENTS" == "true" ]; then

    cat <<EOIPFW >>$NEURON_RSYSLOG_CONFFILE
$inputStr
EOIPFW

  fi

}

#delete neuron.conf file
removeNeuronConfFile() {
  if [ -f "$NEURON_RSYSLOG_CONFFILE" ]; then
    rm -rf "$NEURON_RSYSLOG_CONFFILE"
  fi
}


#compares two version numbers, used for comparing versions of various softwares
compareVersions() {
  typeset IFS='.'
  typeset -a v1=($1)
  typeset -a v2=($2)
  typeset n diff

  for ((n = 0; n < $3; n += 1)); do
    diff=$((v1[n] - v2[n]))
    if [ $diff -ne 0 ]; then
      [ $diff -le 0 ] && echo '-1' || echo '1'
      return
    fi
  done
  echo '0'
}

#restart rsyslog
restartRsyslog() {
  logMsg "INFO" "Restarting the $RSYSLOG_SERVICE service."
  service $RSYSLOG_SERVICE restart
  if [ $? -ne 0 ]; then
    logMsg "WARNING" "$RSYSLOG_SERVICE did not restart gracefully. Please restart $RSYSLOG_SERVICE manually."
  fi
}

#logs message to config syslog
logMsg() {
  #$1 variable will be SUCCESS or ERROR or INFO or WARNING
  #$2 variable will be the message
  cslStatus=$1
  cslMessage=$2
  echo "$(date --rfc-3339=seconds) $cslStatus: $cslMessage"
}


#display usage syntax
usage() {
  cat <<EOF
usage: configure-linux [-i to install] [-s suppress prompts {optional)]
usage: configure-linux [-r to remove]
usage: configure-linux [-h for help]


If you desire, you could also do it manually:
$MANUAL_CONFIG_INSTRUCTION
EOF

}

##########  Get Inputs from User - Start  ##########
if [ $# -eq 0 ]; then
  usage
  exit
else
  while [ "$1" != "" ]; do
    case $1 in
    -i | --install)
      NEURON_INSTALL="true"
      ;;
    -r | --remove)
      NEURON_REMOVE="true"
      ;;
    -s | --suppress)
      SUPPRESS_PROMPT="true"
      ;;
    -h | --help)
      usage
      exit
      ;;
    *)
      usage
      exit
      ;;
    esac
    shift
  done
fi

if [ "$NEURON_INSTALL" == "true" ]; then
  installConf
elif [ "$NEURON_REMOVE" == "true" ]; then
  removeConf
else
  usage
  exit
fi

##########  Get Inputs from User - End  ##########       -------------------------------------------------------
#          End of Syslog Logging Directives for Neuron
#
