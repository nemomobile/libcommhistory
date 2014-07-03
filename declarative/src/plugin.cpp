/* Copyright (C) 2015 Jolla Ltd.
 * Copyright (C) 2012 John Brooks <john.brooks@dereferenced.net>
 * Contact: Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "plugin.h"
#include "constants.h"
#include "groupobject.h"
#include "eventmodel.h"
#include "groupmodel.h"
#include "contactgroupmodel.h"
#include "callproxymodel.h"
#include "conversationproxymodel.h"
#include "recentcontactsmodel.h"
#include "declarativegroupmanager.h"
#include "draftsmodel.h"
#include "draftevent.h"
#include "mmshelper.h"

#include <QtQml>

#define COMM_HISTORY_PLUGIN_URI    QLatin1String("org.nemomobile.commhistory")

void CommHistoryPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_ASSERT(uri == COMM_HISTORY_PLUGIN_URI);
    Q_UNUSED(uri);
    Q_UNUSED(engine);
}

void CommHistoryPlugin::registerTypes(const char *uri)
{
    Q_ASSERT(uri == COMM_HISTORY_PLUGIN_URI);

    qmlRegisterUncreatableType<CommHistoryConstants>(uri, 1, 0, "CommHistory", "Constants-only type");
    qmlRegisterType<CommHistory::EventModel>(uri, 1, 0, "CommEventModel");
    qmlRegisterType<CommHistory::GroupModel>(uri, 1, 0, "CommGroupModel");
    qmlRegisterType<CallProxyModel>(uri, 1, 0, "CommCallModel");
    qmlRegisterType<ConversationProxyModel>(uri, 1, 0, "CommConversationModel");
    qmlRegisterType<CommHistory::ContactGroupModel>(uri, 1, 0, "CommContactGroupModel");
    qmlRegisterType<CommHistory::RecentContactsModel>(uri, 1, 0, "CommRecentContactsModel");
    qmlRegisterType<DeclarativeGroupManager>(uri, 1, 0, "CommGroupManager");
    qmlRegisterType<CommHistory::DraftsModel>(uri, 1, 0, "DraftsModel");
    qmlRegisterType<DraftEvent>(uri, 1, 0, "DraftEvent");
    qmlRegisterType<MmsHelper>(uri, 1, 0, "MmsHelper");

    qmlRegisterType<CommHistory::GroupObject>();
    qmlRegisterType<CommHistory::ContactGroup>();
}
