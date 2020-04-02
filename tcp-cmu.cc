#include "tcp-cmu.h"
#include "tcp-socket-state.h"
#include "network-slices.h"

#include "ns3/log.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpCMU");
NS_OBJECT_ENSURE_REGISTERED (TcpCMU);

// NOTE THIS NEEDS TO BE UPDATED TO MATCH NETWORK.
// IN PRACTICE THIS IS DONE VIA A SYSTEM CALL
NetSlice NETSLICE_QOS = NetSlice(GBR, 0.000001, NA, 750, 300, 1000);


TypeId TcpCMU::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpCMU")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpCMU> ()
    .SetGroupName ("Internet")
  ;
  return tid;
}

TcpCMU::TcpCMU (void)
  : TcpNewReno ()
{
  NS_LOG_FUNCTION (this);
}

TcpCMU::TcpCMU (const TcpCMU& sock)
  : TcpNewReno (sock)
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

  double adder = static_cast<double> (tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
  adder = std::max (1.0, adder);
  uint32_t renotcp = tcb->m_cWnd + static_cast<uint32_t> (adder);


  if (tcb->m_cWnd < tcb->m_ssThresh)
    { // Slow start mode. Veno employs same slow start algorithm as NewReno's.

      NS_LOG_DEBUG ("NewReno Window : " << renotcp);
      NS_LOG_DEBUG ("Our TCP : " << (NETSLICE_QOS.getAverageWindow() * 1024) );

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
  NS_LOG_LOGIC ("CMU ssthresh: " << static_cast<uint32_t> (NETSLICE_QOS.getMinimumWindow() * 1024));

  return NETSLICE_QOS.getMinimumWindow() * 1024;
}

} // namespace ns3


