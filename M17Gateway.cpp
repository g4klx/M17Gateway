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

#include "M17Network.h"
#include "M17Gateway.h"
#include "RptNetwork.h"
#include "Reflectors.h"
#include "StopWatch.h"
#include "M17Utils.h"
#include "Version.h"
#include "Thread.h"
#include "M17LSF.h"
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

enum M17_STATUS{
	M17S_NOTLINKED,
	M17S_LINKED,
	M17S_ECHO
};


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

	CM17Network remoteNetwork(m_conf.getCallsign(), m_conf.getSuffix(), m_conf.getNetworkPort(), m_conf.getNetworkDebug());

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

	M17_STATUS status = M17S_NOTLINKED;
	M17_STATUS oldStatus = M17S_NOTLINKED;

	std::string currentReflector;
	unsigned int currentAddrLen = 0U;
	sockaddr_storage currentAddr;

	std::string startupReflector = m_conf.getNetworkStartup();
	bool revert = m_conf.getNetworkRevert();

	if (voice != NULL)
		voice->unlinked();

	if (!startupReflector.empty()) {
		CM17Reflector* refl = reflectors.find(startupReflector);
		if (refl != NULL) {
			char module = startupReflector.at(M17_CALLSIGN_LENGTH - 1U);
			if (module >= 'A' && module <= 'Z') {
				currentReflector = startupReflector;
				currentAddr      = refl->m_addr;
				currentAddrLen   = refl->m_addrLen;

				remoteNetwork.link(currentReflector, currentAddr, currentAddrLen, module);
				status = oldStatus = M17S_LINKED;

				LogInfo("Linked at startup to %s", currentReflector.c_str());

				if (voice != NULL)
					voice->linkedTo(currentReflector);
			}
		} else {
			startupReflector.clear();
		}
	}

	unsigned int n = 1U;

	for (;;) {
		unsigned char buffer[100U];

		if (status == M17S_LINKED) {
			// From the reflector to the MMDVM
			bool ret = remoteNetwork.read(buffer);
			if (ret) {
				CM17LSF lsf;
				lsf.setNetwork(buffer + 6U);

				if (n > 40U) {
					// Change the type to show that it's callsign data
					lsf.setEncryptionType(M17_ENCRYPTION_TYPE_NONE);
					lsf.setEncryptionSubType(M17_ENCRYPTION_SUB_TYPE_CALLSIGNS);

					// Copy the encoded source and destination into the META field
					lsf.setMeta(buffer + 6U);

					if (n > 45U)
						n = 0U;
				}

				n++;

				// Replace the source callsign with our ours
				lsf.setSource(m_conf.getCallsign());

				// Replace the destination callsign with the broadcast callsign
				lsf.setDest("ALL");

				lsf.getNetwork(buffer + 6U);

				localNetwork->write(buffer);

				hangTimer.start();
			}
		} else if (status == M17S_ECHO) {
			// From the echo unit to the MMDVM
			ECHO_STATE est = echo.read(buffer);
			switch (est) {
				case EST_DATA:
					localNetwork->write(buffer);
					hangTimer.start();
					break;
				case EST_EOF:
					// End of the message, restore the original status
					status = oldStatus;
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
				if (status != M17S_ECHO) {
					oldStatus = status;
					echo.clear();
				}

				echo.write(buffer);
				status = M17S_ECHO;
				hangTimer.start();

				uint16_t fn = (buffer[34U] << 8) + (buffer[35U] << 0);
				if ((fn & 0x8000U) == 0x8000U)
					echo.end();
			} else if (dst == "INFO") {
				hangTimer.start();
				triggerVoice = true;
			} else if (dst == "UNLINK") {
				if (status == M17S_LINKED) {
					LogMessage("Unlinking from reflector %s by %s", currentReflector.c_str(), src.c_str());
					remoteNetwork.unlink();
					if (voice != NULL)
						voice->unlinked();
				}

				triggerVoice = true;
				status = oldStatus = M17S_NOTLINKED;
				hangTimer.stop();
			} else if (dst.size() == M17_CALLSIGN_LENGTH) {
				std::string reflector = dst;
				char module = reflector.at(M17_CALLSIGN_LENGTH - 1U);

				if (reflector != currentReflector && module >= 'A' && module <= 'Z') {
					if (status == M17S_LINKED) {
						LogMessage("Unlinking from reflector %s by %s", currentReflector.c_str(), src.c_str());

						remoteNetwork.unlink();
					}

					triggerVoice = true;
				
					CM17Reflector* refl = reflectors.find(reflector);
					if (refl != NULL) {
						currentReflector = reflector;
						currentAddr      = refl->m_addr;
						currentAddrLen   = refl->m_addrLen;

						// Link to the new reflector
						LogMessage("Switched to reflector %s by %s", currentReflector.c_str(), src.c_str());

						remoteNetwork.link(currentReflector, currentAddr, currentAddrLen, module);
						status = oldStatus = M17S_LINKED;

						if (voice != NULL)
							voice->linkedTo(currentReflector);

						hangTimer.start();
					} else {
						status = oldStatus = M17S_NOTLINKED;

						if (voice != NULL)
							voice->unlinked();

						hangTimer.stop();
					}
				}
			}

			// If we're linked and we have a network, send it on
			if (status == M17S_LINKED) {
				// Replace the destination callsign with the reflector name and module
				CM17Utils::encodeCallsign(currentReflector, buffer + 6U);
				remoteNetwork.write(buffer);
				hangTimer.start();
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

					if (reflector != currentReflector) {
						if (status == M17S_LINKED) {
							LogMessage("Unlinked from reflector %s by remote command", currentReflector.c_str());

							remoteNetwork.unlink();

							if (voice != NULL) {
								voice->unlinked();
								voice->eof();
							}

							status = oldStatus = M17S_NOTLINKED;
							hangTimer.stop();
						}

						CM17Reflector* refl = reflectors.find(reflector);
						if (refl != NULL) {
							char module = reflector.at(M17_CALLSIGN_LENGTH - 1U);
							if (module >= 'A' && module <= 'Z') {
								currentReflector = reflector;
								currentAddr      = refl->m_addr;
								currentAddrLen   = refl->m_addrLen;

								// Link to the new reflector
								LogMessage("Switched to reflector %s by remote command", reflector.c_str());

								remoteNetwork.link(currentReflector, currentAddr, currentAddrLen, module);
								status = oldStatus = M17S_LINKED;

								if (voice != NULL) {
									voice->linkedTo(currentReflector);
									voice->eof();
								}

								hangTimer.start();
							}
						} else {
							currentReflector.clear();
							status = oldStatus = M17S_NOTLINKED;
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

		reflectors.clock(ms);

		localNetwork->clock(ms);

		remoteNetwork.clock(ms);

		echo.clock(ms);

		hangTimer.clock(ms);
		if (hangTimer.isRunning() && hangTimer.hasExpired()) {
			if (revert && !startupReflector.empty() && currentReflector != startupReflector) {
				if (status == M17S_LINKED) {
					remoteNetwork.unlink();
					remoteNetwork.unlink();
					remoteNetwork.unlink();
				}

				LogMessage("Relinked from %s to %s due to inactivity", currentReflector.c_str(), startupReflector.c_str());

				CM17Reflector* refl = reflectors.find(startupReflector);
				currentReflector = startupReflector;
				currentAddr      = refl->m_addr;
				currentAddrLen   = refl->m_addrLen;

				char module = startupReflector.at(M17_CALLSIGN_LENGTH - 1U);

				remoteNetwork.link(currentReflector, currentAddr, currentAddrLen, module);
				status = oldStatus = M17S_LINKED;

				if (voice != NULL) {
					voice->linkedTo(startupReflector);
					voice->eof();
				}

				hangTimer.start();
			} else if (revert && startupReflector.empty() && status == M17S_LINKED) {
				LogMessage("Unlinking from %s due to inactivity", currentReflector.c_str());

				remoteNetwork.unlink();
				status = oldStatus = M17S_NOTLINKED;

				if (voice != NULL) {
					voice->unlinked();
					voice->eof();
				}

				currentReflector.clear();

				hangTimer.stop();
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

	if (status == M17S_LINKED)
		remoteNetwork.unlink();
	remoteNetwork.close();

	::LogFinalise();
}
