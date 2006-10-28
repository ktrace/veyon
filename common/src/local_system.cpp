/*
 * local_system.cpp - namespace localSystem, providing an interface for
 *                    transparent usage of operating-system-dependent functions
 *
 * Copyright (c) 2006 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
 *
 * This file is part of iTALC - http://italc.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QMutex>
#include <QtCore/QProcess>
#include <QtCore/QSettings>
#include <QtNetwork/QTcpServer>


#ifdef BUILD_WIN32

#include <QtCore/QThread>

#define _WIN32_WINNT 0x0501
#include <windows.h>

#if _WIN32_WINNT >= 0x500
#define SHUTDOWN_FLAGS (EWX_FORCE | EWX_FORCEIFHUNG)
#else
#define SHUTDOWN_FLAGS (EWX_FORCE)
#endif

#if _WIN32_WINNT >= 0x501
#include <reason.h>
#define SHUTDOWN_REASON (SHTDN_REASON_MAJOR_SYSTEM/* SHTDN_REASON_MINOR_ENVIRONMENT*/)
#else
#define SHUTDOWN_REASON 0
#endif



class userPollThread : public QThread
{
public:
	userPollThread() : QThread()
	{
		start( LowPriority );
	}

	const QString & name( void ) const
	{
		QMutexLocker m( &m_mutex );
		return( m_name );
	}

private:
	virtual void run( void )
	{
		while( 1 )
		{
			QProcess p;
			p.start( "userinfo" );
			if( p.waitForFinished() )
			{
				QMutexLocker m( &m_mutex );
				( m_name = p.readAll() ).chop( 2 );
			}
		}
	}

	QString m_name;
	mutable QMutex m_mutex;

}  static * __user_poll_thread = NULL;


#else

/*
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_NETINET_ETHER_H
#include <netinet/ether.h>
#endif

#ifdef HAVE_LINUX_IF_H
#include <linux/if.h>
#endif

#ifdef HAVE_NETPACKET_PACKET_H
#include <netpacket/packet.h>
#endif

#define USE_SEND
*/
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif


#endif


#include "local_system.h"



namespace localSystem
{


void initialize( void )
{
	QCoreApplication::setOrganizationName( "EasySchoolSolutions" );
	QCoreApplication::setOrganizationDomain( "ess.org" );
	QCoreApplication::setApplicationName( "iTALC" );

#ifdef BUILD_WIN32
	__user_poll_thread = new userPollThread();
#endif

}




int freePort( void )
{
	QTcpServer t;
	t.listen( QHostAddress::LocalHost );
	return( t.serverPort() );
}




void sleep( const int _ms )
{
#ifdef BUILD_WIN32
	Sleep( static_cast<unsigned int>( _ms ) );
#else
	struct timespec ts = { _ms / 1000, ( _ms % 1000 ) * 1000 * 1000 } ;
	nanosleep( &ts, NULL );
#endif
}




void execInTerminal( const QString & _cmds )
{
	QProcess::startDetached(
#ifdef BUILD_WIN32
			"cmd " +
#else
			"xterm -e " +
#endif
			_cmds );
}




void sendWakeOnLANPacket( const QString & _mac, const QString & _bcast )
{
#ifdef BUILD_WIN32
	QString mac = _mac;
	QProcess::startDetached( "wake " + mac.replace( ':', '-' ) + " " +
							_bcast );

#else
	QProcess::startDetached( "etherwake " + _mac );
#if 0
	const char * ifname = "eth0";

	unsigned char outpack[1000];

	int opt_no_src_addr = 0, opt_broadcast = 0;

#if defined(PF_PACKET)
	struct sockaddr_ll whereto;
#else
	struct sockaddr whereto;	// who to wake up
#endif
	struct ether_addr eaddr;


	const int wol_passwd_size = 6;
	unsigned char wol_passwd[wol_passwd_size];

	const char * mac = _mac.toAscii().constData();

	if( sscanf( mac, "%2x:%2x:%2x:%2x:%2x:%2x",
				(unsigned int *) &wol_passwd[0],
				(unsigned int *) &wol_passwd[1],
				(unsigned int *) &wol_passwd[2],
				(unsigned int *) &wol_passwd[3],
				(unsigned int *) &wol_passwd[4],
				(unsigned int *) &wol_passwd[5] ) != wol_passwd_size )
	{
		printf( "Invalid MAC-address\n" );
		return;
	}


	/* Note: PF_INET, SOCK_DGRAM, IPPROTO_UDP would allow SIOCGIFHWADDR to
	   work as non-root, but we need SOCK_PACKET to specify the Ethernet
	   destination address. */

#if defined(PF_PACKET)
	int s = socket( PF_PACKET, SOCK_RAW, 0 );
#else
	int s = socket( AF_INET, SOCK_PACKET, SOCK_PACKET );
#endif
	if( s < 0 )
	{
		if( errno == EPERM )
		{
			fprintf( stderr, "ether-wake: This program must be run as root.\n" );
		}
		else
		{
			perror( "ether-wake: socket" );
		}
		return;
	}

#if 0
	/* We look up the station address before reporting failure so that
	   errors may be reported even when run as a normal m_user.
	*/
	struct ether_addr * eap = ether_aton( mac );
	if( eap )
	{
		eaddr = *eap;
		printf( "The target station address is %s.\n",
							ether_ntoa( eap ) );
/*	} else if (ether_hostton(m_mac.ascii(), &eaddr) == 0) {
		fprintf(stderr, "Station address for hostname %s is %s.\n",
					m_mac.ascii(), ether_ntoa(&eaddr));*/
	}
	else
	{
		printf ("Invalid MAC-address\n");
		return;
	}
#endif


	if( opt_broadcast )
	{
		memset( outpack+0, 0xff, 6 );
	}
	else
	{
		memcpy( outpack, eaddr.ether_addr_octet, 6 );
	}

	memcpy( outpack+6, eaddr.ether_addr_octet, 6 );
	outpack[12] = 0x08;				/* Or 0x0806 for ARP, 0x8035 for RARP */
	outpack[13] = 0x42;
	int pktsize = 14;

	memset( outpack+pktsize, 0xff, 6 );
	pktsize += 6;

	for( int i = 0; i < 16; i++ )
	{
		memcpy( outpack+pktsize, eaddr.ether_addr_octet, 6 );
		pktsize += 6;
	}
//	fprintf(stderr, "Packet is ");
//	for (i = 0; i < pktsize; i++)
//		fprintf(stderr, " %2.2x", outpack[i]);
//	fprintf(stderr, ".\n");

	/* Fill in the source address, if possible.
	   The code to retrieve the local station address is Linux specific. */
	if( !opt_no_src_addr )
	{
		struct ifreq if_hwaddr;
		//unsigned char * hwaddr = if_hwaddr.ifr_hwaddr.sa_data;

		strcpy( if_hwaddr.ifr_name, ifname );
		if( ioctl( s, SIOCGIFHWADDR, &if_hwaddr ) < 0 )
		{
			fprintf( stderr, "SIOCGIFHWADDR on %s failed: %s\n", ifname, strerror( errno ) );
			/* Magic packets still work if our source address is bogus, but
			   we fail just to be anal. */
			return;
		}
		memcpy( outpack+6, if_hwaddr.ifr_hwaddr.sa_data, 6 );

		//printf("The hardware address (SIOCGIFHWADDR) of %s is type %d  %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x.\n", ifname, if_hwaddr.ifr_hwaddr.sa_family, hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
	}

	memcpy( outpack+pktsize, wol_passwd, wol_passwd_size );
	pktsize += wol_passwd_size;



#if defined(PF_PACKET)
	struct ifreq ifr;
	strncpy( ifr.ifr_name, ifname, sizeof( ifr.ifr_name ) );
	if( ioctl( s, SIOCGIFINDEX, &ifr ) == -1 )
	{
		fprintf( stderr, "SIOCGIFINDEX on %s failed: %s\n", ifname,
							strerror( errno ) );
		return;
	}
	memset( &whereto, 0, sizeof( whereto ) );
	whereto.sll_family = AF_PACKET;
	whereto.sll_ifindex = ifr.ifr_ifindex;
	/* The manual page incorrectly claims the address must be filled.
	   We do so because the code may change to match the docs. */
	whereto.sll_halen = ETH_ALEN;
	memcpy( whereto.sll_addr, outpack, ETH_ALEN );

#else
	whereto.sa_family = 0;
	strcpy( whereto.sa_data, ifname );
#endif

	if( sendto( s, outpack, pktsize, 0, (struct sockaddr *) &whereto,
						sizeof( whereto ) ) < 0 )
	{
		perror( "sendto" );
	}

#ifdef USE_SEND
	if( bind( s, (struct sockaddr *) &whereto, sizeof( whereto ) ) < 0 )
	{
		perror( "bind" );
	}
	else if( send( s, outpack, 100, 0 ) < 0 )
	{
		perror( "send" );
	}
#elif USE_SENDMSG
	struct msghdr msghdr = { 0,};
	struct iovec iovector[1];
	msghdr.msg_name = &whereto;
	msghdr.msg_namelen = sizeof( whereto );
	msghdr.msg_iov = iovector;
	msghdr.msg_iovlen = 1;
	iovector[0].iov_base = outpack;
	iovector[0].iov_len = pktsize;
	if( ( i = sendmsg( s, &msghdr, 0 ) ) < 0 )
	{
		perror( "sendmsg" );
	}
	else if( debug )
	{
		printf( "sendmsg worked, %d (%d).\n", i, errno );
	}
#else

#error no method for sending raw-packet

#endif

#endif

#endif
}



void powerDown( void )
{
#ifdef BUILD_WIN32
	//ExitWindowsEx( EWX_POWEROFF | SHUTDOWN_FLAGS, SHUTDOWN_REASON );
	enablePrivilege( SE_SHUTDOWN_NAME, TRUE );
	InitiateSystemShutdown( NULL,	// local machine
				NULL,	// message for shutdown-box
				0,	// no timeout or possibility to abort
					// system-shutdown
				TRUE,	// force closing all apps
				FALSE	// do not reboot
				);
	enablePrivilege( SE_SHUTDOWN_NAME, FALSE );
#else
	QProcess::startDetached( "halt" );
#endif
}




void reboot( void )
{
#ifdef BUILD_WIN32
	//ExitWindowsEx( EWX_REBOOT | SHUTDOWN_FLAGS, SHUTDOWN_REASON );
	enablePrivilege( SE_SHUTDOWN_NAME, TRUE );
	InitiateSystemShutdown( NULL,	// local machine
				NULL,	// message for shutdown-box
				0,	// no timeout or possibility to abort
					// system-shutdown
				TRUE,	// force closing all apps
				TRUE	// reboot
				);
	enablePrivilege( SE_SHUTDOWN_NAME, FALSE );
#else
	QProcess::startDetached( "reboot" );
#endif
}



void logoutUser( void )
{
#ifdef BUILD_WIN32
	ExitWindowsEx( EWX_LOGOFF | SHUTDOWN_FLAGS, SHUTDOWN_REASON );
#else
	QProcess::startDetached( "killall Xorg" );
#endif
}



QString currentUser( void )
{
	QString ret = "unknown";

#ifdef BUILD_WIN32

	if( !__user_poll_thread->name().isEmpty() )
	{
		ret = __user_poll_thread->name();
	}

#else

	char * user_name = getenv( "USER" );
#ifdef HAVE_PWD_H
	struct passwd * pw_entry = NULL;
	if( user_name )
	{
		pw_entry = getpwnam( user_name );
	}
	if( !pw_entry )
	{
		pw_entry = getpwuid( getuid() );
	}
	if( pw_entry )
	{
		return( QString( "%1 (%2)" ).arg( pw_entry->pw_gecos ).
						arg( pw_entry->pw_name ) );
	}
#endif
	if( user_name )
	{
		return user_name;
	}

#endif

	return( ret );
}



static const QString userRoleNames[] =
{
	"none",
	"teacher",
	"admin",
	"supporter",
	"other"
} ;


QString userRoleName( const ISD::userRoles _role )
{
	return( userRoleNames[_role] );
}


inline QString keyPath( const ISD::userRoles _role, const QString _group )
{
	QSettings settings( QSettings::SystemScope, "EasySchoolSolutions",
								"iTALC" );
	if( _role <= ISD::RoleNone || _role >= ISD::RoleCount )
	{
		printf( "invalid role\n" );
		return( "" );
	}
	const QString fallback_dir =
#ifdef BUILD_LINUX
		"/etc/italc/keys/"
#elif BUILD_WIN32
		"c:\\italc\\keys\\"
#endif
		+ _group + QDir::separator() + userRoleNames[_role] +
						QDir::separator() + "key";
	const QString val = settings.value( "keypaths" + _group + "/" +
					userRoleNames[_role] ).toString();
	if( val.isEmpty() )
	{
		settings.setValue( "keypaths" + _group + "/" +
					userRoleNames[_role], fallback_dir );
		return( fallback_dir );
	}
	return( val );
}


QString privateKeyPath( const ISD::userRoles _role )
{
	return( keyPath( _role, "private" ) );
}


QString publicKeyPath( const ISD::userRoles _role )
{
	return( keyPath( _role, "public" ) );
}




QString snapshotDir( void )
{
	QSettings settings;
	return( settings.value( "paths/snapshots", personalConfigDir() +
						"snapshots" ).toString() +
							QDir::separator() );
}




QString globalConfigPath( void )
{
	QSettings settings;
	return( settings.value( "paths/globalconfig", personalConfigDir() +
					"globalconfig.xml" ).toString() );
}




QString personalConfigDir( void )
{
	QSettings settings;
	const QString d = settings.value( "paths/personalconfig" ).toString();
	return( d.isEmpty() ?
				QDir::homePath() + QDir::separator() +
				".italc" + QDir::separator()
			:
				d );
}




QString personalConfigPath( void )
{
	QSettings settings;
	const QString d = settings.value( "paths/personalconfig" ).toString();
	return( d.isEmpty() ?
				personalConfigDir() + "personalconfig.xml"
			:
				d );
}


bool ensurePathExists( const QString & _path )
{
	if( QDir( _path ).exists() )
	{
		return( TRUE );
	}
	QString p = QDir( _path ).absolutePath();
	if( !QFileInfo( _path ).isDir() )
	{
		p = QFileInfo( _path ).absolutePath();
	}
	QStringList dirs;
	while( !QDir( p ).exists() && !p.isEmpty() )
	{
		dirs.push_front( QDir( p ).dirName() );
		p.chop( dirs.front().size() + 1 );
	}
	if( !p.isEmpty() )
	{
		return( QDir( p ).mkpath( dirs.join( QDir::separator() ) ) );
	}
	return( FALSE );
}



#ifdef BUILD_WIN32
BOOL enablePrivilege( LPCTSTR lpszPrivilegeName, BOOL bEnable )
{
	HANDLE			hToken;
	TOKEN_PRIVILEGES	tp;
	LUID			luid;
	BOOL			ret;

	if( !OpenProcessToken( GetCurrentProcess(),
	      TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_READ, &hToken ) )
		return FALSE;

	if( !LookupPrivilegeValue( NULL, lpszPrivilegeName, &luid ) )
		return FALSE;

	tp.PrivilegeCount           = 1;
	tp.Privileges[0].Luid       = luid;
	tp.Privileges[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;

	ret = AdjustTokenPrivileges( hToken, FALSE, &tp, 0, NULL, NULL );

	CloseHandle( hToken );

	return ret;
}
#endif


} // end of namespace localSystem

