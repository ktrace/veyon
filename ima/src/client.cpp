/*
 * client.cpp - code for client-windows, which are displayed in several
 *              instances in the main-window of iTALC
 *
 * Copyright (c) 2004-2007 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
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

 
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtGui/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QFileDialog>
#include <QtGui/QImage>
#include <QtGui/QInputDialog>
#include <QtGui/QLinearGradient>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QToolBar>
#include <QtGui/QToolButton>
#include <QtGui/QWorkspace>


#include "main_window.h"
#include "client.h"
#include "ivs_connection.h"
#include "classroom_manager.h"
#include "cmd_input_dialog.h"
#include "local_system.h"
#include "snapshot_list.h"
#include "dialogs.h"
#include "messagebox.h"
#include "qnetworkinterface.h"



const QSize DEFAULT_CLIENT_SIZE( 256, 192 );



const client::clientCommand client::s_commands[client::CmdCount] =
{

	{ None,			NULL,				"",								"",			FALSE	},
	{ Reload,		&client::reload,		"",								"", 			TRUE	},
	{ ViewLive,		&client::viewLive,		QT_TRANSLATE_NOOP( "client", "View live and fullscreen" ),	"viewmag.png",		FALSE	},
	{ RemoteControl,	&client::remoteControl,		QT_TRANSLATE_NOOP( "client", "Remote control" ),		"remote_control.png",	FALSE	},
	{ ClientDemo,		&client::clientDemo,		QT_TRANSLATE_NOOP( "client", "Let student show demo" ),		"client_demo.png",	FALSE	},
	{ SendTextMessage,	&client::sendTextMessage,	QT_TRANSLATE_NOOP( "client", "Send text-message" ),		"text_message.png",	TRUE	},
	{ LogonUserCmd,		&client::logonUser,		QT_TRANSLATE_NOOP( "client", "Logon user" ),			"multilogon.png",		FALSE	},
	{ LogoutUser,		&client::logoutUser,		QT_TRANSLATE_NOOP( "client", "Logout user" ),			"logout.png",		TRUE	},
	{ Snapshot,		&client::snapshot,		QT_TRANSLATE_NOOP( "client", "Make a snapshot" ),		"snapshot.png", 	TRUE	},
	{ PowerOn,		&client::powerOn,		QT_TRANSLATE_NOOP( "client", "Power on" ),			"power_on.png",		FALSE	},
	{ Reboot,		&client::reboot,		QT_TRANSLATE_NOOP( "client", "Reboot" ),			"reboot.png",		FALSE	},
	{ PowerDown,		&client::powerDown,		QT_TRANSLATE_NOOP( "client", "Power down" ),			"power_off.png",	FALSE	},
	{ ExecCmds,		&client::execCmds,		QT_TRANSLATE_NOOP( "client", "Execute commands" ),		"run.png", 		FALSE	}

} ;


// resolve static symbols...
QHash<int, client *> client::s_clientIDs;

bool client::s_reloadSnapshotList = FALSE;



client::client( const QString & _local_ip, const QString & _remote_ip,
		const QString & _mac, const QString & _name,
		classRoom * _class_room, mainWindow * _main_window, int _id ) :
	QWidget( _main_window->workspace() ),
	m_mainWindow( _main_window ),
	m_connection( NULL ),
	m_name( _name ),
	m_localIP( _local_ip ),
	m_remoteIP( _remote_ip ),
	m_mac( _mac ),
	m_mode( Mode_Overview ),
	m_user( "" ),
	m_makeSnapshot( FALSE ),
	m_state( State_Unkown ),
	m_statePixmap(),
	m_syncMutex(),
	m_classRoomItem( NULL )
{
	m_mainWindow->workspace()->addWindow( this );

	if( _id <= 0 || clientFromID( _id ) != NULL )
	{
		_id = freeID();
	}
	s_clientIDs[_id] = this;


	m_classRoomItem = new classRoomItem( this, _class_room );

	// make sure remote-ip is ok and set to local IP if neccessary
	setRemoteIP( m_remoteIP );

	m_connection = new ivsConnection( m_localIP,
						ivsConnection::QualityLow);

	setWindowIcon( QPixmap( ":/resources/classroom_manager.png" ) );

	setWhatsThis( tr( "This is a client-window. It either displays the "
				"screen of the according client or a message "
				"about the state of this client (no user "
				"logged in/powered off) is shown. You can "
				"click with the right mouse-button and an "
				"action-menu for this client will appear. "
				"You can also close this client-window. "
				"To open it again, open the classroom-manager-"
				"workspace and search this client and double-"
				"click it.\nYou can change the size of this "
				"(and all other visible) client-windows by "
				"using the functions for increasing, "
				"decreasing or optimizing the client-window-"
								"size." ) );

	setFixedSize( DEFAULT_CLIENT_SIZE );
	//resize( DEFAULT_CLIENT_SIZE );
	setAttribute( Qt::WA_NoSystemBackground, TRUE );
	//setAttribute( Qt::WA_PaintUnclipped, TRUE );
/*	QToolBar * tb = new QToolBar( this );
	tb->setAutoFillBackground( TRUE );
	QPalette pal;
	pal.setBrush( QPalette::Background, QColor( 0, 0, 0, 160 ) );
	tb->setPalette( pal );
	tb->setIconSize( QSize( 16, 16 ) );*/
	QMenu * tb = new QMenu( this );
	m_contextMenu = tb;
	connect( tb, SIGNAL( triggered( QAction * ) ), this,
					SLOT( processCmdSlot( QAction * ) ) );
	tb->addAction( scaled( ":/resources/overview_mode.png", 16, 16 ),
			tr( "Watch only (stops demo and unlocks screen)" ) )->
				setData( CmdCount + Mode_Overview );
	tb->addAction( scaled( ":/resources/fullscreen_demo.png", 16, 16 ),
						tr( "Fullscreen demo" ) )->
				setData( CmdCount + Mode_FullscreenDemo );
	tb->addAction( scaled( ":/resources/window_demo.png", 16, 16 ),
						tr( "Window demo" ) )->
				setData( CmdCount + Mode_WindowDemo );
	tb->addAction( scaled( ":/resources/locked.png", 16, 16 ),
						tr( "Locked display" ) )->
				setData( CmdCount + Mode_Locked );
	tb->addSeparator();
	for( int i = ViewLive; i < CmdCount; ++i )
	{
		QAction * a = tb->addAction( scaled(
			QString( ":/resources/" ) + s_commands[i].m_icon,
								16, 16 ),
						tr( s_commands[i].m_name ) );
		a->setData( i );
		if( s_commands[i].m_insertSep )
		{
			tb->addSeparator();
		}
	}

	m_updateThread = new updateThread( this );
}




client::~client()
{
	m_updateThread->quit();
	m_updateThread->wait();
	changeMode( client::Mode_Overview, m_mainWindow->localISD() );
	m_updateThread->update();
	delete m_updateThread;

	m_syncMutex.lock();
	delete m_connection;
	m_connection = NULL;
	m_syncMutex.unlock();

	delete m_classRoomItem;
}




int client::id( void ) const
{
	QHash<int, client *>::const_iterator it;
	for( it = s_clientIDs.begin(); it != s_clientIDs.end(); ++it )
	{
		if( it.value() == this )
		{
			return( it.key() );
		}
	}
	return( 0 );
}




client * client::clientFromID( int _id )
{
	if( s_clientIDs.contains( _id ) )
	{
		return( s_clientIDs[_id] );
	}
	return( NULL );
}




int client::freeID( void )
{
	const int ID_MAX = 1 << 20;

	int id;
	while( s_clientIDs.contains( id = static_cast<int>( (float) rand() /
						RAND_MAX * ID_MAX + 1 ) ) )
	{
	}
	return( id );
}




void client::changeMode( const modes _new_mode, isdConnection * _conn )
{
	if( _new_mode != m_mode )
	{
		//m_syncMutex.lock();
		switch( m_mode )
		{
			case Mode_Overview:
			case Mode_Unknown:
				break;
			case Mode_FullscreenDemo:
			case Mode_WindowDemo:
				_conn->demoServerDenyClient( m_localIP );
				m_updateThread->enqueueCommand(
						updateThread::StopDemo );
				break;
			case Mode_Locked:
				m_updateThread->enqueueCommand(
						updateThread::UnlockScreen );
				break;
		}
		switch( m_mode = _new_mode )
		{
			case Mode_Overview:
			case Mode_Unknown:
				break;
			case Mode_FullscreenDemo:
			case Mode_WindowDemo:
				_conn->demoServerAllowClient( m_localIP );
				m_updateThread->enqueueCommand(
						updateThread::StartDemo,
	QList<QVariant>()
			<< _conn->host() + ":" + QString::number(
						_conn->demoServerPort() )
			<< (int)( m_mode == Mode_FullscreenDemo ) );
				//m_mode = Mode_FullscreenDemo;
				break;
			case Mode_Locked:
				m_updateThread->enqueueCommand(
						updateThread::LockScreen );
				break;
		}
		//m_syncMutex.unlock();
	}
	// if connection was lost while sending commands such as stop-demo,
	// there should be a way for switching back into normal mode, that's
	// why we offer this lines
	else if( m_mode == Mode_Overview )
	{
		if( _conn != NULL )
		{
			_conn->demoServerDenyClient( m_localIP );
		}
		m_updateThread->enqueueCommand( updateThread::StopDemo );
		m_updateThread->enqueueCommand( updateThread::UnlockScreen );
	}
}




void client::setClassRoom( classRoom * _cr )
{
	delete m_classRoomItem;

	m_classRoomItem = new classRoomItem( this, _cr );
}




void client::resetConnection( void )
{
	m_updateThread->enqueueCommand( updateThread::ResetConnection,
								m_localIP );
}




void client::update( void )
{
/*	QString u = m_user;
	if( u.isEmpty() )
	{
		u = "none";
	}
	if( u.contains( '(' ) && u.contains( ')' ) )
	{
		u = u.section( '(', 1, 1 ).section( ')', 0, 0 );
	}
	setWindowTitle( u + "@" + fullName() );*/
	setWindowTitle( m_name + " (" + m_classRoomItem->parent()->text( 0 ) +
									")" );
	states cur_state = currentState();
	if( cur_state != m_state )
	{
		m_state = cur_state;
		updateStatePixmap();
	}
	QWidget::update();
}




void client::createActionMenu( QMenu * _m )
{
	connect( _m, SIGNAL( triggered( QAction * ) ), this,
					SLOT( processCmdSlot( QAction * ) ) );
	_m->addActions( m_contextMenu->actions() );
}




void client::processCmd( clientCmds _cmd, const QString & _u_data )
{
	if( _cmd < 0 || _cmd >= CmdCount )
	{
		return;
	}

	( this->*( client::s_commands[_cmd].m_exec ) )( _u_data );
}




void client::processCmdSlot( QAction * _action )
{
	int a = _action->data().toInt();
	if( a >= ViewLive && a < CmdCount )
	{
		processCmd( static_cast<clientCmds>( a ) );
	}
	else if( a >= CmdCount+Mode_Overview && a < CmdCount+Mode_Unknown )
	{
		changeMode( static_cast<modes>( a-CmdCount ),
						m_mainWindow->localISD() );
	}
}




void client::updateStatePixmap( void )
{
	m_statePixmap = QPixmap( size() );
	QPainter p( &m_statePixmap );

	QLinearGradient grad( 0, 0, 0, height() );
	grad.setColorAt( 0, QColor( 224, 224, 224 ) );
	grad.setColorAt( 1, QColor( 128, 128, 128 ) );
	p.fillRect( rect(), grad );//QColor( 255, 255, 255 ) );

	QPixmap pm = QPixmap( ":/resources/error.png" );
	QString msg = tr( "Unknown state" );

	switch( m_state )
	{
		case State_Overview:
			return;
		case State_NoUserLoggedIn:
			pm = QPixmap( ":/resources/no_user.png" );
			msg = tr( "No user logged in" );
			break;
		case State_Unreachable:
			pm = QPixmap( ":/resources/host_unreachable.png" );
			msg = tr( "Host unreachable" );
			break;
		case State_Demo:
			pm = QPixmap( ":/resources/window_demo.png" );
			msg = tr( "Demo running" );
			break;
		case State_Locked:
			pm = QPixmap( ":/resources/locked.png" );
			msg = tr( "Desktop locked" );
			break;
		default:
			break;
	}

	QSize s( pm.size() );
	s.scale( width() * 2 / 3, height() * 2 / 3, Qt::KeepAspectRatio );
	p.drawImage( 5, 5, fastQImage( pm ).scaled( s ) );

	QFont f = p.font();
	f.setBold( TRUE );
	f.setPointSize( f.pointSize() + 4 );
	p.setFont( f );
	p.setPen( QColor( 0, 0, 0 ) );
	p.drawText( QRect( 5, 10 + s.height(), width() - 10,
						height() - 15 - s.height() ),
				Qt::TextWordWrap | Qt::AlignRight, msg );
}




bool client::userLoggedIn( void )
{
	QMutexLocker ml( &m_syncMutex );

	return( m_connection->state() == ivsConnection::Connected ||
		m_connection->reset( m_localIP ) == ivsConnection::Connected );
}




void client::closeEvent( QCloseEvent * _ce )
{
	// make sure, client isn't forgotten by teacher after being hidden
	changeMode( Mode_Overview, m_mainWindow->localISD() );
	hide();
	_ce->ignore();
}




void client::contextMenuEvent( QContextMenuEvent * )
{
	m_contextMenu->exec( QCursor::pos() );
}




void client::hideEvent( QHideEvent * )
{
	if( isMinimized() )
	{
		hide();
	}

	if( m_classRoomItem != NULL )
	{
		m_classRoomItem->setVisible( FALSE );
	}
}




void client::mouseDoubleClickEvent( QMouseEvent * _me )
{
	remoteControl( "" );
}




void client::paintEvent( QPaintEvent * _pe )
{
	QPainter p( this );

	if( m_connection->state() == ivsConnection::Connected/* && m_user != ""*/ &&
						m_mode == Mode_Overview )
	{
		p.drawImage( _pe->rect().topLeft(),
				m_connection->scaledScreen(), _pe->rect() );
	}
	else
	{
		p.drawPixmap( 0, 0, m_statePixmap );
	}

	if( m_makeSnapshot )
	{
		QMutexLocker ml( &m_syncMutex );
		m_makeSnapshot = FALSE;

		if( m_user != "" &&
			m_connection->state() == ivsConnection::Connected )
		{
			// construct text
			QString txt = m_user + "@" + m_name + " (" +
					m_localIP + ") " +
					QDate( QDate::currentDate()
						).toString( Qt::ISODate ) +
					" " +
					QTime( QTime::currentTime() ).toString(
								Qt::ISODate );
			const QString dir = localSystem::snapshotDir();
			if( !localSystem::ensurePathExists( dir ) )
			{
				messageBox::information( tr( "Snapshot" ),
		tr( "Could not make a snapshot as directory %1 doesn't exist "
			"and couldn't be created." ).arg( dir ) );
				return;
			}
			// construct filename
			QString file_name =  "_" +
						m_localIP + "_" +
			QDate( QDate::currentDate() ).toString( Qt::ISODate ) +
						"_" +
		QTime( QTime::currentTime() ).toString( Qt::ISODate ) + ".png";
			file_name.replace( ':', '-' );
			file_name = dir + m_user.section( '(', 1, 1 ).
					section( ')', 0, 0 ) + file_name;
			const int FONT_SIZE = 14;
			const int RECT_MARGIN = 10;
			const int RECT_INNER_MARGIN = 5;

			QImage screen( m_connection->screen() );

			QPixmap italc_icon( QPixmap(
					":/resources/client_observed.png" ) );

			QPainter p( &screen );
			QFont fnt = p.font();
			fnt.setPointSize( FONT_SIZE );
			fnt.setBold( TRUE );
			p.setFont( fnt );
			QFontMetrics fm( p.font() );

			const int rx = RECT_MARGIN;
			const int ry = screen.height() - RECT_MARGIN -
					2 * RECT_INNER_MARGIN - FONT_SIZE;
			const int rw = RECT_MARGIN + 4 * RECT_INNER_MARGIN +
				fm.size( Qt::TextSingleLine, txt ).width() +
							italc_icon.width();
			const int rh = 2 * RECT_INNER_MARGIN + FONT_SIZE;
			const int ix = rx + RECT_INNER_MARGIN;
			const int iy = ry + RECT_INNER_MARGIN;
			const int tx = ix + italc_icon.width() +
							2 * RECT_INNER_MARGIN;
			const int ty = ry + RECT_INNER_MARGIN + FONT_SIZE - 2;

			p.fillRect( rx, ry, rw, rh, QColor(
							255, 255, 255, 128 ) );
			p.drawPixmap( ix, iy, italc_icon );
			p.drawText( tx, ty, txt );
			screen.save( file_name, "PNG", 50 );

			s_reloadSnapshotList = TRUE;
		}
	}


}




void client::resizeEvent( QResizeEvent * )
{
	if( findChild<QToolBar *>() )
	{
		findChild<QToolBar *>()->setFixedWidth( width() );
	}
	m_connection->setScaledSize( size() );
	updateStatePixmap();
}





void client::showEvent( QShowEvent * )
{
	if( m_classRoomItem != NULL )
	{
		m_classRoomItem->setVisible( TRUE );
	}
}




void client::reload( const QString & _update )
{
	bool available = FALSE;

	if( userLoggedIn() )
	{
		m_syncMutex.lock();
		if( m_user.isEmpty() || m_user == "none" ||
							m_user == "unknown" )
		{
			m_connection->sendGetUserInformationRequest();
		}
		//if( m_connection->sendGetUserInformationRequest() )
		{
			// only send a framebuffer-update-request if client
			// is in (over)view-mode
			m_connection->handleServerMessages(
						m_mode == Mode_Overview );
		}

		m_user = m_connection->user();
		m_syncMutex.unlock();

		if( m_user != "" )
		{
			available = TRUE;
			if( toolTip() != m_user )
			{
				setToolTip( m_user );
			}
		}
	}

	if( available == FALSE )
	{
		setToolTip( "" );
	}


	// if we are called out of draw-thread, we may update...
	if( _update == CONFIRM_YES )
	{
		update();
	}
}




void client::clientDemo( const QString & )
{
	classroomManager * cm = m_mainWindow->getClassroomManager();
	cm->changeGlobalClientMode( Mode_Overview );

	isdConnection * conn = m_mainWindow->localISD();
	QVector<client *> vc = cm->visibleClients();

	foreach( client * cl, vc )
	{
		if( cl != this )
		{
			cl->changeMode( Mode_FullscreenDemo, conn );
		}
	}

	m_mainWindow->checkModeButton( client::Mode_FullscreenDemo );

	conn->remoteControlDisplay( m_localIP );
}




void client::viewLive( const QString & )
{
	changeMode( Mode_Overview, m_mainWindow->localISD() );
	//m_syncMutex.lock();

	m_mainWindow->localISD()->remoteControlDisplay( m_localIP, TRUE );

	//m_syncMutex.unlock();
}




void client::remoteControl( const QString & )
{
	changeMode( Mode_Overview, m_mainWindow->localISD() );
	//m_syncMutex.lock();

	m_mainWindow->localISD()->remoteControlDisplay( m_localIP );

	//m_syncMutex.unlock();
}




void client::sendTextMessage( const QString & _msg )
{
	if( _msg.isEmpty() )
	{
		QString msg;

		textMessageDialog tmd( msg, this );
		if( tmd.exec() == QDialog::Accepted && msg != "" )
		{
			sendTextMessage( msg );
		}
	}
	else
	{
		m_updateThread->enqueueCommand( updateThread::SendTextMessage,
									_msg );
	}
}



#if 0
void client::distributeFile( const QString & _file )
{
/*	QMessageBox::information( this, tr( "Function not implemented yet." ), tr( "This function is not completely implemented yet. This is why it is disabled at the moment." ), QMessageBox::Ok );
	return;*/

	if( _file == "" )
	{
		QFileDialog ofd( this, tr( "Select file to distribute" ),
							QDir::homePath() );
		ofd.setFileMode( QFileDialog::ExistingFile );
		if( ofd.exec() == QDialog::Accepted &&
					ofd.selectedFiles().empty() == FALSE )
		{
			distributeFile( ofd.selectedFiles().front() );
		}
	}
	else
	{
		m_syncMutex.lock ();
		m_connection->sendFile( _file );
		m_syncMutex.unlock();
	}
}




void client::collectFiles( const QString & _filter )
{
/*	QMessageBox::information( this, tr( "Function not implemented yet."), tr("This function is not completely implemented yet. This is why it is disabled at the moment."), QMessageBox::Ok);
	return;*/

	if( _filter.isEmpty() )
	{
		bool ok;
		QString f = QInputDialog::getText( this, tr( "Collect files" ),
						tr( "Please enter the name of "
							"the file to be "
							"collected.\nOnly "
							"files located in the "
							"PUBLIC-directory are "
							"allowed." ),
						QLineEdit::Normal, "", &ok );
		if( ok && !f.isEmpty() )
		{
			collectFiles( f );
		}
	}
	else
	{
		m_syncMutex.lock ();
		m_connection->collectFiles( _filter );
		m_syncMutex.unlock();
	}
}
#endif



void client::logonUser( const QString & _uname_and_pw )
{

	if( _uname_and_pw.isEmpty() )
	{
		multiLogonDialog mld( this );
		if( mld.exec() == QDialog::Accepted &&
			!mld.userName().isEmpty() && !mld.password().isEmpty() )
		{
			logonUser( mld.userName() + "*" + mld.password() +
							"*" + mld.domain() );
		}
	}
	else
	{
		m_updateThread->enqueueCommand( updateThread::LogonUserCmd,
								_uname_and_pw );
	}
}




void client::logoutUser( const QString & _confirm )
{
	m_updateThread->enqueueCommand( updateThread::LogoutUser );
}




void client::snapshot( const QString & )
{
	m_makeSnapshot = TRUE;
}





void client::powerOn( const QString & )
{
/*	QString bcast;
	QList<QNetworkInterface> ifs = QNetworkInterface::allInterfaces();
	for( QList<QNetworkInterface>::const_iterator it = ifs.begin();
				it != ifs.end() && !bcast.isEmpty(); ++it )
	{
		QList<QNetworkAddressEntry> nae = it->addressEntries();
		for( QList<QNetworkAddressEntry>::const_iterator it2 =
								nae.begin();
						it2 != nae.end(); ++it2 )
		{
			if( !( it2->ip() ==
				QHostAddress( QHostAddress::LocalHost ) ) )
			{
				bcast = it2->broadcast().toString();
				break;
			}
		}
	}

	if( bcast.isEmpty() )
	{
		bcast = QHostAddress( QHostAddress::Broadcast ).toString();
	}*/

	m_mainWindow->localISD()->wakeOtherComputer( m_mac );
}




void client::reboot( const QString & _confirm )
{
/*	if( userLoggedIn() )
	{
		if( QMessageBox::warning( this, tr( "User logged in" ),
			tr( "Warning: you are trying to reboot a client at "
				"which a user is logged in! Continue anyway?" ),
						QMessageBox::Yes,
						QMessageBox::No ) ==
							QMessageBox::No )
		{
			return;
		}
	}
	else if( _confirm == CONFIRM_YES || _confirm.isEmpty() )
	{
		if( QMessageBox::question( this, tr( "Reboot client" ),
			tr( "Are you sure want to reboot selected client?" ),
						QMessageBox::Yes,
						QMessageBox::No ) ==
							QMessageBox::No )
		{
			return;
		}
	}*/
	m_updateThread->enqueueCommand( updateThread::Reboot );
}





void client::powerDown( const QString & _confirm )
{
/*	if( userLoggedIn() )
	{
		if( QMessageBox::warning( this, tr( "User logged in" ),
			tr( "Warning: you are trying to power off a client at "
				"which a user is logged in! Continue anyway?" ),
						QMessageBox::Yes,
						QMessageBox::No ) ==
							QMessageBox::No )
		{
			return;
		}
	}
	else if( _confirm == CONFIRM_YES || _confirm == "" )
	{
		if( QMessageBox::question( this, tr( "Power off client" ),
			tr( "Are you sure want to power off selected client?" ),
						QMessageBox::Yes,
						QMessageBox::No ) ==
							QMessageBox::No )
		{
			return;
		}
	}*/
	m_updateThread->enqueueCommand( updateThread::PowerDown );
}




void client::execCmds( const QString & _cmds )
{
	if( _cmds.isEmpty() )
	{
		QString cmds;

		cmdInputDialog cmd_input_dialog( cmds, this );
		if( cmd_input_dialog.exec() == QDialog::Accepted &&
							!cmds.isEmpty() )
		{
			execCmds( cmds );
		}
	}
	else
	{
		m_updateThread->enqueueCommand( updateThread::ExecCmds, _cmds );
	}
}



client::states client::currentState( void ) const
{
	//QMutexLocker m( &m_syncMutex );
	switch( m_mode )
	{
		case Mode_Overview:
			if( m_connection->state() == ivsConnection::Connected )
			{
				return( State_Overview );
			}
			else if( m_connection->state() ==
					ivsConnection::ConnectionRefused )
			{
				return( State_NoUserLoggedIn );
			}
			return( State_Unreachable );

		case Mode_FullscreenDemo:
		case Mode_WindowDemo:
			return( State_Demo );

		case Mode_Locked:
			return( State_Locked );

		default:
			break;
	}

	return( State_Unkown );
}




updateThread::updateThread( client * _client ) :
	QThread(),
	m_client( _client )
{
	start( LowPriority );
}



void updateThread::update( void )
{
	if( mainWindow::atExit() == FALSE && m_client->isVisible() )
	{
		m_client->processCmd( client::Reload, CONFIRM_NO );
	}

	m_queueMutex.lock();
	m_client->m_syncMutex.lock();
	while( !m_queue.isEmpty() )
	{
		const queueItem i = m_queue.dequeue();
		m_queueMutex.unlock();
		switch( i.first )
		{
			case ResetConnection:
				m_client->m_connection->reset(
						i.second.toString() );
				break;
			case StartDemo:
				m_client->m_connection->startDemo(
					i.second.toList()[0].toString(),
					i.second.toList()[1].toInt() );
				break;
			case StopDemo:
				m_client->m_connection->stopDemo();
				break;
			case LockScreen:
				m_client->m_connection->lockDisplay();
				break;
			case UnlockScreen:
				m_client->m_connection->unlockDisplay();
				break;
			case SendTextMessage:
				m_client->m_connection->
					displayTextMessage(
						i.second.toString() );
				break;
			case LogonUserCmd:
			{
				const QString s = i.second.toString();
				const int pos = s.indexOf( '*' );
				const int pos2 = s.lastIndexOf( '*' );
				m_client->m_connection->logonUser(
						s.left( pos ),
						s.mid( pos + 1,
							pos2-pos-1 ),
						s.mid( pos2 + 1 ) );
				break;
			}
			case LogoutUser:
				m_client->m_connection->logoutUser();
				break;
			case Reboot:
				m_client->m_connection->
						restartComputer();
				break;
			case PowerDown:
				m_client->m_connection->
						powerDownComputer();
				break;
			case ExecCmds:
				m_client->m_connection->execCmds(
						i.second.toString() );
				break;
		}
		m_queueMutex.lock();
	}
	m_client->m_syncMutex.unlock();
	m_queueMutex.unlock();
	
	if( m_client->isVisible() == FALSE &&
		m_client->m_connection->state() == ivsConnection::Connected )
	{
		m_client->m_connection->close();
	}
}




void updateThread::run( void )
{
	QTimer t;
	connect( &t, SIGNAL( timeout() ), this, SLOT( update() ),
							Qt::DirectConnection );
	t.start( m_client->m_mainWindow->getClassroomManager()->updateInterval() *
									1000 );
	exec();
}



#include "client.moc"

