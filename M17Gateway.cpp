/*
*   Copyright (C) 2016,2017,2018,2020,2021 by Jonathan Naylor G4KLX
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

#include "M17Gateway.h"
#include "RptNetwork.h"
#include "Reflectors.h"
#include "StopWatch.h"
#include "M17Utils.h"
#include "Version.h"
#include "Thread.h"
#include "Timer.h"
#include "Voice.h"
#include "Utils.h"
#include "Echo.h"
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

#include <algorithm>
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
m_conf(file),
m_status(M17S_NOTLINKED),
m_oldStatus(M17S_NOTLINKED),
m_network(NULL),
m_timer(1000U, 5U),
m_reflector(),
m_addrLen(0U),
m_addr(),
m_module(' ')
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
		} else if (pid != 0) {
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

	m_network = new CM17Network(m_conf.getCallsign(), m_conf.getSuffix(), m_conf.getNetworkPort(), m_conf.getNetworkDebug());

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

	bool triggerVoice = false;
	CVoice* voice = NULL;
	if (m_conf.getVoiceEnabled()) {
		voice = new CVoice(m_conf.getVoiceDirectory(), m_conf.getVoiceLanguage(), m_conf.getCallsign());
		bool ok = voice->open();
		if (!ok) {
			delete voice;
			voice = NULL;
		}
	}

	CEcho echo(240U);

	CTimer hangTimer(1000U, m_conf.getNetworkHangTime());

	CStopWatch stopWatch;
	stopWatch.start();

	LogMessage("Starting M17Gateway-%s", VERSION);

	std::string startupReflector = m_conf.getNetworkStartup();
	bool revert = m_conf.getNetworkRevert();

	if (voice != NULL)
		voice->unlinked();

	if (!startupReflector.empty()) {
		CM17Reflector* refl = reflectors.find(startupReflector);
		if (refl != NULL) {
			char module = startupReflector.at(M17_CALLSIGN_LENGTH - 1U);
			if (module >= 'A' && module <= 'Z') {
				m_reflector = startupReflector;
				m_addr      = refl->m_addr;
				m_addrLen   = refl->m_addrLen;
				m_module    = module;

				m_status = m_oldStatus = M17S_LINKING;

				LogInfo("Linked at startup to %s", m_reflector.c_str());

				if (voice != NULL)
					voice->linkedTo(m_reflector);

				m_timer.start();
			}
		} else {
			startupReflector.clear();
		}
	}

	for (;;) {
		unsigned char buffer[100U];

		if (m_status == M17S_LINKED) {
			// From the reflector to the MMDVM
			bool ret = m_network->read(buffer);
			if (ret) {
				// Replace the destination callsign with the brodcast callsign
				CM17Utils::encodeCallsign("ALL", buffer + 6U);
				localNetwork->write(buffer);
				hangTimer.start();
			}
		} else if (m_status == M17S_ECHO) {
			// From the echo unit to the MMDVM
			ECHO_STATE est = echo.read(buffer);
			switch (est) {
				case EST_DATA:
					localNetwork->write(buffer);
					hangTimer.start();
					break;
				case EST_EOF:
					// End of the message, restore the original status
					m_status = m_oldStatus;
					break;
				default:
					break;
			}
		}

		// From the MMDVM to the reflector or control data
		bool ret = localNetwork->read(buffer);
		if (ret) {
			// Parse the control information
			std::string src = CM17Utils::decodeCallsign(buffer + 12U);
			std::string dst = CM17Utils::decodeCallsign(buffer + 6U);

			if (dst == "ECHO") {
				if (m_status != M17S_ECHO) {
					m_oldStatus = m_status;
					echo.clear();
				}

				echo.write(buffer);
				m_status = M17S_ECHO;
				hangTimer.start();

				uint16_t fn = (buffer[34U] << 8) + (buffer[35U] << 0);
				if ((fn & 0x8000U) == 0x8000U)
					echo.end();
			} else if (dst == "INFO") {
				hangTimer.start();
				triggerVoice = true;
			} else if (dst == "UNLINK") {
				if (m_status == M17S_LINKED || m_status == M17S_LINKING) {
					LogMessage("Unlinking from reflector %s triggered by %s", m_reflector.c_str(), src.c_str());
					m_network->stop();
					m_network->unlink();

					if (voice != NULL)
						voice->unlinked();

					m_status = m_oldStatus = M17S_UNLINKING;
					m_timer.start();
				}

				triggerVoice = true;
				hangTimer.stop();
			} else if (dst.size() == M17_CALLSIGN_LENGTH) {
				std::string reflector = dst;
				char module = reflector.at(M17_CALLSIGN_LENGTH - 1U);

				if (reflector != m_reflector && module >= 'A' && module <= 'Z') {
					if (m_status == M17S_LINKED || m_status == M17S_LINKING) {
						LogMessage("Unlinking from reflector %s triggered by %s", m_reflector.c_str(), src.c_str());
						m_network->stop();
						m_network->unlink();

						m_timer.start();
					}

					triggerVoice = true;
				
					CM17Reflector* refl = reflectors.find(reflector);
					if (refl != NULL) {
						m_reflector = reflector;
						m_addr      = refl->m_addr;
						m_addrLen   = refl->m_addrLen;
						m_module    = module;

						// Link to the new reflector
						LogMessage("Linking to reflector %s triggered by %s", m_reflector.c_str(), src.c_str());

						m_status = m_oldStatus = M17S_LINKING;

						if (voice != NULL)
							voice->linkedTo(m_reflector);

						hangTimer.start();
						m_timer.start();
					} else {
						if (m_status == M17S_LINKED || m_status == M17S_LINKING)
							m_status = m_oldStatus = M17S_UNLINKING;

						if (voice != NULL)
							voice->unlinked();

						hangTimer.stop();
					}
				}
			}

			if (m_status == M17S_LINKED) {
				// If the link has failed, try and relink
				M17NET_STATUS netStatus = m_network->getStatus();
				if (netStatus == M17N_FAILED) {
					LogMessage("Relinking to reflector %s", m_reflector.c_str());
					m_status = M17S_LINKING;
				}

				// If we're linked and we have a network, send it on
				if (m_status == M17S_LINKED) {
					// Replace the destination callsign with the reflector name and module
					CM17Utils::encodeCallsign(m_reflector, buffer + 6U);
					m_network->write(buffer);
					hangTimer.start();
				}
			}
		}

		if (voice != NULL) {
			if (triggerVoice) {
				uint16_t fn = (buffer[34U] << 8) + (buffer[35U] << 0);
				if ((fn & 0x8000U) == 0x8000U) {
					voice->eof();
					triggerVoice = false;
				}
			}

			ret = voice->read(buffer);
			if (ret)
				localNetwork->write(buffer);
		}

		if (remoteSocket != NULL) {
			sockaddr_storage addr;
			unsigned int addrLen;
			int res = remoteSocket->read(buffer, 200U, addr, addrLen);
			if (res > 0) {
				buffer[res] = '\0';
				if (::memcmp(buffer + 0U, "Reflector", 9U) == 0) {
					std::string reflector = std::string((char*)(buffer + 9U));
					std::replace(reflector.begin(), reflector.end(), '_', ' ');
					reflector.resize(M17_CALLSIGN_LENGTH, ' ');

					if (reflector != m_reflector) {
						if (m_status == M17S_LINKED || m_status == M17S_LINKING) {
							LogMessage("Unlinked from reflector %s by remote command", m_reflector.c_str());

							m_network->stop();
							m_network->unlink();

							hangTimer.stop();
							m_timer.start();
						}

						CM17Reflector* refl = reflectors.find(reflector);
						if (refl != NULL) {
							char module = reflector.at(M17_CALLSIGN_LENGTH - 1U);
							if (module >= 'A' && module <= 'Z') {
								m_reflector = reflector;
								m_addr      = refl->m_addr;
								m_addrLen   = refl->m_addrLen;
								m_module    = module;

								// Link to the new reflector
								LogMessage("Switched to reflector %s by remote command", m_reflector.c_str());

								m_status = m_oldStatus = M17S_LINKING;

								if (voice != NULL) {
									voice->linkedTo(m_reflector);
									voice->eof();
								}

								hangTimer.start();
								m_timer.start();
							}
						} else {
							m_reflector.clear();
							if (m_status == M17S_LINKED || m_status == M17S_LINKING) {
								m_status = m_oldStatus = M17S_UNLINKING;

								if (voice != NULL) {
									voice->unlinked();
									voice->eof();
								}
							}

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

		if (voice != NULL)
			voice->clock(ms);

		m_timer.clock(ms);
		linking();
		unlinking();

		reflectors.clock(ms);

		localNetwork->clock(ms);

		m_network->clock(ms);

		echo.clock(ms);

		hangTimer.clock(ms);
		if (hangTimer.isRunning() && hangTimer.hasExpired()) {
			if (revert && !startupReflector.empty() && m_reflector != startupReflector) {
				if (m_status == M17S_LINKED || m_status == M17S_LINKING) {
					m_network->stop();
					m_network->unlink();
				}

				LogMessage("Relinked from %s to %s due to inactivity", m_reflector.c_str(), startupReflector.c_str());

				CM17Reflector* refl = reflectors.find(startupReflector);
				m_reflector = startupReflector;
				m_addr      = refl->m_addr;
				m_addrLen   = refl->m_addrLen;
				m_module    = startupReflector.at(M17_CALLSIGN_LENGTH - 1U);

				m_status = m_oldStatus = M17S_LINKING;

				if (voice != NULL) {
					voice->linkedTo(startupReflector);
					voice->eof();
				}

				hangTimer.start();
				m_timer.start();
			} else if (revert && startupReflector.empty() && (m_status == M17S_LINKED || m_status == M17S_LINKING)) {
				LogMessage("Unlinking from %s due to inactivity", m_reflector.c_str());

				m_network->stop();
				m_network->unlink();

				m_status = m_oldStatus = M17S_UNLINKING;

				if (voice != NULL) {
					voice->unlinked();
					voice->eof();
				}

				m_reflector.clear();

				hangTimer.stop();
				m_timer.start();
			}
		}

		if (ms < 5U)
			CThread::sleep(5U);
	}

	delete voice;

	localNetwork->close();
	delete localNetwork;

	if (remoteSocket != NULL) {
		remoteSocket->close();
		delete remoteSocket;
	}

	if (m_status == M17S_LINKED || m_status == M17S_LINKING)
		m_network->unlink();
	m_network->close();
	delete m_network;

	::LogFinalise();
}

void CM17Gateway::linking()
{
	if (m_status != M17S_LINKING)
		return;

	M17NET_STATUS status = m_network->getStatus();
	if (status == M17N_NOTLINKED) {
		m_timer.start();
		m_network->link(m_reflector, m_addr, m_addrLen, m_module);
	} else if (status == M17N_LINKED) {
		m_timer.stop();
		m_status = m_oldStatus = M17S_LINKED;
	} else if (status == M17N_REJECTED) {
		m_timer.stop();
		m_status = m_oldStatus = M17S_NOTLINKED;
	} else if (status == M17N_LINKING) {
		if (m_timer.isRunning() && m_timer.hasExpired()) {
			m_network->stop();
			m_timer.stop();
			m_status = m_oldStatus = M17S_NOTLINKED;
			m_reflector.clear();
		}
	} else if (status == M17N_UNLINKING) {
		if (m_timer.isRunning() && m_timer.hasExpired()) {
			m_network->stop();
			m_timer.start();
			m_network->link(m_reflector, m_addr, m_addrLen, m_module);
		}
	}
}

void CM17Gateway::unlinking()
{
	if (m_status != M17S_UNLINKING)
		return;

	M17NET_STATUS status = m_network->getStatus();
	if (status == M17N_NOTLINKED) {
		m_timer.stop();
		m_status = m_oldStatus = M17S_NOTLINKED;
		m_reflector.clear();
	} else if (status == M17N_UNLINKING) {
		if (m_timer.isRunning() && m_timer.hasExpired()) {
			m_network->stop();
			m_timer.stop();
			m_status = m_oldStatus = M17S_NOTLINKED;
			m_reflector.clear();
		}
	}
}
