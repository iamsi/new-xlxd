//
//  cpacketstream.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 06/11/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include "main.h"
#include "cpacketstream.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CPacketStream::CPacketStream()
{
	m_bOpen = false;
	m_uiStreamId = 0;
	m_uiPacketCntr = 0;
	m_OwnerClient = nullptr;
#ifdef TRANSCODER_IP
	m_CodecStream = nullptr;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// open / close

bool CPacketStream::OpenPacketStream(const CDvHeaderPacket &DvHeader, std::shared_ptr<CClient>client)
{
	bool ok = false;

	// not already open?
	if ( !m_bOpen )
	{
		// update status
		m_bOpen = true;
		m_uiStreamId = DvHeader.GetStreamId();
		m_uiPacketCntr = 0;
		m_DvHeader = DvHeader;
		m_OwnerClient = client;
		m_LastPacketTime.Now();
#ifdef TRANSCODER_IP
		if (std::string::npos != std::string(TRANSCODED_MODULES).find(DvHeader.GetRpt2Module()))
			m_CodecStream = g_Transcoder.GetCodecStream(this, client->GetCodec());
		else
			m_CodecStream = g_Transcoder.GetCodecStream(this, CODEC_NONE);
#endif
		ok = true;
	}
	return ok;
}

void CPacketStream::ClosePacketStream(void)
{
	// update status
	m_bOpen = false;
	m_uiStreamId = 0;
	m_OwnerClient = nullptr;
#ifdef TRANSCODER_IP
	g_Transcoder.ReleaseStream(m_CodecStream);
	m_CodecStream = nullptr;
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// push & pop

void CPacketStream::Push(std::unique_ptr<CPacket> Packet)
{
	// update stream dependent packet data
	m_LastPacketTime.Now();
	Packet->UpdatePids(m_uiPacketCntr++);
	// transcoder avaliable ?
#ifdef TRANSCODER_IP
	if ( m_CodecStream != nullptr )
	{
		// todo: verify no possibilty of double lock here
		m_CodecStream->Lock();
		{
			// transcoder ready & frame need transcoding ?
			if ( m_CodecStream->IsConnected() && Packet->HasTranscodableAmbe() )
			{
				// yes, push packet to trancoder queue
				// trancoder will push it after transcoding
				// is completed
				m_CodecStream->push(Packet);
			}
			else
			{
				// no, just bypass tarnscoder
				push(Packet);
			}
		}
		m_CodecStream->Unlock();
	}
	else
#endif
	{
		// otherwise, push direct push
		push(Packet);
	}
}

bool CPacketStream::IsEmpty(void) const
{
#ifdef TRANSCODER_IP
	bool bEmpty = empty();
	// also check no packets still in Codec stream's queue
	if ( bEmpty && (m_CodecStream != nullptr) )
	{
		bEmpty = m_CodecStream->IsEmpty();
	}

	// done
	return bEmpty;
#else
	return empty();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////
// get

const CIp *CPacketStream::GetOwnerIp(void)
{
	if ( m_OwnerClient != nullptr )
	{
		return &(m_OwnerClient->GetIp());
	}
	return nullptr;
}
