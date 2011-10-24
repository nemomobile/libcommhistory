/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
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
"SELECT ?channel" \
"  nie:subject(?channel)" \
"  nie:generator(?channel)" \
"  nie:identifier(?channel)" \
"  nie:title(?channel)" \
"  ?_lastDate " \
"  ( SELECT COUNT(?_total_messages_1)" \
"    WHERE {" \
"      ?_total_messages_1 nmo:communicationChannel ?channel ." \
"      ?_total_messages_1 nmo:isDeleted false ." \
"  })" \
"  ( SELECT COUNT(?_total_unread_messages_1)" \
"    WHERE {" \
"      ?_total_unread_messages_1 nmo:communicationChannel ?channel ." \
"      ?_total_unread_messages_1 nmo:isRead false ." \
"      ?_total_unread_messages_1 nmo:isDeleted false ." \
"  })" \
"  ( SELECT COUNT(?_total_sent_messages_1)" \
"    WHERE {" \
"      ?_total_sent_messages_1 nmo:communicationChannel ?channel ." \
"      ?_total_sent_messages_1 nmo:isSent true ." \
"      ?_total_sent_messages_1 nmo:isDeleted false ." \
"  })" \
"  ?_lastMessage " \
"  (SELECT GROUP_CONCAT(" \
"    fn:concat(tracker:id(?contact), \"\\u001e\", " \
"              tracker:coalesce(nco:nameGiven(?contact), \"\"), \"\\u001e\", " \
"              tracker:coalesce(nco:nameFamily(?contact), \"\"), " \
"              \"\\u001d\"," \
"              tracker:coalesce(nco:nickname(?contact), \"\")," \
"              \"\\u001d\"," \
"              tracker:coalesce(?nicknames, \"\")), " \
"    \"\\u001c\") " \
"  WHERE {" \
"    SELECT ?contact " \
"    (SELECT GROUP_CONCAT(fn:string-join((nco:imID(?addr), ?nickname), \"\\u001f\"), \"\\u001e\")" \
"    WHERE {" \
"      ?contact nco:hasAffiliation [ nco:hasIMAddress ?addr ] . " \
"      ?addr nco:imNickname ?nickname . " \
"    }) AS ?nicknames " \
"    WHERE {" \
"    {" \
"      ?part nco:hasIMAddress ?address . " \
"      ?contact nco:hasAffiliation [ nco:hasIMAddress ?address ] . " \
"    } UNION {" \
"      ?part nco:hasPhoneNumber [ maemo:localPhoneNumber ?number ] . " \
"      ?contact nco:hasAffiliation [ nco:hasPhoneNumber [ maemo:localPhoneNumber ?number ] ] . " \
"    }}" \
"  }) AS ?contacts " \
"  rdf:nil " \
"  fn:string-join((nmo:messageSubject(?_lastMessage),nie:plainTextContent(?_lastMessage)),\"\\u001e\")" \
"  nfo:fileName(nmo:fromVCard(?_lastMessage))" \
"  rdfs:label(nmo:fromVCard(?_lastMessage))" \
"  rdf:type(?_lastMessage) AS ?_type " \
"  nmo:deliveryStatus(?_lastMessage) AS ?_deliveryStatus " \
"  ?_lastModified " \
"  nmo:sentDate(?_lastMessage)" \
"WHERE " \
"{" \
"  {" \
"    SELECT ?channel ?_lastDate ?_lastModified ?part" \
"      ( SELECT ?_message WHERE {" \
"        ?_message nmo:communicationChannel ?channel ." \
"        ?_message nmo:isDeleted false ." \
"        ?_message nmo:sentDate ?messageSentDate ." \
"      } ORDER BY DESC(?messageSentDate) DESC(tracker:id(?_message))" \
"    LIMIT 1) AS ?_lastMessage " \
"" \
"    WHERE" \
"    {" \
"      GRAPH <commhistory:message-channels> {" \
"        ?channel a nmo:CommunicationChannel ." \
"      }" \
"      ?channel nmo:lastMessageDate ?_lastDate ." \
"      ?channel nie:contentLastModified ?_lastModified ." \
"      ?channel nmo:hasParticipant ?part ." \
"      %1 " \
"    }" \
"  }" \
"}" \
"ORDER BY DESC(?_lastDate)" \
)

// NOTE: check CallGroupColumns enum in queryresult.h if you change this!
#define GROUPED_CALL_QUERY QLatin1String( \
"SELECT ?channel" \
"  ?lastCall" \
"  ?lastDate" \
"  nmo:receivedDate(?lastCall)" \
" (SELECT GROUP_CONCAT(tracker:coalesce(nco:imID(?medium), nco:phoneNumber(?medium), ?medium), \"\\u001e\")" \
"  WHERE {" \
"    ?lastCall nmo:from [ nco:hasContactMedium ?medium ]." \
"  })" \
" (SELECT GROUP_CONCAT(tracker:coalesce(nco:imID(?medium), nco:phoneNumber(?medium), ?medium), \"\\u001e\")" \
"  WHERE {" \
"    ?lastCall nmo:to [ nco:hasContactMedium ?medium ]." \
"  })" \
"  nmo:isSent(?lastCall)" \
"  nmo:isAnswered(?lastCall)" \
"  nmo:isEmergency(?lastCall)" \
"  nmo:isRead(?lastCall)" \
"  nie:contentLastModified(?lastCall)" \
"  (SELECT GROUP_CONCAT(" \
"    fn:concat(tracker:id(?contact), \"\\u001e\", " \
"              tracker:coalesce(nco:nameGiven(?contact), \"\"), \"\\u001e\", " \
"              tracker:coalesce(nco:nameFamily(?contact), \"\"), " \
"              \"\\u001d\"," \
"              tracker:coalesce(nco:nickname(?contact), \"\")," \
"              \"\\u001d\"," \
"              tracker:coalesce(?nicknames, \"\")), " \
"    \"\\u001c\") " \
"  WHERE {" \
"    SELECT ?contact " \
"    (SELECT GROUP_CONCAT(fn:string-join((nco:imID(?addr), ?nickname), \"\\u001f\"), \"\\u001e\")" \
"    WHERE {" \
"      ?contact nco:hasAffiliation [ nco:hasIMAddress ?addr ] . " \
"      ?addr nco:imNickname ?nickname . " \
"    }) AS ?nicknames " \
"    WHERE {" \
"    {" \
"      ?part nco:hasIMAddress ?address . " \
"      ?contact nco:hasAffiliation [ nco:hasIMAddress ?address ] . " \
"    } UNION {" \
"      ?part nco:hasPhoneNumber [ maemo:localPhoneNumber ?number ] . " \
"      ?contact nco:hasAffiliation [ nco:hasPhoneNumber [ maemo:localPhoneNumber ?number ] ] . " \
"    }}" \
"  }) AS ?contacts " \
"  rdf:nil " \
"  ?missedCalls " \
"WHERE " \
"{ " \
"  SELECT ?channel ?lastDate ?part" \
"    ( SELECT ?lastCall" \
"      WHERE {" \
"        ?lastCall a nmo:Call ." \
"        ?lastCall nmo:communicationChannel ?channel ." \
"        ?lastCall nmo:sentDate ?lastCallDate ." \
"      } ORDER BY DESC(?lastCallDate) DESC(tracker:id(?lastCall))" \
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
"    %1 " \
"  }" \
"  ORDER BY DESC(?lastDate)" \
"}" \
)

#define DELETE_EMPTY_CALL_GROUPS_QUERY QLatin1String( \
"DELETE { ?chan a rdfs:Resource } WHERE { " \
"  GRAPH <commhistory:call-channels> { " \
"    ?chan a nmo:CommunicationChannel . " \
"  } " \
"  OPTIONAL { " \
"    ?call a nmo:Call ; " \
"    nmo:communicationChannel ?chan . " \
"  } " \
"  FILTER (!BOUND(?call)) " \
"}" \
)


#endif
