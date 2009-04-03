/*
  Copyright (C) 2008 by Eike Hein <hein@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License or (at your option) version 3 or any later version
  accepted by the membership of KDE e.V. (or its successor appro-
  ved by the membership of KDE e.V.), which shall act as a proxy 
  defined in Section 14 of version 3 of the license.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see http://www.gnu.org/licenses/.
*/


#include "sessionstack.h"
#include "settings.h"

#include <KMessageBox>
#include <KLocalizedString>

#include <QtDBus/QtDBus>


SessionStack::SessionStack(QWidget* parent) : QStackedWidget(parent)
{
    QDBusConnection::sessionBus().registerObject("/yakuake/sessions", this, QDBusConnection::ExportScriptableSlots);

    m_activeSessionId = -1;
}

SessionStack::~SessionStack()
{
}

void SessionStack::addSession(Session::SessionType type)
{
    Session* session = new Session(type, this);
    connect(session, SIGNAL(destroyed(int)), this, SLOT(cleanup(int)));
    connect(session, SIGNAL(titleChanged(int, const QString&)),
        this, SIGNAL(titleChanged(int, const QString&)));

    addWidget(session->widget());

    m_sessions.insert(session->id(), session);

    if (Settings::dynamicTabTitles())
        emit sessionAdded(session->id(), session->title());
    else
        emit sessionAdded(session->id());
}

void SessionStack::addSessionTwoHorizontal()
{
addSession(Session::TwoHorizontal);
}

void SessionStack::addSessionTwoVertical()
{
    addSession(Session::TwoVertical);
}

void SessionStack::addSessionQuad()
{
    addSession(Session::Quad);
}

void SessionStack::raiseSession(int sessionId)
{
    if (sessionId == -1 || !m_sessions.contains(sessionId)) return;

    if (m_activeSessionId != -1 && m_sessions.contains(m_activeSessionId))
    {
        disconnect(m_sessions[m_activeSessionId], SLOT(closeTerminal()));
        disconnect(m_sessions[m_activeSessionId], SLOT(focusPreviousTerminal()));
        disconnect(m_sessions[m_activeSessionId], SLOT(focusNextTerminal()));
        disconnect(m_sessions[m_activeSessionId], SLOT(manageProfiles()));
        disconnect(m_sessions[m_activeSessionId], SIGNAL(titleChanged(const QString&)),
            this, SIGNAL(activeTitleChanged(const QString&)));
    }

    m_activeSessionId = sessionId;

    setCurrentWidget(m_sessions[sessionId]->widget());

    if (m_sessions[sessionId]->widget()->focusWidget())
        m_sessions[sessionId]->widget()->focusWidget()->setFocus();

    connect(this, SIGNAL(closeTerminal()), m_sessions[sessionId], SLOT(closeTerminal()));
    connect(this, SIGNAL(previousTerminal()), m_sessions[sessionId], SLOT(focusPreviousTerminal()));
    connect(this, SIGNAL(nextTerminal()), m_sessions[sessionId], SLOT(focusNextTerminal()));
    connect(this, SIGNAL(manageProfiles()), m_sessions[sessionId], SLOT(manageProfiles()));
    connect(m_sessions[sessionId], SIGNAL(titleChanged(const QString&)),
        this, SIGNAL(activeTitleChanged(const QString&)));

    emit sessionRaised(sessionId);

    emit activeTitleChanged(m_sessions[sessionId]->title());
}

void SessionStack::removeSession(int sessionId)
{
    if (sessionId == -1) sessionId = m_activeSessionId;
    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    if (queryClose(sessionId, QueryCloseSession))
        m_sessions[sessionId]->deleteLater();
}

void SessionStack::removeTerminal(int terminalId)
{
    int sessionId = sessionIdForTerminalId(terminalId);

    if (terminalId == -1)
    {
        if (m_activeSessionId == -1) return;
        if (!m_sessions.contains(m_activeSessionId)) return;

        if (m_sessions[m_activeSessionId]->isSessionClosable())
            m_sessions[m_activeSessionId]->closeTerminal();
    }
    else
    {
        if (m_sessions[sessionId]->isSessionClosable())
            m_sessions[sessionId]->closeTerminal(terminalId);
    }
}

void SessionStack::closeActiveTerminal(int sessionId)
{
    if (sessionId == -1) sessionId = m_activeSessionId;
    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    if (queryClose(sessionId, QueryCloseTerminal))
        m_sessions[sessionId]->closeTerminal();
}


void SessionStack::cleanup(int sessionId)
{
    if (sessionId == m_activeSessionId) m_activeSessionId = -1;

    m_sessions.remove(sessionId);

    emit sessionRemoved(sessionId);
}

int SessionStack::activeTerminalId()
{
    if (!m_sessions.contains(m_activeSessionId)) return -1;

    return m_sessions[m_activeSessionId]->activeTerminalId();
}

const QString SessionStack::sessionIdList()
{
    QList<int> keyList = m_sessions.uniqueKeys();
    QStringList idList;

    for (int i = 0; i < keyList.size(); ++i) 
        idList << QString::number(keyList.at(i));

    return idList.join(",");
}

const QString SessionStack::terminalIdList()
{
    QStringList idList;

    QHashIterator<int, Session*> it(m_sessions);

    while (it.hasNext())
    {
        it.next();

        idList << it.value()->terminalIdList();
    }

    return idList.join(",");
}

const QString SessionStack::terminalIdsForSessionId(int sessionId)
{
    if (!m_sessions.contains(sessionId)) return QString::number(-1);

    return m_sessions[sessionId]->terminalIdList();
}

int SessionStack::sessionIdForTerminalId(int terminalId)
{
    int sessionId = -1;

    QHashIterator<int, Session*> it(m_sessions);

    while (it.hasNext())
    {
        it.next();

        if (it.value()->hasTerminal(terminalId))
        {
            sessionId = it.key();
            break;
        }
    }

    return sessionId;
}

void SessionStack::runCommand(const QString& command)
{
    if (m_activeSessionId == -1) return;
    if (!m_sessions.contains(m_activeSessionId)) return;

    m_sessions[m_activeSessionId]->runCommand(command);
}

void SessionStack::runCommandInTerminal(int terminalId, const QString& command)
{
    QHashIterator<int, Session*> it(m_sessions);

    while (it.hasNext()) 
    {
        it.next();

        it.value()->runCommand(command, terminalId);
    }
}

bool SessionStack::isKeyboardInputEnabled(int sessionId)
{
    if (sessionId == -1) sessionId = m_activeSessionId;
    if (sessionId == -1) return false;
    if (!m_sessions.contains(sessionId)) return false;

    return m_sessions[sessionId]->isKeyboardInputEnabled();
}

void SessionStack::setKeyboardInputEnabled(int sessionId, bool keyboardInputEnabled)
{
    if (sessionId == -1) sessionId = m_activeSessionId;
    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    m_sessions[sessionId]->setKeyboardInputEnabled(keyboardInputEnabled);
}

bool SessionStack::isSessionClosable(int sessionId)
{
    if (sessionId == -1) sessionId = m_activeSessionId;
    if (sessionId == -1) return false;
    if (!m_sessions.contains(sessionId)) return false;

    return m_sessions[sessionId]->isSessionClosable();
}

void SessionStack::setSessionClosable(int sessionId, bool sessionClosable)
{
    if (sessionId == -1) sessionId = m_activeSessionId;
    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    m_sessions[sessionId]->setSessionClosable(sessionClosable);
}

bool SessionStack::hasUnclosableSessions() const
{
    QHashIterator<int, Session*> it(m_sessions);

    while (it.hasNext())
    {
        it.next();
        if (!it.value()->isSessionClosable())
            return true;
    }

    return false;
}

void SessionStack::editProfile(int sessionId)
{
    if (sessionId == -1) sessionId = m_activeSessionId;
    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    m_sessions[sessionId]->editProfile();
}

void SessionStack::splitSessionLeftRight(int sessionId)
{
    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    m_sessions[sessionId]->splitLeftRight();
}

void SessionStack::splitSessionTopBottom(int sessionId)
{
    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    m_sessions[sessionId]->splitTopBottom();
}

void SessionStack::splitTerminalLeftRight(int terminalId)
{
    int sessionId = sessionIdForTerminalId(terminalId);

    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    m_sessions[sessionId]->splitLeftRight(terminalId);
}

void SessionStack::splitTerminalTopBottom(int terminalId)
{
    int sessionId = sessionIdForTerminalId(terminalId);

    if (sessionId == -1) return;
    if (!m_sessions.contains(sessionId)) return;

    m_sessions[sessionId]->splitTopBottom(terminalId);
}

void SessionStack::emitTitles()
{
    QString title;

    QHashIterator<int, Session*> it(m_sessions);

    while (it.hasNext()) 
    {
        it.next();

        title = it.value()->title();

        if (!title.isEmpty()) 
            emit titleChanged(it.value()->id(), title);
    }
}

bool SessionStack::queryClose(int sessionId, QueryCloseType type)
{
    if (!m_sessions[sessionId]->isSessionClosable())
    {
        QString closeQuestionIntro = i18nc("@info", "<warning>You have locked this session to prevent accidental closing of terminals.</warning>");
        QString closeQuestion;

        if (type == QueryCloseSession)
            closeQuestion = i18nc("@info", "Are you sure you want to close this session?");
        else if (type == QueryCloseTerminal)
            closeQuestion = i18nc("@info", "Are you sure you want to close this terminal?");

        int result = KMessageBox::warningContinueCancel(this,
            closeQuestionIntro + "<br/><br/>" + closeQuestion,
            i18nc("@title:window", "Really Close?"), KStandardGuiItem::close(), KStandardGuiItem::cancel());

        if (result != KMessageBox::Continue)
            return false;
    }

    return true;
}
