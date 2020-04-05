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

#ifndef TCPBIC2_H
#define TCPBIC2_H

#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-recovery-ops.h"


namespace ns3 {

class TcpBic2 : public TcpCongestionOps
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  TcpBic2 ();

  /**
   * Copy constructor.
   * \param sock The socket to copy from.
   */
  TcpBic2 (const TcpBic2 &sock);

  virtual std::string GetName () const;
  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb,
                               uint32_t segmentsAcked);
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb,
                                uint32_t bytesInFlight);
  virtual void ReduceCwnd (Ptr<TcpSocketState> tcb);

  virtual Ptr<TcpCongestionOps> Fork ();

protected:
  /**
   * \brief Bic window update after a new ack received
   * \param tcb the socket state.
   * \returns The number of segments acked since the last cwnd increment.
   */
  virtual uint32_t Update (Ptr<TcpSocketState> tcb);

private:
  // User parameters
  bool     m_fastConvergence;  //!< Enable or disable fast convergence algorithm
  double   m_beta;             //!< Beta for cubic multiplicative increase
  uint32_t m_maxIncr;          //!< Maximum window increment
  uint32_t m_lowWnd;           //!< Lower bound on congestion window
  uint32_t m_smoothPart;       //!< Number of RTT needed to reach Wmax from Wmax-B

  // Bic parameters
  uint32_t     m_cWndCnt;         //!<  cWnd integer-to-float counter
  uint32_t     m_lastMaxCwnd;     //!<  Last maximum cWnd
  uint32_t     m_lastCwnd;        //!<  Last cWnd
  Time         m_epochStart;      //!<  Beginning of an epoch
  uint8_t      m_b;               //!< Binary search coefficient
};

} // namespace ns3
#endif // TCPBIC2_H