#include "tcp-cmu.h"
#include "tcp-socket-state.h"
#include "network-slices.h"

#include "ns3/log.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpCMU");
NS_OBJECT_ENSURE_REGISTERED (TcpCMU);

// NOTE THIS NEEDS TO BE UPDATED TO MATCH NETWORK.
// IN PRACTICE THIS IS DONE VIA A SYSTEM CALL
NetSlice NETSLICE_QOS = NetSlice(GBR, 0.000001, NA, 750, 300, 1000)


TypeId
TcpCMU::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpCMU")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpCMU> ()
    .SetGroupName ("Internet")
  ;
  return tid;
}

TcpCMU::TcpCMU (void)
  : TcpNewReno (),
    m_baseRtt (NETSLICE_QOS.getAverageTimeout()),
    m_minRtt (NETSLICE_QOS.getAverageTimeout()),
    m_cntRtt (0),
    m_diff (0),
    m_inc (true),
    m_ackCnt (0),
{
  NS_LOG_FUNCTION (this);
}

TcpCMU::TcpCMU (const TcpVeno& sock)
  : TcpNewReno (sock),
    m_baseRtt (sock.m_baseRtt),
    m_minRtt (sock.m_minRtt),
    m_cntRtt (sock.m_cntRtt),
    m_diff (0),
    m_inc (true),
    m_ackCnt (sock.m_ackCnt),
    m_beta (sock.m_beta)
{
  NS_LOG_FUNCTION (this);
}

TcpCMU::~TcpCMU (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps>
TcpCMU::Fork (void)
{
  return CopyObject<TcpCMU> (this);
}


void
TcpCMU::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                    const Time& rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);

  if (rtt.IsZero ())
    {
      return;
    }

  m_minRtt = std::min (m_minRtt, rtt);
  NS_LOG_DEBUG ("Updated m_minRtt= " << m_minRtt);


  m_baseRtt = std::min (m_baseRtt, rtt);
  NS_LOG_DEBUG ("Updated m_baseRtt= " << m_baseRtt);

  // Update RTT counter
  m_cntRtt++;
  NS_LOG_DEBUG ("Updated m_cntRtt= " << m_cntRtt);
}


void
TcpCMU::CongestionStateSet (Ptr<TcpSocketState> tcb,
                             const TcpSocketState::TcpCongState_t newState)
{
  NS_LOG_FUNCTION (this << tcb << newState);
}

void
TcpCMU::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  // Always calculate m_diff, even if we are not doing Veno now
  uint32_t targetCwnd;
  uint32_t segCwnd = tcb->GetCwndInSegments ();

  /*
   * Calculate the cwnd we should have. baseRtt is the minimum RTT
   * per-connection, minRtt is the minimum RTT in this window
   *
   * little trick:
   * desidered throughput is currentCwnd * baseRtt
   * target cwnd is throughput / minRtt
   */
  double tmp = m_baseRtt.GetSeconds () / m_minRtt.GetSeconds ();
  targetCwnd = static_cast<uint32_t> (segCwnd * tmp);
  NS_LOG_DEBUG ("Calculated targetCwnd = " << targetCwnd);
  NS_ASSERT (segCwnd >= targetCwnd); // implies baseRtt <= minRtt

  // Calculate the difference between actual and target cwnd
  m_diff = segCwnd - targetCwnd;
  NS_LOG_DEBUG ("Calculated m_diff = " << m_diff);

  double adder = static_cast<double> (tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
  adder = std::max (1.0, adder);
  uint32_t renotcp = tcb->m_cWnd + static_cast<uint32_t> (adder);


  if (tcb->m_cWnd < tcb->m_ssThresh)
    { // Slow start mode. Veno employs same slow start algorithm as NewReno's.

      NS_LOG_DEBUG ("NewReno Window : " << renotcp);
      NS_LOG_DEBUG ("Our TCP : " << NETSLICE_QOS.getAverageWindow() * 1024 );

      tcb->m_cWnd = NETSLICE_QOS.getAverageWindow() * 1024 ;
    }
  else
  { // Congestion avoidance mode
      tcb->m_cWnd = renotcp;
  }
}

std::string
TcpCMU::GetName () const
{
  return "TcpCMU";
}

uint32_t
TcpCMU::GetSsThresh (Ptr<const TcpSocketState> tcb,
                      uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);

  uint32_t baseline = TcpNewReno::GetSsThresh (tcb, bytesInFlight);
  NS_LOG_LOGIC ("NewReno ssthresh: " << static_cast<uint32_t> (baseline));
  NS_LOG_LOGIC ("CMU ssthresh: " << static_cast<uint32_t> NETSLICE_QOS.getMinimumWindow() * 1024);

  return NETSLICE_QOS.getMinimumWindow() * 1024;
}

} // namespace ns3


