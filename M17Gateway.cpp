/*
*   Copyright (C) 2016,2017,2018,2020 by Jonathan Naylor G4KLX
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

#include "M17Network.h"
#include "M17Gateway.h"
#include "RptNetwork.h"
#include "Reflectors.h"
#include "StopWatch.h"
#include "Version.h"
#include "Thread.h"
#include "Timer.h"
#include "Utils.h"
#include "Log.h"

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
const char* DEFAULT_INI_FILE = "M17Gateway.ini";
#else
const char* DEFAULT_INI_FILE = "/etc/M17Gateway.ini";
#endif

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <ctime>
#include <cstring>


int main(int argc, char** argv)
{
	const char* iniFile = DEFAULT_INI_FILE;
	if (argc > 1) {
		for (int currentArg = 1; currentArg < argc; ++currentArg) {
			std::string arg = argv[currentArg];
			if ((arg == "-v") || (arg == "--version")) {
				::fprintf(stdout, "M17Gateway version %s\n", VERSION);
				return 0;
			} else if (arg.substr(0, 1) == "-") {
				::fprintf(stderr, "Usage: M17Gateway [-v|--version] [filename]\n");
				return 1;
			} else {
				iniFile = argv[currentArg];
			}
		}
	}

	CM17Gateway* gateway = new CM17Gateway(std::string(iniFile));
	gateway->run();
	delete gateway;

	return 0;
}

CM17Gateway::CM17Gateway(const std::string& file) :
m_conf(file)
{
	CUDPSocket::startup();
}

CM17Gateway::~CM17Gateway()
{
	CUDPSocket::shutdown();
}

void CM17Gateway::run()
{
	bool ret = m_conf.read();
	if (!ret) {
		::fprintf(stderr, "M17Gateway: cannot read the .ini file\n");
		return;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	bool m_daemon = m_conf.getDaemon();
	if (m_daemon) {
		// Create new process
		pid_t pid = ::fork();
		if (pid == -1) {
			::fprintf(stderr, "Couldn't fork() , exiting\n");
			return;
		}
		else if (pid != 0) {
			exit(EXIT_SUCCESS);
		}

		// Create new session and process group
		if (::setsid() == -1) {
			::fprintf(stderr, "Couldn't setsid(), exiting\n");
			return;
		}

		// Set the working directory to the root directory
		if (::chdir("/") == -1) {
			::fprintf(stderr, "Couldn't cd /, exiting\n");
			return;
		}

		// If we are currently root...
		if (getuid() == 0) {
			struct passwd* user = ::getpwnam("mmdvm");
			if (user == NULL) {
				::fprintf(stderr, "Could not get the mmdvm user, exiting\n");
				return;
			}

			uid_t mmdvm_uid = user->pw_uid;
			gid_t mmdvm_gid = user->pw_gid;

			// Set user and group ID's to mmdvm:mmdvm
			if (setgid(mmdvm_gid) != 0) {
				::fprintf(stderr, "Could not set mmdvm GID, exiting\n");
				return;
			}

			if (setuid(mmdvm_uid) != 0) {
				::fprintf(stderr, "Could not set mmdvm UID, exiting\n");
				return;
			}

			// Double check it worked (AKA Paranoia) 
			if (setuid(0) != -1) {
				::fprintf(stderr, "It's possible to regain root - something is wrong!, exiting\n");
				return;
			}
		}
	}
#endif

#if !defined(_WIN32) && !defined(_WIN64)
        ret = ::LogInitialise(m_daemon, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel(), m_conf.getLogFileRotate());
#else
        ret = ::LogInitialise(false, m_conf.getLogFilePath(), m_conf.getLogFileRoot(), m_conf.getLogFileLevel(), m_conf.getLogDisplayLevel(), m_conf.getLogFileRotate());
#endif
	if (!ret) {
		::fprintf(stderr, "M17Gateway: unable to open the log file\n");
		return;
	}

#if !defined(_WIN32) && !defined(_WIN64)
	if (m_daemon) {
		::close(STDIN_FILENO);
		::close(STDOUT_FILENO);
		::close(STDERR_FILENO);
	}
#endif

	CRptNetwork* localNetwork = new CRptNetwork(m_conf.getMyPort(), m_conf.getRptAddress(), m_conf.getRptPort(), m_conf.getDebug());
	ret = localNetwork->open();
	if (!ret) {
		::LogFinalise();
		return;
	}

	CM17Network remoteNetwork(m_conf.getCallsign(), m_conf.getNetworkPort(), m_conf.getNetworkDebug());
	ret = remoteNetwork.open();
	if (!ret) {
		localNetwork->close();
		delete localNetwork;
		::LogFinalise();
		return;
	}

	CUDPSocket* remoteSocket = NULL;
	if (m_conf.getRemoteCommandsEnabled()) {
		remoteSocket = new CUDPSocket(m_conf.getRemoteCommandsPort());
		ret = remoteSocket->open();
		if (!ret) {
			delete remoteSocket;
			remoteSocket = NULL;
		}
	}

	CReflectors reflectors(m_conf.getNetworkHosts1(), m_conf.getNetworkHosts2(), m_conf.getNetworkReloadTime());
	reflectors.load();

	CTimer hangTimer(1000U, m_conf.getNetworkHangTime());

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("Starting M17Gateway-%s", VERSION);

	std::string currentReflector;
	unsigned int currentAddrLen = 0U;
	sockaddr_storage currentAddr;

	for (;;) {
		unsigned char buffer[200U];

		// From the reflector to the MMDVM
		bool ret = remoteNetwork.read(buffer);
		if (ret) {
			localNetwork->write(buffer);

			hangTimer.start();
		}

		// From the MMDVM to the reflector or control data
		ret = localNetwork->read(buffer);
		if (ret) {
			// Only process the beginning and ending voice blocks here
			if ((buffer[0U] == 0x81U || buffer[0U] == 0x83U) && (buffer[5U] == 0x01U || buffer[5U] == 0x08U)) {
				grp = (buffer[7U] & 0x20U) == 0x20U;

				srcId  = (buffer[8U] << 8) & 0xFF00U;
				srcId |= (buffer[9U] << 0) & 0x00FFU;

				dstTG  = (buffer[10U] << 8) & 0xFF00U;
				dstTG |= (buffer[11U] << 0) & 0x00FFU;

				if (reflector != currentReflector) {
					if (currentAddrLen > 0U) {
						std::string callsign = lookup->find(srcId);
						LogMessage("Unlinking from reflector %s by %s", currentReflector.c_str(), callsign.c_str());

						remoteNetwork.unlink();
						remoteNetwork.unlink();
						remoteNetwork.unlink();

						hangTimer.stop();
					}

					CM17Reflector* refl = reflectors.find(reflector);
					if (refl != NULL) {
						currentReflector = reflector;
						currentAddr      = refl->m_addr;
						currentAddrLen   = refl->m_addrLen;

						// Link to the new reflector
						std::string callsign = lookup->find(srcId);
						LogMessage("Switched to reflector %s due to RF activity from %s", currentReflector.c_str(), callsign.c_str());

						remoteNetwork.link(currentAddr, currentAddrLen, currentTG);
						remoteNetwork.link(currentAddr, currentAddrLen, currentTG);
						remoteNetwork.link(currentAddr, currentAddrLen, currentTG);

						hangTimer.start();
					} else {
						hangTimer.stop();
					}
				}
			}

			// If we're linked and we have a network, send it on
			if (currentAddrLen > 0U) {
				remoteNetwork.write(buffer);
				hangTimer.start();
			}
		}

		if (remoteSocket != NULL) {
			sockaddr_storage addr;
			unsigned int addrLen;
			int res = remoteSocket->read(buffer, 200U, addr, addrLen);
			if (res > 0) {
				buffer[res] = '\0';
				if (::memcmp(buffer + 0U, "Reflector", 9U) == 0) {
					std::string reflector = std::string((char*)(buffer + 9U));
					reflector.resize(9U, ' ');

					if (reflector != currentReflector) {
						if (currentAddrLen > 0U) {
							LogMessage("Unlinked from reflector %s by remote command", currentReflector.c_str());

							remoteNetwork.unlink();
							remoteNetwork.unlink();
							remoteNetwork.unlink();

							hangTimer.stop();
						}

						CM17Reflector* refl = reflectors.find(reflector);
						if (refl != NULL) {
							currentReflector = reflector;
							currentAddr      = refl->m_addr;
							currentAddrLen   = refl->m_addrLen;

							// Link to the new reflector
							LogMessage("Switched to reflector %s by remote command", reflector.c_str());

							remoteNetwork.link(currentAddr, currentAddrLen, );
							remoteNetwork.link(currentAddr, currentAddrLen, );
							remoteNetwork.link(currentAddr, currentAddrLen, );

							hangTimer.start();
						} else {
							currentReflector.clear();
							currentAddrLen = 0U;
							hangTimer.stop();
						}
					}
				} else {
					CUtils::dump("Invalid remote command received", buffer, res);
				}
			}
		}

		unsigned int ms = stopWatch.elapsed();
		stopWatch.start();

		reflectors.clock(ms);

		localNetwork->clock(ms);

		hangTimer.clock(ms);
		if (hangTimer.isRunning() && hangTimer.hasExpired()) {
			if (currentAddrLen > 0U) {
				LogMessage("Unlinking from %s due to inactivity", currentReflector.c_str());

				remoteNetwork.unlink();
				remoteNetwork.unlink();
				remoteNetwork.unlink();

				currentAddrLen = 0U;

				hangTimer.stop();
			}

			currentReflector.clear();
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	localNetwork->close();
	delete localNetwork;

	if (remoteSocket != NULL) {
		remoteSocket->close();
		delete remoteSocket;
	}

	remoteNetwork.close();

	::LogFinalise();
}
