/*
 * MessageBoxSlave.cpp - an ServiceSlave providing message boxes
 *
 * Copyright (c) 2010-2016 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
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

#include <QMessageBox>

#include "MessageBoxSlave.h"
#include "ItalcSlaveManager.h"


MessageBoxSlave::MessageBoxSlave() :
	ServiceSlave()
{
}



MessageBoxSlave::~MessageBoxSlave()
{
}



bool MessageBoxSlave::handleMessage( const Ipc::Msg &m )
{
	if( m.cmd() == ItalcSlaveManager::MessageBoxSlave::ShowMessageBox )
	{
		if( m.arg( ItalcSlaveManager::MessageBoxSlave::Icon ).toInt() == QMessageBox::Warning )
		{
			QMessageBox::warning( Q_NULLPTR,
								  m.arg( ItalcSlaveManager::MessageBoxSlave::Title ),
								  m.arg( ItalcSlaveManager::MessageBoxSlave::Text ) );
		}
		else if( m.arg( ItalcSlaveManager::MessageBoxSlave::Icon ).toInt() == QMessageBox::Critical )
		{
			QMessageBox::critical( Q_NULLPTR,
								   m.arg( ItalcSlaveManager::MessageBoxSlave::Title ),
								   m.arg( ItalcSlaveManager::MessageBoxSlave::Text ) );
		}
		else
		{
			QMessageBox::information( Q_NULLPTR,
									  m.arg( ItalcSlaveManager::MessageBoxSlave::Title ),
									  m.arg( ItalcSlaveManager::MessageBoxSlave::Text ) );
		}

		return true;
	}

	return false;
}
