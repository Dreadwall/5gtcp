/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Natale Patriciello <natale.patriciello@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "tcp-bic2.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "network-slices.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpBic2");
NS_OBJECT_ENSURE_REGISTERED (TcpBic2);



// NOTE THIS NEEDS TO BE UPDATED TO MATCH NETWORK.
// IN PRACTICE THIS IS DONE VIA A SYSTEM CALL
NetSlice NETSLICE_QOSBIC = NetSlice(GBR, 0.000001, 10, 9, 5, 1000);

int STARTED = 0;



TypeId
TcpBic2::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpBic2")
    .SetParent<TcpCongestionOps> ()
    .AddConstructor<TcpBic2> ()
    .SetGroupName ("Internet")
    .AddAttribute ("FastConvergence", "Turn on/off fast convergence.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcpBic2::m_fastConvergence),
                   MakeBooleanChecker ())
    .AddAttribute ("Beta", "Beta for multiplicative decrease",
                   DoubleValue (0.8),
                   MakeDoubleAccessor (&TcpBic2::m_beta),
                   MakeDoubleChecker <double> (0.0))
    .AddAttribute ("MaxIncr", "Limit on increment allowed during binary search",
                   UintegerValue (16),
                   MakeUintegerAccessor (&TcpBic2::m_maxIncr),
                   MakeUintegerChecker <uint32_t> (1))
    .AddAttribute ("LowWnd", "Threshold window size (in segments) for engaging BIC response",
                   UintegerValue ((NETSLICE_QOSBIC.getMinimumWindow() * 1024) / 1500),
                   MakeUintegerAccessor (&TcpBic2::m_lowWnd),
                   MakeUintegerChecker <uint32_t> ())
    .AddAttribute ("SmoothPart", "Number of RTT needed to approach cWnd_max from "
                   "cWnd_max-BinarySearchCoefficient. It can be viewed as the gradient "
                   "of the slow start AIM phase: less this value is, "
                   "more steep the increment will be.",
                   UintegerValue (5),
                   MakeUintegerAccessor (&TcpBic2::m_smoothPart),
                   MakeUintegerChecker <uint32_t> (1))
    .AddAttribute ("BinarySearchCoefficient", "Inverse of the coefficient for the "
                   "binary search. Default 4, as in Linux",
                   UintegerValue (4),
                   MakeUintegerAccessor (&TcpBic2::m_b),
                   MakeUintegerChecker <uint8_t> (2))
  ;
  return tid;
}


TcpBic2::TcpBic2 ()
  : TcpCongestionOps (),
    m_cWndCnt (0),
    m_lastMaxCwnd ((NETSLICE_QOSBIC.getAverageWindow() * 1024) ),
    m_lastCwnd ((NETSLICE_QOSBIC.getAverageWindow() * 1024)),
    m_epochStart (Time::Min ())
{
  NS_LOG_FUNCTION (this);
}

TcpBic2::TcpBic2 (const TcpBic2 &sock)
  : TcpCongestionOps (sock),
    m_fastConvergence (sock.m_fastConvergence),
    m_beta (sock.m_beta),
    m_maxIncr (sock.m_maxIncr),
    m_lowWnd (sock.m_lowWnd),
    m_smoothPart (sock.m_smoothPart),
    m_cWndCnt (sock.m_cWndCnt),
    m_lastMaxCwnd (sock.m_lastMaxCwnd),
    m_lastCwnd (sock.m_lastCwnd),
    m_epochStart (sock.m_epochStart),
    m_b (sock.m_b)
{
  NS_LOG_FUNCTION (this);
}


void
TcpBic2::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if(STARTED == 0){
    tcb->m_cWnd = NETSLICE_QOSBIC.getAverageWindow() * 1024;
    segmentsAcked = 0;
    STARTED = 1;
    return;
  }


  if (tcb->m_cWnd < tcb->m_ssThresh){
    tcb->m_cWnd = NETSLICE_QOSBIC.getAverageWindow() * 1024;
    segmentsAcked = 0;

     NS_LOG_INFO ("In SlowStart recovery, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    return;
  }


  if (tcb->m_cWnd >= tcb->m_ssThresh && segmentsAcked > 0)
    {
      m_cWndCnt += segmentsAcked;
      uint32_t cnt = Update (tcb);

      /* According to the BIC paper and RFC 6356 even once the new cwnd is
       * calculated you must compare this to the number of ACKs received since
       * the last cwnd update. If not enough ACKs have been received then cwnd
       * cannot be updated.
       */
      if (m_cWndCnt > cnt)
        {
          tcb->m_cWnd += tcb->m_segmentSize;
          m_cWndCnt = 0;
          NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd);
        }
      else
        {
          NS_LOG_INFO ("Not enough segments have been ACKed to increment cwnd."
                       "Until now " << m_cWndCnt);
        }
    }
}

uint32_t
TcpBic2::Update (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  uint32_t segCwnd = tcb->GetCwndInSegments ();
  uint32_t cnt;

  m_lastCwnd = segCwnd;

  if (m_epochStart == Time::Min ())
    {
      m_epochStart = Simulator::Now ();   /* record the beginning of an epoch */
    }

  if (segCwnd < m_lowWnd)
    {
      NS_LOG_INFO ("Under lowWnd, compatibility mode. Behaving as NewReno");
      cnt = segCwnd;
      return cnt;
    }

  if (segCwnd < m_lastMaxCwnd)
    {
      double dist = (m_lastMaxCwnd - segCwnd) / m_b;

      NS_LOG_INFO ("cWnd = " << segCwnd << " under lastMax, " <<
                   m_lastMaxCwnd << " and dist=" << dist);
      if (dist > m_maxIncr)
        {
          /* Linear increase */
          cnt = segCwnd / m_maxIncr;
          NS_LOG_INFO ("Linear increase (maxIncr=" << m_maxIncr << "), cnt=" << cnt);
        }
      else if (dist <= 1)
        {
          /* smoothed binary search increase: when our window is really
           * close to the last maximum, we parameterize in m_smoothPart the number
           * of RTT needed to reach that window.
           */
          cnt = (segCwnd * m_smoothPart) / m_b;

          NS_LOG_INFO ("Binary search increase (smoothPart=" << m_smoothPart <<
                       "), cnt=" << cnt);
        }
      else
        {
          /* binary search increase */
          cnt = static_cast<uint32_t> (segCwnd / dist);

          NS_LOG_INFO ("Binary search increase, cnt=" << cnt);
        }
    }
  else
    {
      NS_LOG_INFO ("cWnd = " << segCwnd << " above last max, " <<
                   m_lastMaxCwnd);
      if (segCwnd < m_lastMaxCwnd + m_b)
        {
          /* slow start AMD linear increase */
          cnt = (segCwnd * m_smoothPart) / m_b;
          NS_LOG_INFO ("Slow start AMD, cnt=" << cnt);
        }
      else if (segCwnd < m_lastMaxCwnd + m_maxIncr * (m_b - 1))
        {
          /* slow start */
          cnt = (segCwnd * (m_b - 1)) / (segCwnd - m_lastMaxCwnd);

          NS_LOG_INFO ("Slow start, cnt=" << cnt);
        }
      else
        {
          /* linear increase */
          cnt = segCwnd / m_maxIncr;

          NS_LOG_INFO ("Linear, cnt=" << cnt);
        }
    }

  /* if in slow start or link utilization is very low. Code taken from Linux
   * kernel, not sure of the source they take it. Usually, it is not reached,
   * since if m_lastMaxCwnd is 0, we are (hopefully) in slow start.
   */
  if (m_lastMaxCwnd == 0)
    {
      if (cnt > 20) /* increase cwnd 5% per RTT */
        {
          cnt = 20;
        }
    }

  if (cnt == 0)
    {
      cnt = 1;
    }

  return cnt;
}

std::string
TcpBic2::GetName () const
{
  return "TcpBic2";
}

uint32_t
TcpBic2::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this);

  uint32_t segCwnd = tcb->GetCwndInSegments ();
  uint32_t ssThresh = NETSLICE_QOSBIC.getMinimumWindow() * 1024;

  m_epochStart = Time::Min ();

  /* Wmax and fast convergence */
  if (segCwnd < m_lastMaxCwnd && m_fastConvergence)
    {
      NS_LOG_INFO ("Fast Convergence. Last max cwnd: " << m_lastMaxCwnd <<
                   " updated to " << static_cast<uint32_t> (m_beta * segCwnd));
      m_lastMaxCwnd = static_cast<uint32_t> (m_beta * segCwnd);
    }
  else
    {
      NS_LOG_INFO ("Last max cwnd: " << m_lastMaxCwnd << " updated to " <<
                   segCwnd);
      m_lastMaxCwnd = segCwnd;
    }

  if (segCwnd < m_lowWnd)
    {
      ssThresh = std::max (2 * tcb->m_segmentSize, (bytesInFlight * 3) / 4);
      NS_LOG_INFO ("Less than lowWindow, ssTh= " << ssThresh);
    }
  else
    {
      ssThresh = static_cast<uint32_t> (std::max (segCwnd * m_beta, 2.0) * tcb->m_segmentSize);
      NS_LOG_INFO ("More than lowWindow, ssTh= " << ssThresh);
    }

  return NETSLICE_QOSBIC.getMinimumWindow() * 1024;;
}

void
TcpBic2::ReduceCwnd (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);

  if(tcb->m_cWnd.Get () > (NETSLICE_QOSBIC.getAverageWindow() * 1024)){
    tcb->m_cWnd = (unsigned int) (tcb->m_cWnd.Get () + (NETSLICE_QOSBIC.getAverageWindow() * 1024)) / 2;
  }
  else{
    tcb->m_cWnd = (unsigned int) std::max ((tcb->m_cWnd.Get () - ((tcb->m_cWnd.Get () - (NETSLICE_QOSBIC.getAverageWindow() * 1024)) / 2)),
      (NETSLICE_QOSBIC.getMinimumWindow() * 1024));
  }
}

Ptr<TcpCongestionOps>
TcpBic2::Fork (void)
{
  return CopyObject<TcpBic2> (this);
}

} // namespace ns3