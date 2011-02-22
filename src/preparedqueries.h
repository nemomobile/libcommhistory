/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Alexander Shalamov <alexander.shalamov@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef COMMHISTORY_PREPAREDQUERIES_H
#define COMMHISTORY_PREPAREDQUERIES_H

// NOTE projections in the query should have same order as Group::Property
#define GROUP_QUERY QLatin1String( \
"SELECT ?_channel" \
"        ?_subject" \
"     nie:generator(?_channel)" \
"     nie:identifier(?_channel)" \
"     nie:title(?_channel)" \
"  ?_lastDate" \
"(SELECT COUNT(?_total_messages)" \
" WHERE {" \
"  ?_total_messages nmo:communicationChannel ?_channel;" \
"                   nmo:isDeleted \"false\" ." \
" }" \
")" \
"(SELECT COUNT(?_total_unread_messages)" \
" WHERE {" \
"  ?_total_unread_messages nmo:communicationChannel ?_channel;" \
"                   nmo:isRead \"false\";" \
"                   nmo:isDeleted \"false\"" \
" }" \
")" \
"(SELECT COUNT(?_total_sent_messages)" \
" WHERE {" \
"  ?_total_sent_messages nmo:communicationChannel ?_channel;" \
"                   nmo:isSent \"true\";" \
"                   nmo:isDeleted \"false\"" \
" }" \
")" \
"  ?_lastMessage" \
"  tracker:id(?_contact_1)" \
"  fn:concat(tracker:coalesce(nco:nameGiven(?_contact_1), \'\'), \'\\u001e\'," \
"            tracker:coalesce(nco:nameFamily(?_contact_1), \'\'), \'\\u001e\'," \
"            tracker:coalesce(?_imNickname, \'\'))" \
"   tracker:coalesce(nmo:messageSubject(?_lastMessage)," \
"                    nie:plainTextContent(?_lastMessage))" \
"     nfo:fileName(nmo:fromVCard(?_lastMessage))" \
"     rdfs:label(nmo:fromVCard(?_lastMessage))" \
"     rdf:type(?_lastMessage)" \
"     nmo:deliveryStatus(?_lastMessage)" \
"  ?_lastModified " \
"WHERE" \
"{" \
"  {" \
"    SELECT ?_channel ?_subject ?_lastDate ?_lastModified" \
"         ( SELECT ?_message" \
"  WHERE {" \
"    ?_message nmo:communicationChannel ?_channel ;" \
"              nmo:isDeleted \"false\" ." \
"  }" \
"    ORDER BY DESC(nmo:receivedDate(?_message)) DESC(tracker:id(?_message)) " \
"    LIMIT 1)" \
"      AS ?_lastMessage" \
"         ( SELECT ?_contact" \
"  WHERE {" \
"        ?_contact rdf:type nco:PersonContact ." \
"      {" \
"        ?_channel nmo:hasParticipant [nco:hasIMAddress ?_12 ] ." \
"        ?_contact nco:hasAffiliation [nco:hasIMAddress ?_12 ]" \
"      }" \
"      UNION" \
"      {" \
"        ?_channel nmo:hasParticipant [nco:hasPhoneNumber [maemo:localPhoneNumber ?_16 ]] ." \
"        ?_contact nco:hasAffiliation [nco:hasPhoneNumber [maemo:localPhoneNumber ?_16 ]]" \
"      }" \
"  })" \
"" \
"      AS ?_contact_1" \
"         ( SELECT ?_13" \
"  WHERE {" \
"" \
"    ?_channel nmo:hasParticipant [nco:hasIMAddress [nco:imNickname ?_13 ]]" \
"  })" \
"" \
"      AS ?_imNickname" \
"    WHERE" \
"    {" \
"      GRAPH <commhistory:message-channels> {" \
"        ?_channel rdf:type nmo:CommunicationChannel . " \
"      }" \
"      ?_channel nie:subject ?_subject ; " \
"      nmo:lastMessageDate ?_lastDate ; " \
"      nie:contentLastModified ?_lastModified ." \
"      %1"\
"    }" \
"  }" \
"}" \
"  ORDER BY DESC(?_lastDate)" \
)

#define GROUPED_CALL_QUERY QLatin1String( \
"SELECT ?channel" \
"  ?lastCall" \
"  ?lastDate" \
"  nmo:receivedDate(?lastCall)" \
"  nco:hasContactMedium(nmo:from(?lastCall))" \
"  (SELECT nco:hasContactMedium(?lastTo) WHERE { ?lastCall nmo:to ?lastTo.})" \
"  nmo:isSent(?lastCall)" \
"  nmo:isAnswered(?lastCall)" \
"  nmo:isEmergency(?lastCall)" \
"  nmo:isRead(?lastCall)" \
"  nie:contentLastModified(?lastCall)" \
"  (SELECT GROUP_CONCAT(fn:string-join((tracker:id(?contact), nco:nameGiven(?contact), nco:nameFamily(?contact)), \"\\u001f\"), \"\\u001e\")" \
"  WHERE {" \
"    {" \
"      ?part nco:hasIMAddress ?address ." \
"      ?contact nco:hasAffiliation [ nco:hasIMAddress ?address ] ." \
"    }" \
"    UNION" \
"    {" \
"      ?part nco:hasPhoneNumber [ maemo:localPhoneNumber ?number ] ." \
"      ?contact nco:hasAffiliation [ nco:hasPhoneNumber [ maemo:localPhoneNumber ?number ] ] ." \
"    }" \
"  }) AS ?contacts" \
"  (SELECT ?nickname { ?part nco:hasIMAddress [ nco:imNickname ?nickname ] })" \
"  ?missedCalls " \
"WHERE " \
"{ " \
"  SELECT ?channel ?lastDate ?part" \
"    ( SELECT ?lastCall" \
"      WHERE {" \
"        ?lastCall a nmo:Call ." \
"        ?lastCall nmo:communicationChannel ?channel ." \
"        ?lastCall nmo:sentDate ?lastDate ." \
"      }" \
"    ) AS ?lastCall" \
"    ( SELECT COUNT(?missed)" \
"      WHERE {" \
"        ?missed a nmo:Call ." \
"        ?missed nmo:communicationChannel ?channel ." \
"        FILTER(nmo:sentDate(?missed) > nmo:lastSuccessfulMessageDate(?channel))" \
"      }" \
"    ) AS ?missedCalls" \
"" \
"  WHERE" \
"  {" \
"    GRAPH <commhistory:call-channels> {" \
"      ?channel a nmo:CommunicationChannel ." \
"    }" \
"    ?channel nmo:lastMessageDate ?lastDate ." \
"    ?channel nmo:hasParticipant ?part ." \
"  }" \
"  ORDER BY DESC(?lastDate)" \
"}" \
)

#endif
