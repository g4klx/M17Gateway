/*
 *   Copyright (C) 2015,2016,2017,2018,2020,2021 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "Conf.h"
#include "Log.h"
#include "M17Defines.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

const int BUFFER_SIZE = 500;

enum SECTION {
  SECTION_NONE,
  SECTION_GENERAL,
  SECTION_LOG,
  SECTION_NETWORK,
  SECTION_REMOTE_COMMANDS
};

CConf::CConf(const std::string& file) :
m_file(file),
m_callsign(),
m_suffix("M"),
m_rptAddress(),
m_rptPort(0U),
m_myPort(0U),
m_debug(false),
m_daemon(false),
m_logDisplayLevel(0U),
m_logFileLevel(0U),
m_logFilePath(),
m_logFileRoot(),
m_logFileRotate(true),
m_networkPort(0U),
m_networkHosts1(),
m_networkHosts2(),
m_networkReloadTime(0U),
m_networkHangTime(60U),
m_networkStartup(),
m_networkRevert(false),
m_networkDebug(false),
m_remoteCommandsEnabled(false),
m_remoteCommandsPort(6075U)
{
}

CConf::~CConf()
{
}

bool CConf::read()
{
  FILE* fp = ::fopen(m_file.c_str(), "rt");
  if (fp == NULL) {
    ::fprintf(stderr, "Couldn't open the .ini file - %s\n", m_file.c_str());
    return false;
  }

  SECTION section = SECTION_NONE;

  char buffer[BUFFER_SIZE];
  while (::fgets(buffer, BUFFER_SIZE, fp) != NULL) {
	  if (buffer[0U] == '#')
		  continue;

	  if (buffer[0U] == '[') {
		  if (::strncmp(buffer, "[General]", 9U) == 0)
			  section = SECTION_GENERAL;
		  else if (::strncmp(buffer, "[Log]", 5U) == 0)
			  section = SECTION_LOG;
		  else if (::strncmp(buffer, "[Network]", 9U) == 0)
			  section = SECTION_NETWORK;
		  else if (::strncmp(buffer, "[Remote Commands]", 17U) == 0)
			  section = SECTION_REMOTE_COMMANDS;
		  else
			  section = SECTION_NONE;

		  continue;
	  }

	  char* key = ::strtok(buffer, " \t=\r\n");
	  if (key == NULL)
		  continue;

	  char* value = ::strtok(NULL, "\r\n");
	  if (value == NULL)
		  continue;

	  // Remove quotes from the value
	  size_t len = ::strlen(value);
	  if (len > 1U && *value == '"' && value[len - 1U] == '"') {
		  value[len - 1U] = '\0';
		  value++;
	  } else {
		  char *p;

		  // if value is not quoted, remove after # (to make comment)
		  if ((p = strchr(value, '#')) != NULL)
			  *p = '\0';

		  // remove trailing tab/space
		  for (p = value + strlen(value) - 1U; p >= value && (*p == '\t' || *p == ' '); p--)
			  *p = '\0';
	  }

	  if (section == SECTION_GENERAL) {
		  if (::strcmp(key, "Callsign") == 0) {
			  // Convert the callsign to upper case
			  for (unsigned int i = 0U; value[i] != 0; i++)
				  value[i] = ::toupper(value[i]);
			  m_callsign = value;
		  } else if (::strcmp(key, "Suffix") == 0) {
			  // Convert the suffix to upper case
			  for (unsigned int i = 0U; value[i] != 0; i++)
				  value[i] = ::toupper(value[i]);
			  m_suffix = value;
		  } else if (::strcmp(key, "RptAddress") == 0)
			  m_rptAddress = value;
		  else if (::strcmp(key, "RptPort") == 0)
			  m_rptPort = (unsigned int)::atoi(value);
		  else if (::strcmp(key, "LocalPort") == 0)
			  m_myPort = (unsigned int)::atoi(value);
		  else if (::strcmp(key, "Debug") == 0)
			  m_debug = ::atoi(value) == 1;
		  else if (::strcmp(key, "Daemon") == 0)
			  m_daemon = ::atoi(value) == 1;
	  } else if (section == SECTION_LOG) {
		  if (::strcmp(key, "FilePath") == 0)
			  m_logFilePath = value;
		  else if (::strcmp(key, "FileRoot") == 0)
			  m_logFileRoot = value;
		  else if (::strcmp(key, "FileLevel") == 0)
			  m_logFileLevel = (unsigned int)::atoi(value);
		  else if (::strcmp(key, "DisplayLevel") == 0)
			  m_logDisplayLevel = (unsigned int)::atoi(value);
		  else if (::strcmp(key, "FileRotate") == 0)
			  m_logFileRotate = ::atoi(value) ==  1;
	  }
	  else if (section == SECTION_NETWORK) {
		  if (::strcmp(key, "Port") == 0)
			  m_networkPort = (unsigned int)::atoi(value);
		  else if (::strcmp(key, "HostsFile1") == 0)
			  m_networkHosts1 = value;
		  else if (::strcmp(key, "HostsFile2") == 0)
			  m_networkHosts2 = value;
		  else if (::strcmp(key, "ReloadTime") == 0)
			  m_networkReloadTime = (unsigned int)::atoi(value);
		  else if (::strcmp(key, "HangTime") == 0)
			  m_networkHangTime = (unsigned int)::atoi(value);
		  else if (::strcmp(key, "Startup") == 0) {
			  m_networkStartup = value;
			  std::replace(m_networkStartup.begin(), m_networkStartup.end(), '_', ' ');
			  m_networkStartup.resize(M17_CALLSIGN_LENGTH, ' ');
		  } else if (::strcmp(key, "Revert") == 0)
			  m_networkRevert = ::atoi(value) == 1;
		  else if (::strcmp(key, "Debug") == 0)
			  m_networkDebug = ::atoi(value) == 1;
	  }  else if (section == SECTION_REMOTE_COMMANDS) {
		  if (::strcmp(key, "Enable") == 0)
			  m_remoteCommandsEnabled = ::atoi(value) == 1;
		  else if (::strcmp(key, "Port") == 0)
			  m_remoteCommandsPort = (unsigned int)::atoi(value);
	  }
  }

  ::fclose(fp);

  return true;
}

std::string CConf::getCallsign() const
{
	return m_callsign;
}

std::string CConf::getSuffix() const
{
	return m_suffix;
}

std::string CConf::getRptAddress() const
{
	return m_rptAddress;
}

unsigned int CConf::getRptPort() const
{
	return m_rptPort;
}

unsigned int CConf::getMyPort() const
{
	return m_myPort;
}

bool CConf::getDebug() const
{
	return m_debug;
}

bool CConf::getDaemon() const
{
	return m_daemon;
}

unsigned int CConf::getLogDisplayLevel() const
{
	return m_logDisplayLevel;
}

unsigned int CConf::getLogFileLevel() const
{
	return m_logFileLevel;
}

std::string CConf::getLogFilePath() const
{
	return m_logFilePath;
}

std::string CConf::getLogFileRoot() const
{
	return m_logFileRoot;
}

bool CConf::getLogFileRotate() const
{
	return m_logFileRotate;
}

unsigned int CConf::getNetworkPort() const
{
	return m_networkPort;
}

std::string CConf::getNetworkHosts1() const
{
	return m_networkHosts1;
}

std::string CConf::getNetworkHosts2() const
{
	return m_networkHosts2;
}

unsigned int CConf::getNetworkReloadTime() const
{
	return m_networkReloadTime;
}

unsigned int CConf::getNetworkHangTime() const
{
	return m_networkHangTime;
}

std::string CConf::getNetworkStartup() const
{
	return m_networkStartup;
}

bool CConf::getNetworkRevert() const
{
	return m_networkRevert;
}

bool CConf::getNetworkDebug() const
{
	return m_networkDebug;
}

bool CConf::getRemoteCommandsEnabled() const
{
	return m_remoteCommandsEnabled;
}

unsigned int CConf::getRemoteCommandsPort() const
{
	return m_remoteCommandsPort;
}
